use crate::{
  protocol::packet::{Packet, Response},
  ClassicConnection,
};
use bluer::{
  gatt::{remote::Characteristic, remote::CharacteristicWriteRequest, WriteOp},
  Address,
};
use futures::StreamExt;
use std::{collections::VecDeque, error::Error, future::Future, time::Duration};
use tokio::sync::mpsc;

#[derive(Clone, Copy, Debug, Default, clap::ValueEnum)]
pub enum Transport {
  #[default]
  Auto,
  Rfcomm,
  Ble,
  /// USB control (custom firmware 306019+) and bootloader updates
  Usb,
}

tokio::task_local! { static TRANSPORT: Transport; }
tokio::task_local! { static USB_PORT: Option<String>; }

pub fn selected_transport() -> Transport {
  TRANSPORT.try_with(|t| *t).unwrap_or_default()
}

/// Select the physical USB port without requiring a Bluetooth address.
pub async fn with_usb_port<F: Future>(port: Option<String>, future: F) -> F::Output {
  USB_PORT.scope(port, future).await
}

/// Select a transport for controller calls within this future.
pub async fn with_transport<F: Future>(transport: Transport, future: F) -> F::Output {
  TRANSPORT.scope(transport, future).await
}

pub(crate) enum DeviceConnection {
  Classic(ClassicConnection),
  Ble(BleConnection),
  Usb(crate::usb_control::UsbConnection),
}

impl DeviceConnection {
  pub async fn connect(address: Address) -> Result<Self, Box<dyn Error>> {
    let transport = selected_transport();
    if matches!(transport, Transport::Usb) {
      let port = USB_PORT.try_with(Clone::clone).unwrap_or_default();
      return Ok(Self::Usb(crate::usb_control::UsbConnection::connect(port.as_deref()).await?));
    }
    if !matches!(transport, Transport::Ble) {
      match ClassicConnection::connect(address).await {
        Ok(mut connection) => {
          if matches!(transport, Transport::Auto) {
            let probe = Packet {
              command: crate::protocol::command::Command::GetVolume,
              payload: vec![],
            };
            match connection.send_and_receive(&probe).await {
              Ok(_) => return Ok(Self::Classic(connection)),
              Err(error) => {
                log::info!("RFCOMM did not answer the read-only connection probe ({error}); falling back to BLE");
                connection.disconnect().await?;
              }
            }
          } else {
            return Ok(Self::Classic(connection));
          }
        }
        Err(error) if matches!(transport, Transport::Auto) => {
          log::info!("RFCOMM connection failed ({error}); falling back to BLE");
        }
        Err(error) => return Err(error),
      }
    }
    Ok(Self::Ble(
      tokio::time::timeout(Duration::from_secs(30), BleConnection::connect(address)).await??,
    ))
  }

  pub async fn fire_and_forget(&mut self, packet: &Packet) -> Result<(), Box<dyn Error>> {
    match self {
      Self::Classic(c) => c.fire_and_forget(packet).await,
      Self::Ble(c) => c.send(packet, None).await.map(|_| ()),
      Self::Usb(c) => c.send(packet, false).await,
    }
  }

  /// Send a stream packet without discarding asynchronous device update events.
  pub async fn stream_write(&mut self, packet: &Packet) -> Result<(), Box<dyn Error>> {
    match self {
      Self::Classic(c) => c.fire_and_forget(packet).await,
      Self::Ble(c) => c.send_inner(packet, None, true).await.map(|_| ()),
      Self::Usb(c) => c.send(packet, true).await,
    }
  }

  pub async fn send_and_receive(&mut self, packet: &Packet) -> Result<Response, Box<dyn Error>> {
    self.exchange(packet, packet.command.value(), &[]).await
  }

  pub async fn exchange(
    &mut self,
    packet: &Packet,
    expected: u8,
    prefix: &[u8],
  ) -> Result<Response, Box<dyn Error>> {
    match self {
      Self::Classic(c) => c.exchange(packet, expected, prefix).await,
      Self::Usb(c) => c.exchange(packet, expected, prefix).await,
      Self::Ble(c) => c
        .send(packet, Some((expected, prefix)))
        .await?
        .ok_or_else(|| "Missing command response".into()),
    }
  }

  pub fn transport_name(&self) -> &'static str {
    match self {
      Self::Classic(_) => "rfcomm",
      Self::Ble(_) => "ble",
      Self::Usb(_) => "usb",
    }
  }

  pub async fn receive(&mut self, timeout: Duration) -> Result<Option<Response>, Box<dyn Error>> {
    if let Self::Usb(c) = self { return c.receive(timeout).await; }
    if let Self::Ble(c) = self {
      if let Some(reply) = c.pending.pop_front() {
        return Ok(Some(reply));
      }
    }
    let rx = match self {
      Self::Classic(c) => &mut c.response_rx,
      Self::Ble(c) => &mut c.responses,
      Self::Usb(_) => unreachable!("USB response handled above"),
    };
    match tokio::time::timeout(timeout, rx.recv()).await {
      Ok(Some(reply)) => Ok(Some(reply)),
      Ok(None) => Err("Device response channel closed".into()),
      Err(_) => Ok(None),
    }
  }

  pub async fn disconnect(self) -> Result<(), Box<dyn Error>> {
    match self {
      Self::Classic(c) => c.disconnect().await,
      Self::Ble(mut c) => c.device.release().await,
      Self::Usb(c) => c.disconnect().await,
    }
  }

  pub async fn delay(&mut self, duration: Duration) -> Result<(), Box<dyn Error>> {
    if let Self::Usb(c) = self { c.delay(duration).await?; }
    else { tokio::time::sleep(duration).await; }
    Ok(())
  }
}

fn envelope(payload: &[u8], sequence: u16) -> Result<Vec<u8>, Box<dyn Error>> {
  let length = u16::try_from(payload.len() + 7)?;
  let mut wire = vec![0xfe, 0xef, 0xaa, 0x55];
  wire.extend(length.to_le_bytes());
  wire.extend(sequence.to_be_bytes());
  wire.extend([0, 0, 0]);
  wire.extend(payload);
  let checksum = wire[4..]
    .iter()
    .fold(0u16, |sum, b| sum.wrapping_add(*b as u16));
  wire.extend(checksum.to_le_bytes());
  Ok(wire)
}

fn parse_frames(buffer: &mut Vec<u8>) -> Result<Vec<Response>, Box<dyn Error + Send + Sync>> {
  let mut replies = Vec::new();
  while buffer.len() >= 3 {
    let size = u16::from_le_bytes([buffer[1], buffer[2]]) as usize + 4;
    if buffer[0] != 1 || size < 9 {
      return Err("Invalid BLE response framing".into());
    }
    if buffer.len() < size {
      break;
    }
    replies.push(Response::deserialize(&buffer[..size])?);
    buffer.drain(..size);
  }
  Ok(replies)
}

pub(crate) struct DeviceLease {
  device: bluer::Device,
  owned: bool,
  le_only: bool,
}

#[derive(Clone, Copy)]
enum LeOperation {
  Connected,
  Connect,
  Disconnect,
}

// bluer does not yet expose BlueZ's per-bearer API. Keep blocking D-Bus work
// off the async executor; each call owns and closes its system-bus connection.
async fn le_bearer(
  device: &bluer::Device,
  operation: LeOperation,
) -> Result<Option<bool>, Box<dyn Error>> {
  let path = format!(
    "/org/bluez/{}/dev_{}",
    device.adapter_name(),
    device.address().to_string().replace(':', "_")
  );
  let result = tokio::task::spawn_blocking(move || -> Result<bool, dbus::Error> {
    use dbus::blocking::stdintf::org_freedesktop_dbus::Properties;
    let connection = dbus::blocking::Connection::new_system()?;
    let proxy = connection.with_proxy("org.bluez", path, Duration::from_secs(10));
    let interface = "org.bluez.Bearer.LE1";
    match operation {
      LeOperation::Connected => proxy.get(interface, "Connected"),
      LeOperation::Connect | LeOperation::Disconnect => {
        let method = if matches!(operation, LeOperation::Connect) {
          "Connect"
        } else {
          "Disconnect"
        };
        proxy.method_call::<(), _, _, _>(interface, method, ())?;
        Ok(true)
      }
    }
  })
  .await?;
  match result {
    Ok(value) => Ok(Some(value)),
    Err(error)
      if matches!(
        error.name(),
        Some(
          "org.freedesktop.DBus.Error.UnknownMethod"
            | "org.freedesktop.DBus.Error.UnknownInterface"
            | "org.freedesktop.DBus.Error.UnknownProperty"
        )
      ) =>
    {
      Ok(None)
    }
    Err(error) => Err(error.into()),
  }
}

async fn disconnect_lease(device: &bluer::Device, le_only: bool) -> Result<(), Box<dyn Error>> {
  if le_only {
    if le_bearer(device, LeOperation::Disconnect).await?.is_some() {
      return Ok(());
    }
    if device.class().await?.is_some() {
      // Older BlueZ can connect LE explicitly but cannot disconnect only LE.
      // Retain that connection rather than tear down another classic profile.
      log::debug!("Leaving BLE connected: BlueZ lacks per-bearer disconnect");
      return Ok(());
    }
  }
  device.disconnect().await?;
  Ok(())
}

impl DeviceLease {
  pub async fn new(device: bluer::Device) -> Result<Self, Box<dyn Error>> {
    let owned = !device.is_connected().await?;
    Ok(Self {
      device,
      owned,
      le_only: false,
    })
  }

  async fn new_ble(device: bluer::Device) -> Result<Self, Box<dyn Error>> {
    let connected = match le_bearer(&device, LeOperation::Connected).await? {
      Some(connected) => connected,
      None => device.is_connected().await?,
    };
    Ok(Self {
      device,
      owned: !connected,
      le_only: true,
    })
  }

  pub async fn release(&mut self) -> Result<(), Box<dyn Error>> {
    if self.owned {
      self.owned = false;
      tokio::time::timeout(
        Duration::from_secs(12),
        disconnect_lease(&self.device, self.le_only),
      )
      .await??;
    }
    Ok(())
  }
}

impl Drop for DeviceLease {
  fn drop(&mut self) {
    if self.owned {
      let device = self.device.clone();
      let le_only = self.le_only;
      if let Ok(runtime) = tokio::runtime::Handle::try_current() {
        runtime.spawn(async move {
          let _ = disconnect_lease(&device, le_only).await;
        });
      }
    }
  }
}

pub(crate) struct BleConnection {
  write: Characteristic,
  stream_write_size: usize,
  responses: mpsc::UnboundedReceiver<Response>,
  pending: VecDeque<Response>,
  reader: tokio::task::JoinHandle<()>,
  sequence: u16,
  _session: bluer::Session,
  device: DeviceLease,
}

impl Drop for BleConnection {
  fn drop(&mut self) {
    self.reader.abort();
  }
}

async fn connect_ble_bearer(
  adapter: &bluer::Adapter,
  device: &bluer::Device,
) -> Result<(), Box<dyn Error>> {
  if let Some(connected) = le_bearer(device, LeOperation::Connected).await? {
    if !connected {
      le_bearer(device, LeOperation::Connect).await?;
    }
    while !device.is_services_resolved().await? {
      tokio::time::sleep(Duration::from_millis(50)).await;
    }
    return Ok(());
  }
  // Device1.Connect may choose BR/EDR for a dual-mode Ditoo and claim its
  // audio connection. ConnectDevice's explicit address type selects LE.
  let address_type = match device.address_type().await? {
    bluer::AddressType::LeRandom => bluer::AddressType::LeRandom,
    _ => bluer::AddressType::LePublic,
  };
  match adapter.connect_device(device.address(), address_type).await {
    Ok(_) => (),
    Err(error) if error.kind == bluer::ErrorKind::AlreadyConnected => (),
    Err(error)
      if matches!(&error.kind,
      bluer::ErrorKind::Internal(bluer::InternalErrorKind::DBus(name))
        if name == "org.freedesktop.DBus.Error.UnknownMethod") =>
    {
      // Keep older BlueZ installations usable for LE-only discovery, but do
      // not silently switch a known audio device to the classic bearer.
      if device.class().await?.is_some() {
        return Err("Explicit BLE selection requires BlueZ Experimental=true in /etc/bluetooth/main.conf; generic Connect could take the Ditoo audio connection".into());
      }
      if !device.is_connected().await? {
        device.connect().await?;
      }
    }
    Err(error) => return Err(error.into()),
  }
  while !device.is_services_resolved().await? {
    tokio::time::sleep(Duration::from_millis(50)).await;
  }
  Ok(())
}

impl BleConnection {
  async fn connect(address: Address) -> Result<Self, Box<dyn Error>> {
    let session = bluer::Session::new().await?;
    let adapter = session.default_adapter().await?;
    adapter.set_powered(true).await?;
    // Discover before reading Device1 properties: BlueZ can remove an
    // unpaired device object after the previous connection ends.
    adapter
      .set_discovery_filter(bluer::DiscoveryFilter {
        transport: bluer::DiscoveryTransport::Le,
        ..Default::default()
      })
      .await?;
    let discovery = adapter.discover_devices().await?;
    tokio::time::sleep(Duration::from_secs(2)).await;
    let device = adapter.device(address)?;
    let lease = DeviceLease::new_ble(device.clone()).await?;
    connect_ble_bearer(&adapter, &device).await?;
    drop(discovery);
    let mut write = None;
    let mut notify = None;
    for service in device.services().await? {
      if service.uuid().await? != uuid::Uuid::from_u128(0x49535343_fe7d_4ae5_8fa9_9fafd205e455) {
        continue;
      }
      for characteristic in service.characteristics().await? {
        match characteristic.uuid().await?.as_u128() {
          0x49535343_8841_43f4_a8d4_ecbe34729bb3 => write = Some(characteristic),
          0x49535343_1e4d_4bd9_ba61_23c647249616 => notify = Some(characteristic),
          _ => (),
        }
      }
    }
    let write = write.ok_or("Divoom BLE write characteristic not found")?;
    let notify = notify.ok_or("Divoom BLE notification characteristic not found")?;
    let notifications = match notify.notify().await {
      Ok(notifications) => notifications,
      Err(error) if error.kind == bluer::ErrorKind::Failed && error.message == "Not connected" => {
        // BlueZ can report a connected classic bearer with cached GATT services.
        connect_ble_bearer(&adapter, &device).await?;
        notify.notify().await?
      }
      Err(error) => return Err(error.into()),
    };
    let (tx, responses) = mpsc::unbounded_channel();
    let reader = tokio::spawn(async move {
      tokio::pin!(notifications);
      let mut buffer = Vec::new();
      while let Some(bytes) = notifications.next().await {
        log::trace!(
          "BLE notification ({} bytes): {}",
          bytes.len(),
          hex::encode(&bytes)
        );
        buffer.extend(bytes);
        match parse_frames(&mut buffer) {
          Ok(replies) => {
            for reply in replies {
              if tx.send(reply).is_err() {
                return;
              }
            }
          }
          Err(error) => {
            log::warn!("BLE response error: {error}");
            return;
          }
        }
      }
    });
    let mut connection = Self {
      write,
      stream_write_size: 20,
      responses,
      pending: VecDeque::new(),
      reader,
      sequence: 0x0101,
      _session: session,
      device: lease,
    };
    let now = chrono::Local::now();
    let init = format!(
      r#"{{"Command":"Device\/SetUTC","Utc":{},"Time":"{}"}}"#,
      now.timestamp(),
      now.format("%Y-%m-%d %H:%M:%S")
    );
    if let Err(error) = connection.write_payload(init.as_bytes(), 1, None).await {
      let not_connected = error
        .downcast_ref::<bluer::Error>()
        .is_some_and(|e| e.kind == bluer::ErrorKind::Failed && e.message == "Not connected");
      if !not_connected {
        return Err(error);
      }
      // Cached notifications can also succeed with only the classic bearer active.
      connect_ble_bearer(&adapter, &connection.device.device).await?;
      connection.write_payload(init.as_bytes(), 1, None).await?;
    }
    tokio::time::sleep(Duration::from_secs(1)).await;
    connection.refresh_stream_capacity().await;
    // Stock BLE remembers the last binary sequence across controller sessions.
    // Reusing it gets an ACK without executing the command. Prime that counter
    // with a read-only volume query; the first user command then has a different
    // sequence whether this primer was executed or deduplicated. JSON clock
    // initialization uses a separate path and does not reset this counter.
    connection
      .send(
        &Packet {
          command: crate::protocol::command::Command::GetVolume,
          payload: Vec::new(),
        },
        None,
      )
      .await?;
    log::info!("Connected via BLE GATT (device clock synchronized)");
    Ok(connection)
  }

  async fn refresh_stream_capacity(&mut self) {
    // bluer's mtu() already subtracts ATT overhead; do not subtract it twice.
    // Cached GATT services may expose MTU only after the bearer reconnects.
    if let Ok(size) = self.write.mtu().await {
      if size > 0 && size.min(512) != self.stream_write_size {
        self.stream_write_size = size.min(512);
        log::info!(
          "BLE streaming write capacity: {} bytes",
          self.stream_write_size
        );
      }
    }
  }

  async fn write_payload(
    &mut self,
    payload: &[u8],
    sequence: u16,
    stream_limit: Option<usize>,
  ) -> Result<(), Box<dyn Error>> {
    let streaming = stream_limit.is_some();
    if streaming && self.stream_write_size <= 20 {
      self.refresh_stream_capacity().await;
    }
    let capacity = if let Some(limit) = stream_limit {
      self.stream_write_size.min(limit)
    } else {
      20
    };
    for chunk in envelope(payload, sequence)?.chunks(capacity) {
      self
        .write
        .write_ext(
          chunk,
          &CharacteristicWriteRequest {
            op_type: WriteOp::Request,
            ..Default::default()
          },
        )
        .await?;
      // Acknowledged GATT writes provide backpressure for source/firmware data.
      if !streaming {
        tokio::time::sleep(Duration::from_millis(50)).await;
      }
    }
    Ok(())
  }

  async fn send(
    &mut self,
    packet: &Packet,
    expected: Option<(u8, &[u8])>,
  ) -> Result<Option<Response>, Box<dyn Error>> {
    self.send_inner(packet, expected, false).await
  }

  async fn send_inner(
    &mut self,
    packet: &Packet,
    expected: Option<(u8, &[u8])>,
    preserve: bool,
  ) -> Result<Option<Response>, Box<dyn Error>> {
    if !preserve {
      self.pending.clear();
      while self.responses.try_recv().is_ok() {}
    }
    let sequence = self.sequence;
    self.sequence = if sequence == 0x01ff {
      0x0101
    } else {
      sequence + 1
    };
    let mut payload = vec![packet.command.value()];
    payload.extend(&packet.payload);
    // Source uploads can span hundreds of fragments. Pace them with ATT write
    // responses, like firmware data, instead of a fixed delay per 20 bytes.
    // Bound each source-data write to 256 bytes even when the peer offers more.
    let stream_limit = if preserve {
      Some(512)
    } else if packet.command.value() == 0x37 && packet.payload.starts_with(b"\x7fDLUA\x04") {
      Some(256)
    } else {
      None
    };
    self.write_payload(&payload, sequence, stream_limit).await?;
    let deadline = tokio::time::Instant::now() + Duration::from_secs(5);
    let mut acknowledged = false;
    let mut command_response = None;
    loop {
      let reply = tokio::time::timeout_at(deadline, self.responses.recv())
        .await
        .map_err(|_| "Timed out waiting for BLE acknowledgment/response")?
        .ok_or("BLE notifications stopped")?;
      if reply.original_command == 0x33 && reply.data == [sequence as u8, 0, 0, 0] {
        if !reply.ack {
          return Err("BLE transport rejected command".into());
        }
        acknowledged = true;
      } else if expected.is_some_and(|(code, prefix)| {
        reply.original_command == code && reply.data.starts_with(prefix)
      }) {
        if !reply.ack {
          return Err("Device rejected command".into());
        }
        command_response = Some(reply);
      } else if preserve {
        self.pending.push_back(reply);
      }
      if acknowledged && (expected.is_none() || command_response.is_some()) {
        return Ok(command_response);
      }
    }
  }
}

#[cfg(test)]
mod tests {
  use super::*;
  #[test]
  fn captured_brightness_packet() -> Result<(), Box<dyn Error>> {
    assert_eq!(
      hex::encode(envelope(&[0x74, 0], 0x0101)?),
      "feefaa550900010100000074007f00"
    );
    Ok(())
  }
  #[test]
  fn fragmented_and_concatenated_notifications() -> Result<(), Box<dyn Error + Send + Sync>> {
    let mut buffer = hex::decode("01090004335501")?;
    assert!(parse_frames(&mut buffer)?.is_empty());
    buffer.extend(hex::decode("000000960002010a000437550157ab0400a10102")?);
    let replies = parse_frames(&mut buffer)?;
    assert_eq!(replies.len(), 2);
    assert_eq!(replies[0].original_command, 0x33);
    assert_eq!(replies[1].data, [1, 0x57, 0xab, 4, 0]);
    assert!(buffer.is_empty());
    Ok(())
  }
  #[test]
  fn corrupt_notification_is_rejected() -> Result<(), Box<dyn Error>> {
    let mut buffer = hex::decode("01090004335501000000970002")?;
    assert!(parse_frames(&mut buffer).is_err());
    Ok(())
  }
}
