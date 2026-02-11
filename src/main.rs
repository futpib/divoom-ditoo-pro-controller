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
  send_divoom_animation, send_image, send_keyboard_backlight,
  send_scrolling_text, send_set_brightness, send_set_datetime
};

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
  }
}

#[derive(Subcommand, Debug)]
enum KeyboardBacklightAction {
  Next,
  Prev,
  Toggle
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
        SendCommand::KeyboardBacklight { action } => {
          let mode = match action {
            KeyboardBacklightAction::Next => 0,
            KeyboardBacklightAction::Prev => 1,
            KeyboardBacklightAction::Toggle => 2
          };
          info!("Keyboard backlight: {:?}", action);
          send_keyboard_backlight(mac_address, mode).await?
        }
        SendCommand::ScrollingText { text, font, font_size } => {
          let font_path = resolve_font(font.as_deref())?;
          info!("Sending scrolling text: {:?} (font: {:?}, size: {})", text, font_path, font_size);
          send_scrolling_text(mac_address, &font_path, &text, font_size).await?
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
