//! Offline MVsilicon B1 MVA inspection. See docs/firmware-format.md for evidence.
use serde_json::{json, Value};
use sha2::{Digest, Sha256};
use std::{error::Error, fs, path::Path};

const MAGIC: &[u8] = b"MV\xb1X";
const KNOWN_SHA: &str = "fc16341c005b11d0ac476917dc2fd98c9bc7481b92a183e64bfe209801566544";

fn crc16(bytes: &[u8], mut crc: u16) -> u16 {
  for byte in bytes {
    crc ^= u16::from(*byte) << 8;
    for _ in 0..8 {
      crc = if crc & 0x8000 != 0 {
        (crc << 1) ^ 0x1021
      } else {
        crc << 1
      };
    }
  }
  crc
}

fn le32(bytes: &[u8]) -> u32 {
  u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]])
}

fn code_crc(bytes: &[u8], end: usize) -> u16 {
  [
    (0, 0xa4),
    (0xa8, 0xbc),
    (0xc0, 0xcc),
    (0xd4, 0xe4),
    (0xec, end),
  ]
  .into_iter()
  .fold(0, |crc, (start, end)| crc16(&bytes[start..end], crc))
}

struct Record<'a> {
  kind: u8,
  offset: usize,
  payload: &'a [u8],
}

fn parse(bytes: &[u8]) -> Result<Vec<Record<'_>>, Box<dyn Error>> {
  if bytes.len() < 9 || &bytes[..4] != MAGIC {
    return Err("Not an MVsilicon B1 MVA package".into());
  }
  let end = bytes.len() - 4;
  if le32(&bytes[end..]) != u32::from(crc16(&bytes[..end], 0)) {
    return Err("MVA package CRC-16 mismatch (or nonzero trailer padding)".into());
  }
  let mut records = Vec::new();
  let mut offset = 5;
  for _ in 0..bytes[4] {
    if end.saturating_sub(offset) < 5 {
      return Err("Truncated MVA record header".into());
    }
    let size = le32(&bytes[offset + 1..offset + 5]) as usize;
    if size > end - offset - 5 {
      return Err("MVA record exceeds package boundary".into());
    }
    let kind = bytes[offset];
    if matches!(kind, 2..=5) && size < 4 {
      return Err("MVA addressed record is missing its address".into());
    }
    records.push(Record {
      kind,
      offset,
      payload: &bytes[offset + 5..offset + 5 + size],
    });
    offset += 5 + size;
  }
  if offset != end {
    return Err("MVA record count leaves unparsed bytes".into());
  }
  Ok(records)
}

/// Validate and describe an MVA package, optionally extracting to a new directory.
/// This function never connects to a device and does not authorize flashing.
pub fn decode(file: &Path, output: Option<&Path>) -> Result<Value, Box<dyn Error>> {
  let bytes = fs::read(file)?;
  let records = parse(&bytes)?;
  let sha = hex::encode(Sha256::digest(&bytes));
  let mut files: Vec<(String, Vec<u8>)> = Vec::new();
  let mut descriptions = Vec::new();
  for (index, record) in records.iter().enumerate() {
    let name = format!("record-{}-type-{}.bin", index + 1, record.kind);
    files.push((name.clone(), record.payload.to_vec()));
    let mut description = json!({"type":record.kind,"record_offset":record.offset,
      "payload_offset":record.offset+5,"payload_bytes":record.payload.len(),
      "payload_sha256":hex::encode(Sha256::digest(record.payload)),"file":name});
    if matches!(record.kind, 2..=5) {
      description["address"] = json!(le32(record.payload));
      description["data_offset"] = json!(record.offset + 9);
      description["data_bytes"] = json!(record.payload.len() - 4);
    }
    if record.kind == 1 {
      description["control_hex"] = json!(hex::encode(record.payload));
    }
    descriptions.push(description);
  }
  let mut report = json!({"format":"MVsilicon B1 MVA","bytes":bytes.len(),"sha256":sha,
    "package_crc16":format!("{:04x}",crc16(&bytes[..bytes.len()-4],0)),
    "package_crc_valid":true,"record_count":records.len(),"records":descriptions,
    "known_ditoo_306007":sha==KNOWN_SHA});
  if sha == KNOWN_SHA {
    let record = records
      .iter()
      .find(|r| r.kind == 2)
      .ok_or("Missing code record")?;
    let code = &record.payload[4..];
    let boot_len = (le32(&code[0xe0..]) & 0xffffff) as usize;
    let boot_crc = code_crc(code, boot_len);
    let full_crc = code_crc(code, code.len());
    if u32::from(boot_crc) != le32(&code[0xbc..]) || u32::from(full_crc) != le32(&code[0xcc..]) {
      return Err("Known image internal code CRC mismatch".into());
    }
    files.push((
      "flash-driver-encoded.bin".into(),
      records[1].payload[4..].to_vec(),
    ));
    files.push(("code.bin".into(), code.to_vec()));
    files.push(("bootloader.bin".into(), code[..0x10000].to_vec()));
    files.push(("application.bin".into(), code[0x10000..].to_vec()));
    report["code"] = json!({"architecture":"Andes NDS32","instruction_endian":"big",
      "data_endian":"little","file_offset":record.offset+9,"address":0,"bytes":code.len(),
      "bootloader_region_bytes":65536,"bootloader_used_bytes":boot_len,
      "application_address":65536,"application_file_offset":record.offset+9+65536,
      "application_bytes":code.len()-65536,"bootloader_crc16":format!("{boot_crc:04x}"),
      "code_crc16":format!("{full_crc:04x}"),"internal_crcs_valid":true,
      "encryption_flag_byte":code[0xff],
      "driver_matches_bootloader_copy":records[1].payload[4..]==code[0x934c..0x993c]});
  }
  if let Some(dir) = output {
    // Require a new directory so an extraction cannot replace existing analysis.
    fs::create_dir(dir)?;
    for (name, data) in files {
      fs::write(dir.join(name), data)?;
    }
    let mut strings = String::from("file_offset_hex\ttext\n");
    let mut start = 0;
    for end in 0..=bytes.len() {
      if end == bytes.len() || !(0x20..=0x7e).contains(&bytes[end]) {
        if end - start >= 8 {
          strings.push_str(&format!(
            "{start:08x}\t{}\n",
            String::from_utf8_lossy(&bytes[start..end])
          ));
        }
        start = end + 1;
      }
    }
    fs::write(dir.join("strings.tsv"), strings)?;
    fs::write(
      dir.join("manifest.json"),
      serde_json::to_string_pretty(&report)?,
    )?;
  }
  Ok(report)
}

#[cfg(test)]
mod tests {
  use super::*;

  fn sealed(mut bytes: Vec<u8>) -> Vec<u8> {
    bytes.extend(u32::from(crc16(&bytes, 0)).to_le_bytes());
    bytes
  }

  #[test]
  fn crc_check_vector_and_vendor_records() -> Result<(), Box<dyn Error>> {
    assert_eq!(crc16(b"123456789", 0), 0x31c3);
    let bytes = include_bytes!("../firmware/306007.MVA");
    let records = parse(bytes)?;
    assert_eq!(records.len(), 3);
    assert_eq!(records[0].payload, [0x35, 0xba, 0x69]);
    assert_eq!(records[1].offset, 0xd);
    assert_eq!(records[2].offset, 0x606);
    let code = &records[2].payload[4..];
    assert_eq!(code_crc(code, 0x9e80), 0x5f08);
    assert_eq!(code_crc(code, code.len()), 0x97e8);
    assert_eq!(crc16(&bytes[..bytes.len() - 4], 0), 0xf9b6);
    Ok(())
  }

  #[test]
  fn malformed_packages_are_rejected_even_with_correct_crc() {
    for body in [
      b"MV\xb1X\x01".to_vec(),
      b"MV\xb1X\x01\x02\xff\xff\xff\xff".to_vec(),
      b"MV\xb1X\x01\x02\x00\x00\x00\x00".to_vec(),
      b"MV\xb1X\x00\x00".to_vec(),
    ] {
      assert!(parse(&sealed(body)).is_err());
    }
    assert!(parse(b"MV").is_err());
    let mut valid = sealed(b"MV\xb1X\x01\x01\x01\x00\x00\x00\x35".to_vec());
    assert!(parse(&valid).is_ok());
    valid[10] ^= 1;
    assert!(parse(&valid).is_err());
  }
}
