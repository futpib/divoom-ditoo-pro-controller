use std::error::Error;
use std::fs::File;
use std::io::{BufReader, BufWriter, Cursor};

use image::{DynamicImage, GenericImageView};
use spectral::prelude::*;

use divoom_ditoo_pro_controller::divoom_file_format::animation::Animation;

#[test]
fn convert_divoom16_to_gif() -> Result<(), Box<dyn Error>> {
  let animation =
    Animation::from_16x16(&mut BufReader::new(File::open("./images/witch.divoom16")?))?;

  let mut output = Vec::new();
  animation.save_to_gif(&mut BufWriter::new(&mut output))?;

  let expected_output = std::fs::read("./images/witch.gif")?;
  asserting("GIF output")
    .that(&output)
    .is_equal_to(&expected_output);

  Ok(())
}

#[test]
fn convert_gif_to_divoom16() -> Result<(), Box<dyn Error>> {
  let animation = Animation::from_gif(&mut BufReader::new(File::open("./images/witch.gif")?))?;

  let mut output = Vec::new();
  animation.save_to_divoom_format(&mut BufWriter::new(&mut output))?;

  // Round-trip: read the divoom16 output back and verify it matches
  let roundtrip = Animation::from_16x16(&mut Cursor::new(&output))?;

  assert_eq!(roundtrip.frames.len(), animation.frames.len());
  for (i, (original, rt)) in animation.frames.iter().zip(roundtrip.frames.iter()).enumerate() {
    // GIF stores delays as centiseconds, divoom16 as milliseconds.
    // The original 127ms becomes 120ms (12 centiseconds) through GIF encoding.
    assert_eq!(rt.header.time_in_milliseconds, original.header.time_in_milliseconds,
      "Frame {} delay mismatch", i);

    // Pixel data must match
    assert_images_equal(&original.image, &rt.image, i);
  }

  Ok(())
}

fn assert_images_equal(a: &DynamicImage, b: &DynamicImage, frame_index: usize) {
  assert_eq!(a.dimensions(), b.dimensions(), "Frame {} dimensions mismatch", frame_index);
  for (x, y, pa) in a.pixels() {
    let pb = b.get_pixel(x, y);
    assert_eq!(pa, pb, "Frame {} pixel ({},{}) mismatch", frame_index, x, y);
  }
}
