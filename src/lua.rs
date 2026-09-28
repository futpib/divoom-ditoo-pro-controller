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

#[derive(Debug, Serialize)]
pub struct Status {
  pub state: &'static str,
  pub peak_memory_bytes: u32,
  pub instructions: u32,
  pub result: String,
}

fn decode(reply: Response) -> Result<Status, Box<dyn Error>> {
  let d = reply.data;
  if !reply.ack || reply.original_command != 0x37 || d.len() < 16 || &d[..5] != b"DLUA\x01" {
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
  if d.len() != 16 + usize::from(d[7]) {
    return Err("Truncated Lua result".into());
  }
  let state = match d[5] {
    0 => "idle",
    1 => "running",
    2 => "done",
    3 => "error",
    _ => return Err("Unknown Lua execution state".into()),
  };
  Ok(Status {
    state,
    peak_memory_bytes: u32::from_le_bytes(d[8..12].try_into()?),
    instructions: u32::from_le_bytes(d[12..16].try_into()?),
    result: String::from_utf8_lossy(&d[16..]).into_owned(),
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

async fn execute(
  conn: &mut DeviceConnection,
  request: &Packet,
  wait: bool,
) -> Result<Status, Box<dyn Error>> {
  // Stock firmware treats nonzero 0x37 selectors differently. Never send the extension to it.
  let version = conn
    .send_and_receive(&Packet {
      command: Command::Raw(0x37),
      payload: vec![0],
    })
    .await?;
  let mut expected = vec![1];
  expected.extend(VERSION.to_le_bytes());
  if !version.ack || version.data != expected {
    return Err(
      format!("Lua requires installed runtime firmware {VERSION}; no program sent").into(),
    );
  }
  let mut status = exchange(conn, request).await?;
  let deadline = tokio::time::Instant::now() + Duration::from_secs(10);
  while wait && status.state == "running" {
    if tokio::time::Instant::now() >= deadline {
      let _ = exchange(conn, &packet(None, true)?).await;
      return Err("Lua result timed out; cancellation requested".into());
    }
    tokio::time::sleep(Duration::from_millis(100)).await;
    status = exchange(conn, &packet(None, false)?).await?;
  }
  Ok(status)
}

pub async fn run(
  address: Address,
  source: Option<&[u8]>,
  cancel: bool,
) -> Result<Status, Box<dyn Error>> {
  let request = packet(source, cancel)?;
  let mut conn = DeviceConnection::connect(address).await?;
  let result = execute(&mut conn, &request, source.is_some()).await;
  if let Err(error) = conn.disconnect().await {
    log::warn!("Lua connection cleanup: {error}");
  }
  result
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
}
