use std::error::Error;
use std::fs::File;
use std::io::BufReader;
use std::time::Duration;

use bluer::Address;
use chrono::{NaiveDateTime, NaiveTime};
use futures::StreamExt;
use log::{debug, info};
use tokio::io::{AsyncReadExt, AsyncWriteExt};

use crate::divoom_file_format::animation::Animation as DivoomAnimation;

pub mod divoom_file_format;
pub mod protocol;

use crate::protocol::alarm::Alarm;
use crate::protocol::animation::{Animation, ControlWord};
use crate::protocol::command::Command;
use crate::protocol::datetime::DateTime;
use crate::protocol::packet::{Packet, Response};


pub async fn list_devices() -> Result<(), Box<dyn Error>> {
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

async fn read_response(stream: &mut bluer::rfcomm::Stream) -> Result<Response, Box<dyn Error>> {
  let result = tokio::time::timeout(Duration::from_secs(5), async {
    let start = stream.read_u8().await?;
    if start != 0x01 {
      return Err(format!("Expected start byte 0x01, got 0x{:02x}", start).into());
    }

    let mut len_bytes = [0u8; 2];
    stream.read_exact(&mut len_bytes).await?;
    let length = u16::from_le_bytes(len_bytes) as usize;

    // Read: payload (length - 2 for checksum) + checksum (2) + end byte (1) = length + 1
    let mut remaining = vec![0u8; length + 1];
    stream.read_exact(&mut remaining).await?;

    let mut frame = vec![0x01];
    frame.extend_from_slice(&len_bytes);
    frame.extend_from_slice(&remaining);

    debug!("Received response: {}", hex::encode(&frame));

    Response::deserialize(&frame)
  }).await?;

  result
}

async fn try_connect(mac_address: Address, packets: &[Packet]) -> Result<(bluer::rfcomm::Stream, Response), Box<dyn Error>> {
  for channel in [2u8].into_iter().chain((1..=30).filter(|&c| c != 2)) {
    let socket = bluer::rfcomm::Socket::new()?;
    let addr = bluer::rfcomm::SocketAddr::new(mac_address, channel);
    let mut s = match socket.connect(addr).await {
      Ok(s) => {
        info!("Connected on RFCOMM channel {}", channel);
        s
      }
      Err(e) => {
        debug!("Channel {} connect failed: {}", channel, e);
        continue;
      }
    };

    let first_serialized = packets[0].serialize()?;
    info!("Sending packet 1/{}..", packets.len());
    debug!("  {}", hex::encode(&first_serialized));
    match s.write_all(&first_serialized).await {
      Ok(()) => {
        info!("  Wrote {} bytes", first_serialized.len());
      }
      Err(e) => {
        debug!("Channel {} write failed: {}", channel, e);
        continue;
      }
    }

    match read_response(&mut s).await {
      Ok(response) => {
        debug!("  Response: {:?}", response);
        return Ok((s, response));
      }
      Err(e) => {
        debug!("Channel {} read response failed: {}", channel, e);
        continue;
      }
    }
  }
  Err("Failed to connect on any RFCOMM channel".into())
}

const MAX_CONNECT_ATTEMPTS: u32 = 3;

async fn send(mac_address: Address, packets: &[Packet]) -> Result<Vec<Response>, Box<dyn Error>> {
  info!("Connecting to device with MAC address {}", mac_address);

  let mut result = None;
  for attempt in 1..=MAX_CONNECT_ATTEMPTS {
    if attempt > 1 {
      info!("Retrying connection (attempt {}/{})..", attempt, MAX_CONNECT_ATTEMPTS);
      tokio::time::sleep(Duration::from_secs(1)).await;
    }
    match try_connect(mac_address, packets).await {
      Ok(s) => {
        result = Some(s);
        break;
      }
      Err(e) => {
        debug!("Connection attempt {} failed: {}", attempt, e);
      }
    }
  }
  let (mut stream, first_response) = result.ok_or("Failed to connect on any RFCOMM channel")?;

  let mut responses = Vec::new();

  if !first_response.ack {
    return Err(format!(
      "Device NAK'd command 0x{:02x}",
      first_response.original_command
    ).into());
  }
  responses.push(first_response);

  for (index, packet) in packets.iter().enumerate().skip(1) {
    info!("Sending packet {}/{}..", index + 1, packets.len());
    let serialized = packet.serialize()?;
    debug!("  {}", hex::encode(&serialized));

    stream.write_all(&serialized).await?;
    info!(
      "  Wrote {} bytes",
      serialized.len()
    );

    let response = read_response(&mut stream).await?;
    debug!("  Response: {:?}", response);

    if !response.ack {
      return Err(format!(
        "Device NAK'd command 0x{:02x}",
        response.original_command
      ).into());
    }
    responses.push(response);

    tokio::time::sleep(Duration::from_millis(200)).await;
  }

  Ok(responses)
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
  send(mac_address, &[packet]).await?;
  Ok(())
}

pub async fn send_divoom_animation(
  mac_address: Address,
  reader: &mut (dyn std::io::Read + Send)
) -> Result<(), Box<dyn Error>> {
  let mut animation = Vec::new();
  reader.read_to_end(&mut animation)?;

  let packets = create_network_packets_from(&animation)?;
  send(mac_address, &packets).await?;
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
  send(mac_address, &[packet]).await?;
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
  send(mac_address, &[packet]).await?;
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
  send(mac_address, &[packet]).await?;
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
  send(mac_address, &packets).await?;
  Ok(())
}
