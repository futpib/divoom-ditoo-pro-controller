#[derive(Clone, Copy, Debug)]
pub enum Command {
  Alarm,
  Animation,
  SetDateTime,
  LightArrowSwitch
}

impl Command {
  pub fn value(&self) -> u8 {
    match *self {
      Command::SetDateTime => 0x18,
      Command::LightArrowSwitch => 0x23,
      Command::Alarm => 0x43,
      Command::Animation => 0x8b
    }
  }
}
