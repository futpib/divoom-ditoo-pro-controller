use std::error::Error;
use std::fs::File;
use std::io::{BufReader, BufWriter};
use std::path::PathBuf;
use bluer::Address;
use chrono::NaiveDateTime;
use clap::{Parser, Subcommand};
use env_logger::{Builder, Env};
use log::{debug, info};

use divoom_ditoo_pro_controller::divoom_file_format::animation::Animation;
use divoom_ditoo_pro_controller::divoom_file_format::frame::bits_per_pixel;
use divoom_ditoo_pro_controller::{
  find_paired_ditoo_pro_devices, scan_devices, list_paired_devices, send_alarm,
  send_divoom_animation, send_get_clock_face, send_get_volume, send_image,
  send_keyboard_backlight, send_scrolling_text, send_set_brightness,
  send_set_box_mode, send_set_clock_face, send_set_datetime, send_set_language,
  send_set_play_status, send_set_volume, send_static_text
};
use divoom_ditoo_pro_controller::protocol::extended_command;
use divoom_ditoo_pro_controller::protocol::scrolling_text::{HAlign, VAlign};

/// CLI tool to send bluetooth commands to a Divoom Ditoo Pro
#[derive(Parser, Debug)]
#[command(author, version, about, long_about = None)]
pub struct Args {
  /// Device MAC address (auto-detected if only one paired Ditoo Pro exists)
  #[arg(long)]
  device: Option<String>,

  #[command(subcommand)]
  command: Command
}

#[derive(Subcommand, Debug)]
enum Command {
  /// Scan for available bluetooth devices
  Scan,

  /// List paired Divoom Ditoo Pro devices
  Devices,

  /// Convert between Divoom and GIF formats
  Convert {
    #[command(subcommand)]
    convert: ConvertCommand
  },

  /// Show detailed information about an image in Divoom file format
  DebugImage { filename: String },

  /// Send a static image to the display
  Image { filename: String },

  /// Send an animation to the display
  Animation { filename: String },

  /// Display scrolling text (supports \n for multiline)
  ScrollingText {
    text: String,
    #[arg(long)]
    font: Option<String>,
    #[arg(long, default_value_t = 16.0)]
    font_size: f32,
    /// Foreground color (e.g. red, #FF0000, rgb(255,0,0))
    #[arg(long, default_value = "white")]
    color: String,
    /// Background color (e.g. green, #001100, rgb(0,17,0))
    #[arg(long, default_value = "black")]
    bg_color: String,
    /// Horizontal text alignment
    #[arg(long, default_value = "center", value_enum)]
    align: HAlign,
    /// Vertical text alignment
    #[arg(long, default_value = "center", value_enum)]
    valign: VAlign,
  },

  /// Display static text on the 16x16 display (supports \n for multiline)
  StaticText {
    text: String,
    #[arg(long)]
    font: Option<String>,
    #[arg(long, default_value_t = 16.0)]
    font_size: f32,
    /// Foreground color (e.g. red, #FF0000, rgb(255,0,0))
    #[arg(long, default_value = "white")]
    color: String,
    /// Background color (e.g. green, #001100, rgb(0,17,0))
    #[arg(long, default_value = "black")]
    bg_color: String,
    /// Horizontal text alignment
    #[arg(long, default_value = "center", value_enum)]
    align: HAlign,
    /// Vertical text alignment
    #[arg(long, default_value = "center", value_enum)]
    valign: VAlign,
  },

  /// Set screen brightness (0-100)
  Brightness {
    #[arg(value_parser = clap::value_parser!(u8).range(0..=100))]
    level: u8
  },

  /// Get or set the clock face
  Clock {
    #[command(subcommand)]
    action: ClockCommand
  },

  /// Set the display mode
  Mode {
    #[command(subcommand)]
    mode: BoxMode
  },

  /// Get or set the volume
  Volume {
    #[command(subcommand)]
    action: VolumeCommand
  },

  /// Start playback
  Play,

  /// Pause playback
  Pause,

  /// Set the device date and time
  SetDatetime {
    /// Date and time to set (e.g. "2025-01-15T12:30:00"). Defaults to current local time.
    datetime: Option<NaiveDateTime>
  },

  /// Set the device language
  Language {
    /// Language code (en, zh-hans, zh-hant, ja, th, fr, it, he, es, de, ru, pt, ko, nl, uk, ms)
    language: String
  },

  /// Control the keyboard backlight
  KeyboardBacklight {
    #[command(subcommand)]
    action: KeyboardBacklightAction
  },

  /// Toggle the alarm
  Alarm {
    #[arg(required = true, number_of_values = 1, value_parser = clap::builder::BoolishValueParser::new())]
    enable: bool
  },
}

#[derive(Subcommand, Debug)]
enum VolumeCommand {
  /// Get the current volume
  Get,
  /// Set the volume (0-16)
  Set {
    #[arg(value_parser = clap::value_parser!(u8).range(0..=16))]
    volume: u8
  },
}

#[derive(Subcommand, Debug)]
enum ClockCommand {
  /// Get the current clock face ID
  Get,
  /// Set the clock face by ID
  Set {
    /// Clock face ID
    clock_id: u16
  },
}

#[derive(Subcommand, Debug)]
enum KeyboardBacklightAction {
  Next,
  Prev,
  Toggle
}

#[derive(Subcommand, Debug)]
enum BoxMode {
  /// Light mode: color, clock overlay, temperature, sound-reactive, etc.
  Light {
    /// Sub-mode: 0=clock, 1=temp, 2=color, 3=special, 4=sound, 5=sound-user, 6=music
    #[arg(value_parser = clap::value_parser!(u8).range(0..=6))]
    sub_mode: u8,
    /// Color (e.g. red, #FF0000)
    #[arg(long, default_value = "red")]
    color: String,
    /// Brightness (0-100)
    #[arg(long, default_value_t = 100)]
    brightness: u8,
    /// On/off
    #[arg(long, default_value_t = true)]
    on: bool,
  },
  /// Trending/hot animations
  Hot,
  /// Special effects
  Special {
    /// Effect sub-type index
    sub_type: u8
  },
  /// Music visualizer
  Music {
    /// Visualizer sub-type index
    sub_type: u8
  },
  /// Raw payload (for experimentation)
  Raw {
    /// Payload bytes as hex (e.g. "06 00 00")
    payload_hex: Vec<String>
  },
}

#[derive(Subcommand, Debug)]
enum ConvertCommand {
  ToGif {
    input_filename: String,
    output_filename: String
  },
  ToDivoom16 {
    input_filename: String,
    output_filename: String
  }
}

fn parse_color(s: &str) -> Result<[u8; 3], Box<dyn Error>> {
  let c = csscolorparser::parse(s).map_err(|e| format!("Invalid color '{}': {}", s, e))?;
  let [r, g, b, _] = c.to_rgba8();
  Ok([r, g, b])
}

fn resolve_font(font: Option<&str>) -> Result<PathBuf, Box<dyn Error>> {
  match font {
    Some(name_or_path) => {
      let path = PathBuf::from(name_or_path);
      if path.exists() {
        return Ok(path);
      }
      let fc = fontconfig::Fontconfig::new()
        .ok_or("Failed to initialize fontconfig")?;
      let font = fc.find(name_or_path, None)
        .ok_or_else(|| format!("Font {:?} not found", name_or_path))?;
      Ok(font.path.clone())
    }
    None => {
      let fc = fontconfig::Fontconfig::new()
        .ok_or("Failed to initialize fontconfig")?;
      let font = fc.find("monospace", None)
        .ok_or("No monospace font found; use --font")?;
      Ok(font.path.clone())
    }
  }
}

async fn resolve_device(device: Option<String>) -> Result<Address, Box<dyn Error>> {
  match device {
    Some(addr) => addr
      .parse()
      .map_err(|_| format!("Invalid MAC address: '{}'", addr).into()),
    None => {
      let devices = find_paired_ditoo_pro_devices().await?;
      match devices.len() {
        0 => Err("No paired Ditoo Pro devices found. Specify a MAC address with --device.".into()),
        1 => Ok(devices[0]),
        _ => {
          let list = devices
            .iter()
            .map(|d| d.to_string())
            .collect::<Vec<_>>()
            .join(", ");
          Err(format!(
            "Multiple paired Ditoo Pro devices found: {}. Specify a MAC address with --device.",
            list
          ).into())
        }
      }
    }
  }
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
  Builder::from_env(Env::default().default_filter_or("debug")).init();

  let args = Args::parse();

  match args.command {
    Command::Scan => scan_devices().await?,
    Command::Devices => list_paired_devices().await?,
    Command::Convert { convert } => match convert {
      ConvertCommand::ToGif {
        input_filename,
        output_filename
      } => {
        let animation = Animation::from_16x16(&mut BufReader::new(File::open(input_filename)?))?;
        animation.save_to_gif(&mut BufWriter::new(File::create(output_filename)?))?;
      }
      ConvertCommand::ToDivoom16 {
        input_filename,
        output_filename
      } => {
        let animation = if input_filename.ends_with(".gif") || input_filename.ends_with(".GIF") {
          Animation::from_gif(&mut BufReader::new(File::open(&input_filename)?))?
        } else {
          Animation::from_image(image::open(&input_filename)?)?
        };
        animation.save_to_divoom_format(&mut BufWriter::new(File::create(output_filename)?))?;
      }
    },
    Command::DebugImage { filename } => {
      let animation = Animation::from_16x16(&mut BufReader::new(File::open(filename)?))?;
      animation
        .frames
        .iter()
        .enumerate()
        .for_each(|(index, frame)| {
          let bits_per_pixel = bits_per_pixel(frame.palette.len() as u32);
          let pixel_data_in_bits = 16 * 16 * bits_per_pixel as u32;
          let pixel_data_in_bytes = pixel_data_in_bits.div_ceil(8);

          debug!("Frame #{}", index);
          debug!(
            "  Pixel data size: {} bits = {} bytes",
            pixel_data_in_bits, pixel_data_in_bytes
          );
          debug!("  {:?}", frame.header);
          debug!(
            "  Color count: {} / Bits per pixel: {}",
            frame.palette.len(),
            bits_per_pixel
          );
          debug!(
            "  Local palette: {:?}",
            frame
              .local_palette
              .iter()
              .map(|color| format!("#{:02X}{:02X}{:02X}", color[0], color[1], color[2]))
              .collect::<Vec<_>>()
          );
        })
    }
    Command::Image { filename } => {
      let mac = resolve_device(args.device).await?;
      info!("Sending image {}", filename);
      send_image(mac, &filename).await?;
    }
    Command::Animation { filename } => {
      let mac = resolve_device(args.device).await?;
      let mut file = File::open(&filename)?;
      send_divoom_animation(mac, &mut file).await?;
    }
    Command::ScrollingText { text, font, font_size, color, bg_color, align, valign } => {
      let mac = resolve_device(args.device).await?;
      let font_path = resolve_font(font.as_deref())?;
      let fg_color = parse_color(&color)?;
      let bg_color_rgb = parse_color(&bg_color)?;
      info!("Sending scrolling text: {:?} (font: {:?}, size: {}, color: {}, bg: {})", text, font_path, font_size, color, bg_color);
      send_scrolling_text(mac, &font_path, &text, font_size, fg_color, bg_color_rgb, align, valign).await?
    }
    Command::StaticText { text, font, font_size, color, bg_color, align, valign } => {
      let mac = resolve_device(args.device).await?;
      let font_path = resolve_font(font.as_deref())?;
      let fg_color = parse_color(&color)?;
      let bg_color_rgb = parse_color(&bg_color)?;
      info!("Sending static text: {:?} (font: {:?}, size: {}, color: {}, bg: {})", text, font_path, font_size, color, bg_color);
      send_static_text(mac, &font_path, &text, font_size, fg_color, bg_color_rgb, align, valign).await?
    }
    Command::Brightness { level } => {
      let mac = resolve_device(args.device).await?;
      info!("Setting brightness to {}", level);
      send_set_brightness(mac, level).await?
    }
    Command::Clock { action } => {
      let mac = resolve_device(args.device).await?;
      match action {
        ClockCommand::Get => {
          let clock_id = send_get_clock_face(mac).await?;
          println!("{}", clock_id);
        }
        ClockCommand::Set { clock_id } => {
          info!("Setting clock face to {}", clock_id);
          send_set_clock_face(mac, clock_id).await?
        }
      }
    }
    Command::Mode { mode } => {
      let mac = resolve_device(args.device).await?;
      let payload = match mode {
        BoxMode::Light { sub_mode, color, brightness, on } => {
          let [r, g, b] = parse_color(&color)?;
          info!("Setting light mode (sub={}, color=#{:02X}{:02X}{:02X}, brightness={}, on={})", sub_mode, r, g, b, brightness, on);
          vec![0x01, sub_mode, r, g, b, brightness, on as u8, 0, 0, 0]
        }
        BoxMode::Hot => {
          info!("Setting hot/trending mode");
          vec![0x02]
        }
        BoxMode::Special { sub_type } => {
          info!("Setting special mode (sub_type={})", sub_type);
          vec![0x03, sub_type]
        }
        BoxMode::Music { sub_type } => {
          info!("Setting music visualizer mode (sub_type={})", sub_type);
          vec![0x04, sub_type, 0, 0, 0, 0, 0, 0, 0, 0]
        }
        BoxMode::Raw { payload_hex } => {
          let bytes: Vec<u8> = payload_hex.iter()
            .map(|s| u8::from_str_radix(s, 16).map_err(|_| format!("Invalid hex byte: '{}'", s)))
            .collect::<Result<_, _>>()?;
          info!("Setting box mode with raw payload: {}", hex::encode(&bytes));
          bytes
        }
      };
      send_set_box_mode(mac, payload).await?
    }
    Command::Volume { action } => {
      let mac = resolve_device(args.device).await?;
      match action {
        VolumeCommand::Get => {
          let volume = send_get_volume(mac).await?;
          println!("{}", volume);
        }
        VolumeCommand::Set { volume } => {
          info!("Setting volume to {}", volume);
          send_set_volume(mac, volume).await?
        }
      }
    }
    Command::Play => {
      let mac = resolve_device(args.device).await?;
      info!("Playing");
      send_set_play_status(mac, true).await?
    }
    Command::Pause => {
      let mac = resolve_device(args.device).await?;
      info!("Pausing");
      send_set_play_status(mac, false).await?
    }
    Command::SetDatetime { datetime } => {
      let mac = resolve_device(args.device).await?;
      let datetime = datetime.unwrap_or_else(|| chrono::Local::now().naive_local());
      info!("Setting date/time to {}", datetime);
      send_set_datetime(mac, datetime).await?
    }
    Command::Language { language } => {
      let mac = resolve_device(args.device).await?;
      let lang_index = extended_command::language_index(&language)
        .ok_or_else(|| format!(
          "Unknown language '{}'. Supported: {}",
          language,
          extended_command::SUPPORTED_LANGUAGES.join(", ")
        ))?;
      info!("Setting language to {} (index {})", language, lang_index);
      send_set_language(mac, lang_index).await?
    }
    Command::KeyboardBacklight { action } => {
      let mac = resolve_device(args.device).await?;
      let mode = match action {
        KeyboardBacklightAction::Next => 0,
        KeyboardBacklightAction::Prev => 1,
        KeyboardBacklightAction::Toggle => 2
      };
      info!("Keyboard backlight: {:?}", action);
      send_keyboard_backlight(mac, mode).await?
    }
    Command::Alarm { enable } => {
      let mac = resolve_device(args.device).await?;
      match enable {
        true => info!("Enabling alarm.."),
        false => info!("Disabling alarm..")
      }
      send_alarm(mac).await?
    }
  }

  Ok(())
}
