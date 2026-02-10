use std::error::Error;
use std::io::{BufWriter, Write};

use byteorder::{LittleEndian, ReadBytesExt, WriteBytesExt};

use super::command::Command;

#[derive(Debug)]
pub struct Packet {
  pub command: Command,
  pub payload: Vec<u8>
}

impl Packet {
  pub fn serialize(&self) -> Result<Vec<u8>, Box<dyn Error>> {
    let mut buffer = Vec::<u8>::new();
    let mut writer = BufWriter::new(&mut buffer);
    // start, always 1
    writer.write_u8(1)?;
    // length
    writer.write_u16::<LittleEndian>(self.payload.len() as u16 + 3)?;
    writer.write_u8(self.command.value())?;
    writer.write_all(&self.payload)?;
    writer.write_u16::<LittleEndian>(self.checksum()?)?;
    // end, always 2
    writer.write_u8(2)?;
    drop(writer);

    Ok(buffer)
  }

  fn checksum(&self) -> Result<u16, Box<dyn Error>> {
    let mut buffer = Vec::<u8>::new();
    let mut writer = BufWriter::new(&mut buffer);
    writer.write_u16::<LittleEndian>(self.payload.len() as u16 + 3)?;
    writer.write_u8(self.command.value())?;
    writer.write_all(&self.payload)?;
    drop(writer);

    Ok(Self::checksum_from_buffer(&buffer))
  }

  fn checksum_from_buffer(buffer: &[u8]) -> u16 {
    buffer.iter().fold(0u16, |acc, x| acc + *x as u16)
  }
}

const RESPONSE_COMMAND_BYTE: u8 = 0x04;
const ACK_BYTE: u8 = 0x55;

#[derive(Debug)]
pub struct Response {
  pub original_command: u8,
  pub ack: bool,
  pub data: Vec<u8>,
}

impl Response {
  pub fn deserialize(bytes: &[u8]) -> Result<Self, Box<dyn Error>> {
    if bytes.len() < 7 {
      return Err("Response too short".into());
    }

    if bytes[0] != 0x01 {
      return Err(format!("Invalid start byte: 0x{:02x}", bytes[0]).into());
    }

    if bytes[bytes.len() - 1] != 0x02 {
      return Err(format!("Invalid end byte: 0x{:02x}", bytes[bytes.len() - 1]).into());
    }

    let length = (&bytes[1..3]).read_u16::<LittleEndian>()? as usize;

    if bytes.len() != length + 4 {
      return Err(format!(
        "Length mismatch: field says {} but packet is {} bytes",
        length, bytes.len() - 4
      ).into());
    }

    if bytes[3] != RESPONSE_COMMAND_BYTE {
      return Err(format!("Not a response packet: command byte 0x{:02x}", bytes[3]).into());
    }

    let checksum_range = &bytes[1..bytes.len() - 3];
    let expected_checksum = Packet::checksum_from_buffer(checksum_range);
    let actual_checksum = (&bytes[bytes.len() - 3..bytes.len() - 1]).read_u16::<LittleEndian>()?;

    if actual_checksum != expected_checksum {
      return Err(format!(
        "Checksum mismatch: expected 0x{:04x}, got 0x{:04x}",
        expected_checksum, actual_checksum
      ).into());
    }

    let original_command = bytes[4];
    let ack = bytes[5] == ACK_BYTE;
    let data = bytes[6..bytes.len() - 3].to_vec();

    Ok(Response {
      original_command,
      ack,
      data,
    })
  }
}

#[cfg(test)]
mod tests {
  use super::*;

  fn build_response(cmd: u8, ack: u8, data: &[u8]) -> Vec<u8> {
    let mut payload = vec![RESPONSE_COMMAND_BYTE, cmd, ack];
    payload.extend_from_slice(data);

    let length = (payload.len() + 2) as u16;
    let mut frame = vec![0x01];
    frame.extend_from_slice(&length.to_le_bytes());
    frame.extend_from_slice(&payload);

    let checksum: u16 = frame[1..frame.len()].iter().fold(0u16, |acc, &x| acc + x as u16);
    frame.extend_from_slice(&checksum.to_le_bytes());
    frame.push(0x02);
    frame
  }

  #[test]
  fn test_deserialize_ack() {
    let bytes = build_response(0x74, ACK_BYTE, &[]);
    let response = Response::deserialize(&bytes).unwrap();
    assert_eq!(response.original_command, 0x74);
    assert!(response.ack);
    assert!(response.data.is_empty());
  }

  #[test]
  fn test_deserialize_nak() {
    let bytes = build_response(0x74, 0x00, &[]);
    let response = Response::deserialize(&bytes).unwrap();
    assert_eq!(response.original_command, 0x74);
    assert!(!response.ack);
    assert!(response.data.is_empty());
  }

  #[test]
  fn test_deserialize_with_data() {
    let bytes = build_response(0x09, ACK_BYTE, &[0x0A]);
    let response = Response::deserialize(&bytes).unwrap();
    assert_eq!(response.original_command, 0x09);
    assert!(response.ack);
    assert_eq!(response.data, vec![0x0A]);
  }

  #[test]
  fn test_deserialize_bad_checksum() {
    let mut bytes = build_response(0x74, ACK_BYTE, &[]);
    let crc_pos = bytes.len() - 3;
    bytes[crc_pos] = bytes[crc_pos].wrapping_add(1);
    assert!(Response::deserialize(&bytes).is_err());
  }

  #[test]
  fn test_deserialize_too_short() {
    assert!(Response::deserialize(&[0x01, 0x02]).is_err());
  }

  #[test]
  fn test_deserialize_bad_start_byte() {
    let mut bytes = build_response(0x74, ACK_BYTE, &[]);
    bytes[0] = 0xFF;
    assert!(Response::deserialize(&bytes).is_err());
  }

  #[test]
  fn test_deserialize_bad_end_byte() {
    let mut bytes = build_response(0x74, ACK_BYTE, &[]);
    let last = bytes.len() - 1;
    bytes[last] = 0xFF;
    assert!(Response::deserialize(&bytes).is_err());
  }
}
