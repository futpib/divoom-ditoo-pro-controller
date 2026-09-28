//! Command-level access without an editor or phone UI.
use crate::{
  protocol::{
    command::Command,
    packet::{Packet, Response},
  },
  transport::DeviceConnection,
};
use bluer::Address;
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::{error::Error, time::Duration};

#[derive(Debug, Deserialize, Serialize)]
pub struct CommandInfo {
  pub name: String,
  pub opcode: u8,
  pub prefix: Vec<u8>,
  pub source: String,
  pub source_line: usize,
  pub support: String,
}

pub fn catalogue() -> Result<Vec<CommandInfo>, Box<dyn Error>> {
  Ok(serde_json::from_str(include_str!(
    "../data/protocol-commands.json"
  ))?)
}

pub fn hex_bytes(value: &str) -> Result<Vec<u8>, Box<dyn Error>> {
  let compact: String = value
    .split_whitespace()
    .map(|s| s.strip_prefix("0x").unwrap_or(s))
    .collect();
  Ok(hex::decode(compact)?)
}

fn resolve(value: &str) -> Result<(u8, Vec<u8>), Box<dyn Error>> {
  if let Some(entry) = catalogue()?
    .into_iter()
    .find(|entry| entry.name.eq_ignore_ascii_case(value))
  {
    return Ok((entry.opcode, entry.prefix));
  }
  let code = if let Some(hex) = value.strip_prefix("0x") {
    u8::from_str_radix(hex, 16)?
  } else {
    value.parse::<u8>().map_err(|_| {
      format!("Unknown command {value}; use raw list or an opcode such as 0x37")
    })?
  };
  Ok((code, Vec::new()))
}

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Request {
  pub command: String,
  #[serde(default)]
  pub payload_hex: String,
  /// Reply command name/opcode; None sends without waiting for a command reply.
  #[serde(default)]
  pub response: Option<String>,
  /// Additional prefix after any extended command selector.
  #[serde(default)]
  pub response_prefix_hex: String,
  #[serde(default)]
  pub delay_ms: u64,
  /// Collect additional responses/events on this connection after sending.
  #[serde(default)]
  pub listen_ms: u64,
}

pub struct PreparedRequest {
  packet: Packet,
  expected: Option<(u8, Vec<u8>)>,
  delay: Duration,
  listen: Duration,
}

impl Request {
  pub fn bytes(opcode: u8, data: &[u8], query: bool) -> Self {
    let command = format!("0x{opcode:02x}");
    Self {
      response: query.then(|| command.clone()),
      command,
      payload_hex: hex::encode(data),
      response_prefix_hex: String::new(),
      delay_ms: 0,
      listen_ms: 0,
    }
  }

  /// Validate the entire request before opening a connection.
  pub fn prepare(&self) -> Result<PreparedRequest, Box<dyn Error>> {
    let (code, mut payload) = resolve(&self.command)?;
    payload.extend(hex_bytes(&self.payload_hex)?);
    if payload.len() > 65527 {
      return Err("Payload exceeds the BLE framing length limit".into());
    }
    if self.delay_ms > 60000 || self.listen_ms > 60000 {
      return Err("Delay and listen duration must each be at most 60000 ms".into());
    }
    let expected = if let Some(name) = &self.response {
      let (code, mut prefix) = resolve(name)?;
      prefix.extend(hex_bytes(&self.response_prefix_hex)?);
      Some((code, prefix))
    } else {
      if !self.response_prefix_hex.is_empty() {
        return Err("Response prefix requires a response command".into());
      }
      None
    };
    Ok(PreparedRequest {
      packet: Packet {
        command: Command::Raw(code),
        payload,
      },
      expected,
      delay: Duration::from_millis(self.delay_ms),
      listen: Duration::from_millis(self.listen_ms),
    })
  }
}

impl PreparedRequest {
  pub fn describe(&self) -> Value {
    json!({"opcode":self.packet.command.value(), "payload_hex":hex::encode(&self.packet.payload),
      "delay_ms":self.delay.as_millis(), "listen_ms":self.listen.as_millis(),
      "rfcomm_frame_hex":self.packet.serialize().ok().map(hex::encode),
      "response":self.expected.as_ref().map(|(code,prefix)| json!({"opcode":code,"prefix_hex":hex::encode(prefix)}))})
  }
}

pub fn response_json(response: &Response) -> Value {
  let mut value = json!({"opcode":response.original_command,"ack":response.ack,"data_hex":hex::encode(&response.data)});
  let d = &response.data;
  if response.ack {
    match response.original_command {
      0x37 if !d.is_empty() && d.len() == 1 + d[0] as usize * 4 => {
        let versions: Vec<u32> = d[1..]
          .chunks_exact(4)
          .map(|b| u32::from_le_bytes([b[0], b[1], b[2], b[3]]))
          .collect();
        value["firmware_versions"] = json!(versions);
      }
      0x46 if d.len() >= 11 => {
        value["mode"] = json!(d[0]);
        value["brightness"] = json!(d[10]);
      }
      0x09 if d.len() == 1 => value["volume"] = json!(d[0]),
      _ => (),
    }
  }
  value
}

pub struct Session {
  connection: DeviceConnection,
}
impl Session {
  pub async fn connect(address: Address) -> Result<Self, Box<dyn Error>> {
    Ok(Self {
      connection: DeviceConnection::connect(address).await?,
    })
  }

  pub async fn execute(&mut self, request: &PreparedRequest) -> Result<Value, Box<dyn Error>> {
    let response = if let Some((code, prefix)) = &request.expected {
      Some(response_json(
        &self
          .connection
          .exchange(&request.packet, *code, prefix)
          .await?,
      ))
    } else {
      self.connection.fire_and_forget(&request.packet).await?;
      None
    };
    let mut events = Vec::new();
    let deadline = tokio::time::Instant::now() + request.listen;
    while let Some(remaining) = deadline.checked_duration_since(tokio::time::Instant::now()) {
      if let Some(reply) = self.receive(remaining).await? {
        events.push(reply);
      } else {
        break;
      }
    }
    self.connection.delay(request.delay).await?;
    Ok(
      json!({"transport":self.connection.transport_name(),"request":request.describe(),"response":response,"events":events,
      "delivery":if response.is_some() {"command_reply"} else if matches!(self.connection.transport_name(), "ble" | "usb") {"transport_ack"} else {"written"}}),
    )
  }

  pub async fn receive(&mut self, timeout: Duration) -> Result<Option<Value>, Box<dyn Error>> {
    Ok(
      self
        .connection
        .receive(timeout)
        .await?
        .as_ref()
        .map(response_json),
    )
  }

  pub async fn disconnect(self) -> Result<(), Box<dyn Error>> {
    self.connection.disconnect().await
  }
}

#[cfg(test)]
mod tests {
  use super::*;
  #[test]
  fn apk_extended_selector_and_response_prefix() -> Result<(), Box<dyn Error>> {
    let request = Request {
      command: "SPP_SECOND_SET_AUTO_CONNECT_CFG".into(),
      payload_hex: "01 00".into(),
      response: Some("SPP_SECOND_SET_AUTO_CONNECT_CFG".into()),
      response_prefix_hex: String::new(),
      delay_ms: 0,
      listen_ms: 0,
    }
    .prepare()?;
    assert_eq!(request.packet.command.value(), 0xbd);
    assert_eq!(request.packet.payload, [0x1a, 1, 0]);
    assert_eq!(request.expected, Some((0xbd, vec![0x1a])));
    assert_eq!(catalogue()?.len(), 196);
    Ok(())
  }
  #[test]
  fn decode_real_device_firmware_and_state() -> Result<(), Box<dyn Error + Send + Sync>> {
    let firmware = Response::deserialize(&hex::decode("010a000437550157ab0400a10102")?)?;
    assert_eq!(
      response_json(&firmware)["firmware_versions"],
      json!([306007])
    );
    let state = Response::deserialize(&hex::decode(
      "011b00044655000000ff5000000001030000ff500006010000003535cd0302",
    )?)?;
    assert_eq!(response_json(&state)["brightness"], 0);
    Ok(())
  }
  #[test]
  fn validation_rejects_unknown_fields_and_bad_payloads() {
    assert!(serde_json::from_str::<Request>(r#"{"command":"0x37","paylod_hex":"00"}"#).is_err());
    let mut request = Request::bytes(0x37, &[0], true);
    request.payload_hex = "0".into();
    assert!(request.prepare().is_err());
    request.payload_hex = "00".repeat(65528);
    assert!(request.prepare().is_err());
  }
}
