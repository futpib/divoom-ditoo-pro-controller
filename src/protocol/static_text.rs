use std::error::Error;
use std::path::Path;

use bdf_parser::BdfFont;
use fontdue::{Font, FontSettings};
use image::{DynamicImage, Rgb, RgbImage};

use super::scrolling_text::{rasterize_glyph, rasterize_glyph_bdf, render_frame};

pub fn build_static_text_image(
    font_path: &Path,
    text: &str,
    font_size: f32,
    fg_color: [u8; 3],
    bg_color: [u8; 3],
) -> Result<DynamicImage, Box<dyn Error>> {
    let chars: Vec<char> = text.chars().collect();

    if chars.is_empty() {
        return Err("Text must not be empty".into());
    }

    let font_data = std::fs::read(font_path)?;
    let is_bdf = font_path
        .extension()
        .map_or(false, |ext| ext.eq_ignore_ascii_case("bdf"));

    let mut wide_bitmap: Vec<[u8; 2]> = Vec::new();
    if is_bdf {
        let bdf_font = BdfFont::parse(&font_data)
            .map_err(|e| format!("Failed to parse BDF font: {:?}", e))?;
        for &ch in &chars {
            let glyph = rasterize_glyph_bdf(&bdf_font, ch);
            for &col_data in &glyph.columns {
                wide_bitmap.push(col_data.to_le_bytes());
            }
        }
    } else {
        let font = Font::from_bytes(font_data, FontSettings::default())
            .map_err(|e| format!("Failed to load font: {}", e))?;
        for &ch in &chars {
            let glyph = rasterize_glyph(&font, ch, font_size);
            for &col_data in &glyph.columns {
                wide_bitmap.push(col_data.to_le_bytes());
            }
        }
    }

    let offset = ((wide_bitmap.len() as i32 - 16) / 2).max(0);
    let pixels = render_frame(&wide_bitmap, offset, fg_color, bg_color);

    let mut image = RgbImage::new(16, 16);
    for (i, pixel) in pixels.iter().enumerate() {
        image.put_pixel((i % 16) as u32, (i / 16) as u32, Rgb(*pixel));
    }

    Ok(DynamicImage::ImageRgb8(image))
}
