// Mock BMS Device - PTY-based serial port simulator
// STAR Project - Texas A&M University

use app_lib::{read_frame_from_buffer, Frame, FRAME_TYPE_REQUEST, FRAME_TYPE_RESPONSE};
use prost::Message;
use star_proto::*;
use std::fs::OpenOptions;
use std::io::{Read, Write};
use std::os::unix::fs::OpenOptionsExt;
use std::time::Duration;

const MOCK_MANUFACTURER: &str = "Texas Instruments";
const MOCK_DEVICE_NAME: &str = "BQ78350-R1A";
const MOCK_CHEMISTRY: &str = "LION";
const MOCK_SERIAL: u32 = 0x12345678;
const MOCK_FW_VERSION: &str = "v1.2.3";
const MOCK_HW_VERSION: &str = "v0.1";

fn create_mock_telemetry() -> BmsTelemetryData {
    BmsTelemetryData {
        voltage_mv: 14800,            // 14.8V (4S pack at 3.7V/cell)
        current_ma: -1500,            // -1.5A (discharging)
        average_current_ma: -1200,    // -1.2A average
        relative_soc_percent: 75,     // 75% SOC
        absolute_soc_percent: 73,     // 73% absolute
        temperature_celsius: 25,      // 25°C
        remaining_capacity_mah: 2250, // 2.25Ah remaining
        full_capacity_mah: 3000,      // 3Ah full capacity
        design_capacity_mah: 3200,    // 3.2Ah design
        cycle_count: 42,              // 42 cycles
        time_to_empty_min: 90,        // 90 minutes to empty
        time_to_full_min: 0xFFFF,     // Not charging
        is_charging: false,
        is_fully_charged: false,
        is_fully_discharged: false,
        is_low_capacity: false,
    }
}

fn create_mock_cell_voltages(num_cells: u32) -> BmsCellVoltagesData {
    let num = num_cells.min(16) as usize;
    let cell_voltages: Vec<u32> = (0..num)
        .map(|i| 3700 + (i as u32 * 10)) // 3.70V, 3.71V, 3.72V, etc.
        .collect();

    let min = *cell_voltages.iter().min().unwrap();
    let max = *cell_voltages.iter().max().unwrap();
    let pack: u32 = cell_voltages.iter().sum();

    BmsCellVoltagesData {
        cell_mv: cell_voltages,
        pack_mv: pack,
        min_cell_mv: min,
        max_cell_mv: max,
        delta_mv: max - min,
    }
}

fn create_mock_device_info() -> BmsDeviceInfoData {
    BmsDeviceInfoData {
        manufacturer: MOCK_MANUFACTURER.to_string(),
        device_name: MOCK_DEVICE_NAME.to_string(),
        chemistry: MOCK_CHEMISTRY.to_string(),
        serial_number: MOCK_SERIAL,
        firmware_version: MOCK_FW_VERSION.to_string(),
        hardware_version: MOCK_HW_VERSION.to_string(),
        design_capacity_mah: 3200,
        design_voltage_mv: 14800,
        num_cells: 4,
    }
}

fn create_mock_register(address: u32, num_bytes: u32) -> BmsRegisterData {
    // Simulate some realistic register values
    let value = match address {
        0x00 => 0x0350,      // Voltage register
        0x02 => 0xFFF0,      // Current register (negative)
        0x04 => 0x4B,        // SOC register
        0x06 => 0x0C80,      // Capacity register
        0x08 => 0x0019,      // Temperature register
        _ => address * 0x11, // Some pattern for unknown registers
    };

    BmsRegisterData {
        address,
        value,
        num_bytes,
    }
}

fn create_mock_protection_status() -> BmsProtectionStatusData {
    BmsProtectionStatusData {
        cell_overvoltage: false,
        pack_overvoltage: false,
        cell_undervoltage: false,
        pack_undervoltage: false,
        charge_overcurrent: false,
        discharge_overcurrent: false,
        short_circuit: false,
        overtemperature_charge: false,
        overtemperature_discharge: false,
        undertemperature_charge: false,
        undertemperature_discharge: false,
        cell_balancing_active: true, // Cell balancing is active
        permanent_failure: false,
        safety_status_alert: false,
    }
}

fn create_mock_manufacturer_access(subcommand: u32, data: &[u8]) -> BmsManufacturerAccessData {
    // Simulate common BQ4050/BQ78350 ManufacturerAccess sub-commands
    let response_data = match subcommand {
        0x0001 => {
            // Device Type
            vec![0x50, 0x40] // BQ4050 device ID
        }
        0x0002 => {
            // Firmware Version
            vec![0x01, 0x02, 0x03] // v1.2.3
        }
        0x0003 => {
            // Hardware Version
            vec![0x00, 0x01] // v0.1
        }
        0x0021 => {
            // IT Status (Impedance Track)
            vec![0x01] // IT enabled
        }
        0x0023 => {
            // FET Status
            vec![0x03] // Both CHG and DSG FETs enabled
        }
        0x0024 => {
            // CHG FET Control (write command)
            if !data.is_empty() {
                println!(
                    "[Mock] CHG FET {} command received",
                    if data[0] == 0x01 { "enable" } else { "disable" }
                );
            }
            vec![0x00] // ACK
        }
        0x0025 => {
            // DSG FET Control (write command)
            if !data.is_empty() {
                println!(
                    "[Mock] DSG FET {} command received",
                    if data[0] == 0x01 { "enable" } else { "disable" }
                );
            }
            vec![0x00] // ACK
        }
        0x0026 => {
            // Cell Balancing Control (write command)
            if !data.is_empty() {
                println!(
                    "[Mock] Cell Balancing {} command received",
                    if data[0] == 0x01 { "enable" } else { "disable" }
                );
            }
            vec![0x00] // ACK
        }
        0x0070 => {
            // Safety Status
            vec![0x00, 0x00] // No safety alerts
        }
        0x0071 => {
            // PF Status (Permanent Failure)
            vec![0x00, 0x00] // No permanent failures
        }
        _ => {
            // Unknown sub-command, return generic response
            vec![0x00, 0x00]
        }
    };

    BmsManufacturerAccessData {
        subcommand,
        data: response_data,
    }
}

fn handle_request(request: &BmsCommandRequest) -> Result<BmsCommandResponse, String> {
    println!(
        "[Mock] Processing request: {:?}",
        request.command.as_ref().map(|c| match c {
            bms_command_request::Command::ReadTelemetry(_) => "ReadTelemetry",
            bms_command_request::Command::ReadCellVoltages(_) => "ReadCellVoltages",
            bms_command_request::Command::ReadDeviceInfo(_) => "ReadDeviceInfo",
            bms_command_request::Command::ReadRegister(_) => "ReadRegister",
            bms_command_request::Command::WriteRegister(_) => "WriteRegister",
            bms_command_request::Command::ReadBlock(_) => "ReadBlock",
            bms_command_request::Command::WriteBlock(_) => "WriteBlock",
            bms_command_request::Command::ReadProtectionStatus(_) => "ReadProtectionStatus",
            bms_command_request::Command::ManufacturerAccess(_) => "ManufacturerAccess",
            _ => "Unknown",
        })
    );

    let response = match &request.command {
        Some(bms_command_request::Command::ReadTelemetry(_)) => {
            bms_command_response::Response::TelemetryData(create_mock_telemetry())
        }
        Some(bms_command_request::Command::ReadCellVoltages(cmd)) => {
            bms_command_response::Response::CellVoltagesData(create_mock_cell_voltages(
                cmd.num_cells,
            ))
        }
        Some(bms_command_request::Command::ReadDeviceInfo(_)) => {
            bms_command_response::Response::DeviceInfoData(create_mock_device_info())
        }
        Some(bms_command_request::Command::ReadRegister(cmd)) => {
            bms_command_response::Response::RegisterData(create_mock_register(
                cmd.address,
                cmd.num_bytes,
            ))
        }
        Some(bms_command_request::Command::WriteRegister(_)) => {
            bms_command_response::Response::AckData(BmsAckData {
                success: true,
                message: "Mock write successful".to_string(),
            })
        }
        Some(bms_command_request::Command::ReadBlock(cmd)) => {
            // Mock Data Flash read - return 32 bytes of sample data
            let mut data = vec![0u8; cmd.max_length.min(32) as usize];
            // Fill with some pattern data
            for (i, byte) in data.iter_mut().enumerate() {
                *byte = ((cmd.address + i as u32) & 0xFF) as u8;
            }
            bms_command_response::Response::BlockData(BmsBlockData {
                address: cmd.address,
                data,
            })
        }
        Some(bms_command_request::Command::WriteBlock(cmd)) => {
            println!(
                "[Mock] WriteBlock: address=0x{:02X}, {} bytes",
                cmd.address,
                cmd.data.len()
            );
            bms_command_response::Response::AckData(BmsAckData {
                success: true,
                message: format!("Mock block write successful ({} bytes)", cmd.data.len()),
            })
        }
        Some(bms_command_request::Command::ReadProtectionStatus(_)) => {
            bms_command_response::Response::ProtectionStatusData(create_mock_protection_status())
        }
        Some(bms_command_request::Command::ManufacturerAccess(cmd)) => {
            bms_command_response::Response::ManufacturerAccessData(create_mock_manufacturer_access(
                cmd.subcommand,
                &cmd.data,
            ))
        }
        _ => {
            return Err("Unknown command".to_string());
        }
    };

    Ok(BmsCommandResponse {
        header: Some(ResponseHeader {
            request_id: request
                .header
                .as_ref()
                .map(|h| h.request_id.clone())
                .unwrap_or_default(),
            status: Status::Ok as i32,
            error_message: String::new(),
            server_timestamp: None,
            latency_us: 1000, // 1ms mock latency
        }),
        response: Some(response),
    })
}

fn run_mock_device(port_name: &str) -> Result<(), Box<dyn std::error::Error>> {
    println!("Mock BMS Device starting...");
    println!("Opening port: {}", port_name);

    // For PTY devices, use raw file I/O instead of serialport
    // PTYs don't support serial port ioctls (baud rate, parity, etc.)
    let mut port = OpenOptions::new()
        .read(true)
        .write(true)
        .custom_flags(libc::O_RDWR | libc::O_NOCTTY | libc::O_NONBLOCK)
        .open(port_name)?;

    println!("Mock device ready on {}", port_name);
    println!("Waiting for connections...\n");

    let mut read_buffer = Vec::with_capacity(4096);

    loop {
        // Read incoming data
        let mut chunk = [0u8; 256];
        match port.read(&mut chunk) {
            Ok(n) if n > 0 => {
                read_buffer.extend_from_slice(&chunk[..n]);
                println!("[Mock] Received {} bytes", n);

                // Try to decode a frame
                let (frame_opt, bytes_consumed, error_opt) = read_frame_from_buffer(&read_buffer);

                if bytes_consumed > 0 {
                    read_buffer.drain(..bytes_consumed);
                }

                if let Some(err) = error_opt {
                    eprintln!("[Mock] Frame error: {}", err);
                    continue;
                }

                if let Some(frame) = frame_opt {
                    println!(
                        "[Mock] Decoded frame: seq={} type=0x{:02X} payload_len={}",
                        frame.sequence,
                        frame.frame_type,
                        frame.payload.len()
                    );

                    if frame.frame_type == FRAME_TYPE_REQUEST {
                        // Decode protobuf request
                        match BmsCommandRequest::decode(frame.payload.as_slice()) {
                            Ok(request) => {
                                // Handle request and generate response
                                match handle_request(&request) {
                                    Ok(response) => {
                                        // Encode response protobuf
                                        let mut response_payload = Vec::new();
                                        if let Err(e) = response.encode(&mut response_payload) {
                                            eprintln!("[Mock] Failed to encode response: {}", e);
                                            continue;
                                        }

                                        // Encode response frame
                                        match Frame::encode(
                                            frame.sequence,
                                            FRAME_TYPE_RESPONSE,
                                            0,
                                            &response_payload,
                                        ) {
                                            Ok(response_frame) => {
                                                let len = response_frame.len();
                                                println!(
                                                    "[Mock] Sending response: seq={} len={}",
                                                    frame.sequence, len
                                                );

                                                if let Err(e) = port.write_all(&response_frame) {
                                                    eprintln!(
                                                        "[Mock] Failed to write response: {}",
                                                        e
                                                    );
                                                } else if let Err(e) = port.flush() {
                                                    eprintln!("[Mock] Failed to flush: {}", e);
                                                }
                                            }
                                            Err(e) => {
                                                eprintln!("[Mock] Failed to encode frame: {}", e)
                                            }
                                        }
                                    }
                                    Err(e) => eprintln!("[Mock] Failed to handle request: {}", e),
                                }
                            }
                            Err(e) => eprintln!("[Mock] Failed to decode request: {}", e),
                        }
                    }
                }
            }
            Ok(_) => {
                // No data, continue
                std::thread::sleep(Duration::from_millis(10));
            }
            Err(ref e) if e.kind() == std::io::ErrorKind::TimedOut => {
                // Timeout is normal, continue
            }
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                // WouldBlock is normal for non-blocking I/O, continue
            }
            Err(e) => {
                eprintln!("[Mock] Read error: {}", e);
                std::thread::sleep(Duration::from_millis(100));
            }
        }
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();

    if args.len() < 2 {
        eprintln!("Usage: {} <serial_port>", args[0]);
        eprintln!("\nExamples:");
        eprintln!("  macOS PTY:  {} /dev/ttys000", args[0]);
        eprintln!("  Linux PTY:  {} /dev/pts/5", args[0]);
        eprintln!("  Windows:    {} COM3", args[0]);
        eprintln!("\nTo create a PTY pair on macOS/Linux:");
        eprintln!("  socat -d -d pty,raw,echo=0 pty,raw,echo=0");
        eprintln!("  Then use one of the PTY paths shown");
        std::process::exit(1);
    }

    let port_name = &args[1];

    if let Err(e) = run_mock_device(port_name) {
        eprintln!("Mock device error: {}", e);
        std::process::exit(1);
    }
}
