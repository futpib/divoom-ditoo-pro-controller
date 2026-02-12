use std::error::Error;
use std::fs::File;
use std::io::BufReader;
#[cfg(feature = "text")]
use std::path::Path;
use std::time::Duration;

use bluer::Address;
use chrono::{NaiveDateTime, NaiveTime};
use futures::StreamExt;
use log::{debug, info};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::sync::mpsc;

use crate::divoom_file_format::animation::Animation as DivoomAnimation;

pub mod divoom_file_format;
pub mod protocol;

use crate::protocol::alarm::Alarm;
use crate::protocol::animation::{Animation, ControlWord};
use crate::protocol::command::Command;
use crate::protocol::datetime::DateTime;
use crate::protocol::packet::{Packet, Response};


pub async fn scan_devices() -> Result<(), Box<dyn Error>> {
  let session = bluer::Session::new().await?;
  let adapter = session.default_adapter().await?;
  adapter.set_powered(true).await?;

  let duration = Duration::from_secs(20);
  info!("Scanning bluetooth devices for {:?}", duration);

  let discover = adapter.discover_devices().await?;
  tokio::pin!(discover);

  let timeout = tokio::time::sleep(duration);
  tokio::pin!(timeout);

  loop {
    tokio::select! {
      Some(event) = discover.next() => {
        if let bluer::AdapterEvent::DeviceAdded(addr) = event {
          let device = adapter.device(addr)?;
          let name = device.name().await?.unwrap_or_default();
          info!("Found device: {} ({})", name, addr);
        }
      }
      _ = &mut timeout => {
        info!("Scan complete");
        break;
      }
    }
  }

  Ok(())
}

pub async fn find_paired_ditoo_pro_devices() -> Result<Vec<Address>, Box<dyn Error>> {
  let session = bluer::Session::new().await?;
  let adapter = session.default_adapter().await?;

  let addresses = adapter.device_addresses().await?;

  let mut result = Vec::new();
  for addr in addresses {
    let device = adapter.device(addr)?;
    let is_paired = device.is_paired().await?;
    if !is_paired {
      continue;
    }

    let name = device.name().await?.unwrap_or_default();
    if !name.contains("DitooPro") {
      continue;
    }

    result.push(addr);
  }

  Ok(result)
}

pub async fn list_paired_devices() -> Result<(), Box<dyn Error>> {
  let session = bluer::Session::new().await?;
  let adapter = session.default_adapter().await?;

  for addr in find_paired_ditoo_pro_devices().await? {
    let device = adapter.device(addr)?;
    let name = device.name().await?.unwrap_or_default();
    let is_connected = device.is_connected().await?;
    info!(
      "{} ({}) - connected: {}",
      name, addr, is_connected
    );
  }

  Ok(())
}

async fn read_response(reader: &mut bluer::rfcomm::stream::OwnedReadHalf) -> Result<Response, Box<dyn Error + Send + Sync>> {
  let start = reader.read_u8().await?;
  if start != 0x01 {
    return Err(format!("Expected start byte 0x01, got 0x{:02x}", start).into());
  }

  let mut len_bytes = [0u8; 2];
  reader.read_exact(&mut len_bytes).await?;
  let length = u16::from_le_bytes(len_bytes) as usize;

  // Read: payload (length - 2 for checksum) + checksum (2) + end byte (1) = length + 1
  let mut remaining = vec![0u8; length + 1];
  reader.read_exact(&mut remaining).await?;

  let mut frame = vec![0x01];
  frame.extend_from_slice(&len_bytes);
  frame.extend_from_slice(&remaining);

  debug!("Received response: {}", hex::encode(&frame));

  Response::deserialize(&frame)
}

const MAX_CONNECT_ATTEMPTS: u32 = 3;
const INTER_PACKET_DELAY: Duration = Duration::from_millis(40);

struct DeviceConnection {
  writer: bluer::rfcomm::stream::OwnedWriteHalf,
  _reader_handle: tokio::task::JoinHandle<()>,
  response_rx: mpsc::UnboundedReceiver<Response>,
  _session: bluer::Session,
  _profile_handle: bluer::rfcomm::ProfileHandle,
}

impl DeviceConnection {
  async fn connect(mac_address: Address) -> Result<Self, Box<dyn Error>> {
    info!("Connecting to device with MAC address {}", mac_address);

    for attempt in 1..=MAX_CONNECT_ATTEMPTS {
      if attempt > 1 {
        info!("Retrying connection (attempt {}/{})..", attempt, MAX_CONNECT_ATTEMPTS);
        tokio::time::sleep(Duration::from_secs(1)).await;
      }

      match Self::try_connect(mac_address).await {
        Ok(conn) => return Ok(conn),
        Err(e) => {
          debug!("Connection attempt {} failed: {}", attempt, e);
        }
      }
    }

    Err("Failed to connect to device".into())
  }

  async fn try_connect(mac_address: Address) -> Result<Self, Box<dyn Error>> {
    let session = bluer::Session::new().await?;
    let adapter = session.default_adapter().await?;
    adapter.set_powered(true).await?;

    let spp_uuid = uuid::Uuid::from_u128(0x00001101_0000_1000_8000_00805f9b34fb);

    let profile = bluer::rfcomm::Profile {
      uuid: spp_uuid,
      name: Some("divoom-controller".to_string()),
      role: Some(bluer::rfcomm::Role::Client),
      require_authentication: Some(false),
      require_authorization: Some(false),
      auto_connect: Some(true),
      ..Default::default()
    };

    let mut profile_handle = session.register_profile(profile).await?;
    let device = adapter.device(mac_address)?;

    if !device.is_connected().await? {
      debug!("Device not connected, connecting...");
      device.connect().await?;
    }

    match device.connect_profile(&spp_uuid).await {
      Ok(()) => debug!("connect_profile succeeded"),
      Err(e) => debug!("connect_profile: {} (waiting for profile handle)", e),
    }

    let stream = loop {
      let req = profile_handle.next().await.ok_or("ProfileHandle stream closed")?;
      if req.device() == mac_address {
        break req.accept()?;
      }
    };

    info!("Connected via SDP profile");
    let (reader, writer) = stream.into_split();
    let (tx, rx) = mpsc::unbounded_channel();

    // Spawn background reader task
    let reader_handle = tokio::spawn(async move {
      let mut reader = reader;
      loop {
        match read_response(&mut reader).await {
          Ok(response) => {
            debug!("Background reader got response: {:?}", response);
            if tx.send(response).is_err() {
              debug!("Response channel closed, stopping reader");
              break;
            }
          }
          Err(e) => {
            debug!("Background reader error: {}", e);
            break;
          }
        }
      }
    });

    Ok(DeviceConnection {
      writer,
      _reader_handle: reader_handle,
      response_rx: rx,
      _session: session,
      _profile_handle: profile_handle,
    })
  }

  async fn send_and_receive(&mut self, packet: &Packet) -> Result<Response, Box<dyn Error>> {
    let serialized = packet.serialize()?;
    let expected_command = packet.command.value();
    debug!("send_and_receive 0x{:02x}: {}", expected_command, hex::encode(&serialized));
    self.writer.write_all(&serialized).await?;
    tokio::time::sleep(INTER_PACKET_DELAY).await;
    let deadline = tokio::time::Instant::now() + Duration::from_secs(5);
    loop {
      let response = tokio::time::timeout_at(deadline, self.response_rx.recv()).await
        .map_err(|_| format!("Timed out waiting for response to command 0x{:02x}", expected_command))?
        .ok_or("Response channel closed")?;
      if response.original_command != expected_command {
        debug!("Skipping unsolicited response for command 0x{:02x}", response.original_command);
        continue;
      }
      if !response.ack {
        return Err(format!("Device NAK'd command 0x{:02x}", expected_command).into());
      }
      return Ok(response);
    }
  }

  async fn fire_and_forget(&mut self, packet: &Packet) -> Result<(), Box<dyn Error>> {
    let serialized = packet.serialize()?;
    debug!("fire_and_forget 0x{:02x}: {}", packet.command.value(), hex::encode(&serialized));
    self.writer.write_all(&serialized).await?;
    tokio::time::sleep(INTER_PACKET_DELAY).await;
    Ok(())
  }

  async fn disconnect(mut self) -> Result<(), Box<dyn Error>> {
    info!("Disconnecting from device");
    self._reader_handle.abort();
    self.writer.shutdown().await?;
    Ok(())
  }
}

fn create_network_packets_from(animation: &[u8]) -> Result<Vec<Packet>, Box<dyn Error>> {
  let mut packets = Vec::<Packet>::new();
  packets.push(Packet {
    command: Command::Animation,
    payload: Animation {
      control_word: ControlWord::StartSeeding,
      file_size: animation.len() as u32,
      offset_id: 0,
      image_part: Vec::new()
    }
    .serialize()?
  });

  let mut animation_packets = animation
    .chunks(256)
    .enumerate()
    .map(|(index, chunk)| {
      Ok(Packet {
        command: Command::Animation,
        payload: Animation {
          control_word: ControlWord::SendingData,
          file_size: animation.len() as u32,
          offset_id: index as u16,
          image_part: chunk.to_vec()
        }
        .serialize()?
      })
    })
    .collect::<Result<Vec<_>, Box<dyn Error>>>()?;
  packets.append(&mut animation_packets);

  Ok(packets)
}

pub async fn send_alarm(mac_address: Address) -> Result<(), Box<dyn Error>> {
  let alarm = Alarm {
    index: 0,
    enable: false,
    time: NaiveTime::from_hms_opt(13, 37, 0).ok_or("Invalid time")?,
    repeat: 0,
    mode: 0,
    trigger_mode: 0,
    fm: [0, 0],
    volume: 100
  };
  let packet = Packet {
    command: Command::Alarm,
    payload: alarm.serialize()?
  };
  let mut conn = DeviceConnection::connect(mac_address).await?;
  conn.fire_and_forget(&packet).await?;
  conn.disconnect().await?;
  Ok(())
}

pub async fn send_divoom_animation(
  mac_address: Address,
  reader: &mut (dyn std::io::Read + Send)
) -> Result<(), Box<dyn Error>> {
  let mut animation = Vec::new();
  reader.read_to_end(&mut animation)?;

  let packets = create_network_packets_from(&animation)?;
  let mut conn = DeviceConnection::connect(mac_address).await?;
  for (index, packet) in packets.iter().enumerate() {
    info!("Sending packet {}/{}..", index + 1, packets.len());
    conn.fire_and_forget(packet).await?;
  }
  conn.disconnect().await?;
  Ok(())
}

pub async fn send_set_datetime(
  mac_address: Address,
  datetime: NaiveDateTime
) -> Result<(), Box<dyn Error>> {
  let payload = DateTime { datetime };
  let packet = Packet {
    command: Command::SetDateTime,
    payload: payload.serialize()?
  };
  let mut conn = DeviceConnection::connect(mac_address).await?;
  conn.fire_and_forget(&packet).await?;
  conn.disconnect().await?;
  Ok(())
}

pub async fn send_set_brightness(
  mac_address: Address,
  brightness: u8
) -> Result<(), Box<dyn Error>> {
  let packet = Packet {
    command: Command::SetBrightness,
    payload: vec![brightness]
  };
  let mut conn = DeviceConnection::connect(mac_address).await?;
  conn.fire_and_forget(&packet).await?;
  conn.disconnect().await?;
  Ok(())
}

pub async fn send_set_volume(
  mac_address: Address,
  volume: u8
) -> Result<(), Box<dyn Error>> {
  let packet = Packet {
    command: Command::SetVolume,
    payload: vec![volume]
  };
  let mut conn = DeviceConnection::connect(mac_address).await?;
  conn.fire_and_forget(&packet).await?;
  conn.disconnect().await?;
  Ok(())
}

pub async fn send_set_play_status(
  mac_address: Address,
  playing: bool
) -> Result<(), Box<dyn Error>> {
  let packet = Packet {
    command: Command::SetPlayStatus,
    payload: vec![if playing { 1 } else { 0 }]
  };
  let mut conn = DeviceConnection::connect(mac_address).await?;
  conn.fire_and_forget(&packet).await?;
  conn.disconnect().await?;
  Ok(())
}

pub async fn send_get_volume(
  mac_address: Address,
) -> Result<u8, Box<dyn Error>> {
  let packet = Packet {
    command: Command::GetVolume,
    payload: vec![]
  };
  let mut conn = DeviceConnection::connect(mac_address).await?;
  let response = conn.send_and_receive(&packet).await?;
  conn.disconnect().await?;
  let volume = response.data.first()
    .ok_or("GetVolume response contained no data")?;
  Ok(*volume)
}

pub async fn send_set_box_mode(
  mac_address: Address,
  payload: Vec<u8>
) -> Result<(), Box<dyn Error>> {
  let packet = Packet {
    command: Command::SetBoxMode,
    payload
  };
  let mut conn = DeviceConnection::connect(mac_address).await?;
  conn.fire_and_forget(&packet).await?;
  conn.disconnect().await?;
  Ok(())
}

pub async fn send_set_clock_face(
  mac_address: Address,
  clock_id: u16
) -> Result<(), Box<dyn Error>> {
  let packet = protocol::extended_command::build_packet(
    protocol::extended_command::SET_USER_DEFINE_TIME,
    &clock_id.to_le_bytes(),
  );
  let mut conn = DeviceConnection::connect(mac_address).await?;
  conn.fire_and_forget(&packet).await?;
  conn.disconnect().await?;
  Ok(())
}

pub async fn send_get_clock_face(
  mac_address: Address,
) -> Result<u16, Box<dyn Error>> {
  let packet = protocol::extended_command::build_packet(
    protocol::extended_command::GET_USER_DEFINE_TIME,
    &[],
  );
  let mut conn = DeviceConnection::connect(mac_address).await?;
  let response = conn.send_and_receive(&packet).await?;
  conn.disconnect().await?;
  if response.data.len() < 3 {
    return Err("GetClockFace response too short".into());
  }
  let clock_id = u16::from_le_bytes([response.data[1], response.data[2]]);
  Ok(clock_id)
}

pub async fn send_set_language(
  mac_address: Address,
  lang_index: u8
) -> Result<(), Box<dyn Error>> {
  let packet = protocol::extended_command::build_packet(
    protocol::extended_command::SET_LANGUAGE,
    &[lang_index],
  );
  let mut conn = DeviceConnection::connect(mac_address).await?;
  conn.fire_and_forget(&packet).await?;
  conn.disconnect().await?;
  Ok(())
}

pub async fn send_keyboard_backlight(
  mac_address: Address,
  mode: u8
) -> Result<(), Box<dyn Error>> {
  let packet = Packet {
    command: Command::LightArrowSwitch,
    payload: protocol::keyboard_backlight::serialize(mode)
  };
  let mut conn = DeviceConnection::connect(mac_address).await?;
  conn.fire_and_forget(&packet).await?;
  conn.disconnect().await?;
  Ok(())
}

#[cfg(feature = "text")]
pub async fn send_scrolling_text(
  mac_address: Address,
  font_path: &Path,
  text: &str,
  font_size: f32,
  fg_color: [u8; 3],
  bg_color: [u8; 3],
  halign: protocol::scrolling_text::HAlign,
  valign: protocol::scrolling_text::VAlign,
) -> Result<(), Box<dyn Error>> {
  let scrolling_text = protocol::scrolling_text::build_scrolling_text_frames(font_path, text, font_size, fg_color, bg_color, halign, valign)?;
  let mut conn = DeviceConnection::connect(mac_address).await?;

  conn.fire_and_forget(&Packet {
    command: Command::DrawingCtrlMoviePlay,
    payload: vec![0x00],
  }).await?;

  conn.fire_and_forget(&Packet {
    command: Command::DrawingCtrlMoviePlay,
    payload: vec![0x01],
  }).await?;

  let speed: u16 = 60;
  let total = scrolling_text.encoded_frames.len();
  for (index, encoded_data) in scrolling_text.encoded_frames.iter().enumerate() {
    let data_len = encoded_data.len() as u16;
    let mut payload = Vec::new();
    payload.extend_from_slice(&speed.to_le_bytes());
    payload.extend_from_slice(&data_len.to_le_bytes());
    payload.extend_from_slice(encoded_data);

    let packet = Packet {
      command: Command::DrawingEncodeMoviePlay,
      payload,
    };

    if index % 100 == 0 {
      info!("Sending frame {}/{}..", index + 1, total);
    }
    conn.fire_and_forget(&packet).await?;
    tokio::time::sleep(Duration::from_millis(20)).await;
  }

  conn.fire_and_forget(&Packet {
    command: Command::DrawingCtrlMoviePlay,
    payload: vec![0x00],
  }).await?;

  conn.disconnect().await?;
  Ok(())
}


#[cfg(feature = "text")]
pub async fn send_static_text(
  mac_address: Address,
  font_path: &Path,
  text: &str,
  font_size: f32,
  fg_color: [u8; 3],
  bg_color: [u8; 3],
  halign: protocol::scrolling_text::HAlign,
  valign: protocol::scrolling_text::VAlign,
) -> Result<(), Box<dyn Error>> {
  let image = protocol::static_text::build_static_text_image(font_path, text, font_size, fg_color, bg_color, halign, valign)?;
  let animation = DivoomAnimation::from_image(image)?;
  let mut buf = Vec::new();
  animation.save_to_divoom_format(&mut buf)?;
  let packets = create_network_packets_from(&buf)?;
  let mut conn = DeviceConnection::connect(mac_address).await?;
  for (index, packet) in packets.iter().enumerate() {
    info!("Sending packet {}/{}..", index + 1, packets.len());
    conn.fire_and_forget(packet).await?;
  }
  conn.disconnect().await?;
  Ok(())
}

pub async fn send_image(
  mac_address: Address,
  filename: &str
) -> Result<(), Box<dyn Error>> {
  let animation = if filename.ends_with(".gif") || filename.ends_with(".GIF") {
    DivoomAnimation::from_gif(&mut BufReader::new(File::open(filename)?))?
  } else {
    DivoomAnimation::from_image(image::open(filename)?)?
  };
  let mut buf = Vec::new();
  animation.save_to_divoom_format(&mut buf)?;
  let packets = create_network_packets_from(&buf)?;
  let mut conn = DeviceConnection::connect(mac_address).await?;
  for (index, packet) in packets.iter().enumerate() {
    info!("Sending packet {}/{}..", index + 1, packets.len());
    conn.fire_and_forget(packet).await?;
  }
  conn.disconnect().await?;
  Ok(())
}
