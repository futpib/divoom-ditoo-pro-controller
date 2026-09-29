//! Read-only, bounded Ditoo-side Bluetooth metadata. No Lua callback is involved.
use crate::{
  protocol::{
    command::Command,
    packet::{Packet, Response},
  },
  transport::DeviceConnection,
};
use bluer::Address;
use serde_json::{json, Value};
use std::{
  error::Error,
  io::{self, Write},
  time::Duration,
};
use tokio::time::{sleep, Instant};
type Result<T> = std::result::Result<T, Box<dyn Error>>;

struct Page {
  oldest: u32,
  latest: u32,
  events: Vec<Value>,
}
fn word(p: &[u8]) -> u16 {
  u16::from_le_bytes([p[0], p[1]])
}
fn number(p: &[u8]) -> u32 {
  u32::from_le_bytes([p[0], p[1], p[2], p[3]])
}
fn address(p: &[u8]) -> String {
  p[..6]
    .iter()
    .rev()
    .map(|v| format!("{v:02X}"))
    .collect::<Vec<_>>()
    .join(":")
}
fn hci_reason(value: u8) -> &'static str {
  match value {
    0 => "success",
    2 => "unknown connection identifier",
    4 => "page timeout",
    5 => "authentication failure",
    6 => "PIN or key missing",
    8 => "connection timeout",
    9 => "connection limit exceeded",
    0xc => "command disallowed",
    0xd => "limited resources",
    0xe => "security rejection",
    0xf => "unacceptable address",
    0x10 => "connection accept timeout",
    0x13 => "remote user terminated",
    0x14 => "remote low resources",
    0x15 => "remote power off",
    0x16 => "local host terminated",
    0x17 => "repeated attempts",
    0x18 => "pairing not allowed",
    0x22 => "LMP response timeout",
    0x23 => "LMP transaction collision",
    0x2f => "insufficient security",
    0x35 => "role switch failed",
    0x38 => "host busy pairing",
    0x3d => "MIC failure",
    _ => "unmapped HCI code",
  }
}
fn event(row: &[u8]) -> Result<Value> {
  let kind = row[8];
  let code = row[9];
  let n = usize::from(row[10]);
  if n > 12 || row[11] != 0 {
    return Err("Invalid Bluetooth event length/reserved byte".into());
  }
  let p = &row[12..12 + n];
  let mut v = json!({"sequence":number(row),"ms":number(&row[4..]),"kind":kind,"event":code,
    "data_hex":p.iter().map(|b| format!("{b:02x}")).collect::<String>()});
  match kind {
    1 => {
      v["layer"] = json!("hci");
      let (name, expected) = match code {
        3 => ("connection_complete", 11),
        4 => ("connection_request", 10),
        5 => ("disconnection_complete", 4),
        6 => ("authentication_complete", 3),
        8 => ("encryption_change", 4),
        15 => ("command_status", 4),
        0x16 => ("pin_request", 6),
        0x17 => ("link_key_request", 6),
        0x18 => ("link_key_notification", 7),
        0x31 => ("io_capability_request", 6),
        0x32 => ("io_capability_response", 9),
        0x33 => ("user_confirmation_request", 6),
        0x34 => ("user_passkey_request", 6),
        0x35 => ("remote_oob_request", 6),
        0x36 => ("simple_pairing_complete", 7),
        _ => return Err("Unknown HCI trace event".into()),
      };
      if n != expected {
        return Err("Malformed HCI trace event".into());
      }
      v["name"] = json!(name);
      match code {
        3 => {
          v["peer"] = json!(address(&p[3..]));
          v["link_type"] = json!(p[9]);
          v["encrypted"] = json!(p[10]);
        }
        4 => {
          v["peer"] = json!(address(p));
          v["class"] = json!(u32::from(p[6]) | (u32::from(p[7]) << 8) | (u32::from(p[8]) << 16));
        }
        0x36 => {
          v["peer"] = json!(address(&p[1..]));
        }
        0x16..=0x18 | 0x31..=0x35 => {
          v["peer"] = json!(address(p));
        }
        _ => {}
      }
      if matches!(code, 3 | 5 | 6 | 8 | 15 | 0x36) {
        v["status"] = json!(p[0]);
        v["status_name"] = json!(hci_reason(p[0]));
      }
      if matches!(code, 3 | 5 | 6 | 8) {
        v["handle"] = json!(word(&p[1..]));
      }
      if code == 5 {
        v["reason"] = json!(p[3]);
        v["reason_name"] = json!(hci_reason(p[3]));
      }
      if code == 8 {
        v["encrypted"] = json!(p[3]);
      }
      if code == 15 {
        v["opcode"] = json!(word(&p[2..]));
      }
      if code == 0x18 {
        v["key_type"] = json!(p[6]);
      }
      if code == 0x32 {
        v["io_capability"] = json!(p[6]);
        v["oob"] = json!(p[7]);
        v["authentication_requirements"] = json!(p[8]);
      }
    }
    2 => {
      let expected = match code {
        8 | 10 => 1,
        15 => 2,
        _ => 0,
      };
      if n != expected {
        return Err("Malformed stack trace event".into());
      }
      v["layer"] = json!("stack");
      if code == 10 {
        v["access_mode"] = json!(p[0]);
      }
      if code == 8 {
        v["mode"] = json!(p[0]);
      }
      if code == 15 {
        v["error"] = json!(word(p));
      }
      v["name"] = json!(match code {
        1 => "initialized",
        2 => "out_of_memory",
        4 => "uninitialized",
        6 => "inquiry_complete",
        7 => "inquiry_canceled",
        8 => "mode_change",
        10 => "access_mode",
        11 => "connection_aborted",
        12 => "page_timeout",
        13 => "bond_added",
        14 => "bond_deleted",
        15 => "baseband_lost",
        16 => "pairing_failure",
        _ => "unknown",
      });
    }
    3 => {
      if n != 11 {
        return Err("Malformed L2CAP trace event".into());
      }
      v["layer"] = json!("hid_l2cap");
      v["psm"] = json!(p[0]);
      v["status"] = json!(word(&p[1..]));
      v["cid"] = json!(word(&p[3..]));
      v["peer"] = json!(address(&p[5..]));
      v["name"] = json!(match code {
        1 => "incoming",
        2 => "opened",
        4 => "closed",
        _ => "other",
      });
    }
    4 | 5 => {
      if n != 4 {
        return Err("Malformed HID trace event".into());
      }
      v["layer"] = json!(if kind == 4 { "hid_link" } else { "hid_error" });
      v["status"] = json!(number(p));
    }
    _ => return Err("Unknown Bluetooth trace layer".into()),
  }
  Ok(v)
}
fn decode(reply: Response) -> Result<Page> {
  let d = reply.data;
  if !reply.ack
    || reply.original_command != 0x37
    || d.len() < 16
    || &d[..6] != b"DBTR\x01\x00"
    || d[7] != 32
    || d[6] > 6
    || d.len() != 16 + usize::from(d[6]) * 24
  {
    return Err("Invalid Bluetooth trace response".into());
  }
  let oldest = number(&d[8..]);
  let latest = number(&d[12..]);
  if oldest != latest.saturating_sub(32).saturating_add(1) && !(latest < 32 && oldest == 1) {
    return Err("Invalid Bluetooth trace retention window".into());
  }
  let mut events = Vec::new();
  let mut previous: Option<u32> = None;
  for row in d[16..].chunks_exact(24) {
    let seq = number(row);
    if seq < oldest || seq > latest || previous.is_some_and(|p| p.checked_add(1) != Some(seq)) {
      return Err("Invalid Bluetooth trace sequence".into());
    }
    previous = Some(seq);
    events.push(event(row)?);
  }
  Ok(Page {
    oldest,
    latest,
    events,
  })
}
fn emit(v: Value) -> Result<()> {
  let mut out = io::stdout().lock();
  writeln!(out, "{v}")?;
  out.flush()?;
  Ok(())
}
async fn session(
  conn: &mut DeviceConnection,
  duration: Duration,
  interval: Duration,
) -> Result<()> {
  let reply = conn
    .send_and_receive(&Packet {
      command: Command::Raw(0x37),
      payload: vec![0],
    })
    .await?;
  if !reply.ack || reply.data.len() != 5 || reply.data[0] != 1 || number(&reply.data[1..]) != 306024
  {
    return Err("Bluetooth trace requires firmware 306024; no diagnostic sent".into());
  }
  let deadline = Instant::now() + duration;
  let mut after = 0;
  let mut first = true;
  loop {
    let mut payload = b"\x7fDLUA\x0d".to_vec();
    payload.extend_from_slice(&u32::to_le_bytes(after));
    let page = decode(
      conn
        .send_and_receive(&Packet {
          command: Command::Raw(0x37),
          payload,
        })
        .await?,
    )?;
    if page.latest < after {
      emit(json!({"notice":"trace_reset","previous":after,"latest":page.latest}))?;
      after = 0;
      continue;
    }
    if first {
      emit(
        json!({"notice":"trace","firmware":306024,"capacity":32,"oldest":page.oldest,"latest":page.latest}),
      )?;
      first = false;
    }
    if after.saturating_add(1) < page.oldest {
      emit(json!({"notice":"overwritten","events":page.oldest-after-1}))?;
      after = page.oldest - 1;
    }
    let empty = page.events.is_empty();
    for row in page.events {
      let seq = row["sequence"].as_u64().ok_or("Invalid trace sequence")? as u32;
      if seq != after + 1 {
        return Err("Bluetooth trace skipped an unreported event".into());
      }
      after = seq;
      emit(row)?;
    }
    if after < page.latest && !empty {
      continue;
    }
    if Instant::now() >= deadline {
      return Ok(());
    }
    sleep(interval.min(deadline.saturating_duration_since(Instant::now()))).await;
  }
}
pub async fn watch(address: Address, duration: Duration, interval: Duration) -> Result<()> {
  if duration > Duration::from_secs(86400)
    || !(Duration::from_millis(50)..=Duration::from_secs(5)).contains(&interval)
  {
    return Err("Bluetooth trace interval must be 50..5000 ms; duration 0..86400 seconds".into());
  }
  let mut conn = DeviceConnection::connect(address).await?;
  let result = tokio::select! {
    result=session(&mut conn,duration,interval)=>result,
    signal=tokio::signal::ctrl_c()=>signal.map_err(Into::into),
  };
  if let Err(e) = conn.disconnect().await {
    log::warn!("Bluetooth trace cleanup: {e}");
  }
  result
}

#[cfg(test)]
mod tests {
  use super::*;
  fn response(data: Vec<u8>) -> Response {
    Response {
      original_command: 0x37,
      ack: true,
      data,
    }
  }
  #[test]
  fn pairing_failure_and_redacted_key() {
    let mut row = [0; 24];
    row[8] = 1;
    row[9] = 0x36;
    row[10] = 7;
    row[12..19].copy_from_slice(&[5, 1, 2, 3, 4, 5, 6]);
    let v = event(&row).unwrap();
    assert_eq!(v["status_name"], "authentication failure");
    assert_eq!(v["peer"], "06:05:04:03:02:01");
    row[9] = 0x18;
    let v = event(&row).unwrap();
    assert_eq!(v["key_type"], 6);
    row[10] = 23;
    assert!(event(&row).is_err());
  }
  #[test]
  fn framing_and_retention() {
    let mut data = b"DBTR\x01\x00\x00\x20".to_vec();
    data.extend_from_slice(&1u32.to_le_bytes());
    data.extend_from_slice(&0u32.to_le_bytes());
    assert_eq!(decode(response(data.clone())).unwrap().latest, 0);
    data[6] = 1;
    assert!(decode(response(data.clone())).is_err());
    data.extend_from_slice(&[0; 24]);
    data[12] = 1;
    data[16] = 1;
    data[24] = 1;
    data[25] = 6;
    data[26] = 3;
    assert_eq!(decode(response(data.clone())).unwrap().events.len(), 1);
    data[16] = 2;
    assert!(decode(response(data.clone())).is_err());
    data[16] = 1;
    data[26] = 2;
    assert!(decode(response(data)).is_err());
  }
}
