//! MVsilicon USB bootloader protocol traced in the pinned Ditoo 306007 image.
use crate::{firmware::Image, transport::DeviceConnection, with_transport, Transport};
use bluer::Address;
use futures::future::try_join_all;
use nusb::{
  transfer::{ControlIn, ControlOut, ControlType, Recipient, TransferError},
  DeviceInfo, Interface,
};
use serde_json::json;
use std::{error::Error, time::Duration};
use tokio::time::Instant;

type Result<T> = std::result::Result<T, Box<dyn Error>>;
const REPORT: usize = 256;
const BLOCK: usize = 4096;
const APP_START: usize = 0x10000;
const USER_DATA: usize = 0x1f0000;
const IO_TIMEOUT: Duration = Duration::from_secs(3);
const ERASE_TIMEOUT: Duration = Duration::from_secs(20);
const ENUM_TIMEOUT: Duration = Duration::from_secs(20);
const BOOT_REPORT: &str =
  "0600ff0a55aaa101150026ff007508960001090181029600010901910295010901b102c0";
const APP_REPORT: &str = "0600ff0aaa55a101150026ff007508960001090181029600010901910295080901b102c0";

struct Application<'a> {
  image: &'a Image,
  bytes: &'a [u8],
}

fn word(bytes: &[u8], at: usize) -> u32 {
  u32::from_le_bytes([bytes[at], bytes[at + 1], bytes[at + 2], bytes[at + 3]])
}

impl<'a> Application<'a> {
  fn new(image: &'a Image) -> Result<Self> {
    let code = image.code();
    if code.len() <= APP_START || code.len().next_multiple_of(BLOCK) > USER_DATA {
      return Err("Application is outside the Ditoo application flash partition".into());
    }
    if word(code, 0xbc) != 0x5f08 || word(code, 0xe0) & 0xffffff != 0x9e80 || code[0xff] != 0xff {
      return Err("Image does not match the supported USB bootloader".into());
    }
    Ok(Self {
      image,
      bytes: &code[APP_START..],
    })
  }

  fn blocks(&self) -> usize {
    self.bytes.len().div_ceil(BLOCK)
  }

  fn block(&self, index: usize) -> [u8; BLOCK] {
    let mut buffer = [0xff; BLOCK];
    let start = index * BLOCK;
    let end = (start + BLOCK).min(self.bytes.len());
    buffer[..end - start].copy_from_slice(&self.bytes[start..end]);
    buffer
  }

  fn metadata(&self, reflash: bool) -> [u8; REPORT] {
    let code = self.image.code();
    let mut packet = command(b"cxxx");
    let marker = word(code, APP_START + 0xcc);
    for (at, value) in [
      (8, self.bytes.len() as u32),
      (24, word(code, APP_START + 0xb8)),
      (32, word(code, 0xe0) & 0xffffff),
      (36, word(code, 0xbc)),
      (48, if reflash { !marker } else { marker }),
    ] {
      packet[at..at + 4].copy_from_slice(&value.to_le_bytes());
    }
    packet[20] = code[0xff];
    packet[52] = 2;
    packet
  }

  fn describe(&self, reflash: bool) -> serde_json::Value {
    json!({"transport":"usb", "version":self.image.version(), "sha256":self.image.sha256(),
      "application_bytes":self.bytes.len(), "blocks":self.blocks(), "block_bytes":BLOCK,
      "report_bytes":REPORT, "flash_start":APP_START, "flash_end":APP_START+self.blocks()*BLOCK,
      "bootloader_preserved":true, "first_block_written_last":true,
      "tail_padding":255, "metadata_hex":hex::encode(self.metadata(reflash))})
  }
}

fn command(name: &[u8]) -> [u8; REPORT] {
  let mut packet = [0; REPORT];
  packet[..name.len()].copy_from_slice(name);
  packet
}

fn block_order(blocks: usize) -> impl Iterator<Item = usize> {
  (1..blocks).chain(std::iter::once(0))
}

fn metadata_reply(reply: &[u8]) -> Result<bool> {
  if reply.len() != REPORT || &reply[..4] != b"cxxx" {
    return Err("Malformed USB metadata reply; no erase or data command sent".into());
  }
  match &reply[8..12] {
    [0x55, 0xff, 0xff, 0xff] => Ok(true),
    [0xff, 0xff, 0xff, 0xff] => Ok(false),
    flags => Err(
      format!(
        "USB bootloader rejected application-only update (flags {}); no erase or data command sent",
        hex::encode(flags)
      )
      .into(),
    ),
  }
}

fn is_app(device: &DeviceInfo) -> bool {
  device.vendor_id() == 0x8888 && matches!(device.product_id(), 0x1719 | 0x171e)
}

fn is_boot(device: &DeviceInfo) -> bool {
  device.vendor_id() == 0 && device.product_id() == 0x2244
}

/// Stable physical port, including the hub chain; Linux example: 1-6 or 1-2.3.
fn port(device: &DeviceInfo) -> String {
  let bus = device
    .bus_id()
    .parse::<u32>()
    .map(|n| n.to_string())
    .unwrap_or_else(|_| device.bus_id().to_owned());
  let chain = device
    .port_chain()
    .iter()
    .map(u8::to_string)
    .collect::<Vec<_>>()
    .join(".");
  format!("{bus}-{chain}")
}

async fn select(requested: Option<&str>) -> Result<DeviceInfo> {
  let devices: Vec<_> = nusb::list_devices()
    .await?
    .filter(|d| is_app(d) || is_boot(d))
    .collect();
  let available = devices
    .iter()
    .map(|d| format!("{} ({:04x}:{:04x})", port(d), d.vendor_id(), d.product_id()))
    .collect::<Vec<_>>()
    .join(", ");
  let mut matching = devices.into_iter().filter(|d| match requested {
    Some(wanted) => port(d) == wanted,
    None => is_app(d),
  });
  let device = matching.next().ok_or_else(|| format!("No matching Ditoo USB device. Available: [{available}]. Recovery from 0000:2244 requires --usb-port BUS-PORT; use lsusb -t to identify it"))?;
  if matching.next().is_some() {
    return Err(
      format!("Multiple Ditoo USB devices: [{available}]; select --usb-port BUS-PORT").into(),
    );
  }
  Ok(device)
}

async fn wait_for(physical_port: &str, boot: bool) -> Result<DeviceInfo> {
  let start = Instant::now();
  while start.elapsed() < ENUM_TIMEOUT {
    if let Some(device) = nusb::list_devices()
      .await?
      .find(|d| port(d) == physical_port && if boot { is_boot(d) } else { is_app(d) })
    {
      if accessible(&device).await? {
        return Ok(device);
      }
    }
    tokio::time::sleep(Duration::from_millis(100)).await;
  }
  Err(
    format!(
      "Timed out waiting for USB {} on port {physical_port}",
      if boot { "bootloader" } else { "application" }
    )
    .into(),
  )
}

async fn accessible(device: &DeviceInfo) -> Result<bool> {
  // Enumeration can precede configuration and the udev uaccess ACL. Poll the
  // actual readiness, without adding a delay to an already accessible device.
  match device.open().await {
    Ok(opened) => Ok(opened.active_configuration().is_ok()),
    Err(error)
      if matches!(
        error.kind(),
        nusb::ErrorKind::PermissionDenied
          | nusb::ErrorKind::NotFound
          | nusb::ErrorKind::Disconnected
      ) =>
    {
      Ok(false)
    }
    Err(error) => Err(error.into()),
  }
}

async fn claim(device: &DeviceInfo) -> Result<Interface> {
  let opened = device.open().await.map_err(|e| {
    format!(
      "Cannot open USB port {}: {e}; see docs/usb.md for USB permissions",
      port(device)
    )
  })?;
  let config = opened.active_configuration()?;
  let mut interfaces = config.interface_alt_settings().filter(|i| {
    i.alternate_setting() == 0
      && i.class() == 3
      && i.subclass() == 0
      && i.protocol() == 0
      && i.num_endpoints() == 0
  });
  let index = interfaces
    .next()
    .ok_or("USB vendor HID interface is absent")?
    .interface_number();
  if interfaces.next().is_some() || (is_boot(device) && index != 0) {
    return Err("Unexpected USB HID configuration".into());
  }
  let interface = opened.detach_and_claim_interface(index).await?;
  let report = interface
    .control_in(
      ControlIn {
        control_type: ControlType::Standard,
        recipient: Recipient::Interface,
        request: 6,
        value: 0x2200,
        index: u16::from(index),
        length: 36,
      },
      IO_TIMEOUT,
    )
    .await?;
  let expected = if is_boot(device) {
    BOOT_REPORT
  } else {
    APP_REPORT
  };
  if hex::encode(&report) != expected {
    return Err(
      format!(
        "Unexpected vendor HID report descriptor: {}",
        hex::encode(report)
      )
      .into(),
    );
  }
  Ok(interface)
}

async fn output(
  interface: &Interface,
  value: u16,
  data: &[u8],
  timeout: Duration,
) -> std::result::Result<(), TransferError> {
  interface
    .control_out(
      ControlOut {
        control_type: ControlType::Class,
        recipient: Recipient::Interface,
        request: 9,
        value,
        index: u16::from(interface.interface_number()),
        data,
      },
      timeout,
    )
    .await
}

async fn input(
  interface: &Interface,
  value: u16,
  length: u16,
) -> std::result::Result<Vec<u8>, TransferError> {
  interface
    .control_in(
      ControlIn {
        control_type: ControlType::Class,
        recipient: Recipient::Interface,
        request: 1,
        value,
        index: u16::from(interface.interface_number()),
        length,
      },
      IO_TIMEOUT,
    )
    .await
}

async fn enter(device: DeviceInfo, physical_port: &str) -> Result<DeviceInfo> {
  if is_boot(&device) {
    return Ok(device);
  }
  let interface = claim(&device).await?;
  output(&interface, 0x300, &[0xaa, 0, 0, 0, 0, 0, 0, 0], IO_TIMEOUT)
    .await
    .map_err(|e| format!("USB upgrade arm failed: {e}"))?;
  match input(&interface, 0x300, 8).await {
    Ok(reply) if reply.len() == 8 && reply[0] == 0x55 => (),
    Ok(reply) => {
      return Err(
        format!(
          "Unexpected USB upgrade-entry response: {}",
          hex::encode(reply)
        )
        .into(),
      )
    }
    Err(error) => {
      // The application disables USB inside GET_FEATURE. Hosts can report
      // EPROTO as well as disconnect; require the real bootloader next.
      log::info!("USB entry response ended with {error}; checking bootloader enumeration");
    }
  }
  drop(interface);
  wait_for(physical_port, true).await
}

async fn finish(interface: &Interface) -> Result<()> {
  let mut packet = command(b"upinfo");
  packet[8..10].copy_from_slice(b"ok");
  // There is no IN reply. The bootloader disconnects to boot the application.
  match output(interface, 0x200, &packet, IO_TIMEOUT).await {
    Ok(()) | Err(TransferError::Disconnected) => Ok(()),
    Err(error) => Err(error.into()),
  }
}

async fn exit_after_metadata_reset(boot: &DeviceInfo, physical_port: &str) -> Result<()> {
  // cxxx/no-change resets back into USB mode. Finish on the new enumeration,
  // never on the stale handle and never while application data is in flight.
  let started = Instant::now();
  while started.elapsed() < ENUM_TIMEOUT {
    if let Some(device) = nusb::list_devices()
      .await?
      .find(|d| port(d) == physical_port && d.id() != boot.id() && (is_app(d) || is_boot(d)))
    {
      if is_app(&device) {
        return Ok(());
      }
      if accessible(&device).await? {
        let interface = claim(&device).await?;
        finish(&interface).await?;
        drop(interface);
        wait_for(physical_port, false).await?;
        return Ok(());
      }
    }
    tokio::time::sleep(Duration::from_millis(100)).await;
  }
  Err("No-change handshake finished, but the device did not re-enumerate".into())
}

/// Offline validation; this never enumerates or arms a USB device.
pub fn describe(image: &Image, reflash: bool) -> Result<serde_json::Value> {
  Ok(Application::new(image)?.describe(reflash))
}

async fn transfer(interface: &Interface, app: &Application<'_>, depth: usize) -> Result<f64> {
  let started = Instant::now();
  output(interface, 0x200, &command(b"codedata"), IO_TIMEOUT).await?;
  let mut first_block_seconds = 0.0;
  let mut percent = usize::MAX;
  for (completed, block) in block_order(app.blocks()).enumerate() {
    let buffer = app.block(block);
    let timeout = if completed == 0 {
      ERASE_TIMEOUT
    } else {
      IO_TIMEOUT
    };
    // EP0 is ordered. Queue only this block: the next block requires its read-back ACK.
    for batch in buffer.chunks(REPORT * depth) {
      try_join_all(
        batch
          .chunks(REPORT)
          .map(|data| output(interface, 0x200, data, timeout)),
      )
      .await
      .map_err(|e| format!("USB block {block} transfer failed: {e}; no report was retried"))?;
    }
    let reply = input(interface, 0x100, REPORT as u16)
      .await
      .map_err(|e| format!("USB block {block} verification reply failed: {e}"))?;
    if reply.len() != REPORT || reply[8] != 0x55 {
      return Err(
        format!(
          "USB block {block} failed device read-back verification (reply {})",
          hex::encode(reply)
        )
        .into(),
      );
    }
    if completed == 0 {
      first_block_seconds = started.elapsed().as_secs_f64();
    }
    let progress = (completed + 1) * 100 / app.blocks();
    if progress != percent {
      percent = progress;
      println!(
        "{}",
        json!({"event":"progress", "transport":"usb", "percent":progress,
        "blocks_verified":completed+1, "blocks_total":app.blocks(), "block":block,
        "elapsed_seconds":started.elapsed().as_secs_f64()})
      );
    }
  }
  let elapsed = started.elapsed().as_secs_f64();
  println!(
    "{}",
    json!({"event":"all_blocks_verified", "application_bytes":app.bytes.len(),
    "erase_transfer_verify_seconds":elapsed, "erase_and_first_block_seconds":first_block_seconds,
    "remaining_blocks_seconds":elapsed-first_block_seconds,
    "bytes_per_second":app.bytes.len() as f64/elapsed})
  );
  Ok(elapsed)
}

async fn verify_version(address: Address, expected: u32) -> Result<()> {
  let mut last_error = String::new();
  for attempt in 1..=3 {
    let result = with_transport(Transport::Ble, async {
      let mut connection = DeviceConnection::connect(address).await?;
      let versions = crate::firmware::versions(&mut connection).await;
      if let Err(error) = connection.disconnect().await {
        log::warn!("BLE disconnect after USB version check: {error}");
      }
      versions
    })
    .await;
    match result {
      Ok(versions) => {
        if versions.first() != Some(&expected) {
          return Err(
            format!("USB finished, but BLE reports unexpected firmware {versions:?}").into(),
          );
        }
        println!(
          "{}",
          json!({"event":"verified", "transport":"usb", "firmware_versions":versions, "reconnect_attempt":attempt})
        );
        return Ok(());
      }
      Err(error) => last_error = error.to_string(),
    }
    tokio::time::sleep(Duration::from_secs(1)).await;
  }
  Err(format!("USB finished, but optional BLE version check failed: {last_error}").into())
}

/// Flash the application only, preserving the bootloader and user-data partition.
pub async fn flash(
  image: &Image,
  physical_port: Option<&str>,
  reflash: bool,
  queue_depth: u8,
  verify_address: Option<Address>,
) -> Result<()> {
  if !(1..=16).contains(&queue_depth) {
    return Err("USB queue depth must be 1 through 16 reports".into());
  }
  let app = Application::new(image)?;
  let device = select(physical_port).await?;
  let starting_in_bootloader = is_boot(&device);
  if starting_in_bootloader && !reflash {
    return Err("Bootloader recovery requires --reflash to rewrite every application block even if its old header marker matches".into());
  }
  let physical_port = port(&device);
  println!(
    "{}",
    json!({"event":"preflight", "transport":"usb", "usb_port":physical_port,
    "queue_depth":queue_depth, "starting_in_bootloader":is_boot(&device), "image":app.describe(reflash)})
  );
  let started = Instant::now();
  let boot = enter(device, &physical_port).await?;
  println!(
    "{}",
    json!({"event":"bootloader_entered", "usb_port":physical_port})
  );
  let interface = claim(&boot).await?;
  output(&interface, 0x200, &app.metadata(reflash), IO_TIMEOUT)
    .await
    .map_err(|e| format!("USB metadata write failed: {e}; no erase command sent"))?;
  let reply = match input(&interface, 0x100, REPORT as u16).await {
    Ok(reply) => reply,
    Err(error) => {
      drop(interface);
      // The no-change/rejected metadata path can reset before EP0 completes.
      // Only return to a known working application; recovery may be incomplete.
      if !starting_in_bootloader {
        exit_after_metadata_reset(&boot, &physical_port).await?;
        println!(
          "{}",
          json!({"event":"no_write_exit", "flash_writes":false, "application_reenumerated":true})
        );
      }
      return Err(format!("USB metadata reply failed: {error}; no erase command sent. An unchanged image can reset before its reply completes; use --reflash to reinstall").into());
    }
  };
  let changed = metadata_reply(&reply)?;
  println!(
    "{}",
    json!({"event":"metadata_accepted", "flags":hex::encode(&reply[8..12]), "changed":changed})
  );
  if !changed {
    drop(interface);
    exit_after_metadata_reset(&boot, &physical_port).await?;
    println!(
      "{}",
      json!({"event":"unchanged", "flash_writes":false, "application_reenumerated":true})
    );
    return Err(
      "Application is already installed; no flash writes made. Use --reflash to reinstall over USB"
        .into(),
    );
  }
  let elapsed = transfer(&interface, &app, usize::from(queue_depth)).await.map_err(|e| {
    format!("{e}. Bootloader was preserved. After the bootloader re-enumerates, restart the entire update with --transport usb firmware-update FILE --usb-port {physical_port} --reflash; see docs/usb.md")
  })?;
  finish(&interface).await?;
  drop(interface);
  wait_for(&physical_port, false).await?;
  println!(
    "{}",
    json!({"event":"usb_complete", "version":image.version(), "usb_port":physical_port,
    "blocks_verified":app.blocks(), "application_reenumerated":true,
    "erase_transfer_verify_seconds":elapsed, "total_usb_seconds":started.elapsed().as_secs_f64(),
    "ble_version_check_requested":verify_address.is_some()})
  );
  if let Some(address) = verify_address {
    verify_version(address, image.version()).await?;
  }
  Ok(())
}

#[cfg(test)]
mod tests {
  use super::*;
  use std::path::Path;

  #[test]
  fn pinned_images_fit_application_partition_and_keep_boot_identity() -> Result<()> {
    for (file, bytes, blocks) in [
      ("306007.MVA", 1_810_288, 442),
      ("306008-reflash-probe.MVA", 1_810_288, 442),
      ("306009-lua-probe.MVA", 1_810_600, 443),
      ("306012-lua.MVA", 1_958_608, 479),
      ("306013-lua.MVA", 1_959_420, 479),
      ("306014-lua.MVA", 1_959_420, 479),
      ("306015-lua.MVA", 1_959_420, 479),
      ("306016-lua.MVA", 1_959_420, 479),
      ("306017-lua.MVA", 1_959_420, 479),
      ("306018-lua.MVA", 1_959_420, 479),
    ] {
      let image = Image::load(
        &Path::new(env!("CARGO_MANIFEST_DIR"))
          .join("firmware")
          .join(file),
      )?;
      let app = Application::new(&image)?;
      assert_eq!(app.bytes.len(), bytes);
      assert_eq!(app.blocks(), blocks);
      let packet = app.metadata(false);
      assert_eq!(word(&packet, 8), bytes as u32);
      assert_eq!(packet[20], 0xff); // Encryption flag is code[0xff], not code[0x100].
      assert_eq!(word(&packet, 32), 0x9e80);
      assert_eq!(word(&packet, 36), 0x5f08);
      assert_eq!(packet[52..55], [2, 0, 0]);
      assert_eq!(&packet[12..20], &[0; 8]);
      assert_eq!(word(&app.metadata(true), 48), !word(&packet, 48));
      let tail = app.block(blocks - 1);
      let used = bytes - (blocks - 1) * BLOCK;
      assert_eq!(&tail[..used], &app.bytes[(blocks - 1) * BLOCK..]);
      assert!(tail[used..].iter().all(|b| *b == 0xff));
      assert_eq!(&app.block(0), &app.bytes[..BLOCK]);
    }
    Ok(())
  }

  #[test]
  fn commit_application_header_only_after_every_other_block() {
    assert_eq!(block_order(1).collect::<Vec<_>>(), [0]);
    assert_eq!(block_order(4).collect::<Vec<_>>(), [1, 2, 3, 0]);
  }

  #[test]
  fn reject_bootloader_resource_or_encryption_changes_before_erase() -> Result<()> {
    let mut reply = command(b"cxxx");
    reply[8..12].copy_from_slice(&[0x55, 0xff, 0xff, 0xff]);
    assert!(metadata_reply(&reply)?);
    reply[8] = 0xff;
    assert!(!metadata_reply(&reply)?);
    for flags in [
      [0x33, 0xff, 0xff, 0x33],
      [0x55, 0xff, 0xff, 0x55],
      [0x55, 0x55, 0xff, 0xff],
      [0x55, 0xff, 0x55, 0xff],
    ] {
      reply[8..12].copy_from_slice(&flags);
      assert!(metadata_reply(&reply).is_err());
    }
    assert!(metadata_reply(&reply[..11]).is_err());
    reply[0] = 0;
    assert!(metadata_reply(&reply).is_err());
    Ok(())
  }
}
