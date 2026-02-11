use super::command::Command;
use super::packet::Packet;

pub fn build_packet(ext_cmd_type: u8, params: &[u8]) -> Packet {
  let mut payload = vec![ext_cmd_type];
  payload.extend_from_slice(params);
  Packet {
    command: Command::ExtendedCommand,
    payload,
  }
}

pub const SET_USER_DEFINE_TIME: u8 = 0x14;
pub const GET_USER_DEFINE_TIME: u8 = 0x15;
pub const SET_LANGUAGE: u8 = 0x26;

pub fn language_index(lang: &str) -> Option<u8> {
  match lang {
    "en" => Some(0),
    "zh-hans" => Some(1),
    "zh-hant" => Some(2),
    "ja" => Some(3),
    "th" => Some(4),
    "fr" => Some(5),
    "it" => Some(6),
    "he" => Some(7),
    "es" => Some(8),
    "de" => Some(9),
    "ru" => Some(10),
    "pt" => Some(11),
    "ko" => Some(12),
    "nl" => Some(13),
    "uk" => Some(14),
    "ms" => Some(15),
    _ => None,
  }
}

pub const SUPPORTED_LANGUAGES: &[&str] = &[
  "en", "zh-hans", "zh-hant", "ja", "th", "fr", "it", "he",
  "es", "de", "ru", "pt", "ko", "nl", "uk", "ms",
];
