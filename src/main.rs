use std::error::Error;
use std::fs::File;
use std::io::{BufReader, BufWriter};
use std::path::PathBuf;
use bluer::Address;
use chrono::NaiveDateTime;
use clap::{Parser, Subcommand};
use env_logger::{Builder, Env};
use log::{debug, info};

use Command::{Convert, DebugImage, ListDevices, ListPairedDevices, Send};

use divoom_ditoo_pro_controller::divoom_file_format::animation::Animation;
use divoom_ditoo_pro_controller::divoom_file_format::frame::bits_per_pixel;
use divoom_ditoo_pro_controller::{
  find_paired_ditoo_pro_devices, list_devices, list_paired_devices, send_alarm,
  send_divoom_animation, send_get_clock_face, send_get_volume, send_image,
  send_keyboard_backlight, send_scrolling_text, send_set_brightness,
  send_set_box_mode, send_set_clock_face, send_set_datetime, send_set_language,
  send_set_play_status, send_set_volume
};
use divoom_ditoo_pro_controller::protocol::extended_command;

/// CLI tool to send bluetooth commands to a Divoom Ditoo Pro
#[derive(Parser, Debug)]
#[command(author, version, about, long_about = None)]
pub struct Args {
  #[command(subcommand)]
  command: Command
}

#[derive(Subcommand, Debug)]
enum Command {
  /// Lists all available bluetooth devices and tries to find a Divoom
  ListDevices,

  /// Lists paired Divoom Ditoo Pro devices
  ListPairedDevices,

  /// Connects to a Divoom via it's MAC address and sends a command
  Send {
    mac_address: Option<String>,
    #[command(subcommand)]
    send: SendCommand
  },

  /// Converts a Divoom animation to GIF and vice versa
  Convert {
    #[command(subcommand)]
    convert: ConvertCommand
  },

  /// Show detailed information about an image in Divoom file format
  DebugImage { filename: String }
}

#[derive(Subcommand, Debug)]
enum SendCommand {
  Alarm {
    #[arg(required = true, number_of_values = 1, value_parser = clap::builder::BoolishValueParser::new())]
    enable: bool
  },
  Animation {
    filename: String
  },
  Image {
    filename: String
  },
  SetDateTime {
    /// Date and time to set (e.g. "2025-01-15T12:30:00"). Defaults to current local time.
    datetime: Option<NaiveDateTime>
  },
  Brightness {
    #[arg(value_parser = clap::value_parser!(u8).range(0..=100))]
    level: u8
  },
  SetVolume {
    #[arg(value_parser = clap::value_parser!(u8).range(0..=16))]
    volume: u8
  },
  GetVolume,
  Play,
  Pause,
  SetLanguage {
    /// Language code (en, zh-hans, zh-hant, ja, th, fr, it, he, es, de, ru, pt, ko, nl, uk, ms)
    language: String
  },
  SetBoxMode {
    #[command(subcommand)]
    mode: BoxMode
  },
  SetClockFace {
    /// Clock face ID
    clock_id: u16
  },
  GetClockFace,
  KeyboardBacklight {
    #[command(subcommand)]
    action: KeyboardBacklightAction
  },
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
  }
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

#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
  Builder::from_env(Env::default().default_filter_or("debug")).init();

  let args = Args::parse();

  match args.command {
    ListDevices => list_devices().await?,
    ListPairedDevices => list_paired_devices().await?,
    Send { mac_address, send } => {
      let mac_address: Address = match mac_address {
        Some(addr) => addr
          .parse()
          .map_err(|_| format!("Invalid MAC address: '{}'", addr))?,
        None => {
          let devices = find_paired_ditoo_pro_devices().await?;
          match devices.len() {
            0 => return Err("No paired Ditoo Pro devices found. Specify a MAC address.".into()),
            1 => devices[0],
            _ => {
              let list = devices
                .iter()
                .map(|d| d.to_string())
                .collect::<Vec<_>>()
                .join(", ");
              return Err(format!(
                "Multiple paired Ditoo Pro devices found: {}. Specify a MAC address.",
                list
              ).into());
            }
          }
        }
      };
      match send {
        SendCommand::Alarm { enable } => {
          match enable {
            true => info!("Enabling alarm.."),
            false => info!("Disabling alarm..")
          }
          send_alarm(mac_address).await?
        }
        SendCommand::Animation { filename } => {
          let mut file = File::open(&filename)?;
          send_divoom_animation(mac_address, &mut file).await?;
        }
        SendCommand::Image { filename } => {
          info!("Sending image {}", filename);
          send_image(mac_address, &filename).await?;
        }
        SendCommand::SetDateTime { datetime } => {
          let datetime = datetime.unwrap_or_else(|| chrono::Local::now().naive_local());
          info!("Setting date/time to {}", datetime);
          send_set_datetime(mac_address, datetime).await?
        }
        SendCommand::Brightness { level } => {
          info!("Setting brightness to {}", level);
          send_set_brightness(mac_address, level).await?
        }
        SendCommand::SetVolume { volume } => {
          info!("Setting volume to {}", volume);
          send_set_volume(mac_address, volume).await?
        }
        SendCommand::GetVolume => {
          let volume = send_get_volume(mac_address).await?;
          println!("{}", volume);
        }
        SendCommand::Play => {
          info!("Playing");
          send_set_play_status(mac_address, true).await?
        }
        SendCommand::Pause => {
          info!("Pausing");
          send_set_play_status(mac_address, false).await?
        }
        SendCommand::SetLanguage { language } => {
          let lang_index = extended_command::language_index(&language)
            .ok_or_else(|| format!(
              "Unknown language '{}'. Supported: {}",
              language,
              extended_command::SUPPORTED_LANGUAGES.join(", ")
            ))?;
          info!("Setting language to {} (index {})", language, lang_index);
          send_set_language(mac_address, lang_index).await?
        }
        SendCommand::SetBoxMode { mode } => {
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
          send_set_box_mode(mac_address, payload).await?
        }
        SendCommand::SetClockFace { clock_id } => {
          info!("Setting clock face to {}", clock_id);
          send_set_clock_face(mac_address, clock_id).await?
        }
        SendCommand::GetClockFace => {
          let clock_id = send_get_clock_face(mac_address).await?;
          println!("{}", clock_id);
        }
        SendCommand::KeyboardBacklight { action } => {
          let mode = match action {
            KeyboardBacklightAction::Next => 0,
            KeyboardBacklightAction::Prev => 1,
            KeyboardBacklightAction::Toggle => 2
          };
          info!("Keyboard backlight: {:?}", action);
          send_keyboard_backlight(mac_address, mode).await?
        }
        SendCommand::ScrollingText { text, font, font_size, color, bg_color } => {
          let font_path = resolve_font(font.as_deref())?;
          let fg_color = parse_color(&color)?;
          let bg_color_rgb = parse_color(&bg_color)?;
          info!("Sending scrolling text: {:?} (font: {:?}, size: {}, color: {}, bg: {})", text, font_path, font_size, color, bg_color);
          send_scrolling_text(mac_address, &font_path, &text, font_size, fg_color, bg_color_rgb).await?
        }
      }
    }
    Convert { convert } => match convert {
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
    DebugImage { filename } => {
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
  }

  Ok(())
}
