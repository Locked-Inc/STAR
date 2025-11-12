use anyhow::{Context, Result};
use log::{debug, info};
use std::process::Command;

use super::{FlashResult, Flasher};

pub struct AndroidFlasher;

impl AndroidFlasher {
    pub fn new() -> Self {
        Self
    }

    fn check_adb(&self) -> Result<()> {
        let output = Command::new("adb")
            .arg("version")
            .output()
            .context("Failed to execute adb - is it installed?")?;

        if !output.status.success() {
            anyhow::bail!("adb is not working properly");
        }

        debug!("adb found");
        Ok(())
    }

    fn check_device_connected(&self, device_id: &str) -> Result<()> {
        let output = Command::new("adb")
            .args(["devices"])
            .output()
            .context("Failed to list adb devices")?;

        if !output.status.success() {
            anyhow::bail!("Failed to list devices");
        }

        let stdout = String::from_utf8_lossy(&output.stdout);

        // Check if device is in the list
        if device_id == "any" {
            // Check if any device is connected
            let device_count = stdout.lines().skip(1).filter(|line| !line.trim().is_empty()).count();
            if device_count == 0 {
                anyhow::bail!("No Android devices connected");
            }
            info!("Found {} connected device(s)", device_count);
        } else if !stdout.contains(device_id) {
            anyhow::bail!("Device {} not found. Connected devices:\n{}", device_id, stdout);
        } else {
            info!("Device {} found", device_id);
        }

        Ok(())
    }
}

impl Flasher for AndroidFlasher {
    fn flash(&self, binary_path: &str, target_device: &str) -> Result<FlashResult> {
        info!("Starting Android flash process");
        info!("APK: {}", binary_path);
        info!("Target device: {}", target_device);

        // Check if adb is available
        self.check_adb()?;

        // Check if APK exists
        if !std::path::Path::new(binary_path).exists() {
            return Ok(FlashResult::Failure(anyhow::anyhow!(
                "APK file not found: {}",
                binary_path
            )));
        }

        // Check if device is connected
        if let Err(e) = self.check_device_connected(target_device) {
            return Ok(FlashResult::Failure(e));
        }

        // Build adb install command
        let mut install_args = vec!["install", "-r"]; // -r for reinstall

        // Add device specifier if not "any"
        if target_device != "any" {
            install_args.insert(0, "-s");
            install_args.insert(1, target_device);
        }

        install_args.push(binary_path);

        // Install APK
        info!("Installing APK...");
        let install_output = Command::new("adb")
            .args(&install_args)
            .output()
            .context("Failed to install APK")?;

        if !install_output.status.success() {
            let stderr = String::from_utf8_lossy(&install_output.stderr);
            let stdout = String::from_utf8_lossy(&install_output.stdout);
            return Ok(FlashResult::Failure(anyhow::anyhow!(
                "APK installation failed:\nStdout: {}\nStderr: {}",
                stdout,
                stderr
            )));
        }

        let stdout = String::from_utf8_lossy(&install_output.stdout);

        // Check for success message
        if stdout.contains("Success") {
            info!("Android APK installed successfully");
            Ok(FlashResult::Success)
        } else {
            Ok(FlashResult::Failure(anyhow::anyhow!(
                "APK installation did not report success: {}",
                stdout
            )))
        }
    }
}
