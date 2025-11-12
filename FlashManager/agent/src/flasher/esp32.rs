use anyhow::{Context, Result};
use log::{debug, info};
use std::process::Command;

use super::{FlashResult, Flasher};

pub struct ESP32Flasher;

impl ESP32Flasher {
    pub fn new() -> Self {
        Self
    }

    fn check_esptool(&self) -> Result<()> {
        let output = Command::new("esptool.py")
            .arg("--version")
            .output()
            .context("Failed to execute esptool.py - is it installed?")?;

        if !output.status.success() {
            anyhow::bail!("esptool.py is not working properly");
        }

        debug!("esptool.py found");
        Ok(())
    }
}

impl Flasher for ESP32Flasher {
    fn flash(&self, binary_path: &str, target_device: &str) -> Result<FlashResult> {
        info!("Starting ESP32 flash process");
        info!("Binary: {}", binary_path);
        info!("Target device: {}", target_device);

        // Check if esptool is available
        self.check_esptool()?;

        // Check if binary exists
        if !std::path::Path::new(binary_path).exists() {
            return Ok(FlashResult::Failure(anyhow::anyhow!(
                "Binary file not found: {}",
                binary_path
            )));
        }

        // Erase flash
        info!("Erasing flash on {}", target_device);
        let erase_output = Command::new("esptool.py")
            .args([
                "--chip", "esp32",
                "--port", target_device,
                "erase_flash",
            ])
            .output()
            .context("Failed to erase flash")?;

        if !erase_output.status.success() {
            let stderr = String::from_utf8_lossy(&erase_output.stderr);
            return Ok(FlashResult::Failure(anyhow::anyhow!(
                "Flash erase failed: {}",
                stderr
            )));
        }

        // Flash binary
        info!("Flashing binary to {}", target_device);
        let flash_output = Command::new("esptool.py")
            .args([
                "--chip", "esp32",
                "--port", target_device,
                "--baud", "460800",
                "write_flash",
                "-z",
                "0x0",
                binary_path,
            ])
            .output()
            .context("Failed to flash binary")?;

        if !flash_output.status.success() {
            let stderr = String::from_utf8_lossy(&flash_output.stderr);
            return Ok(FlashResult::Failure(anyhow::anyhow!(
                "Flash write failed: {}",
                stderr
            )));
        }

        info!("ESP32 flash completed successfully");
        Ok(FlashResult::Success)
    }
}
