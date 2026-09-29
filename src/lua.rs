//! Upload bounded Lua programs to the native runtime, without a firmware write.
use crate::{
  protocol::{
    command::Command,
    packet::{Packet, Response},
  },
  transport::DeviceConnection,
};
use bluer::Address;
use serde::Serialize;
use std::{error::Error, time::Duration};

pub const VERSION: u32 = 306012;
pub const SOURCE_LIMIT: usize = 2048;
pub const APP_VERSION: u32 = 306013;
pub const IO_VERSION: u32 = 306014;
pub const STORAGE_VERSION: u32 = 306015;
pub const DEVICE_VERSION: u32 = 306016;
pub const BLUETOOTH_VERSION: u32 = 306017;
pub const MUTE_VERSION: u32 = 306018;
pub const USB_VERSION: u32 = 306019;
pub const KEYBOARD_VERSION: u32 = 306020;
pub const KEYBOARD_PAIRING_VERSION: u32 = 306021;
pub const STANDALONE_VERSION: u32 = 306022;
pub const MEMORY_VERSION: u32 = 306023;
pub const BLUETOOTH_TRACE_VERSION: u32 = 306024;
pub const KEYBOARD_ONLY_VERSION: u32 = 306025;
pub const APP_SOURCE_LIMIT: usize = 8192;

#[derive(Debug, Serialize)]
pub struct Status {
  pub state: &'static str,
  pub peak_memory_bytes: u32,
  pub instructions: u32,
  pub result: String,
  pub abi: u8,
  pub memory_bytes: u32,
  pub frames: u32,
  pub callbacks: u32,
  pub dropped_keys: u32,
  pub held_keys: u32,
  pub generation: u32,
}

pub fn decode(reply: Response) -> Result<Status, Box<dyn Error>> {
  let d = reply.data;
  if !reply.ack
    || reply.original_command != 0x37
    || d.len() < 16
    || (&d[..4] != b"DLUA" || !matches!(d[4], 1 | 2))
  {
    return Err("Invalid Lua runtime response".into());
  }
  if d[6] != 0 {
    return Err(
      match d[6] {
        1 => "Lua runtime rejected the source length",
        2 => "Lua runtime is busy",
        3 => "Lua worker allocation failed",
        _ => "Lua runtime rejected the operation",
      }
      .into(),
    );
  }
  let header = if d[4] == 2 { 40 } else { 16 };
  if d.len() != header + usize::from(d[7]) {
    return Err("Truncated Lua result".into());
  }
  let state = match d[5] {
    0 => "idle",
    1 => "running",
    2 => "done",
    3 => "error",
    4 => "active",
    5 => "paused",
    6 => "uploading",
    7 => "saving",
    _ => return Err("Unknown Lua execution state".into()),
  };
  Ok(Status {
    state,
    peak_memory_bytes: u32::from_le_bytes(d[8..12].try_into()?),
    instructions: u32::from_le_bytes(d[12..16].try_into()?),
    result: String::from_utf8_lossy(&d[header..]).into_owned(),
    abi: d[4],
    memory_bytes: if header == 40 {
      u32::from_le_bytes(d[16..20].try_into()?)
    } else {
      0
    },
    frames: if header == 40 {
      u32::from_le_bytes(d[20..24].try_into()?)
    } else {
      0
    },
    callbacks: if header == 40 {
      u32::from_le_bytes(d[24..28].try_into()?)
    } else {
      0
    },
    dropped_keys: if header == 40 {
      u32::from_le_bytes(d[28..32].try_into()?)
    } else {
      0
    },
    held_keys: if header == 40 {
      u32::from_le_bytes(d[32..36].try_into()?)
    } else {
      0
    },
    generation: if header == 40 {
      u32::from_le_bytes(d[36..40].try_into()?)
    } else {
      0
    },
  })
}

fn packet(source: Option<&[u8]>, cancel: bool) -> Result<Packet, Box<dyn Error>> {
  let mut payload = b"\x7fDLUA".to_vec();
  payload.push(if source.is_some() {
    1
  } else if cancel {
    2
  } else {
    0
  });
  if let Some(source) = source {
    if source.is_empty() || source.len() > SOURCE_LIMIT {
      return Err(format!("Lua source must contain 1..={SOURCE_LIMIT} bytes").into());
    }
    payload.extend((source.len() as u16).to_le_bytes());
    payload.extend(source);
  }
  Ok(Packet {
    command: Command::Raw(0x37),
    payload,
  })
}

async fn exchange(conn: &mut DeviceConnection, packet: &Packet) -> Result<Status, Box<dyn Error>> {
  conn.stream_write(packet).await?;
  let deadline = tokio::time::Instant::now() + Duration::from_secs(5);
  while let Some(remaining) = deadline.checked_duration_since(tokio::time::Instant::now()) {
    if let Some(reply) = conn.receive(remaining).await? {
      if reply.original_command == 0x37 {
        return decode(reply);
      }
    } else {
      break;
    }
  }
  Err("Timed out waiting for Lua runtime reply".into())
}

pub enum Action<'a> {
  Run(&'a [u8]),
  Start(&'a [u8]),
  Install(&'a [u8]),
  Uninstall,
  Status,
  Stop,
  Pause,
  Resume,
  Send(&'a [u8]),
  Receive,
}

fn operation(op: u8, data: &[u8]) -> Packet {
  let mut payload = b"\x7fDLUA".to_vec();
  payload.push(op);
  payload.extend(data);
  Packet {
    command: Command::Raw(0x37),
    payload,
  }
}

async fn wait_for_worker(
  conn: &mut DeviceConnection,
  mut status: Status,
  stopping: bool,
) -> Result<Status, Box<dyn Error>> {
  let deadline = tokio::time::Instant::now() + Duration::from_secs(if status.state=="saving" { 15 } else { 5 });
  while matches!(status.state, "running" | "saving") || (stopping && matches!(status.state, "active" | "paused")) {
    if tokio::time::Instant::now() >= deadline {
      let _ = exchange(conn, &operation(2, &[])).await;
      return Err("Lua worker timed out; cancellation requested".into());
    }
    tokio::time::sleep(Duration::from_millis(50)).await;
    status = exchange(conn, &operation(0, &[])).await?;
  }
  Ok(status)
}

pub(crate) async fn execute(
  conn: &mut DeviceConnection,
  action: Action<'_>,
) -> Result<Status, Box<dyn Error>> {
  // Stock firmware gives these selectors a different meaning. Check before sending any extension.
  let version = conn
    .send_and_receive(&Packet {
      command: Command::Raw(0x37),
      payload: vec![0],
    })
    .await?;
  if !version.ack || version.data.len() != 5 || version.data[0] != 1 {
    return Err("Invalid firmware version reply; no Lua command sent".into());
  }
  let installed = u32::from_le_bytes(version.data[1..5].try_into()?);
  if !matches!(
    installed,
    VERSION
      | APP_VERSION
      | IO_VERSION
      | STORAGE_VERSION
      | DEVICE_VERSION
      | BLUETOOTH_VERSION
      | MUTE_VERSION | USB_VERSION | KEYBOARD_VERSION | KEYBOARD_PAIRING_VERSION | STANDALONE_VERSION | MEMORY_VERSION | BLUETOOTH_TRACE_VERSION | KEYBOARD_ONLY_VERSION
  ) {
    return Err(
      format!("Lua requires firmware {VERSION}, {APP_VERSION}, {IO_VERSION}, {STORAGE_VERSION}, {DEVICE_VERSION}, {BLUETOOTH_VERSION}, {MUTE_VERSION}, {USB_VERSION}, {KEYBOARD_VERSION}, {KEYBOARD_PAIRING_VERSION}, {STANDALONE_VERSION} or {MEMORY_VERSION}, {BLUETOOTH_TRACE_VERSION}, {KEYBOARD_ONLY_VERSION}; no program sent")
        .into(),
    );
  }
  if installed == VERSION {
    let (request, wait) = match action {
      Action::Run(source) => (packet(Some(source), false)?, true),
      Action::Status => (packet(None, false)?, false),
      Action::Stop => (packet(None, true)?, true),
      _ => return Err(format!("Resident apps require firmware {APP_VERSION}").into()),
    };
    let status = exchange(conn, &request).await?;
    return if wait {
      wait_for_worker(conn, status, false).await
    } else {
      Ok(status)
    };
  }
  if matches!(action, Action::Install(_) | Action::Uninstall) && installed < STANDALONE_VERSION {
    return Err(format!("Saved apps require firmware {STANDALONE_VERSION} or later; no program sent").into());
  }
  match action {
    Action::Run(source) | Action::Start(source) | Action::Install(source) => {
      if source.is_empty() || source.len() > APP_SOURCE_LIMIT {
        return Err(format!("Lua source must contain 1..={APP_SOURCE_LIMIT} bytes").into());
      }
      let status = exchange(conn, &operation(2, &[])).await?;
      wait_for_worker(conn, status, true).await?;
      let mut begin = (source.len() as u16).to_le_bytes().to_vec();
      begin.push(if matches!(action, Action::Install(_)) { 2 } else { u8::from(matches!(action, Action::Start(_))) });
      exchange(conn, &operation(3, &begin)).await?;
      for (index, chunk) in source.chunks(512).enumerate() {
        let mut data = ((index * 512) as u16).to_le_bytes().to_vec();
        data.extend(chunk);
        exchange(conn, &operation(4, &data)).await?;
      }
      let status = exchange(conn, &operation(5, &[])).await?;
      wait_for_worker(conn, status, false).await
    }
    Action::Uninstall => {
      let status = exchange(conn, &operation(2, &[])).await?;
      wait_for_worker(conn, status, true).await?;
      let status = exchange(conn, &operation(11, &[])).await?;
      wait_for_worker(conn, status, false).await
    }
    Action::Stop => {
      let status = exchange(conn, &operation(2, &[])).await?;
      wait_for_worker(conn, status, true).await
    }
    Action::Send(data) => {
      if data.is_empty() || data.len() > 128 {
        return Err("App messages must contain 1..=128 bytes".into());
      }
      exchange(conn, &operation(8, data)).await
    }
    Action::Pause | Action::Resume => {
      let paused = matches!(action, Action::Pause);
      let expected = if paused { "paused" } else { "active" };
      let mut status = exchange(conn, &operation(if paused { 6 } else { 7 }, &[])).await?;
      let deadline = tokio::time::Instant::now() + Duration::from_secs(5);
      while status.state != expected {
        if !matches!(status.state, "active" | "paused") {
          return Err(
            format!(
              "Lua app became {} while changing state: {}",
              status.state, status.result
            )
            .into(),
          );
        }
        if tokio::time::Instant::now() >= deadline {
          return Err("Lua state change timed out".into());
        }
        tokio::time::sleep(Duration::from_millis(50)).await;
        status = exchange(conn, &operation(0, &[])).await?;
      }
      Ok(status)
    }
    other => {
      exchange(
        conn,
        &operation(
          match other {
            Action::Status => 0,
            Action::Receive => 9,
            _ => unreachable!(),
          },
          &[],
        ),
      )
      .await
    }
  }
}

pub async fn control(address: Address, action: Action<'_>) -> Result<Status, Box<dyn Error>> {
  let mut conn = DeviceConnection::connect(address).await?;
  let result = execute(&mut conn, action).await;
  if let Err(error) = conn.disconnect().await {
    log::warn!("Lua connection cleanup: {error}");
  }
  result
}

pub async fn run(
  address: Address,
  source: Option<&[u8]>,
  cancel: bool,
) -> Result<Status, Box<dyn Error>> {
  control(
    address,
    match source {
      Some(source) => Action::Run(source),
      None if cancel => Action::Stop,
      None => Action::Status,
    },
  )
  .await
}

#[cfg(test)]
mod tests {
  use super::*;
  #[test]
  fn upload_length_and_limits() -> Result<(), Box<dyn Error>> {
    let p = packet(Some(b"return 42"), false)?;
    assert_eq!(p.payload, b"\x7fDLUA\x01\x09\x00return 42");
    assert!(packet(Some(b""), false).is_err());
    assert!(packet(Some(&vec![b'x'; SOURCE_LIMIT + 1]), false).is_err());
    Ok(())
  }
  #[test]
  fn malformed_runtime_responses_are_rejected() -> Result<(), Box<dyn Error>> {
    for data in [vec![], b"DLUA\x01\x00\x00\x01\0\0\0\0\0\0\0\0".to_vec()] {
      assert!(decode(Response {
        ack: true,
        original_command: 0x37,
        data
      })
      .is_err());
    }
    let data = b"DLUA\x01\x02\x00\x02\x00\x10\0\0\x64\0\0\0"
      .iter()
      .copied()
      .chain(*b"42")
      .collect();
    let status = decode(Response {
      ack: true,
      original_command: 0x37,
      data,
    })?;
    assert_eq!(status.result, "42");
    assert_eq!(status.peak_memory_bytes, 4096);
    Ok(())
  }

  #[test]
  fn resident_status_requires_complete_header_and_decodes_counters() -> Result<(), Box<dyn Error>> {
    let mut data = b"DLUA\x02\x04\x00\x02".to_vec();
    for value in [16384u32, 120, 15000, 25, 50, 3, 4, 7] {
      data.extend(value.to_le_bytes());
    }
    data.extend(b"ok");
    let response = |data| Response {
      ack: true,
      original_command: 0x37,
      data,
    };
    for n in 0..data.len() {
      assert!(decode(response(data[..n].to_vec())).is_err());
    }
    let status = decode(response(data))?;
    assert_eq!(status.abi, 2);
    assert_eq!(status.state, "active");
    assert_eq!(status.result, "ok");
    assert_eq!(status.memory_bytes, 15000);
    assert_eq!(status.frames, 25);
    assert_eq!(status.callbacks, 50);
    assert_eq!(status.dropped_keys, 3);
    assert_eq!(status.held_keys, 4);
    assert_eq!(status.generation, 7);
    Ok(())
  }
}
