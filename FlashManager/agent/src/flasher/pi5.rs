use anyhow::{Context, Result};
use log::{debug, info, warn};
use std::process::Command;

use super::{FlashResult, Flasher};

pub struct Pi5Flasher;

impl Pi5Flasher {
    pub fn new() -> Self {
        Self
    }

    fn check_dd(&self) -> Result<()> {
        let output = Command::new("dd")
            .arg("--version")
            .output()
            .context("Failed to execute dd - is it installed?")?;

        if !output.status.success() {
            anyhow::bail!("dd is not working properly");
        }

        debug!("dd command found");
        Ok(())
    }

    fn check_device_writable(&self, device: &str) -> Result<()> {
        // Check if device exists
        if !std::path::Path::new(device).exists() {
            anyhow::bail!("Target device does not exist: {}", device);
        }

        // Check if we can write to the device (requires sudo/root)
        let test_output = Command::new("test")
            .args(["-w", device])
            .output()
            .context("Failed to check device write permissions")?;

        if !test_output.status.success() {
            warn!("Device may not be writable without sudo: {}", device);
        }

        Ok(())
    }
}

impl Flasher for Pi5Flasher {
    fn flash(&self, binary_path: &str, target_device: &str) -> Result<FlashResult> {
        info!("Starting Raspberry Pi 5 flash process");
        info!("Image: {}", binary_path);
        info!("Target device: {}", target_device);

        // Check if dd is available
        self.check_dd()?;

        // Check if binary exists
        if !std::path::Path::new(binary_path).exists() {
            return Ok(FlashResult::Failure(anyhow::anyhow!(
                "Image file not found: {}",
                binary_path
            )));
        }

        // Check device writability
        if let Err(e) = self.check_device_writable(target_device) {
            return Ok(FlashResult::Failure(e));
        }

        // Unmount the device if it's mounted
        info!("Unmounting {}", target_device);
        let _ = Command::new("umount")
            .arg(target_device)
            .output();

        // Flash the image using dd
        info!("Writing image to {} (this may take several minutes)", target_device);

        // Use sudo for dd if not running as root
        let dd_output = Command::new("sudo")
            .args([
                "dd",
                &format!("if={}", binary_path),
                &format!("of={}", target_device),
                "bs=4M",
                "status=progress",
                "conv=fsync",
            ])
            .output()
            .context("Failed to execute dd command")?;

        if !dd_output.status.success() {
            let stderr = String::from_utf8_lossy(&dd_output.stderr);
            return Ok(FlashResult::Failure(anyhow::anyhow!(
                "dd command failed: {}",
                stderr
            )));
        }

        // Sync to ensure all data is written
        info!("Syncing data...");
        Command::new("sync")
            .output()
            .context("Failed to sync")?;

        info!("Raspberry Pi 5 flash completed successfully");
        info!("You can safely remove the SD card/USB drive");

        Ok(FlashResult::Success)
    }
}
