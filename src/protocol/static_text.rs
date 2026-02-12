use std::error::Error;
use std::path::Path;

use bdf_parser::BdfFont;
use fontdue::{Font, FontSettings};
use image::{DynamicImage, Rgb, RgbImage};

use super::scrolling_text::{
    layout_text, rasterize_line_bdf, rasterize_line_ttf, render_frame, HAlign, VAlign,
};

pub fn build_static_text_image(
    font_path: &Path,
    text: &str,
    font_size: f32,
    fg_color: [u8; 3],
    bg_color: [u8; 3],
    halign: HAlign,
    valign: VAlign,
) -> Result<DynamicImage, Box<dyn Error>> {
    if text.is_empty() {
        return Err("Text must not be empty".into());
    }

    let text_lines: Vec<&str> = text.split('\n').collect();
    let num_lines = text_lines.len() as i32;
    let line_height = 16 / num_lines;
    if line_height == 0 {
        return Err("Too many lines (maximum 16)".into());
    }

    let font_data = std::fs::read(font_path)?;
    let is_bdf = font_path
        .extension()
        .is_some_and(|ext| ext.eq_ignore_ascii_case("bdf"));

    let lines: Vec<Vec<u16>> = if is_bdf {
        let bdf_font = BdfFont::parse(&font_data)
            .map_err(|e| format!("Failed to parse BDF font: {:?}", e))?;
        text_lines
            .iter()
            .map(|line| rasterize_line_bdf(&bdf_font, line, line_height))
            .collect()
    } else {
        let font = Font::from_bytes(font_data, FontSettings::default())
            .map_err(|e| format!("Failed to load font: {}", e))?;
        text_lines
            .iter()
            .map(|line| rasterize_line_ttf(&font, line, font_size, line_height))
            .collect()
    };

    let wide_bitmap = layout_text(&lines, line_height, halign, valign);

    let offset = match halign {
        HAlign::Left => 0,
        HAlign::Center => (wide_bitmap.len() as i32 - 16) / 2,
        HAlign::Right => wide_bitmap.len() as i32 - 16,
    };
    let pixels = render_frame(&wide_bitmap, offset, fg_color, bg_color);

    let mut image = RgbImage::new(16, 16);
    for (i, pixel) in pixels.iter().enumerate() {
        image.put_pixel((i % 16) as u32, (i / 16) as u32, Rgb(*pixel));
    }

    Ok(DynamicImage::ImageRgb8(image))
}
