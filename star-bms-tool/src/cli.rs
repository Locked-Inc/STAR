// CLI mode for BMS tool
// STAR Project - Texas A&M University

use crate::bms::BmsApp;
use anyhow::{Context, Result};
use clap::{Parser, Subcommand};
use star_proto::*;
use std::time::Duration;

const DEFAULT_TIMEOUT: Duration = Duration::from_secs(2);

#[derive(Parser)]
#[command(name = "star-bms-tool")]
#[command(about = "STAR BMS Evaluation Tool - CLI Mode", long_about = None)]
pub struct Cli {
    /// Serial port to connect to (e.g., /dev/ttyUSB0, COM3, or /dev/pts/X for PTY mock)
    #[arg(short, long)]
    pub port: String,

    #[command(subcommand)]
    pub command: Commands,
}

#[derive(Subcommand)]
pub enum Commands {
    /// Read telemetry data from BMS
    Telemetry,

    /// Read cell voltages
    CellVoltages {
        /// Number of cells to read
        #[arg(default_value_t = 16)]
        num_cells: u32,
    },

    /// Read device information
    DeviceInfo,

    /// Read a register at the specified address
    ReadRegister {
        /// Register address (hex or decimal)
        #[arg(value_parser = parse_hex_or_dec)]
        address: u32,

        /// Number of bytes to read
        #[arg(default_value_t = 1)]
        num_bytes: u32,
    },

    /// Write a value to a register
    WriteRegister {
        /// Register address (hex or decimal)
        #[arg(value_parser = parse_hex_or_dec)]
        address: u32,

        /// Value to write (hex or decimal)
        #[arg(value_parser = parse_hex_or_dec)]
        value: u32,

        /// Number of bytes to write
        #[arg(default_value_t = 1)]
        num_bytes: u32,
    },

    /// Run interactive shell mode
    Shell,
}

fn parse_hex_or_dec(s: &str) -> Result<u32, std::num::ParseIntError> {
    if let Some(hex_str) = s.strip_prefix("0x") {
        u32::from_str_radix(hex_str, 16)
    } else {
        s.parse::<u32>()
    }
}

pub fn run_cli(cli: Cli) -> Result<()> {
    let app = BmsApp::new();

    // Connect to device
    println!("Connecting to {}...", cli.port);
    let port = serialport::new(&cli.port, 115_200)
        .timeout(Duration::from_millis(2000))
        .data_bits(serialport::DataBits::Eight)
        .parity(serialport::Parity::None)
        .stop_bits(serialport::StopBits::One)
        .open()
        .context(format!("Failed to open port {}", cli.port))?;

    *app.port.lock().unwrap() = Some(port);
    println!("Connected successfully!\n");

    // Execute command
    match cli.command {
        Commands::Telemetry => {
            let req = BmsCommandRequest {
                header: Some(create_request_header("telemetry-cli")),
                command: Some(bms_command_request::Command::ReadTelemetry(
                    ReadTelemetryCommand {},
                )),
            };

            let resp = app
                .send_request(&req, DEFAULT_TIMEOUT)
                .context("Failed to read telemetry")?;

            match resp.response {
                Some(bms_command_response::Response::TelemetryData(data)) => {
                    print_telemetry(&data);
                }
                _ => anyhow::bail!("Unexpected response type"),
            }
        }

        Commands::CellVoltages { num_cells } => {
            let req = BmsCommandRequest {
                header: Some(create_request_header("cells-cli")),
                command: Some(bms_command_request::Command::ReadCellVoltages(
                    ReadCellVoltagesCommand { num_cells },
                )),
            };

            let resp = app
                .send_request(&req, DEFAULT_TIMEOUT)
                .context("Failed to read cell voltages")?;

            match resp.response {
                Some(bms_command_response::Response::CellVoltagesData(data)) => {
                    print_cell_voltages(&data);
                }
                _ => anyhow::bail!("Unexpected response type"),
            }
        }

        Commands::DeviceInfo => {
            let req = BmsCommandRequest {
                header: Some(create_request_header("info-cli")),
                command: Some(bms_command_request::Command::ReadDeviceInfo(
                    ReadDeviceInfoCommand {},
                )),
            };

            let resp = app
                .send_request(&req, DEFAULT_TIMEOUT)
                .context("Failed to read device info")?;

            match resp.response {
                Some(bms_command_response::Response::DeviceInfoData(data)) => {
                    print_device_info(&data);
                }
                _ => anyhow::bail!("Unexpected response type"),
            }
        }

        Commands::ReadRegister { address, num_bytes } => {
            let req = BmsCommandRequest {
                header: Some(create_request_header("reg-read-cli")),
                command: Some(bms_command_request::Command::ReadRegister(
                    ReadRegisterCommand { address, num_bytes },
                )),
            };

            let resp = app
                .send_request(&req, DEFAULT_TIMEOUT)
                .context("Failed to read register")?;

            match resp.response {
                Some(bms_command_response::Response::RegisterData(data)) => {
                    print_register(&data, address);
                }
                _ => anyhow::bail!("Unexpected response type"),
            }
        }

        Commands::WriteRegister {
            address,
            value,
            num_bytes,
        } => {
            let req = BmsCommandRequest {
                header: Some(create_request_header("reg-write-cli")),
                command: Some(bms_command_request::Command::WriteRegister(
                    WriteRegisterCommand {
                        address,
                        value,
                        num_bytes,
                    },
                )),
            };

            let resp = app
                .send_request(&req, DEFAULT_TIMEOUT)
                .context("Failed to write register")?;

            match resp.response {
                Some(bms_command_response::Response::AckData(ack)) => {
                    if ack.success {
                        println!("✓ Write successful: {}", ack.message);
                    } else {
                        anyhow::bail!("Write failed: {}", ack.message);
                    }
                }
                _ => anyhow::bail!("Unexpected response type"),
            }
        }

        Commands::Shell => {
            println!("Interactive shell mode not yet implemented");
            println!("Use individual commands for now (telemetry, cell-voltages, etc.)");
        }
    }

    Ok(())
}

fn create_request_header(request_id: &str) -> RequestHeader {
    RequestHeader {
        request_id: format!(
            "{}-{}",
            request_id,
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        ),
        client_timestamp: None,
        client_version: env!("CARGO_PKG_VERSION").to_string(),
        timeout: None,
    }
}

fn print_telemetry(data: &BmsTelemetryData) {
    println!("BMS Telemetry:");
    println!(
        "  Voltage:         {:.2} V",
        data.voltage_mv as f64 / 1000.0
    );
    println!(
        "  Current:         {:.2} A",
        data.current_ma as f64 / 1000.0
    );
    println!(
        "  Avg Current:     {:.2} A",
        data.average_current_ma as f64 / 1000.0
    );
    println!("  SOC (Relative):  {}%", data.relative_soc_percent);
    println!("  SOC (Absolute):  {}%", data.absolute_soc_percent);
    println!("  Temperature:     {} °C", data.temperature_celsius);
    println!(
        "  Remaining:       {:.2} Ah",
        data.remaining_capacity_mah as f64 / 1000.0
    );
    println!(
        "  Full Capacity:   {:.2} Ah",
        data.full_capacity_mah as f64 / 1000.0
    );
    println!("  Cycle Count:     {}", data.cycle_count);
}

fn print_cell_voltages(data: &BmsCellVoltagesData) {
    println!("Cell Voltages:");
    println!("  Pack:  {:.2} V", data.pack_mv as f64 / 1000.0);
    println!("  Min:   {:.3} V", data.min_cell_mv as f64 / 1000.0);
    println!("  Max:   {:.3} V", data.max_cell_mv as f64 / 1000.0);
    println!("  Delta: {:.3} V", data.delta_mv as f64 / 1000.0);
    println!("\n  Individual cells:");
    for (i, &voltage_mv) in data.cell_mv.iter().enumerate() {
        println!("    Cell {:2}: {:.3} V", i + 1, voltage_mv as f64 / 1000.0);
    }
}

fn print_device_info(data: &BmsDeviceInfoData) {
    println!("Device Information:");
    println!("  Manufacturer: {}", data.manufacturer);
    println!("  Device Name:  {}", data.device_name);
    println!("  Chemistry:    {}", data.chemistry);
    println!("  Serial:       0x{:08X}", data.serial_number);
    println!("  Firmware:     {}", data.firmware_version);
    println!("  Hardware:     {}", data.hardware_version);
    println!("  Cells:        {}", data.num_cells);
    println!(
        "  Capacity:     {:.2} Ah",
        data.design_capacity_mah as f64 / 1000.0
    );
    println!(
        "  Voltage:      {:.2} V",
        data.design_voltage_mv as f64 / 1000.0
    );
}

fn print_register(data: &BmsRegisterData, _address: u32) {
    println!("Register 0x{:04X}:", data.address);
    match data.num_bytes {
        1 => {
            let val = (data.value & 0xFF) as u8;
            println!("  Value: 0x{:02X} ({})", val, val);
        }
        2 => {
            let val = (data.value & 0xFFFF) as u16;
            println!("  Value: 0x{:04X} ({})", val, val);
        }
        4 => {
            println!("  Value: 0x{:08X} ({})", data.value, data.value);
        }
        _ => println!("  Value: 0x{:08X} ({} bytes)", data.value, data.num_bytes),
    }
}
