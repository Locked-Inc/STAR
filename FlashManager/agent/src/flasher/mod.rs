use anyhow::Result;

pub mod esp32;
pub mod pi5;
pub mod android;

pub use esp32::ESP32Flasher;
pub use pi5::Pi5Flasher;
pub use android::AndroidFlasher;

#[derive(Debug)]
pub enum FlashResult {
    Success,
    Failure(anyhow::Error),
}

pub trait Flasher {
    fn flash(&self, binary_path: &str, target_device: &str) -> Result<FlashResult>;
}
