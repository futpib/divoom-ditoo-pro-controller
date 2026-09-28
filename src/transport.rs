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
}

tokio::task_local! { static TRANSPORT: Transport; }

/// Select a transport for controller calls within this future.
pub async fn with_transport<F: Future>(transport: Transport, future: F) -> F::Output {
  TRANSPORT.scope(transport, future).await
}

pub(crate) enum DeviceConnection {
  Classic(ClassicConnection),
  Ble(BleConnection),
}

impl DeviceConnection {
  pub async fn connect(address: Address) -> Result<Self, Box<dyn Error>> {
    let transport = TRANSPORT.try_with(|t| *t).unwrap_or_default();
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
    }
  }

  /// Send a stream packet without discarding asynchronous device update events.
  pub async fn stream_write(&mut self, packet: &Packet) -> Result<(), Box<dyn Error>> {
    match self {
      Self::Classic(c) => c.fire_and_forget(packet).await,
      Self::Ble(c) => c.send_inner(packet, None, true).await.map(|_| ()),
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
    }
  }

  pub async fn receive(&mut self, timeout: Duration) -> Result<Option<Response>, Box<dyn Error>> {
    if let Self::Ble(c) = self {
      if let Some(reply) = c.pending.pop_front() {
        return Ok(Some(reply));
      }
    }
    let rx = match self {
      Self::Classic(c) => &mut c.response_rx,
      Self::Ble(c) => &mut c.responses,
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
    }
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
}

impl DeviceLease {
  pub async fn new(device: bluer::Device) -> Result<Self, Box<dyn Error>> {
    let owned = !device.is_connected().await?;
    Ok(Self { device, owned })
  }

  pub async fn release(&mut self) -> Result<(), Box<dyn Error>> {
    if self.owned {
      self.device.disconnect().await?;
      self.owned = false;
    }
    Ok(())
  }
}

impl Drop for DeviceLease {
  fn drop(&mut self) {
    if self.owned {
      let device = self.device.clone();
      if let Ok(runtime) = tokio::runtime::Handle::try_current() {
        runtime.spawn(async move {
          let _ = device.disconnect().await;
        });
      }
    }
  }
}

pub(crate) struct BleConnection {
  write: Characteristic,
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
  adapter
    .set_discovery_filter(bluer::DiscoveryFilter {
      transport: bluer::DiscoveryTransport::Le,
      ..Default::default()
    })
    .await?;
  let discovery = adapter.discover_devices().await?;
  tokio::time::sleep(Duration::from_secs(2)).await;
  device.connect().await?;
  drop(discovery);
  Ok(())
}

impl BleConnection {
  async fn connect(address: Address) -> Result<Self, Box<dyn Error>> {
    let session = bluer::Session::new().await?;
    let adapter = session.default_adapter().await?;
    adapter.set_powered(true).await?;
    let device = adapter.device(address)?;
    let lease = DeviceLease::new(device.clone()).await?;
    if !device.is_connected().await? || !device.is_services_resolved().await? {
      connect_ble_bearer(&adapter, &device).await?;
    }
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
    if let Err(error) = connection.write_payload(init.as_bytes(), 1).await {
      let not_connected = error
        .downcast_ref::<bluer::Error>()
        .is_some_and(|e| e.kind == bluer::ErrorKind::Failed && e.message == "Not connected");
      if !not_connected {
        return Err(error);
      }
      // Cached notifications can also succeed with only the classic bearer active.
      connect_ble_bearer(&adapter, &connection.device.device).await?;
      connection.write_payload(init.as_bytes(), 1).await?;
    }
    tokio::time::sleep(Duration::from_secs(1)).await;
    log::info!("Connected via BLE GATT (device clock synchronized)");
    Ok(connection)
  }

  async fn write_payload(&mut self, payload: &[u8], sequence: u16) -> Result<(), Box<dyn Error>> {
    for chunk in envelope(payload, sequence)?.chunks(20) {
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
      tokio::time::sleep(Duration::from_millis(50)).await;
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
    self.write_payload(&payload, sequence).await?;
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
