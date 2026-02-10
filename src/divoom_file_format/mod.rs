use image::{DynamicImage, GenericImageView, Rgb};
use indexmap::IndexSet;

pub mod animation;
pub mod frame;
pub mod frame_header;

pub(crate) fn prepare_image(image: &DynamicImage) -> DynamicImage {
  image.resize_exact(16, 16, image::imageops::FilterType::Lanczos3)
}

fn get_palette_from_images(images: &[DynamicImage]) -> IndexSet<Rgb<u8>> {
  images
    .iter()
    .flat_map(|image| {
      image
        .pixels()
        .map(|(_x, _y, pixel_data)| Rgb([pixel_data[0], pixel_data[1], pixel_data[2]]))
        .collect::<IndexSet<_>>()
    })
    .collect()
}
