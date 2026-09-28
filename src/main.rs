mod control_cli;
mod ids;
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
  find_paired_ditoo_pro_devices, scan_devices, list_paired_devices,
  send_divoom_animation, send_get_clock_face, send_get_volume, send_image,
  send_keyboard_backlight, send_set_brightness,
  send_set_box_mode, send_set_clock_face, send_set_datetime, send_set_language,
  send_set_play_status, send_set_volume,
};
#[cfg(feature = "text")]
use divoom_ditoo_pro_controller::{send_scrolling_text, send_static_text};
#[cfg(feature = "video")]
use divoom_ditoo_pro_controller::send_video;
use divoom_ditoo_pro_controller::protocol::extended_command;
#[cfg(feature = "text")]
use divoom_ditoo_pro_controller::protocol::scrolling_text::{HAlign, VAlign};

/// Control a Divoom Ditoo Pro and update its firmware over Bluetooth or USB
#[derive(Parser, Debug)]
#[command(author, version, about, long_about = None)]
pub struct Args {
  /// Bluetooth MAC (auto-detected for Bluetooth; optional USB post-flash version check)
  #[arg(long, global = true)]
  device: Option<String>,

  /// Transport; auto tries RFCOMM then BLE. USB control needs firmware 306019+
  #[arg(long, value_enum, default_value = "auto", global = true)]
  transport: divoom_ditoo_pro_controller::Transport,

  /// USB physical port (e.g. 1-6); automatic when one Ditoo is attached
  #[arg(long, global = true)]
  usb_port: Option<String>,

  #[command(subcommand)]
  command: Command
}

#[derive(Subcommand, Debug)]
enum Command {
  /// Run uploaded Lua programs on the device (requires Lua runtime firmware)
  Lua { #[command(subcommand)] action: LuaCommand },

  /// Decode and validate an MVA firmware package without connecting to a device
  FirmwareDecode {
    file: PathBuf,
    /// Extract records, code and strings into a new directory
    #[arg(long)] output: Option<PathBuf>,
  },

  /// Install a pinned stock or experimental image over Bluetooth or USB
  FirmwareUpdate {
    file: PathBuf,
    /// Reinstall the same image (USB forces it; Bluetooth may still reject it)
    #[arg(long)] reflash: bool,
    /// Restore pinned stock 306007 from a supported experimental firmware
    #[arg(long)] restore_stock: bool,
    /// Number of USB reports in flight within each verified block
    #[arg(long, default_value_t = 16, value_parser = clap::value_parser!(u8).range(1..=16))]
    usb_queue_depth: u8,
    /// Validate image and show metadata without connecting or writing
    #[arg(long)] dry_run: bool,
  },

  /// List known game IDs and other selectors without connecting to a device
  Ids {
    /// Category to inspect; omit to list categories
    category: Option<String>,
    /// Print source metadata and mappings as JSON
    #[arg(long)] json: bool,
  },

  /// Low-level protocol catalogue, commands, scripts and response monitoring
  Raw { #[command(subcommand)] action: control_cli::RawCommand },

  #[command(flatten)]
  Device(control_cli::DeviceCommand),

  /// Scan for Bluetooth devices or list attached USB Ditoos
  Scan,

  /// List paired Bluetooth Ditoos or attached USB Ditoos
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

  #[cfg(feature = "text")]
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

  #[cfg(feature = "text")]
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

  #[cfg(feature = "video")]
  /// Play a video file on the 16x16 display
  Video {
    /// Path to the video file
    filename: String,
    /// Extra mpv option (e.g. --mpv-option volume=50)
    #[arg(long = "mpv-option")]
    mpv_option: Vec<String>,
  },

  /// Set screen brightness (0-100)
  Brightness {
    #[arg(value_parser = clap::value_parser!(u8).range(0..=100))]
    level: u8
  },

  /// Get or set the clock face
  #[command(after_help = "See `ids clock-faces` for the ID reference.")]
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

  /// Write an alarm slot, including its enabled state and time
  #[command(after_help = "See `ids alarm-modes` for the ID reference.")]
  Alarm {
    #[arg(required = true, number_of_values = 1, value_parser = clap::builder::BoolishValueParser::new())]
    enable: bool,
    /// Full alarm slot configuration is written; specify its time explicitly
    #[arg(long)] time: String,
    #[arg(long, default_value_t=0)] index: u8,
    /// Repeat bit mask: bit 0 Sunday through bit 6 Saturday
    #[arg(long, default_value_t=0, value_parser=clap::value_parser!(u8).range(0..=127))] repeat: u8,
    #[arg(long, default_value_t=0)] mode: u8,
    #[arg(long, default_value_t=0)] trigger: u8,
    /// Radio frequency in tenths of MHz (875 = 87.5 MHz)
    #[arg(long, default_value_t=0, value_parser=clap::value_parser!(u16).range(0..=25599))] frequency: u16,
    #[arg(long, default_value_t=50, value_parser=clap::value_parser!(u8).range(0..=100))] volume: u8,
    #[arg(long)] dry_run: bool
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
  #[command(after_help = "See `ids light-modes` for the ID reference.")]
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
  #[command(after_help = "See `ids special-modes` for the ID reference.")]
  Special {
    /// Effect sub-type index
    sub_type: u8
  },
  /// Music visualizer
  #[command(after_help = "See `ids music-modes` for the ID reference.")]
  Music {
    /// Visualizer sub-type index
    sub_type: u8
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

#[derive(Subcommand, Debug)]
enum LuaCommand {
  /// Watch runtime status on one connection (JSONL; Ctrl-C disconnects)
  Watch {
    #[arg(long, default_value_t=500, value_parser=clap::value_parser!(u64).range(20..=60000))]
    interval_ms: u64,
    #[arg(long, default_value_t=30, value_parser=clap::value_parser!(u64).range(1..=86400))]
    seconds: u64,
    /// Include unchanged observations and changing frame/instruction counters
    #[arg(long)] all: bool,
  },
  /// Run a JSON array of Lua actions on one connection
  Sequence {
    file: PathBuf,
    #[arg(long, default_value_t=120, value_parser=clap::value_parser!(u64).range(1..=3600))]
    timeout: u64,
    /// Validate all steps and source files without opening a device
    #[arg(long)] dry_run: bool,
  },
  /// Decode saved raw replies or hex payloads offline; '-' reads stdin
  Decode { file: PathBuf },
  /// Start or replace a resident app; the device continues after disconnect
  Start { file: PathBuf },
  /// Stop the app and release its controls
  Stop,
  /// Pause callbacks and release display/input ownership
  Pause,
  /// Resume a paused app
  Resume,
  /// Deliver data to the running app's message callback
  Send { message: String },
  /// Read and acknowledge the app's outgoing message
  Receive,
  /// Upload and execute a Lua source file without reflashing
  Run { file: PathBuf },
  /// Execute a Lua source expression/program without reflashing
  Eval { source: String },
  /// Read the last execution result
  Status,
  /// Request cancellation of the running program
  Cancel,
}

fn parse_color(s: &str) -> Result<[u8; 3], Box<dyn Error>> {
  let c = csscolorparser::parse(s).map_err(|e| format!("Invalid color '{}': {}", s, e))?;
  let [r, g, b, _] = c.to_rgba8();
  Ok([r, g, b])
}

#[cfg(feature = "text")]
fn find_bitmap_monospace_font() -> Option<PathBuf> {
  let output = std::process::Command::new("fc-match")
    .args(["monospace:scalable=false", "--format=%{file}"])
    .output()
    .ok()?;
  if !output.status.success() {
    return None;
  }
  let path = PathBuf::from(String::from_utf8(output.stdout).ok()?.trim().to_string());
  if path.exists() { Some(path) } else { None }
}

#[cfg(feature = "text")]
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
      if let Some(path) = find_bitmap_monospace_font() {
        return Ok(path);
      }
      let fc = fontconfig::Fontconfig::new()
        .ok_or("Failed to initialize fontconfig")?;
      let font = fc.find("monospace", None)
        .ok_or("No monospace font found; use --font")?;
      Ok(font.path.clone())
    }
  }
}

async fn resolve_device(device: Option<String>) -> Result<Address, Box<dyn Error>> {
  if matches!(divoom_ditoo_pro_controller::selected_transport(), divoom_ditoo_pro_controller::Transport::Usb) {
    // Library APIs retain the address argument; USB uses only the physical port.
    return Ok(Address::new([0; 6]));
  }
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

  divoom_ditoo_pro_controller::with_usb_port(args.usb_port.clone(),
    divoom_ditoo_pro_controller::with_transport(args.transport, run(args))).await
}

async fn run(args: Args) -> Result<(), Box<dyn Error>> {
  if args.usb_port.is_some() && !matches!(args.transport, divoom_ditoo_pro_controller::Transport::Usb) {
    return Err("--usb-port requires --transport usb".into());
  }
  match args.command {
    Command::Lua { action } => {
      use divoom_ditoo_pro_controller::lua::{self, Action};
      use divoom_ditoo_pro_controller::lua_tools;
      use std::time::Duration;
      // Offline inspection and validation must not resolve a Bluetooth device.
      let action = match action {
        LuaCommand::Decode { file } => {
          if file.as_os_str() == "-" {
            lua_tools::decode_lines(std::io::stdin().lock(), std::io::stdout().lock())?;
          } else {
            lua_tools::decode_lines(BufReader::new(File::open(file)?), std::io::stdout().lock())?;
          }
          return Ok(());
        }
        LuaCommand::Sequence { file, timeout, dry_run } => {
          let sequence = lua_tools::Sequence::load(&file)?;
          if dry_run { println!("{}", serde_json::to_string_pretty(&sequence.describe())?); }
          else {
            lua_tools::sequence(resolve_device(args.device).await?, &sequence, Duration::from_secs(timeout)).await?;
          }
          return Ok(());
        }
        LuaCommand::Watch { interval_ms, seconds, all } => {
          lua_tools::watch(resolve_device(args.device).await?, Duration::from_millis(interval_ms), Duration::from_secs(seconds), all).await?;
          return Ok(());
        }
        action => action,
      };
      let address = resolve_device(args.device).await?;
      let status = match action {
        LuaCommand::Run { file } => lua::control(address, Action::Run(&std::fs::read(file)?)).await?,
        LuaCommand::Start { file } => lua::control(address, Action::Start(&std::fs::read(file)?)).await?,
        LuaCommand::Eval { source } => lua::control(address, Action::Run(source.as_bytes())).await?,
        LuaCommand::Status => lua::control(address, Action::Status).await?,
        LuaCommand::Cancel | LuaCommand::Stop => lua::control(address, Action::Stop).await?,
        LuaCommand::Pause => lua::control(address, Action::Pause).await?,
        LuaCommand::Resume => lua::control(address, Action::Resume).await?,
        LuaCommand::Send { message } => lua::control(address, Action::Send(message.as_bytes())).await?,
        LuaCommand::Receive => lua::control(address, Action::Receive).await?,
        LuaCommand::Decode { .. } | LuaCommand::Sequence { .. } | LuaCommand::Watch { .. } => unreachable!(),
      };
      println!("{}", serde_json::to_string(&status)?);
      if status.state == "error" { return Err(format!("Lua: {}", status.result).into()); }
    }
    Command::FirmwareDecode { file, output } => {
      let report = divoom_ditoo_pro_controller::firmware_decode::decode(&file, output.as_deref())?;
      println!("{}", serde_json::to_string_pretty(&report)?);
    }
    Command::FirmwareUpdate { file, reflash, restore_stock, usb_queue_depth, dry_run } => {
      let usb_port = args.usb_port;
      let image = divoom_ditoo_pro_controller::firmware::Image::load(&file)?;
      if matches!(args.transport, divoom_ditoo_pro_controller::Transport::Usb) {
        let metadata = divoom_ditoo_pro_controller::usb_firmware::describe(&image, reflash)?;
        if restore_stock && image.version() != 306007 {
          return Err("--restore-stock requires pinned stock 306007".into());
        }
        if dry_run { println!("{metadata}"); }
        else {
          let address = args.device.map(|s| s.parse::<Address>()).transpose()?;
          divoom_ditoo_pro_controller::usb_firmware::flash(&image, usb_port.as_deref(), reflash, usb_queue_depth, address).await?;
        }
      } else {
        if usb_port.is_some() || usb_queue_depth != 16 {
          return Err("--usb-port and --usb-queue-depth require --transport usb".into());
        }
        if dry_run { println!("{}", image.describe()); }
        else { divoom_ditoo_pro_controller::firmware::flash(resolve_device(args.device).await?, &image, reflash, restore_stock).await?; }
      }
    }

    Command::Ids { category, json } => ids::show(category.as_deref(), json)?,
    Command::Raw { action } => control_cli::run(args.device, action).await?,
    Command::Device(action) => control_cli::run_requests(args.device, vec![action.request()?], action.dry_run()).await?,
    Command::Scan | Command::Devices if matches!(args.transport, divoom_ditoo_pro_controller::Transport::Usb) => {
      println!("{}", divoom_ditoo_pro_controller::usb_firmware::devices().await?);
    }
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
    #[cfg(feature = "text")]
    Command::ScrollingText { text, font, font_size, color, bg_color, align, valign } => {
      let mac = resolve_device(args.device).await?;
      let font_path = resolve_font(font.as_deref())?;
      let fg_color = parse_color(&color)?;
      let bg_color_rgb = parse_color(&bg_color)?;
      info!("Sending scrolling text: {:?} (font: {:?}, size: {}, color: {}, bg: {})", text, font_path, font_size, color, bg_color);
      send_scrolling_text(mac, &font_path, &text, font_size, fg_color, bg_color_rgb, align, valign).await?
    }
    #[cfg(feature = "text")]
    Command::StaticText { text, font, font_size, color, bg_color, align, valign } => {
      let mac = resolve_device(args.device).await?;
      let font_path = resolve_font(font.as_deref())?;
      let fg_color = parse_color(&color)?;
      let bg_color_rgb = parse_color(&bg_color)?;
      info!("Sending static text: {:?} (font: {:?}, size: {}, color: {}, bg: {})", text, font_path, font_size, color, bg_color);
      send_static_text(mac, &font_path, &text, font_size, fg_color, bg_color_rgb, align, valign).await?
    }
    #[cfg(feature = "video")]
    Command::Video { filename, mpv_option } => {
      let mac = resolve_device(args.device).await?;
      let opts: Vec<(String, String)> = mpv_option.iter().map(|s| {
        let (k, v) = s.split_once('=').unwrap_or((s, ""));
        (k.to_string(), v.to_string())
      }).collect();
      info!("Playing video: {}", filename);
      send_video(mac, &filename, &opts).await?
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
    Command::Alarm { enable, time, index, repeat, mode, trigger, frequency, volume, dry_run } => {
      let alarm = divoom_ditoo_pro_controller::protocol::alarm::Alarm {
        enable, index, repeat, mode, trigger_mode: trigger, fm: [(frequency % 100) as u8, (frequency / 100) as u8], volume,
        time: chrono::NaiveTime::parse_from_str(&time, "%H:%M")?,
      };
      let request = divoom_ditoo_pro_controller::control::Request::bytes(0x43, &alarm.serialize()?, false);
      control_cli::run_requests(args.device, vec![request], dry_run).await?;
    }
  }

  Ok(())
}

#[cfg(test)]
mod cli_tests {
  use super::*;
  #[test]
  fn top_level_controls_preserve_dry_run_and_global_options() -> Result<(), Box<dyn Error>> {
    for args in [
      vec!["divoom", "--device", "B1:21:81:DD:B8:9B", "--transport", "ble", "scoreboard", "12", "34", "--dry-run"],
      vec!["divoom", "scoreboard", "12", "34", "--dry-run", "--device", "B1:21:81:DD:B8:9B", "--transport", "ble"],
    ] {
      let parsed = Args::try_parse_from(args)?;
      assert_eq!(parsed.device.as_deref(), Some("B1:21:81:DD:B8:9B"));
      assert!(matches!(parsed.transport, divoom_ditoo_pro_controller::Transport::Ble));
      let Command::Device(action) = parsed.command else {
        return Err("Expected a friendly device control".into());
      };
      assert!(action.dry_run());
      assert_eq!(action.request()?.payload_hex, "01010c002200");
    }
    assert!(Args::try_parse_from(["divoom", "device", "status"]).is_err());
    let parsed = Args::try_parse_from(["divoom", "raw", "send", "0x45", "--data", "060000", "--dry-run", "--device", "B1:21:81:DD:B8:9B"])?;
    assert_eq!(parsed.device.as_deref(), Some("B1:21:81:DD:B8:9B"));
    assert!(matches!(parsed.command, Command::Raw { action: control_cli::RawCommand::Send { dry_run: true, .. } }));
    Ok(())
  }

  #[test]
  fn usb_flash_options_work_without_a_bluetooth_address() -> Result<(), Box<dyn Error>> {
    let args = Args::try_parse_from(["divoom", "firmware-update", "firmware/306007.MVA", "--transport", "usb", "--usb-port", "1-2.3", "--reflash", "--dry-run"])?;
    assert!(args.device.is_none());
    assert!(matches!(args.transport, divoom_ditoo_pro_controller::Transport::Usb));
    assert_eq!(args.usb_port.as_deref(), Some("1-2.3"));
    assert!(matches!(args.command, Command::FirmwareUpdate { usb_queue_depth: 16, reflash: true, dry_run: true, .. }));
    for depth in ["0", "17"] {
      assert!(Args::try_parse_from(["divoom", "firmware-update", "firmware/306007.MVA", "--usb-queue-depth", depth]).is_err());
    }
    Ok(())
  }

  #[test]
  fn alarm_requires_explicit_time_and_preserves_enabled_value() -> Result<(), Box<dyn Error>> {
    assert!(Args::try_parse_from(["divoom", "alarm", "true"]).is_err());
    for (text, expected) in [("true", true), ("false", false)] {
      let args = Args::try_parse_from(["divoom", "alarm", text, "--time", "07:30", "--repeat", "62", "--dry-run"])?;
      match args.command {
        Command::Alarm { enable, time, repeat, dry_run, .. } => {
          assert_eq!(enable, expected);
          assert_eq!(time, "07:30");
          assert_eq!(repeat, 62);
          assert!(dry_run);
        }
        _ => return Err("Parsed a different command".into()),
      }
    }
    Ok(())
  }
}
