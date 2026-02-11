#[derive(Clone, Copy, Debug)]
pub enum Command {
  Alarm,
  Animation,
  SetVolume,
  GetVolume,
  SetPlayStatus,
  SetDateTime,
  LightArrowSwitch,
  SetBoxMode,
  SetBrightness,
  DrawingEncodeMoviePlay,
  DrawingCtrlMoviePlay,
  LedUpdateFontInfo,
  LedWordCmd,
  ExtendedCommand
}

impl Command {
  pub fn value(&self) -> u8 {
    match *self {
      Command::SetVolume => 0x08,
      Command::GetVolume => 0x09,
      Command::SetPlayStatus => 0x0a,
      Command::SetDateTime => 0x18,
      Command::LightArrowSwitch => 0x23,
      Command::Alarm => 0x43,
      Command::SetBoxMode => 0x45,
      Command::DrawingEncodeMoviePlay => 0x6c,
      Command::DrawingCtrlMoviePlay => 0x6e,
      Command::SetBrightness => 0x74,
      Command::LedUpdateFontInfo => 0x7c,
      Command::LedWordCmd => 0x86,
      Command::Animation => 0x8b,
      Command::ExtendedCommand => 0xbd
    }
  }
}
