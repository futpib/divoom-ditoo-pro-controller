//! Full native command transport through the custom firmware's vendor HID.
use crate::{
  protocol::packet::{Packet, Response},
  usb_firmware,
};
use nusb::Interface;
use std::{
  collections::VecDeque,
  error::Error,
  time::{Duration, SystemTime, UNIX_EPOCH},
};
use tokio::time::Instant;

type Result<T> = std::result::Result<T, Box<dyn Error>>;
const REPORT: usize = 256;
const HEADER: usize = 32;
const FRAME: usize = 4100;
const TIMEOUT: Duration = Duration::from_secs(5);

fn word(p: &[u8], at: usize) -> u32 {
  u32::from_le_bytes([p[at], p[at + 1], p[at + 2], p[at + 3]])
}
fn short(p: &[u8], at: usize) -> usize {
  u16::from_le_bytes([p[at], p[at + 1]]) as usize
}
fn capabilities(p: &[u8]) -> Result<()> {
  if p.len() != REPORT || &p[..5] != b"DUSB\x01" || short(p, 20) != FRAME || short(p, 22) & 1 == 0 {
    return Err("USB device control requires custom firmware 306019 or later with DUSB ABI 1; no command was sent. Stock USB firmware flashing remains available".into());
  }
  Ok(())
}

pub(crate) struct UsbConnection {
  interface: Interface,
  session: u32,
  sequence: u32,
  acknowledged: u32,
  replies: Replies,
}

#[derive(Default)]
struct Replies {
  cursor: u32,
  buffer: Vec<u8>,
  pending: VecDeque<Response>,
}

impl UsbConnection {
  pub async fn connect(port: Option<&str>) -> Result<Self> {
    let device = usb_firmware::select(port).await?;
    if !usb_firmware::is_app(&device) {
      return Err(
        "Ditoo is in the USB bootloader; restore an application with firmware-update first".into(),
      );
    }
    let interface = usb_firmware::claim(&device).await?;
    // This is an input report, never the firmware-entry feature report.
    capabilities(&usb_firmware::input(&interface, 0x017d, REPORT as u16).await?)?;
    let session = ((SystemTime::now().duration_since(UNIX_EPOCH)?.as_nanos() as u32)
      ^ std::process::id().rotate_left(16))
    .max(1);
    let mut conn = Self {
      interface,
      session,
      sequence: 0,
      acknowledged: 0,
      replies: Replies::default(),
    };
    conn.issue(1, 0, 0, &[]).await?;
    log::info!("Connected via USB vendor HID (Bluetooth is not used)");
    Ok(conn)
  }

  async fn poll(&mut self) -> Result<(u32, bool)> {
    let p = usb_firmware::input(&self.interface, 0x017d, REPORT as u16).await?;
    capabilities(&p)?;
    if self.sequence == 1 && word(&p, 8) != self.session && p[6] & 2 != 0 {
      return Ok((0, true));
    }
    if p[5] != 0 {
      let reason = match p[5] {
        1 => "invalid sequence/fragment/acknowledgment",
        2 => "busy",
        3 => "insufficient native heap",
        4 => "response queue overflow",
        5 => "session expired or replaced",
        6 => "invalid native packet checksum/framing",
        7 => "native dispatcher is not ready",
        _ => "unknown bridge error",
      };
      return Err(
        format!("USB bridge rejected request: {reason}; reconnect before retrying").into(),
      );
    }
    // During OPEN the firmware may still be servicing the previous session.
    if word(&p, 8) != self.session {
      if self.sequence == 1 && p[6] & 2 != 0 {
        return Ok((0, true));
      }
      return Err("USB control session ended or changed".into());
    }
    self.replies.accept(&p)?;
    Ok((word(&p, 12), p[6] & 2 != 0))
  }

  async fn issue(&mut self, op: u8, total: usize, offset: usize, data: &[u8]) -> Result<()> {
    self.sequence = self
      .sequence
      .checked_add(1)
      .ok_or("USB sequence exhausted; reconnect")?;
    let mut p = [0; REPORT];
    p[..5].copy_from_slice(b"DUSB\x01");
    p[5] = op;
    p[8..12].copy_from_slice(&self.session.to_le_bytes());
    p[12..16].copy_from_slice(&self.sequence.to_le_bytes());
    p[16..18].copy_from_slice(&(total as u16).to_le_bytes());
    p[18..20].copy_from_slice(&(offset as u16).to_le_bytes());
    p[20..22].copy_from_slice(&(data.len() as u16).to_le_bytes());
    p[24..28].copy_from_slice(&self.replies.cursor.to_le_bytes());
    p[HEADER..HEADER + data.len()].copy_from_slice(data);
    usb_firmware::output(&self.interface, 0x027d, &p, TIMEOUT).await?;
    self.acknowledged = self.replies.cursor;
    let deadline = Instant::now() + TIMEOUT;
    loop {
      let (sequence, busy) = self.poll().await?;
      if sequence == self.sequence && !busy {
        return Ok(());
      }
      if Instant::now() >= deadline {
        return Err("Timed out waiting for USB command dispatch; execution is uncertain, command was not retried".into());
      }
      tokio::time::sleep(Duration::from_millis(2)).await;
    }
  }

  pub async fn send(&mut self, packet: &Packet, preserve: bool) -> Result<()> {
    if packet.payload.len() > FRAME - 7 {
      return Err("USB command exceeds native 4096-byte length limit".into());
    }
    if !preserve {
      self.drain().await?;
    }
    let bytes = packet.serialize()?;
    for (index, chunk) in bytes.chunks(REPORT - HEADER).enumerate() {
      self
        .issue(2, bytes.len(), index * (REPORT - HEADER), chunk)
        .await?;
    }
    Ok(())
  }

  async fn pump(&mut self) -> Result<()> {
    if self.acknowledged != self.replies.cursor {
      self.issue(3, 0, 0, &[]).await
    } else {
      self.poll().await.map(|_| ())
    }
  }

  async fn drain(&mut self) -> Result<()> {
    // Finish old multi-report replies before a new exchange can match them.
    let deadline = Instant::now() + TIMEOUT;
    loop {
      let before = self.replies.cursor;
      self.pump().await?;
      self.replies.pending.clear();
      if before == self.replies.cursor {
        if !self.replies.buffer.is_empty() {
          return Err("Truncated USB response before command".into());
        }
        return Ok(());
      }
      if Instant::now() >= deadline {
        return Err("USB event stream did not become idle".into());
      }
    }
  }

  pub async fn delay(&mut self, duration: Duration) -> Result<()> {
    let deadline = Instant::now() + duration;
    while let Some(remaining) = deadline.checked_duration_since(Instant::now()) {
      self.pump().await?;
      tokio::time::sleep(remaining.min(Duration::from_secs(1))).await;
    }
    Ok(())
  }

  pub async fn receive(&mut self, timeout: Duration) -> Result<Option<Response>> {
    let deadline = Instant::now() + timeout;
    loop {
      if let Some(reply) = self.replies.pending.pop_front() {
        return Ok(Some(reply));
      }
      if Instant::now() >= deadline {
        return Ok(None);
      }
      if self.acknowledged != self.replies.cursor {
        self.issue(3, 0, 0, &[]).await?;
      } else {
        self.poll().await?;
      }
      if self.replies.pending.is_empty() {
        tokio::time::sleep(Duration::from_millis(5)).await;
      }
    }
  }

  pub async fn exchange(
    &mut self,
    packet: &Packet,
    expected: u8,
    prefix: &[u8],
  ) -> Result<Response> {
    self.send(packet, false).await?;
    let deadline = Instant::now() + TIMEOUT;
    while let Some(remaining) = deadline.checked_duration_since(Instant::now()) {
      if let Some(reply) = self.receive(remaining).await? {
        if reply.original_command == expected && reply.data.starts_with(prefix) {
          if !reply.ack {
            return Err("Device rejected command".into());
          }
          return Ok(reply);
        }
      }
    }
    Err("Timed out waiting for USB native command reply".into())
  }

  pub async fn disconnect(mut self) -> Result<()> {
    self.sequence += 1;
    let mut p = [0; REPORT];
    p[..5].copy_from_slice(b"DUSB\x01");
    p[5] = 4;
    p[8..12].copy_from_slice(&self.session.to_le_bytes());
    p[12..16].copy_from_slice(&self.sequence.to_le_bytes());
    p[24..28].copy_from_slice(&self.replies.cursor.to_le_bytes());
    usb_firmware::output(&self.interface, 0x027d, &p, TIMEOUT).await?;
    let deadline = Instant::now() + TIMEOUT;
    loop {
      let p = usb_firmware::input(&self.interface, 0x017d, REPORT as u16).await?;
      capabilities(&p)?;
      if word(&p, 8) == 0 && p[6] & 3 == 0 {
        return Ok(());
      }
      if Instant::now() >= deadline {
        return Err("USB session did not close".into());
      }
      tokio::time::sleep(Duration::from_millis(2)).await;
    }
  }
}

impl Replies {
  fn accept(&mut self, p: &[u8]) -> Result<()> {
    let count = short(p, 28);
    let skip = self.cursor.wrapping_sub(word(p, 24)) as usize;
    if count > REPORT - HEADER || skip > count {
      return Err("Invalid USB response stream cursor/length".into());
    }
    self
      .buffer
      .extend_from_slice(&p[HEADER + skip..HEADER + count]);
    self.cursor = self.cursor.wrapping_add((count - skip) as u32);
    while self.buffer.len() >= 3 {
      let size = short(&self.buffer, 1) + 4;
      if self.buffer[0] != 1 || !(9..=FRAME + 2).contains(&size) {
        return Err("Invalid USB native response framing".into());
      }
      if self.buffer.len() < size {
        break;
      }
      let reply = Response::deserialize(&self.buffer[..size]).map_err(|e| e.to_string())?;
      if self.pending.len() >= 1024 {
        return Err("USB host event queue overflow".into());
      }
      self.pending.push_back(reply);
      self.buffer.drain(..size);
    }
    Ok(())
  }
}

#[cfg(test)]
mod tests {
  use super::*;

  fn input(cursor: u32, bytes: &[u8]) -> Vec<u8> {
    let mut p = vec![0; REPORT];
    p[..5].copy_from_slice(b"DUSB\x01");
    p[20..22].copy_from_slice(&(FRAME as u16).to_le_bytes());
    p[22] = 1;
    p[24..28].copy_from_slice(&cursor.to_le_bytes());
    p[28..30].copy_from_slice(&(bytes.len() as u16).to_le_bytes());
    p[HEADER..HEADER + bytes.len()].copy_from_slice(bytes);
    p
  }

  #[test]
  fn repeated_reports_fragmented_frames_and_cursor_wrap() -> Result<()> {
    let mut frame = vec![1, 0x02, 0x10, 4, 0x37, 0x55];
    frame.extend(vec![0x77; 4093]);
    let checksum = frame[1..]
      .iter()
      .fold(0u16, |sum, b| sum.wrapping_add(*b as u16));
    frame.extend(checksum.to_le_bytes());
    frame.push(2);
    let mut replies = Replies {
      cursor: u32::MAX - 100,
      ..Replies::default()
    };
    for chunk in frame.chunks(REPORT - HEADER) {
      let p = input(replies.cursor, chunk);
      capabilities(&p)?;
      replies.accept(&p)?;
      replies.accept(&p)?;
    }
    assert_eq!(replies.pending.len(), 1);
    let reply = replies.pending.pop_front().ok_or("missing reply")?;
    assert_eq!(reply.original_command, 0x37);
    assert!(reply.ack);
    assert_eq!(reply.data, vec![0x77; 4093]);
    assert!(replies.buffer.is_empty());
    Ok(())
  }

  #[test]
  fn stock_reports_and_corrupt_streams_fail_closed() -> Result<()> {
    assert!(capabilities(&[0; REPORT]).is_err());
    assert!(capabilities(&[]).is_err());
    let mut replies = Replies::default();
    assert!(replies.accept(&input(1, &[0])).is_err());
    let mut p = input(0, &[]);
    p[28..30].copy_from_slice(&225u16.to_le_bytes());
    assert!(replies.accept(&p).is_err());
    assert!(replies.accept(&input(0, &[1, 0xff, 0xff])).is_err());
    let mut replies = Replies::default();
    assert!(replies
      .accept(&input(0, &[1, 5, 0, 4, 0x37, 0x55, 0, 0, 2]))
      .is_err());
    Ok(())
  }
}
