#[derive(Clone, Copy, Debug)]
pub enum Command {
  Alarm,
  Animation,
  SetDateTime,
  LightArrowSwitch,
  SetBrightness,
  DrawingEncodeMoviePlay,
  DrawingCtrlMoviePlay,
  LedUpdateFontInfo,
  LedWordCmd
}

impl Command {
  pub fn value(&self) -> u8 {
    match *self {
      Command::SetDateTime => 0x18,
      Command::LightArrowSwitch => 0x23,
      Command::Alarm => 0x43,
      Command::DrawingEncodeMoviePlay => 0x6c,
      Command::DrawingCtrlMoviePlay => 0x6e,
      Command::SetBrightness => 0x74,
      Command::LedUpdateFontInfo => 0x7c,
      Command::LedWordCmd => 0x86,
      Command::Animation => 0x8b
    }
  }
}
