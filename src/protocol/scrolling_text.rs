use std::error::Error;
use std::path::Path;

use fontdue::{Font, FontSettings};
use image::{DynamicImage, Rgb, RgbImage};
use indexmap::IndexSet;

use crate::divoom_file_format::frame::Frame;
use crate::divoom_file_format::frame_header::FrameHeader;

struct RasterizedGlyph {
    columns: Vec<u16>,
}

fn rasterize_glyph(font: &Font, ch: char, px: f32) -> RasterizedGlyph {
    let (metrics, bitmap) = font.rasterize(ch, px);

    let line_metrics = font.horizontal_line_metrics(px);
    let (ascent, descent) = match line_metrics {
        Some(m) => (m.ascent, m.descent),
        None => (px, 0.0),
    };
    let total_height = (ascent - descent).round() as i32;
    let scale = if total_height > 0 {
        16.0 / total_height as f64
    } else {
        1.0
    };
    let scaled_ascent = (ascent as f64 * scale).round() as i32;

    let y_offset = scaled_ascent - metrics.height as i32 - metrics.ymin;
    let x_offset = metrics.xmin;

    let advance = metrics.advance_width.round() as i32;
    let width = advance.max(1) as usize;

    let mut columns = vec![0u16; width];
    for (col_idx, word) in columns.iter_mut().enumerate() {
        let col = col_idx as i32;
        for row in 0..16i32 {
            let bx = col - x_offset;
            let by = row - y_offset;
            if bx >= 0 && bx < metrics.width as i32 && by >= 0 && by < metrics.height as i32 {
                let alpha = bitmap[by as usize * metrics.width + bx as usize];
                if alpha >= 128 {
                    *word |= 1 << row;
                }
            }
        }
    }
    RasterizedGlyph { columns }
}

fn render_frame(wide_bitmap: &[[u8; 2]], scroll_offset: i32) -> [[u8; 3]; 256] {
    let fg = [0xFF, 0xFF, 0xFF];
    let bg = [0x00, 0x00, 0x00];
    let mut pixels = [bg; 256];

    for screen_col in 0..16i32 {
        let src_col = scroll_offset + screen_col;
        let column_data: u16 = if src_col >= 0 && (src_col as usize) < wide_bitmap.len() {
            let pair = wide_bitmap[src_col as usize];
            u16::from_le_bytes(pair)
        } else {
            0
        };

        for row in 0..16i32 {
            let bit_set = (column_data >> row) & 1 == 1;
            if bit_set {
                pixels[(row * 16 + screen_col) as usize] = fg;
            }
        }
    }

    pixels
}

pub struct ScrollingText {
    pub encoded_frames: Vec<Vec<u8>>,
}

pub fn build_scrolling_text_frames(
    font_path: &Path,
    text: &str,
    font_size: f32,
) -> Result<ScrollingText, Box<dyn Error>> {
    let chars: Vec<char> = text.chars().collect();

    if chars.is_empty() {
        return Err("Text must not be empty".into());
    }

    let font_data = std::fs::read(font_path)?;
    let font = Font::from_bytes(font_data, FontSettings::default())
        .map_err(|e| format!("Failed to load font: {}", e))?;

    let mut wide_bitmap: Vec<[u8; 2]> = Vec::new();
    for &ch in &chars {
        let glyph = rasterize_glyph(&font, ch, font_size);
        for &col_data in &glyph.columns {
            wide_bitmap.push(col_data.to_le_bytes());
        }
    }

    let total_width = wide_bitmap.len() as i32;
    let mut encoded_frames = Vec::new();

    for offset in -16..total_width {
        let pixels = render_frame(&wide_bitmap, offset);

        let mut palette = IndexSet::new();
        let mut image = RgbImage::new(16, 16);
        for (i, pixel) in pixels.iter().enumerate() {
            palette.insert(Rgb(*pixel));
            image.put_pixel((i % 16) as u32, (i / 16) as u32, Rgb(*pixel));
        }
        let palette: Vec<Rgb<u8>> = palette.into_iter().collect();

        let frame = Frame {
            header: FrameHeader {
                time_in_milliseconds: 60,
                reuse_palette: false,
                color_count: palette.len() as u8,
            },
            palette: palette.clone(),
            local_palette: palette.clone(),
            image: DynamicImage::ImageRgb8(image),
        };

        let mut encoded = Vec::new();
        frame.serialize(&palette, &mut encoded)?;
        encoded_frames.push(encoded);
    }

    Ok(ScrollingText { encoded_frames })
}
