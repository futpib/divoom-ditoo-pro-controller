use clap::{Subcommand, ValueEnum};
use divoom_ditoo_pro_controller::control::{self, Request, Session};
use std::{
  error::Error,
  path::PathBuf,
  time::{Duration, Instant},
};

#[derive(Debug, Subcommand)]
pub enum RawCommand {
  /// List Android protocol symbols; catalogue presence does not prove device support
  List {
    #[arg(long)]
    filter: Option<String>,
  },
  /// Send an opcode/symbol and payload, optionally wait for a command response
  Send {
    command: String,
    #[arg(long, default_value = "")]
    data: String,
    #[arg(long)]
    query: bool,
    #[arg(long)]
    expect: Option<String>,
    #[arg(long, default_value = "")]
    prefix: String,
    #[arg(long, default_value_t = 0)]
    listen_ms: u64,
    #[arg(long)]
    dry_run: bool,
  },
  /// Execute a JSON request array on one connection, stopping at the first error
  Run {
    file: PathBuf,
    #[arg(long)]
    dry_run: bool,
  },
  /// Print incoming framed responses/events without sending application commands
  Monitor {
    #[arg(long,default_value_t=10,value_parser=clap::value_parser!(u64).range(1..=60))]
    seconds: u64,
  },
}

#[derive(Debug, Subcommand)]
pub enum DeviceCommand {
  /// Read mode and brightness, retaining the full raw response
  Status {
    #[arg(long)]
    dry_run: bool,
  },
  /// Read firmware version list
  Firmware {
    #[arg(long)]
    dry_run: bool,
  },
  /// Read alarm slots (raw response)
  Alarms {
    #[arg(long)]
    dry_run: bool,
  },
  /// Read music playback status
  PlaybackStatus {
    #[arg(long)]
    dry_run: bool,
  },
  /// Read a tool: 0 stopwatch, 1 scoreboard, 2 noise meter, 3 countdown
  ToolStatus {
    #[arg(long)]
    dry_run: bool,
    #[arg(value_parser=clap::value_parser!(u8).range(0..=3))]
    tool: u8,
  },
  /// Set scoreboard scores using the Android tool protocol
  Scoreboard {
    #[arg(long)]
    dry_run: bool,
    #[arg(long,action=clap::ArgAction::Set,default_value_t=true)]
    enabled: bool,
    #[arg(value_parser=clap::value_parser!(u16).range(0..=999))]
    red: u16,
    #[arg(value_parser=clap::value_parser!(u16).range(0..=999))]
    blue: u16,
  },
  /// Configure countdown, with 0..59 minutes and seconds
  Countdown {
    #[arg(long)]
    dry_run: bool,
    #[arg(long,action=clap::ArgAction::Set,default_value_t=true)]
    enabled: bool,
    #[arg(value_parser=clap::value_parser!(u8).range(0..=59))]
    minutes: u8,
    #[arg(value_parser=clap::value_parser!(u8).range(0..=59))]
    seconds: u8,
  },
  /// Stopwatch control byte as used by the app (see protocol documentation)
  Stopwatch {
    #[arg(long)]
    dry_run: bool,
    action: u8,
  },
  /// Noise-meter control byte as used by the app
  NoiseMeter {
    #[arg(long)]
    dry_run: bool,
    action: u8,
  },
  /// Enter a built-in game; IDs follow firmware, not app labels
  Game {
    #[arg(long)]
    dry_run: bool,
    id: u8,
    #[arg(long)]
    exit: bool,
  },
  /// Send a virtual game key press or release; does not bind physical keys
  GameKey {
    #[arg(long)]
    dry_run: bool,
    code: u8,
    #[arg(long)]
    release: bool,
  },
  /// Send signed Celsius temperature and firmware weather-condition code
  Weather {
    #[arg(long)]
    dry_run: bool,
    #[arg(allow_hyphen_values = true)]
    celsius: i8,
    condition: u8,
  },
  /// Read/set a firmware setting; boolean values are 0 or 1
  Setting {
    #[arg(long)]
    dry_run: bool,
    #[arg(value_enum)]
    name: Setting,
    value: Option<u16>,
  },
  /// Read TF-card track/playback information
  SdStatus {
    #[arg(long)]
    dry_run: bool,
  },
  /// Select a TF-card track by its firmware ID
  SdTrack {
    #[arg(long)]
    dry_run: bool,
    id: u16,
  },
  /// Set TF-card playback position in device units
  SdSeek {
    #[arg(long)]
    dry_run: bool,
    position: u16,
  },
  /// Set TF-card repeat/play mode by firmware mode ID
  SdPlayMode {
    #[arg(long)]
    dry_run: bool,
    mode: u8,
  },
  /// Previous/next track protocol selector, 0 or 1
  Track {
    #[arg(long)]
    dry_run: bool,
    #[arg(value_parser=clap::value_parser!(u8).range(0..=1))]
    direction: u8,
  },
}

#[derive(Clone, Copy, Debug, ValueEnum)]
pub enum Setting {
  Hour24,
  Fahrenheit,
  SaveVolume,
  AutoConnect,
  IdlePowerOff,
  StartupChannel,
  SleepMode,
}

impl DeviceCommand {
  pub fn dry_run(&self) -> bool {
    match self {
      Self::Status { dry_run, .. } => *dry_run,
      Self::Firmware { dry_run, .. } => *dry_run,
      Self::Alarms { dry_run, .. } => *dry_run,
      Self::PlaybackStatus { dry_run, .. } => *dry_run,
      Self::ToolStatus { dry_run, .. } => *dry_run,
      Self::Scoreboard { dry_run, .. } => *dry_run,
      Self::Countdown { dry_run, .. } => *dry_run,
      Self::Stopwatch { dry_run, .. } => *dry_run,
      Self::NoiseMeter { dry_run, .. } => *dry_run,
      Self::Game { dry_run, .. } => *dry_run,
      Self::GameKey { dry_run, .. } => *dry_run,
      Self::Weather { dry_run, .. } => *dry_run,
      Self::Setting { dry_run, .. } => *dry_run,
      Self::SdStatus { dry_run, .. } => *dry_run,
      Self::SdTrack { dry_run, .. } => *dry_run,
      Self::SdSeek { dry_run, .. } => *dry_run,
      Self::SdPlayMode { dry_run, .. } => *dry_run,
      Self::Track { dry_run, .. } => *dry_run,
    }
  }

  pub fn request(&self) -> Result<Request, Box<dyn Error>> {
    use DeviceCommand::*;
    let request = match *self {
      Status { .. } => Request::bytes(0x46, &[], true),
      Firmware { .. } => Request::bytes(0x37, &[0], true),
      Alarms { .. } => Request::bytes(0x42, &[], true),
      PlaybackStatus { .. } => Request::bytes(0x0b, &[], true),
      ToolStatus { tool, .. } => Request::bytes(0x71, &[tool], true),
      Scoreboard {
        enabled, red, blue, ..
      } => {
        let mut p = vec![1, enabled as u8];
        p.extend(red.to_le_bytes());
        p.extend(blue.to_le_bytes());
        Request::bytes(0x72, &p, false)
      }
      Countdown {
        enabled,
        minutes,
        seconds,
        ..
      } => Request::bytes(0x72, &[3, enabled as u8, minutes, seconds], false),
      Stopwatch { action, .. } => Request::bytes(0x72, &[0, action], false),
      NoiseMeter { action, .. } => Request::bytes(0x72, &[2, action], false),
      Game { id, exit, .. } => Request::bytes(0xa0, &[!exit as u8, id], false),
      GameKey { code, release, .. } => {
        Request::bytes(if release { 0x21 } else { 0x17 }, &[code], false)
      }
      Weather {
        celsius, condition, ..
      } => Request::bytes(0x5f, &[celsius as u8, condition], false),
      SdStatus { .. } => Request::bytes(0xb4, &[], true),
      SdTrack { id, .. } => Request::bytes(0x11, &id.to_le_bytes(), false),
      SdSeek { position, .. } => Request::bytes(0xb8, &position.to_le_bytes(), false),
      SdPlayMode { mode, .. } => Request::bytes(0xb9, &[mode], false),
      Track { direction, .. } => Request::bytes(0x12, &[direction], false),
      Setting { name, value, .. } => {
        if matches!(
          name,
          self::Setting::Hour24
            | self::Setting::Fahrenheit
            | self::Setting::SaveVolume
            | self::Setting::AutoConnect
        ) && value.is_some_and(|v| v > 1)
        {
          return Err("Boolean setting requires 0 or 1".into());
        }
        let mut req = match name {
          self::Setting::IdlePowerOff => Request::bytes(
            if value.is_some() { 0xab } else { 0xac },
            &value.map(|v| v.to_le_bytes().to_vec()).unwrap_or_default(),
            value.is_none(),
          ),
          self::Setting::StartupChannel => {
            let p = if let Some(v) = value {
              vec![1, u8::try_from(v)?]
            } else {
              vec![0]
            };
            Request::bytes(0x8a, &p, value.is_none())
          }
          self::Setting::SaveVolume | self::Setting::AutoConnect => {
            let ext = if matches!(name, self::Setting::SaveVolume) {
              0x19
            } else {
              0x1a
            };
            let mut r = Request::bytes(
              0xbd,
              &[ext, value.map(u8::try_from).transpose()?.unwrap_or(255)],
              value.is_none(),
            );
            r.response_prefix_hex = format!("{ext:02x}");
            if value.is_some() {
              r.response_prefix_hex.clear();
            }
            r
          }
          _ => {
            let code = match name {
              self::Setting::Hour24 => 0x2d,
              self::Setting::Fahrenheit => 0x2b,
              _ => 0x79,
            };
            Request::bytes(
              code,
              &[value.map(u8::try_from).transpose()?.unwrap_or(255)],
              value.is_none(),
            )
          }
        };
        req.delay_ms = 0;
        req
      }
    };
    Ok(request)
  }
}

pub async fn run_requests(
  device: Option<String>,
  requests: Vec<Request>,
  dry_run: bool,
) -> Result<(), Box<dyn Error>> {
  let prepared = requests
    .iter()
    .map(Request::prepare)
    .collect::<Result<Vec<_>, _>>()?;
  if dry_run {
    for request in prepared {
      println!("{}", request.describe());
    }
    return Ok(());
  }
  if prepared.is_empty() {
    return Ok(());
  }
  let address = crate::resolve_device(device).await?;
  let mut session = Session::connect(address).await?;
  let result = async {
    for request in prepared {
      println!("{}", session.execute(&request).await?);
    }
    Ok::<(), Box<dyn Error>>(())
  }
  .await;
  let cleanup = session.disconnect().await;
  result?;
  cleanup
}

pub async fn run(device: Option<String>, action: RawCommand) -> Result<(), Box<dyn Error>> {
  match action {
    RawCommand::List { filter } => {
      let filter = filter.unwrap_or_default().to_lowercase();
      let items: Vec<_> = control::catalogue()?
        .into_iter()
        .filter(|c| c.name.to_lowercase().contains(&filter))
        .collect();
      println!("{}", serde_json::to_string_pretty(&items)?);
      Ok(())
    }
    RawCommand::Send {
      command,
      data,
      query,
      expect,
      prefix,
      listen_ms,
      dry_run,
    } => {
      let response = expect.or_else(|| query.then(|| command.clone()));
      run_requests(
        device,
        vec![Request {
          command,
          payload_hex: data,
          response,
          response_prefix_hex: prefix,
          delay_ms: 0,
          listen_ms,
        }],
        dry_run,
      )
      .await
    }
    RawCommand::Run { file, dry_run } => {
      run_requests(
        device,
        serde_json::from_slice(&std::fs::read(file)?)?,
        dry_run,
      )
      .await
    }
    RawCommand::Monitor { seconds } => {
      let mut session = Session::connect(crate::resolve_device(device).await?).await?;
      let deadline = Instant::now() + Duration::from_secs(seconds);
      let result = async {
        while let Some(remaining) = deadline.checked_duration_since(Instant::now()) {
          if let Some(reply) = session.receive(remaining).await? {
            println!("{reply}");
          } else {
            break;
          }
        }
        Ok::<(), Box<dyn Error>>(())
      }
      .await;
      let cleanup = session.disconnect().await;
      result?;
      cleanup
    }
  }
}

#[cfg(test)]
mod tests {
  use super::*;
  #[test]
  fn android_tool_and_signed_temperature_payloads() -> Result<(), Box<dyn Error>> {
    assert_eq!(
      DeviceCommand::Scoreboard {
        dry_run: false,
        enabled: true,
        red: 258,
        blue: 999
      }
      .request()?
      .payload_hex,
      "01010201e703"
    );
    assert_eq!(
      DeviceCommand::Countdown {
        dry_run: false,
        enabled: true,
        minutes: 12,
        seconds: 34
      }
      .request()?
      .payload_hex,
      "03010c22"
    );
    assert_eq!(
      DeviceCommand::Weather {
        dry_run: false,
        celsius: -5,
        condition: 8
      }
      .request()?
      .payload_hex,
      "fb08"
    );
    assert!(DeviceCommand::Setting {
      dry_run: false,
      name: Setting::Hour24,
      value: Some(2)
    }
    .request()
    .is_err());
    Ok(())
  }
}
