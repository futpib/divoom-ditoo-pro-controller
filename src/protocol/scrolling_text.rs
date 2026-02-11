use std::error::Error;
use std::path::Path;

use bdf_parser::{BdfFont, Property};
use fontdue::{Font, FontSettings};
use image::{DynamicImage, Rgb, RgbImage};
use indexmap::IndexSet;

use crate::divoom_file_format::frame::Frame;
use crate::divoom_file_format::frame_header::FrameHeader;

#[derive(Clone, Copy, Debug, Default, clap::ValueEnum)]
pub enum HAlign {
    Left,
    #[default]
    Center,
    Right,
}

#[derive(Clone, Copy, Debug, Default, clap::ValueEnum)]
pub enum VAlign {
    Top,
    #[default]
    Center,
    Bottom,
}

pub(crate) struct RasterizedGlyph {
    pub(crate) columns: Vec<u16>,
}

pub(crate) fn rasterize_glyph_bdf(font: &BdfFont, ch: char, target_height: i32) -> RasterizedGlyph {
    let glyph = match font.glyphs.get(ch) {
        Some(g) => g,
        None => {
            return RasterizedGlyph {
                columns: vec![0u16; 1],
            };
        }
    };

    let font_ascent: i32 = font
        .properties
        .try_get::<i32>(Property::FontAscent)
        .unwrap_or(font.metadata.bounding_box.size.y);

    let font_descent: i32 = font
        .properties
        .try_get::<i32>(Property::FontDescent)
        .unwrap_or(0);

    let font_height = font_ascent + font_descent;

    let bb = &glyph.bounding_box;
    let glyph_width = bb.size.x;
    let glyph_height = bb.size.y;
    let x_off = bb.offset.x;
    let y_off = bb.offset.y;

    let advance = glyph.device_width.x;
    let width = advance.max(1) as usize;

    // Scale the ascent proportionally so the glyph is repositioned
    // within target_height rather than being pushed out of view.
    let scaled_ascent = if font_height > 0 {
        (font_ascent as f64 * target_height as f64 / font_height as f64).round() as i32
    } else {
        font_ascent
    };

    let mut columns = vec![0u16; width];
    for (col_idx, word) in columns.iter_mut().enumerate() {
        let col = col_idx as i32;
        for row in 0..target_height {
            let pixel_x = col - x_off;
            let pixel_y = y_off + glyph_height - scaled_ascent + row;
            if pixel_x >= 0 && pixel_x < glyph_width && pixel_y >= 0 && pixel_y < glyph_height {
                if glyph.pixel(pixel_x as usize, pixel_y as usize) {
                    *word |= 1 << row;
                }
            }
        }
    }
    RasterizedGlyph { columns }
}

pub(crate) fn rasterize_glyph(font: &Font, ch: char, px: f32, target_height: i32) -> RasterizedGlyph {
    let (metrics, bitmap) = font.rasterize(ch, px);

    let line_metrics = font.horizontal_line_metrics(px);
    let (ascent, descent) = match line_metrics {
        Some(m) => (m.ascent, m.descent),
        None => (px, 0.0),
    };
    let total_height = (ascent - descent).round() as i32;
    let scale = if total_height > 0 {
        target_height as f64 / total_height as f64
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
        for row in 0..target_height {
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

pub(crate) fn rasterize_line_bdf(font: &BdfFont, text: &str, target_height: i32) -> Vec<u16> {
    let mut columns = Vec::new();
    for ch in text.chars() {
        let glyph = rasterize_glyph_bdf(font, ch, target_height);
        columns.extend(glyph.columns.iter().copied());
    }
    columns
}

pub(crate) fn rasterize_line_ttf(font: &Font, text: &str, font_size: f32, target_height: i32) -> Vec<u16> {
    let mut columns = Vec::new();
    for ch in text.chars() {
        let glyph = rasterize_glyph(font, ch, font_size, target_height);
        columns.extend(glyph.columns.iter().copied());
    }
    columns
}

pub(crate) fn layout_text(
    lines: &[Vec<u16>],
    line_height: i32,
    halign: HAlign,
    valign: VAlign,
) -> Vec<[u8; 2]> {
    let num_lines = lines.len() as i32;
    let total_text_height = num_lines * line_height;

    let max_width = lines.iter().map(|l| l.len()).max().unwrap_or(0);

    let y_offset = match valign {
        VAlign::Top => 0,
        VAlign::Center => (16 - total_text_height) / 2,
        VAlign::Bottom => 16 - total_text_height,
    };

    let mut wide_bitmap = vec![[0u8; 2]; max_width];

    for (line_idx, line) in lines.iter().enumerate() {
        let x_offset = match halign {
            HAlign::Left => 0,
            HAlign::Center => (max_width - line.len()) / 2,
            HAlign::Right => max_width - line.len(),
        };

        let line_y = y_offset + line_idx as i32 * line_height;

        for (col_idx, &col_data) in line.iter().enumerate() {
            let target_col = x_offset + col_idx;
            if target_col < max_width && line_y >= 0 {
                let shifted = (col_data as u16) << line_y;
                let existing = u16::from_le_bytes(wide_bitmap[target_col]);
                wide_bitmap[target_col] = (existing | shifted).to_le_bytes();
            }
        }
    }

    wide_bitmap
}

pub(crate) fn render_frame(wide_bitmap: &[[u8; 2]], scroll_offset: i32, fg: [u8; 3], bg: [u8; 3]) -> [[u8; 3]; 256] {
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
    fg_color: [u8; 3],
    bg_color: [u8; 3],
    halign: HAlign,
    valign: VAlign,
) -> Result<ScrollingText, Box<dyn Error>> {
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
        .map_or(false, |ext| ext.eq_ignore_ascii_case("bdf"));

    let lines: Vec<Vec<u16>> = if is_bdf {
        let bdf_font = BdfFont::parse(&font_data)
            .map_err(|e| format!("Failed to parse BDF font: {:?}", e))?;
        text_lines.iter().map(|line| rasterize_line_bdf(&bdf_font, line, line_height)).collect()
    } else {
        let font = Font::from_bytes(font_data, FontSettings::default())
            .map_err(|e| format!("Failed to load font: {}", e))?;
        text_lines.iter().map(|line| rasterize_line_ttf(&font, line, font_size, line_height)).collect()
    };

    let wide_bitmap = layout_text(&lines, line_height, halign, valign);

    let total_width = wide_bitmap.len() as i32;
    let mut encoded_frames = Vec::new();

    for offset in -16..total_width {
        let pixels = render_frame(&wide_bitmap, offset, fg_color, bg_color);

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

#[cfg(test)]
mod tests {
    use super::*;

    // Minimal BDF font: 4-wide, 6-tall glyph for 'A' in a font with ascent=6, descent=2 (height=8).
    //
    // Bitmap (row-major, MSB=leftmost pixel):
    //   F0 = ####  (row 0)
    //   90 = #  #  (row 1)
    //   F0 = ####  (row 2)
    //   90 = #  #  (row 3)
    //   90 = #  #  (row 4)
    //   90 = #  #  (row 5)
    const TEST_BDF: &[u8] = b"STARTFONT 2.1
FONT -Test
SIZE 8 72 72
FONTBOUNDINGBOX 4 8 0 -2
STARTPROPERTIES 2
FONT_ASCENT 6
FONT_DESCENT 2
ENDPROPERTIES
CHARS 1
STARTCHAR A
ENCODING 65
SWIDTH 400 0
DWIDTH 4 0
BBX 4 6 0 0
BITMAP
F0
90
F0
90
90
90
ENDCHAR
ENDFONT
";

    fn parse_test_font() -> BdfFont {
        BdfFont::parse(TEST_BDF).expect("failed to parse test BDF font")
    }

    /// Format glyph columns as a visual grid for debugging.
    /// Each column is a u16 where bit N means row N is set.
    fn dump_columns(columns: &[u16], height: i32) -> String {
        let mut lines = Vec::new();
        for row in 0..height {
            let mut line = String::new();
            for &col in columns {
                line.push(if (col >> row) & 1 == 1 { '#' } else { '.' });
            }
            lines.push(line);
        }
        lines.join("\n")
    }

    #[test]
    fn bdf_rasterize_full_height() {
        let font = parse_test_font();
        let glyph = rasterize_glyph_bdf(&font, 'A', 8);

        let visual = dump_columns(&glyph.columns, 8);
        eprintln!("full height (8):\n{}", visual);

        assert_eq!(glyph.columns.len(), 4, "advance width should be 4");
        // Column 0 (x=0): all 6 glyph rows on, rows 6-7 empty
        assert_eq!(glyph.columns[0], 0b00111111);
        // Column 1 (x=1): only rows 0,2 on (the F0 rows)
        assert_eq!(glyph.columns[1], 0b00000101);
        // Column 2 (x=2): same as column 1
        assert_eq!(glyph.columns[2], 0b00000101);
        // Column 3 (x=3): same as column 0
        assert_eq!(glyph.columns[3], 0b00111111);
    }

    #[test]
    fn bdf_rasterize_half_height() {
        let font = parse_test_font();
        let glyph = rasterize_glyph_bdf(&font, 'A', 4);

        let visual = dump_columns(&glyph.columns, 4);
        eprintln!("half height (4):\n{}", visual);

        // Width stays the same (no pixel scaling, just repositioning)
        assert_eq!(glyph.columns.len(), 4);
        // The glyph should have pixels set (not pushed out of view)
        let total_bits: u32 = glyph.columns.iter().map(|c| c.count_ones()).sum();
        assert!(total_bits > 0, "glyph should have pixels set");
    }

    #[test]
    fn bdf_rasterize_missing_glyph() {
        let font = parse_test_font();
        let glyph = rasterize_glyph_bdf(&font, 'Z', 8);
        assert_eq!(glyph.columns.len(), 1);
        assert_eq!(glyph.columns[0], 0);
    }

    #[test]
    fn bdf_rasterize_line() {
        let font = parse_test_font();
        let columns = rasterize_line_bdf(&font, "AA", 8);
        // Two glyphs, each 4 wide
        assert_eq!(columns.len(), 8);
    }

    #[test]
    fn layout_single_line_center() {
        let line = vec![0b11u16, 0b10u16, 0b01u16];
        let result = layout_text(&[line], 16, HAlign::Center, VAlign::Center);
        assert_eq!(result.len(), 3);
        // With single line at height 16 and valign center, y_offset = 0
        assert_eq!(u16::from_le_bytes(result[0]), 0b11);
        assert_eq!(u16::from_le_bytes(result[1]), 0b10);
        assert_eq!(u16::from_le_bytes(result[2]), 0b01);
    }

    #[test]
    fn layout_two_lines_vertical_positioning() {
        // Two 8-px-tall lines
        let line0 = vec![0b1111_1111u16]; // all 8 bits set
        let line1 = vec![0b0000_0001u16]; // only bit 0
        let result = layout_text(&[line0, line1], 8, HAlign::Center, VAlign::Top);
        assert_eq!(result.len(), 1);
        let col = u16::from_le_bytes(result[0]);
        // Line 0 at y_offset=0: bits 0-7 from line0 → 0xFF
        // Line 1 at y_offset=8: bit 0 from line1 shifted left by 8 → bit 8 set
        assert_eq!(col, 0b1_0000_0000 | 0b1111_1111);
    }

    #[test]
    fn layout_two_lines_halign_left() {
        let line0 = vec![1u16, 2u16, 3u16]; // 3 wide
        let line1 = vec![4u16]; // 1 wide
        let result = layout_text(&[line0, line1], 8, HAlign::Left, VAlign::Top);
        // Max width is 3
        assert_eq!(result.len(), 3);
        // Line 1 left-aligned: col 0 has line1 data, cols 1-2 have nothing from line1
        let col0 = u16::from_le_bytes(result[0]);
        assert_eq!(col0 & 0xFF, 1); // line0 contribution in low 8 bits
        assert_eq!((col0 >> 8) & 0xFF, 4); // line1 contribution in high 8 bits
    }

    #[test]
    fn layout_two_lines_halign_right() {
        let line0 = vec![1u16, 2u16, 3u16]; // 3 wide
        let line1 = vec![4u16]; // 1 wide
        let result = layout_text(&[line0, line1], 8, HAlign::Right, VAlign::Top);
        assert_eq!(result.len(), 3);
        // Line 1 right-aligned: placed at col 2 (max_width 3 - line_width 1 = 2)
        let col2 = u16::from_le_bytes(result[2]);
        assert_eq!(col2 & 0xFF, 3); // line0[2]
        assert_eq!((col2 >> 8) & 0xFF, 4); // line1[0] shifted to col 2
    }

    fn dump_wide_bitmap(wide_bitmap: &[[u8; 2]]) -> String {
        let mut lines = Vec::new();
        for row in 0..16 {
            let mut line = String::new();
            for col_bytes in wide_bitmap {
                let col = u16::from_le_bytes(*col_bytes);
                line.push(if (col >> row) & 1 == 1 { '#' } else { '.' });
            }
            lines.push(line);
        }
        lines.join("\n")
    }

    // Taller font: ascent=9, descent=2 (height=11), simulating creep-like metrics.
    // Glyph is still 4x6, but positioned higher in the taller line height.
    const TALL_BDF: &[u8] = b"STARTFONT 2.1
FONT -Tall
SIZE 11 72 72
FONTBOUNDINGBOX 4 11 0 -2
STARTPROPERTIES 2
FONT_ASCENT 9
FONT_DESCENT 2
ENDPROPERTIES
CHARS 1
STARTCHAR A
ENCODING 65
SWIDTH 400 0
DWIDTH 4 0
BBX 4 6 0 0
BITMAP
F0
90
F0
90
90
90
ENDCHAR
ENDFONT
";

    fn parse_tall_font() -> BdfFont {
        BdfFont::parse(TALL_BDF).expect("failed to parse tall BDF font")
    }

    #[test]
    fn bdf_tall_font_full_height() {
        let font = parse_tall_font();
        let glyph = rasterize_glyph_bdf(&font, 'A', 16);

        let visual = dump_columns(&glyph.columns, 16);
        eprintln!("tall font, target=16:\n{}", visual);

        let total_bits: u32 = glyph.columns.iter().map(|c| c.count_ones()).sum();
        assert!(total_bits > 0, "glyph should be visible at full height");
    }

    #[test]
    fn bdf_tall_font_half_height() {
        // Simulates 2-line mode: target_height = 8, but font_height = 11
        let font = parse_tall_font();
        let glyph = rasterize_glyph_bdf(&font, 'A', 8);

        let visual = dump_columns(&glyph.columns, 8);
        eprintln!("tall font, target=8:\n{}", visual);

        let total_bits: u32 = glyph.columns.iter().map(|c| c.count_ones()).sum();
        assert!(total_bits > 0, "glyph should still be visible with repositioning");
        // Should show more than just 2-3 rows of the glyph
        assert!(total_bits >= 8, "glyph should show most of its rows, got {} bits", total_bits);
    }

    #[test]
    fn bdf_multiline_two_lines() {
        let font = parse_test_font();

        let line0 = rasterize_line_bdf(&font, "A", 8);
        let line1 = rasterize_line_bdf(&font, "A", 8);
        let composed = layout_text(&[line0, line1], 8, HAlign::Center, VAlign::Center);

        let visual = dump_wide_bitmap(&composed);
        eprintln!("two lines 'A\\nA':\n{}", visual);

        // Should have 4 columns (both lines same width)
        assert_eq!(composed.len(), 4);
        // Both halves (rows 0-7 and 8-15) should have pixels
        let top_bits: u32 = composed.iter()
            .map(|c| (u16::from_le_bytes(*c) & 0x00FF).count_ones())
            .sum();
        let bottom_bits: u32 = composed.iter()
            .map(|c| (u16::from_le_bytes(*c) >> 8).count_ones())
            .sum();
        assert!(top_bits > 0, "top line should have pixels");
        assert!(bottom_bits > 0, "bottom line should have pixels");
    }
}
