//! Ditoo Pro updater following the Android p166s1 updater and its 0x98/0x99 events.
use crate::{
  protocol::{
    command::Command,
    packet::{Packet, Response},
  },
  transport::DeviceConnection,
};
use bluer::Address;
use serde_json::json;
use sha2::{Digest, Sha256};
use std::{collections::VecDeque, error::Error, path::Path, time::Duration};

const VERSION: u32 = 306007;
const IMAGE_SIZE: usize = 1_877_379;
const SHA256: &str = "fc16341c005b11d0ac476917dc2fd98c9bc7481b92a183e64bfe209801566544";
const PROBE_VERSION: u32 = 306008;
const PROBE_SHA256: &str = "6cef319b7b7f2dceb56f370cf79d74b27489aca55b9f78bca1ca756f6cf9281b";
const LUA_PROBE_VERSION: u32 = 306009;
const LUA_PROBE_SHA256: &str = "4ee39062806d46095573865c9e65f36091fd50a3359897d0bcc843d46298f59e";
const LUA_PROBE_SIZE: usize = 1_877_691;
const LUA_VERSION: u32 = 306012;
const LUA_SHA256: &str = "cfb36afbf644ddb7dbaeb90b0b50c8fcd1021fa01933c18584b44e9e48839a56";
const LUA_SIZE: usize = 2_025_699;
const APP_VERSION: u32 = 306013;
const APP_SHA256: &str = "ab8f89fea9d460e8a96e9ec571340357a94ab3133c0f35f97ad5085d5b179b1b";
const APP_SIZE: usize = 2026511;
const IO_VERSION: u32 = 306014;
const IO_SHA256: &str = "c1c9fa3b0874092bfdfd2e51ff47d61908d25dcd6a064b0a2d30fdfddd5ebd70";
const IO_SIZE: usize = 2026511;
const STORAGE_VERSION: u32 = 306015;
const STORAGE_SHA256: &str = "5e7aa25fd7ac511a655b14ce52049083d2372454f07b177b165dac51bf583e87";
const STORAGE_SIZE: usize = 2026511;
const DEVICE_VERSION: u32 = 306016;
const DEVICE_SHA256: &str = "a955d25fa90434d12981b92972b7c5780368c875b6ed905b16edd20eb715a099";
const DEVICE_SIZE: usize = 2026511;
const BLUETOOTH_VERSION: u32 = 306017;
const BLUETOOTH_SHA256: &str = "00fd46616ad27bc3db0c30bf8c19668f884dc22bb052b04f378180dfb27a4bce";
const BLUETOOTH_SIZE: usize = 2026511;
const MUTE_VERSION: u32 = 306018;
const MUTE_SHA256: &str = "9bc2b1966c32ce184b25d1aaacf18fa80283fb894ffb244a80d58346704d8c6d";
const MUTE_SIZE: usize = 2026511;
const USB_VERSION: u32 = 306019;
const USB_SHA256: &str = "ae6653954ca95cb459d5a3666670fe8e6af9f9fb90cda94995ed9e67ad0c6435";
const USB_SIZE: usize = 2026511;
const KEYBOARD_VERSION: u32 = 306020;
const KEYBOARD_SHA256: &str = "3bc22855f5480f529c5bb4aafa04ac918e0eb4774fd4eeca8f7f9a76fa00132f";
const KEYBOARD_SIZE: usize = 2026739;
const KEYBOARD_PAIRING_VERSION: u32 = 306021;
const KEYBOARD_PAIRING_SHA256: &str = "652ba88287bd8d8c683a6d1212128717ad1d1dddda30a17839fbbab7f0f6622d";
const KEYBOARD_PAIRING_SIZE: usize = 2026739;
const STANDALONE_VERSION: u32 = 306022;
const STANDALONE_SHA256: &str = "89fad691d2895ac81353a76d56f132c9e1ce0008c6f150fea55c2bbf9693f6b9";
const STANDALONE_SIZE: usize = 2032883;
const CHUNK_SIZE: usize = 256;
const EVENT_TIMEOUT: Duration = Duration::from_secs(30);

pub struct Image {
  bytes: Vec<u8>,
  checksum: u32,
  version: u32,
  sha256: String,
}

impl Image {
  pub(crate) fn code(&self) -> &[u8] {
    &self.bytes[0x60f..self.bytes.len() - 4]
  }

  pub fn version(&self) -> u32 {
    self.version
  }

  pub(crate) fn sha256(&self) -> &str {
    &self.sha256
  }

  /// Restrict writes to pinned stock and reproducible experimental images.
  pub fn load(path: &Path) -> Result<Self, Box<dyn Error>> {
    let bytes = std::fs::read(path)?;
    let sha256 = hex::encode(Sha256::digest(&bytes));
    let version = match sha256.as_str() {
      SHA256 => VERSION,
      PROBE_SHA256 => PROBE_VERSION,
      LUA_PROBE_SHA256 => LUA_PROBE_VERSION,
      LUA_SHA256 => LUA_VERSION,
      APP_SHA256 => APP_VERSION,
      IO_SHA256 => IO_VERSION,
      STORAGE_SHA256 => STORAGE_VERSION,
      DEVICE_SHA256 => DEVICE_VERSION,
      BLUETOOTH_SHA256 => BLUETOOTH_VERSION,
      MUTE_SHA256 => MUTE_VERSION,
      USB_SHA256 => USB_VERSION,
      KEYBOARD_SHA256 => KEYBOARD_VERSION,
      KEYBOARD_PAIRING_SHA256 => KEYBOARD_PAIRING_VERSION,
      STANDALONE_SHA256 => STANDALONE_VERSION,
      _ => return Err("Unrecognized firmware image: only pinned stock 306007, reflash probe 306008 and native memory probe 306009 and Lua runtimes 306012/306013/306014/306015/306016/306017/306018/306019/306020/306021/306022 are supported".into()),
    };
    let expected_size = match version {
      LUA_VERSION => LUA_SIZE,
      APP_VERSION => APP_SIZE,
      IO_VERSION => IO_SIZE,
      STORAGE_VERSION => STORAGE_SIZE,
      DEVICE_VERSION => DEVICE_SIZE,
      BLUETOOTH_VERSION => BLUETOOTH_SIZE,
      MUTE_VERSION => MUTE_SIZE,
      USB_VERSION => USB_SIZE,
      KEYBOARD_VERSION => KEYBOARD_SIZE,
      LUA_PROBE_VERSION => LUA_PROBE_SIZE,
      KEYBOARD_PAIRING_VERSION => KEYBOARD_PAIRING_SIZE,
      STANDALONE_VERSION => STANDALONE_SIZE,
      _ => IMAGE_SIZE,
    };
    if bytes.len() != expected_size {
      return Err("Unexpected firmware image size".into());
    }
    let checksum = bytes.iter().map(|b| u32::from(*b)).sum();
    Ok(Self {
      bytes,
      checksum,
      version,
      sha256,
    })
  }

  fn count(&self) -> usize {
    self.bytes.len().div_ceil(CHUNK_SIZE)
  }

  fn metadata(&self) -> Packet {
    let mut payload = vec![0];
    payload.extend(self.version.to_le_bytes());
    payload.extend((self.bytes.len() as u32).to_le_bytes());
    payload.extend(self.checksum.to_le_bytes());
    Packet {
      command: Command::Raw(0x98),
      payload,
    }
  }

  fn chunk(&self, index: u16) -> Result<Packet, Box<dyn Error>> {
    let start = usize::from(index) * CHUNK_SIZE;
    if start >= self.bytes.len() {
      return Err(format!("Device requested invalid chunk {index}").into());
    }
    let end = (start + CHUNK_SIZE).min(self.bytes.len());
    let mut payload = index.to_le_bytes().to_vec();
    payload.extend(&self.bytes[start..end]);
    payload.resize(CHUNK_SIZE + 2, 0);
    Ok(Packet {
      command: Command::Raw(0x99),
      payload,
    })
  }

  pub fn describe(&self) -> serde_json::Value {
    json!({"version":self.version,"hardware_family":self.version/1000,"bytes":self.bytes.len(),
      "sha256":self.sha256,"checksum":self.checksum,"chunk_bytes":CHUNK_SIZE,"chunks":self.count(),
      "metadata_hex":hex::encode(self.metadata().payload),"final_chunk_zero_padded":true})
  }
}

#[derive(Debug, PartialEq)]
enum Event {
  Ready(u16),
  Retry(u16),
  Complete,
}

fn event(reply: &Response) -> Result<Option<Event>, Box<dyn Error>> {
  if !matches!(reply.original_command, 0x98 | 0x99 | 0x48) {
    return Ok(None);
  }
  if !reply.ack {
    return Err("Device rejected firmware update command".into());
  }
  if reply.original_command == 0x48 {
    return Err("Device requested firmware transfer stop (0x48)".into());
  }
  let d = &reply.data;
  if d.len() < 2 || d[0] != 0 {
    return Err("Unexpected firmware event type/length".into());
  }
  match (reply.original_command, d[1]) {
    (0x98, 0) | (0x99, 0) => {
      if d.len() < 4 {
        return Err("Truncated firmware chunk index".into());
      }
      let index = u16::from_le_bytes([d[2], d[3]]);
      Ok(Some(if reply.original_command == 0x98 {
        Event::Ready(index)
      } else {
        Event::Retry(index)
      }))
    }
    (0x98, status) => {
      Err(format!("Device not ready for update (status {status}); no chunks sent").into())
    }
    (0x99, 1) => Ok(Some(Event::Complete)),
    (0x99, 2) => Err("Device reported firmware update failure".into()),
    (_, status) => Err(format!("Unknown firmware event status {status}").into()),
  }
}

async fn next_event(
  conn: &mut DeviceConnection,
  timeout: Duration,
) -> Result<Option<Event>, Box<dyn Error>> {
  let deadline = tokio::time::Instant::now() + timeout;
  while let Some(remaining) = deadline.checked_duration_since(tokio::time::Instant::now()) {
    let Some(reply) = conn.receive(remaining).await? else {
      return Ok(None);
    };
    if matches!(reply.original_command, 0x98 | 0x99 | 0x48) {
      println!(
        "{}",
        json!({"event":"device_reply","opcode":reply.original_command,"data_hex":hex::encode(&reply.data)})
      );
    }
    if let Some(event) = event(&reply)? {
      return Ok(Some(event));
    }
  }
  Ok(None)
}

pub(crate) async fn versions(conn: &mut DeviceConnection) -> Result<Vec<u32>, Box<dyn Error>> {
  let packet = Packet {
    command: Command::Raw(0x37),
    payload: vec![0],
  };
  let mut error = String::new();
  for _ in 0..3 {
    match conn.send_and_receive(&packet).await {
      Ok(reply) => {
        let d = reply.data;
        if d.is_empty() || d.len() != 1 + usize::from(d[0]) * 4 {
          return Err("Malformed firmware version response".into());
        }
        return Ok(
          d[1..]
            .chunks_exact(4)
            .map(|b| u32::from_le_bytes([b[0], b[1], b[2], b[3]]))
            .collect(),
        );
      }
      Err(e) => error = e.to_string(),
    }
  }
  Err(format!("Could not read firmware version: {error}").into())
}

fn validate_target(
  installed: u32,
  version: u32,
  reflash: bool,
  restore_stock: bool,
) -> Result<(), Box<dyn Error>> {
  if restore_stock
    && !(matches!(
      installed,
      PROBE_VERSION
        | LUA_PROBE_VERSION
        | 306010
        | 306011
        | LUA_VERSION
        | APP_VERSION
        | IO_VERSION
        | STORAGE_VERSION
        | DEVICE_VERSION
        | BLUETOOTH_VERSION
        | MUTE_VERSION
        | USB_VERSION | KEYBOARD_VERSION | KEYBOARD_PAIRING_VERSION | STANDALONE_VERSION
    ) && version == VERSION)
  {
    return Err("--restore-stock requires a supported probe device (306008 through 306022) and pinned stock 306007".into());
  }
  if installed / 1000 != version / 1000 {
    return Err(format!("Hardware mismatch: device {installed}, image {version}").into());
  }
  if installed > version && !restore_stock {
    return Err("Firmware downgrade is not supported".into());
  }
  if installed == version && !reflash {
    return Err("Version already installed; use --reflash to explicitly reinstall it".into());
  }
  Ok(())
}

async fn transfer(
  conn: &mut DeviceConnection,
  image: &Image,
  reflash: bool,
  restore_stock: bool,
) -> Result<(), Box<dyn Error>> {
  let current = versions(conn).await?;
  let installed = *current
    .first()
    .ok_or("Device reported no firmware version")?;
  validate_target(installed, image.version, reflash, restore_stock)?;
  println!(
    "{}",
    json!({"event":"preflight","installed_versions":current,"transport":conn.transport_name(),"image":image.describe()})
  );
  conn.stream_write(&image.metadata()).await?;
  let Some(Event::Ready(start)) = next_event(conn, EVENT_TIMEOUT).await? else {
    return Err("No valid firmware-ready response; no chunks sent".into());
  };
  if usize::from(start) >= image.count() {
    return Err("Device resume index is outside the image".into());
  }
  println!("{}", json!({"event":"resume","chunk_index":start}));
  let started = tokio::time::Instant::now();
  let mut next = usize::from(start);
  let mut retries = VecDeque::new();
  let mut retry_counts = vec![0u8; image.count()];
  let mut percent = usize::MAX;
  loop {
    let pending = if next < image.count() || !retries.is_empty() {
      Duration::from_millis(1)
    } else {
      EVENT_TIMEOUT
    };
    if let Some(reply) = next_event(conn, pending).await? {
      match reply {
        Event::Complete => {
          if next < image.count() {
            return Err("Premature firmware success before all chunks were sent".into());
          }
          println!(
            "{}",
            json!({"event":"device_update_complete","version":image.version})
          );
          return Ok(());
        }
        Event::Retry(index) => {
          let count = retry_counts
            .get_mut(usize::from(index))
            .ok_or("Device requested out-of-range retry")?;
          *count += 1;
          if *count > 16 {
            return Err("Firmware chunk retry limit exceeded".into());
          }
          retries.push_back(index);
        }
        Event::Ready(_) => return Err("Unexpected second firmware-ready event".into()),
      }
    } else if next == image.count() && retries.is_empty() {
      return Err(
        "All chunks sent but no device success event; firmware completion is unverified".into(),
      );
    }
    let index = if let Some(index) = retries.pop_front() {
      index
    } else if next < image.count() {
      let index = u16::try_from(next)?;
      next += 1;
      index
    } else {
      continue;
    };
    conn.stream_write(&image.chunk(index)?).await?;
    let progress = next * 100 / image.count();
    if progress != percent {
      percent = progress;
      println!(
        "{}",
        json!({"event":"progress","percent":percent,"chunks_sent":next,"chunks_total":image.count(),
          "elapsed_seconds":started.elapsed().as_secs_f64(),
          "bytes_per_second":(next-usize::from(start)) as f64*CHUNK_SIZE as f64/started.elapsed().as_secs_f64()})
      );
    }
  }
}

/// Flash a validated image, require explicit device completion, then verify after reconnecting.
pub async fn flash(
  address: Address,
  image: &Image,
  reflash: bool,
  restore_stock: bool,
) -> Result<(), Box<dyn Error>> {
  let mut connection = DeviceConnection::connect(address).await?;
  let result = transfer(&mut connection, image, reflash, restore_stock).await;
  let cleanup = connection.disconnect().await;
  result?;
  if let Err(error) = cleanup {
    log::warn!("Disconnect after device completion: {error}; checking reconnect/version next");
  }
  for attempt in 1..=6 {
    tokio::time::sleep(Duration::from_secs(5)).await;
    if let Ok(mut connection) = DeviceConnection::connect(address).await {
      let result = versions(&mut connection).await;
      if let Ok(versions) = &result {
        println!(
          "{}",
          json!({"event":"post_update_version","firmware_versions":versions,"reconnect_attempt":attempt})
        );
      }
      let cleanup = connection.disconnect().await;
      if let Err(error) = cleanup {
        log::warn!("Disconnect after version query: {error}; retaining the query result");
      }
      if let Ok(versions) = result {
        if versions.first() == Some(&image.version) {
          println!(
            "{}",
            json!({"event":"verified","firmware_versions":versions,"reconnect_attempt":attempt})
          );
          return Ok(());
        }
        return Err(
          format!("Device completed update but reported unexpected versions {versions:?}").into(),
        );
      }
    }
  }
  Err("Device reported update success, but reconnect/version verification failed".into())
}

#[cfg(test)]
mod tests {
  use super::*;
  #[test]
  fn vendor_image_metadata_and_final_padding() -> Result<(), Box<dyn Error>> {
    let image = Image::load(Path::new(concat!(
      env!("CARGO_MANIFEST_DIR"),
      "/firmware/306007.MVA"
    )))?;
    assert_eq!(image.checksum, 161583048);
    assert_eq!(
      image.metadata().payload,
      hex::decode("0057ab040083a51c00c88fa109")?
    );
    let last = image.chunk((image.count() - 1) as u16)?;
    let tail_len = IMAGE_SIZE % CHUNK_SIZE;
    assert_eq!(
      &last.payload[2..2 + tail_len],
      &image.bytes[IMAGE_SIZE - tail_len..]
    );
    assert!(last.payload[2 + tail_len..].iter().all(|b| *b == 0));
    assert!(image.chunk(image.count() as u16).is_err());
    Ok(())
  }
  #[test]
  fn probe_image_has_its_own_version_and_wire_checksum() -> Result<(), Box<dyn Error>> {
    let image = Image::load(Path::new(concat!(
      env!("CARGO_MANIFEST_DIR"),
      "/firmware/306008-reflash-probe.MVA"
    )))?;
    assert_eq!(image.version, PROBE_VERSION);
    assert_eq!(image.checksum, 161582659);
    assert_eq!(
      image.metadata().payload,
      hex::decode("0058ab040083a51c00438ea109")?
    );
    Ok(())
  }

  #[test]
  fn native_probe_uses_its_pinned_size_and_identity() -> Result<(), Box<dyn Error>> {
    let image = Image::load(Path::new(concat!(
      env!("CARGO_MANIFEST_DIR"),
      "/firmware/306009-lua-probe.MVA"
    )))?;
    assert_eq!(image.version, LUA_PROBE_VERSION);
    assert_eq!(image.bytes.len(), LUA_PROBE_SIZE);
    assert_eq!(
      image.metadata().payload,
      hex::decode("0059ab0400bba61c00e8d9a109")?
    );
    Ok(())
  }

  #[test]
  fn lua_runtime_image_is_pinned() -> Result<(), Box<dyn Error>> {
    let image = Image::load(Path::new(concat!(
      env!("CARGO_MANIFEST_DIR"),
      "/firmware/306012-lua.MVA"
    )))?;
    assert_eq!(image.version, LUA_VERSION);
    assert_eq!(image.bytes.len(), LUA_SIZE);
    assert_eq!(image.checksum, 173062405);
    assert_eq!(image.version, crate::lua::VERSION);
    Ok(())
  }

  #[test]
  fn resident_runtime_image_is_pinned() -> Result<(), Box<dyn Error>> {
    let image = Image::load(Path::new(concat!(
      env!("CARGO_MANIFEST_DIR"),
      "/firmware/306013-lua.MVA"
    )))?;
    assert_eq!(image.version, APP_VERSION);
    assert_eq!(image.bytes.len(), APP_SIZE);
    assert_eq!(image.checksum, 173866409);
    assert_eq!(image.version, crate::lua::APP_VERSION);
    Ok(())
  }

  #[test]
  fn lighting_runtime_image_is_pinned() -> Result<(), Box<dyn Error>> {
    let image = Image::load(Path::new(concat!(env!("CARGO_MANIFEST_DIR"), "/firmware/306014-lua.MVA")))?;
    assert_eq!(image.version, IO_VERSION);
    assert_eq!(image.bytes.len(), IO_SIZE);
    assert_eq!(image.checksum, 173980384);
    assert_eq!(image.version, crate::lua::IO_VERSION);
    Ok(())
  }

  #[test]
  fn storage_probe_image_is_pinned() -> Result<(), Box<dyn Error>> {
    let image = Image::load(Path::new(concat!(env!("CARGO_MANIFEST_DIR"), "/firmware/306015-lua.MVA")))?;
    assert_eq!(image.version, STORAGE_VERSION);
    assert_eq!(image.bytes.len(), STORAGE_SIZE);
    assert_eq!(image.checksum, 174013971);
    assert_eq!(image.version, crate::lua::STORAGE_VERSION);
    Ok(())
  }

  #[test]
  fn target_validation_rejects_wrong_hardware_and_implicit_reflash() {
    assert!(validate_target(306007, PROBE_VERSION, false, false).is_ok());
    assert!(validate_target(306008, VERSION, false, true).is_ok());
    assert!(validate_target(306009, VERSION, false, true).is_ok());
    assert!(validate_target(306010, VERSION, false, true).is_ok());
    assert!(validate_target(306011, VERSION, false, true).is_ok());
    assert!(validate_target(306012, VERSION, false, true).is_ok());
    assert!(validate_target(306013, VERSION, false, true).is_ok());
    assert!(validate_target(306014, VERSION, false, true).is_ok());
    assert!(validate_target(306015, VERSION, false, true).is_ok());
    assert!(validate_target(306016, VERSION, false, true).is_ok());
    assert!(validate_target(306017, VERSION, false, true).is_ok());
    assert!(validate_target(306018, VERSION, false, true).is_ok());
    assert!(validate_target(306019, VERSION, false, true).is_ok());
    assert!(validate_target(306020, VERSION, false, true).is_ok());
    assert!(validate_target(306021, VERSION, false, true).is_ok());
    assert!(validate_target(306022, VERSION, false, true).is_ok());
    assert!(validate_target(306023, VERSION, false, true).is_err());
    assert!(validate_target(306007, VERSION, false, true).is_err());
    assert!(validate_target(306008, PROBE_VERSION, false, true).is_err());
    assert!(validate_target(306006, VERSION, false, false).is_ok());
    assert!(validate_target(306007, VERSION, false, false).is_err());
    assert!(validate_target(306007, VERSION, true, false).is_ok());
    assert!(validate_target(306008, VERSION, true, false).is_err());
    assert!(validate_target(41007, VERSION, true, false).is_err());
  }

  #[test]
  fn update_events_require_ready_and_explicit_success() -> Result<(), Box<dyn Error>> {
    let response = |code, data| Response {
      original_command: code,
      ack: true,
      data,
    };
    assert_eq!(
      event(&response(0x98, vec![0, 0, 0x34, 0x12]))?,
      Some(Event::Ready(0x1234))
    );
    assert_eq!(
      event(&response(0x99, vec![0, 0, 2, 0]))?,
      Some(Event::Retry(2))
    );
    assert_eq!(event(&response(0x99, vec![0, 1]))?, Some(Event::Complete));
    assert!(event(&response(0x98, vec![0, 1])).is_err());
    assert!(event(&response(0x98, vec![0, 2])).is_err());
    assert!(event(&response(0x99, vec![0, 2])).is_err());
    assert!(event(&response(0x99, vec![0, 0, 1])).is_err());
    assert!(event(&response(0x48, vec![])).is_err());
    assert_eq!(event(&response(0x33, vec![1, 0, 0, 0]))?, None);
    Ok(())
  }
  #[test]
  fn device_runtime_image_is_pinned() -> Result<(), Box<dyn Error>> {
    let image = Image::load(Path::new(concat!(
      env!("CARGO_MANIFEST_DIR"),
      "/firmware/306016-lua.MVA"
    )))?;
    assert_eq!(image.version, crate::lua::DEVICE_VERSION);
    assert_eq!(image.bytes.len(), DEVICE_SIZE);
    assert!(validate_target(DEVICE_VERSION, VERSION, false, true).is_ok());
    Ok(())
  }
  #[test]
  fn bluetooth_runtime_image_is_pinned() -> Result<(), Box<dyn Error>> {
    let image = Image::load(Path::new(concat!(
      env!("CARGO_MANIFEST_DIR"),
      "/firmware/306017-lua.MVA"
    )))?;
    assert_eq!(image.version, crate::lua::BLUETOOTH_VERSION);
    assert_eq!(image.bytes.len(), BLUETOOTH_SIZE);
    assert!(validate_target(BLUETOOTH_VERSION, VERSION, false, true).is_ok());
    Ok(())
  }
  #[test]
  fn mute_runtime_image_is_pinned() -> Result<(), Box<dyn Error>> {
    let image = Image::load(Path::new(concat!(
      env!("CARGO_MANIFEST_DIR"),
      "/firmware/306018-lua.MVA"
    )))?;
    assert_eq!(image.version, crate::lua::MUTE_VERSION);
    assert_eq!(image.bytes.len(), MUTE_SIZE);
    assert!(validate_target(MUTE_VERSION, VERSION, false, true).is_ok());
    Ok(())
  }

  #[test]
  fn usb_runtime_image_is_pinned() -> Result<(), Box<dyn Error>> {
    let image = Image::load(Path::new(concat!(env!("CARGO_MANIFEST_DIR"), "/firmware/306019-lua.MVA")))?;
    assert_eq!(image.version, crate::lua::USB_VERSION);
    assert_eq!(image.bytes.len(), USB_SIZE);
    assert!(validate_target(USB_VERSION, VERSION, false, true).is_ok());
    Ok(())
  }

  #[test]
  fn pinned_keyboard_image() -> Result<(), Box<dyn Error>> {
    let image = Image::load(Path::new(concat!(env!("CARGO_MANIFEST_DIR"), "/firmware/306020-lua.MVA")))?;
    assert_eq!(image.version, crate::lua::KEYBOARD_VERSION);
    assert_eq!(image.bytes.len(), KEYBOARD_SIZE);
    assert!(validate_target(KEYBOARD_VERSION, VERSION, false, true).is_ok());
    Ok(())
  }

}
