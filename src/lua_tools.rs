//! Persistent Lua sessions and offline decoding using the runtime's wire parser.
use crate::{
  lua::{self, Action, Status},
  protocol::packet::Response,
  transport::DeviceConnection,
};
use bluer::Address;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::{
  error::Error,
  io::{BufRead, Write},
  path::Path,
  time::Duration,
};
use tokio::time::{sleep, Instant};

type Result<T> = std::result::Result<T, Box<dyn Error>>;

/// Decode raw-run JSONL, Lua status JSONL, or a single hexadecimal payload.
/// Non-Lua records in a raw transcript are skipped; malformed Lua records fail.
pub fn decode_lines(reader: impl BufRead, mut output: impl Write) -> Result<usize> {
  let mut count = 0;
  for (index, line) in reader.lines().enumerate() {
    let line = line?;
    if line.trim().is_empty() {
      continue;
    }
    let result = decode_line(&line).map_err(|e| format!("line {}: {e}", index + 1))?;
    if let Some(status) = result {
      writeln!(output, "{}", json!({"line":index+1,"status":status}))?;
      count += 1;
    }
  }
  if count == 0 {
    return Err("No Lua responses found".into());
  }
  Ok(count)
}

fn decode_line(line: &str) -> Result<Option<Value>> {
  let (hex, ack, command) = if line.trim_start().starts_with('{') {
    let value: Value = serde_json::from_str(line)?;
    // Already-decoded output is useful when concatenating old and new captures.
    let status = value.get("status").unwrap_or(&value);
    if status.get("abi").is_some() && status.get("state").is_some() {
      return Ok(Some(status.clone()));
    }
    let response = value.get("response").unwrap_or(&value);
    let Some(hex) = response.get("data_hex").and_then(Value::as_str) else {
      return Ok(None);
    };
    let command = response
      .get("opcode")
      .or_else(|| response.get("original_command"))
      .or_else(|| response.get("command"));
    let command = match command {
      Some(Value::Number(n)) => n.as_u64().ok_or("Invalid response command")?,
      Some(Value::String(s)) => u64::from_str_radix(s.strip_prefix("0x").unwrap_or(s), 16)?,
      _ => 0x37,
    };
    (
      hex.to_owned(),
      response.get("ack").and_then(Value::as_bool).unwrap_or(true),
      command,
    )
  } else {
    (line.to_owned(), true, 0x37)
  };
  let data = crate::control::hex_bytes(&hex)?;
  if command != 0x37 || !data.starts_with(b"DLUA") {
    return Ok(None);
  }
  Ok(Some(serde_json::to_value(lua::decode(Response {
    original_command: 0x37,
    ack,
    data,
  })?)?))
}

fn default_poll() -> u64 {
  200
}
fn default_wait() -> u64 {
  5000
}

#[derive(Debug, Deserialize, Serialize)]
#[serde(tag = "op", rename_all = "kebab-case", deny_unknown_fields)]
pub enum Step {
  Start {
    file: String,
  },
  Run {
    file: String,
  },
  Eval {
    source: String,
  },
  Status,
  Stop,
  Pause,
  Resume,
  Send {
    message: String,
  },
  Receive,
  Sleep {
    ms: u64,
  },
  Wait {
    #[serde(flatten)]
    condition: Condition,
    #[serde(default = "default_wait")]
    timeout_ms: u64,
    #[serde(default = "default_poll")]
    interval_ms: u64,
  },
}

#[derive(Debug, Default, Deserialize, Serialize)]
pub struct Condition {
  pub state: Option<String>,
  pub result: Option<String>,
  pub result_contains: Option<String>,
  pub memory_bytes: Option<u32>,
}

impl Condition {
  fn validate(&self) -> Result<()> {
    if self.state.is_none()
      && self.result.is_none()
      && self.result_contains.is_none()
      && self.memory_bytes.is_none()
    {
      return Err(
        "wait requires a state, result, result_contains or memory_bytes condition".into(),
      );
    }
    if let Some(s) = &self.state {
      if ![
        "idle",
        "running",
        "done",
        "error",
        "active",
        "paused",
        "uploading",
      ]
      .contains(&s.as_str())
      {
        return Err(format!("Unknown Lua state {s}").into());
      }
    }
    Ok(())
  }

  fn matches(&self, status: &Status) -> bool {
    self.state.as_ref().is_none_or(|v| v == status.state)
      && self.result.as_ref().is_none_or(|v| v == &status.result)
      && self
        .result_contains
        .as_ref()
        .is_none_or(|v| status.result.contains(v))
      && self.memory_bytes.is_none_or(|v| v == status.memory_bytes)
  }
}

pub struct Sequence {
  steps: Vec<(Step, Option<Vec<u8>>)>,
}

impl Sequence {
  /// Resolve and validate every source and step before opening a transport.
  pub fn load(path: &Path) -> Result<Self> {
    let steps: Vec<Step> = serde_json::from_slice(&std::fs::read(path)?)?;
    if steps.is_empty() || steps.len() > 1024 {
      return Err("Sequence requires 1..1024 steps".into());
    }
    let mut prepared = Vec::new();
    for (i, step) in steps.into_iter().enumerate() {
      let source = match &step {
        Step::Start { file } | Step::Run { file } => Some(crate::lua_bundle::bundle(
          &path.parent().unwrap_or(Path::new(".")).join(file),
        )?),
        Step::Eval { source } => Some(source.as_bytes().to_vec()),
        Step::Send { message } if message.is_empty() || message.len() > 128 => {
          return Err(format!("step {}: message requires 1..128 bytes", i + 1).into())
        }
        Step::Sleep { ms } if *ms > 60000 => return Err("sleep must be at most 60000 ms".into()),
        Step::Wait {
          condition,
          timeout_ms,
          interval_ms,
        } => {
          condition.validate()?;
          if !(1..=60000).contains(timeout_ms) || !(20..=60000).contains(interval_ms) {
            return Err("wait timeout must be 1..60000 ms; interval 20..60000 ms".into());
          }
          None
        }
        _ => None,
      };
      if let Some(bytes) = &source {
        if bytes.is_empty() || bytes.len() > lua::APP_SOURCE_LIMIT {
          return Err(
            format!(
              "step {}: source requires 1..{} bytes",
              i + 1,
              lua::APP_SOURCE_LIMIT
            )
            .into(),
          );
        }
      }
      prepared.push((step, source));
    }
    Ok(Self { steps: prepared })
  }

  pub fn describe(&self) -> Value {
    json!({"steps":self.steps.iter().map(|(s,source)| json!({"step":s,"source_bytes":source.as_ref().map(Vec::len)})).collect::<Vec<_>>()})
  }
}

fn emit(value: Value) -> Result<()> {
  let mut out = std::io::stdout().lock();
  writeln!(out, "{value}")?;
  out.flush()?;
  Ok(())
}

fn same_observation(a: &Status, b: &Status) -> bool {
  a.state == b.state
    && a.result == b.result
    && a.generation == b.generation
    && a.held_keys == b.held_keys
    && a.dropped_keys == b.dropped_keys
}

async fn keepalive_sleep(conn: &mut DeviceConnection, duration: Duration) -> Result<()> {
  let deadline = Instant::now() + duration;
  while let Some(remaining) = deadline.checked_duration_since(Instant::now()) {
    sleep(remaining.min(Duration::from_secs(5))).await;
    if Instant::now() < deadline {
      lua::execute(conn, Action::Status).await?;
    }
  }
  Ok(())
}

async fn watch_session(
  conn: &mut DeviceConnection,
  interval: Duration,
  all: bool,
  observed: &mut bool,
) -> Result<()> {
  let started = Instant::now();
  let mut previous: Option<Status> = None;
  loop {
    let status = lua::execute(conn, Action::Status).await?;
    *observed = true;
    if all
      || previous
        .as_ref()
        .is_none_or(|v| !same_observation(v, &status))
    {
      emit(json!({"elapsed_ms":started.elapsed().as_millis(),"status":status}))?;
    }
    if status.state == "error" {
      return Err(format!("Lua: {}", status.result).into());
    }
    previous = Some(status);
    keepalive_sleep(conn, interval).await?;
  }
}

pub async fn watch(
  address: Address,
  interval: Duration,
  duration: Duration,
  all: bool,
) -> Result<()> {
  if interval < Duration::from_millis(20)
    || interval > Duration::from_secs(60)
    || duration.is_zero()
    || duration > Duration::from_secs(86400)
  {
    return Err("watch interval must be 20..60000 ms; duration 1..86400 seconds".into());
  }
  let mut conn = DeviceConnection::connect(address).await?;
  let mut observed = false;
  let result = tokio::select! {
    result = watch_session(&mut conn,interval,all,&mut observed) => result,
    _ = sleep(duration) => if observed { Ok(()) } else { Err("Watch deadline expired without a Lua reply".into()) },
    signal = tokio::signal::ctrl_c() => signal.map_err(Into::into),
  };
  if let Err(e) = conn.disconnect().await {
    log::warn!("Lua watch cleanup: {e}");
  }
  result
}

async fn sequence_session(conn: &mut DeviceConnection, sequence: &Sequence) -> Result<()> {
  for (index, (step, source)) in sequence.steps.iter().enumerate() {
    let bytes = source.as_deref().unwrap_or_default();
    let action = match step {
      Step::Start { .. } => Action::Start(bytes),
      Step::Run { .. } | Step::Eval { .. } => Action::Run(bytes),
      Step::Status => Action::Status,
      Step::Stop => Action::Stop,
      Step::Pause => Action::Pause,
      Step::Resume => Action::Resume,
      Step::Send { message } => Action::Send(message.as_bytes()),
      Step::Receive => Action::Receive,
      Step::Sleep { ms } => {
        keepalive_sleep(conn, Duration::from_millis(*ms)).await?;
        emit(json!({"step":index+1,"op":"sleep","ms":ms}))?;
        continue;
      }
      Step::Wait {
        condition,
        timeout_ms,
        interval_ms,
      } => {
        let wait = async {
          loop {
            let status = lua::execute(conn, Action::Status).await?;
            if condition.matches(&status) {
              return Ok::<_, Box<dyn Error>>(status);
            }
            if status.state == "error" {
              return Err(format!("Lua: {}", status.result).into());
            }
            keepalive_sleep(conn, Duration::from_millis(*interval_ms)).await?;
          }
        };
        let status = tokio::time::timeout(Duration::from_millis(*timeout_ms), wait)
          .await
          .map_err(|_| format!("step {}: wait timed out after {timeout_ms} ms", index + 1))??;
        emit(json!({"step":index+1,"op":"wait","status":status}))?;
        continue;
      }
    };
    let status = lua::execute(conn, action).await?;
    emit(json!({"step":index+1,"op":serde_json::to_value(step)?["op"],"status":status}))?;
    if status.state == "error" {
      return Err(format!("step {}: Lua: {}", index + 1, status.result).into());
    }
  }
  Ok(())
}

pub async fn sequence(address: Address, sequence: &Sequence, timeout: Duration) -> Result<()> {
  if timeout.is_zero() || timeout > Duration::from_secs(3600) {
    return Err("Sequence timeout must be 1..3600 seconds".into());
  }
  let mut conn = DeviceConnection::connect(address).await?;
  let result = tokio::select! {
    result = sequence_session(&mut conn,sequence) => result,
    _ = sleep(timeout) => Err("Sequence deadline exceeded; remaining steps were not sent".into()),
    signal = tokio::signal::ctrl_c() => match signal {
      Ok(()) => Err("Sequence interrupted; remaining steps were not sent".into()),
      Err(e) => Err(e.into()),
    },
  };
  if let Err(e) = conn.disconnect().await {
    log::warn!("Lua sequence cleanup: {e}");
  }
  result
}

#[cfg(test)]
mod tests {
  use super::*;

  #[test]
  fn transcript_decoder_uses_abi_and_preserves_line_numbers() -> Result<()> {
    let mut data = b"DLUA\x02\x04\x00\x02".to_vec();
    data.resize(40, 0);
    data.extend(b"ok");
    let transcript = format!(
      "{{\"event\":\"preflight\"}}\n{}\n{}\n",
      json!({"response":{"opcode":55,"ack":true,"data_hex":hex::encode(&data)}}),
      json!({"response":{"opcode":56,"ack":true,"data_hex":hex::encode(&data)}})
    );
    let mut output = Vec::new();
    assert_eq!(decode_lines(transcript.as_bytes(), &mut output)?, 1);
    let result: Value = serde_json::from_slice(&output)?;
    assert_eq!(result["line"], 2);
    assert_eq!(result["status"]["result"], "ok");
    data.pop();
    assert!(decode_lines(hex::encode(data).as_bytes(), Vec::new()).is_err());
    assert!(decode_lines(b"444c5541".as_slice(), Vec::new()).is_err());
    assert!(decode_lines(b"{}".as_slice(), Vec::new()).is_err());
    Ok(())
  }

  #[test]
  fn wait_conditions_and_unknown_fields_are_checked() -> Result<()> {
    let step: Step = serde_json::from_str(r#"{"op":"wait","state":"active","result":"ready"}"#)?;
    if let Step::Wait {
      condition,
      timeout_ms,
      interval_ms,
    } = step
    {
      condition.validate()?;
      assert_eq!((timeout_ms, interval_ms), (5000, 200));
    } else {
      return Err("Wrong step".into());
    }
    assert!(serde_json::from_str::<Step>(r#"{"op":"wait","sttae":"active"}"#).is_err());
    assert!(Condition::default().validate().is_err());
    assert!(Condition {
      state: Some("typo".into()),
      ..Default::default()
    }
    .validate()
    .is_err());
    Ok(())
  }
}
