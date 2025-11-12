use anyhow::Result;
use clap::Parser;
use log::{error, info};
use std::time::Duration;
use tokio::time::sleep;

mod client;
mod config;
mod flasher;

use client::FlashManagerClient;
use config::Config;
use flasher::{FlashResult, Flasher};

#[derive(Parser, Debug)]
#[command(name = "flashmanager-agent")]
#[command(about = "FlashManager Agent - Local hardware flashing agent", long_about = None)]
struct Args {
    /// API key for device authentication
    #[arg(short, long)]
    api_key: String,

    /// Backend server URL
    #[arg(short, long, default_value = "http://localhost:8081")]
    server_url: String,

    /// Polling interval in seconds
    #[arg(short, long, default_value = "5")]
    poll_interval: u64,

    /// Device name
    #[arg(short, long)]
    name: Option<String>,
}

#[tokio::main]
async fn main() -> Result<()> {
    env_logger::init();

    let args = Args::parse();

    info!("FlashManager Agent starting...");
    info!("Server URL: {}", args.server_url);
    info!("Poll interval: {}s", args.poll_interval);

    let config = Config {
        server_url: args.server_url.clone(),
        api_key: args.api_key.clone(),
        poll_interval_secs: args.poll_interval,
    };

    let mut client = FlashManagerClient::new(config)?;

    // Authenticate device
    info!("Authenticating device...");
    client.login().await?;
    info!("Device authenticated successfully");

    // Start polling loop
    info!("Starting polling loop...");
    loop {
        match poll_and_process(&mut client).await {
            Ok(processed) => {
                if processed > 0 {
                    info!("Processed {} flash job(s)", processed);
                }
            }
            Err(e) => {
                error!("Error in polling loop: {}", e);
            }
        }

        // Send heartbeat
        if let Err(e) = client.heartbeat().await {
            error!("Failed to send heartbeat: {}", e);
        }

        sleep(Duration::from_secs(config.poll_interval_secs)).await;
    }
}

async fn poll_and_process(client: &mut FlashManagerClient) -> Result<usize> {
    // Poll for pending flash jobs
    let jobs = client.poll_flash_jobs().await?;

    if jobs.is_empty() {
        return Ok(0);
    }

    info!("Found {} pending flash job(s)", jobs.len());

    let mut processed = 0;

    for job in jobs {
        info!("Processing flash job: {}", job.id);

        // Claim the job
        match client.claim_flash_job(&job.id).await {
            Ok(_) => {
                info!("Claimed flash job: {}", job.id);
            }
            Err(e) => {
                error!("Failed to claim flash job {}: {}", job.id, e);
                continue;
            }
        }

        // Update status to RUNNING
        if let Err(e) = client.update_flash_status(&job.id, "RUNNING").await {
            error!("Failed to update status to RUNNING: {}", e);
            continue;
        }

        // Determine component type and flash
        let start_time = std::time::Instant::now();
        let result = match job.component.as_str() {
            "ESP32_FIRMWARE" => {
                info!("Flashing ESP32 firmware...");
                flash_esp32(&job)
            }
            "PI5_OS" => {
                info!("Flashing Pi5 OS...");
                flash_pi5(&job)
            }
            "ANDROID_APP" => {
                info!("Flashing Android app...");
                flash_android(&job)
            }
            _ => {
                error!("Unsupported component: {}", job.component);
                Err(anyhow::anyhow!("Unsupported component"))
            }
        };

        let duration = start_time.elapsed().as_millis() as i64;

        // Report result
        match result {
            Ok(FlashResult::Success) => {
                info!("Flash completed successfully in {}ms", duration);
                if let Err(e) = client
                    .complete_flash_job(&job.id, true, duration, None)
                    .await
                {
                    error!("Failed to report success: {}", e);
                }
            }
            Ok(FlashResult::Failure(msg)) | Err(msg) => {
                error!("Flash failed: {}", msg);
                let error_message = msg.to_string();
                if let Err(e) = client
                    .complete_flash_job(&job.id, false, duration, Some(&error_message))
                    .await
                {
                    error!("Failed to report failure: {}", e);
                }
            }
        }

        processed += 1;
    }

    Ok(processed)
}

fn flash_esp32(job: &client::FlashJob) -> Result<FlashResult> {
    let flasher = flasher::ESP32Flasher::new();
    flasher.flash(&job.binary_path, &job.target_device)
}

fn flash_pi5(job: &client::FlashJob) -> Result<FlashResult> {
    let flasher = flasher::Pi5Flasher::new();
    flasher.flash(&job.binary_path, &job.target_device)
}

fn flash_android(job: &client::FlashJob) -> Result<FlashResult> {
    let flasher = flasher::AndroidFlasher::new();
    flasher.flash(&job.binary_path, &job.target_device)
}
