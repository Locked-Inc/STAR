// BMS communication module
// STAR Project - Texas A&M University

use crate::frame::{self, Frame, FRAME_TYPE_REQUEST, FRAME_TYPE_RESPONSE};
use crate::packet_capture::{
    request_to_json, response_to_json, PacketCapture, PacketDirection, ParsedPacket, RawPacket,
};
use anyhow::{Context, Result};
use prost::Message;
use serialport::{SerialPort, SerialPortInfo};
use star_proto::*;
use std::collections::{HashMap, VecDeque};
use std::fs::{File, OpenOptions};
use std::io::{Read, Write};
use std::os::unix::fs::OpenOptionsExt;
use std::os::unix::io::AsRawFd;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};
use tauri::State;

fn close_port_async(port: Option<Box<dyn SerialPort>>) {
    if let Some(port) = port {
        std::thread::spawn(move || {
            drop(port);
        });
    }
}

// PTY wrapper that implements SerialPort trait for pseudo-terminal devices
struct PtyPort {
    file: File,
    name: String,
}

impl PtyPort {
    fn open(path: &str) -> std::io::Result<Self> {
        // Open PTY in blocking mode for GUI (non-blocking is only for mock_device)
        let file = OpenOptions::new()
            .read(true)
            .write(true)
            .custom_flags(libc::O_RDWR | libc::O_NOCTTY)
            .open(path)?;

        Ok(Self {
            file,
            name: path.to_string(),
        })
    }
}

const DEV_MOCK_PORT: &str = "DEV:MOCK";

fn create_mock_telemetry() -> BmsTelemetryData {
    BmsTelemetryData {
        voltage_mv: 14800,
        current_ma: -1500,
        average_current_ma: -1200,
        relative_soc_percent: 75,
        absolute_soc_percent: 73,
        temperature_celsius: 25,
        remaining_capacity_mah: 2250,
        full_capacity_mah: 3000,
        design_capacity_mah: 3200,
        cycle_count: 42,
        time_to_empty_min: 90,
        time_to_full_min: 0xFFFF,
        is_charging: false,
        is_fully_charged: false,
        is_fully_discharged: false,
        is_low_capacity: false,
    }
}

fn create_mock_cell_voltages(num_cells: u32) -> BmsCellVoltagesData {
    let count = num_cells.max(1).min(16) as usize;
    let mut cell_mv = Vec::with_capacity(count);
    for i in 0..count {
        cell_mv.push(3700 + (i as u32 * 10));
    }
    let pack_mv: u32 = cell_mv.iter().sum();
    let min_cell_mv = *cell_mv.iter().min().unwrap_or(&0);
    let max_cell_mv = *cell_mv.iter().max().unwrap_or(&0);
    let delta_mv = max_cell_mv.saturating_sub(min_cell_mv);

    BmsCellVoltagesData {
        cell_mv,
        pack_mv,
        min_cell_mv,
        max_cell_mv,
        delta_mv,
    }
}

fn create_mock_device_info() -> BmsDeviceInfoData {
    BmsDeviceInfoData {
        manufacturer: "Texas Instruments".to_string(),
        device_name: "BQ78350-R1A".to_string(),
        chemistry: "LION".to_string(),
        serial_number: 0x12345678,
        firmware_version: "v1.2.3".to_string(),
        hardware_version: "v0.1".to_string(),
        design_capacity_mah: 3200,
        design_voltage_mv: 14800,
        num_cells: 4,
    }
}

fn create_mock_register(address: u32, num_bytes: u32) -> BmsRegisterData {
    BmsRegisterData {
        address,
        value: 0x0350,
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
        cell_balancing_active: true,
        permanent_failure: false,
        safety_status_alert: false,
    }
}

fn create_mock_manufacturer_access(subcommand: u32, data: &[u8]) -> BmsManufacturerAccessData {
    let response_data = match subcommand {
        0x0001 => vec![0x50, 0x40], // Device Type
        0x0002 => vec![0x01, 0x02, 0x03], // Firmware Version
        0x0003 => vec![0x00, 0x01], // Hardware Version
        0x0021 => vec![0x01], // IT Status
        0x0023 => vec![0x03], // FET Status
        0x0024 | 0x0025 | 0x0026 => {
            let _ = data;
            vec![0x00] // ACK for writes
        }
        0x0070 | 0x0071 => vec![0x00, 0x00], // Safety/PF Status
        _ => vec![0x00, 0x00],
    };

    BmsManufacturerAccessData {
        subcommand,
        data: response_data,
    }
}

#[derive(Debug, Clone)]
struct MockDeviceState {
    registers: HashMap<u32, u32>,
    blocks: HashMap<u32, Vec<u8>>,
    telemetry: BmsTelemetryData,
    cell_voltages: Vec<u32>,
    device_info: BmsDeviceInfoData,
    tick: u64,
}

impl MockDeviceState {
    fn new() -> Self {
        let telemetry = create_mock_telemetry();
        let cell_voltages = create_mock_cell_voltages(4).cell_mv;
        let device_info = create_mock_device_info();
        Self {
            registers: HashMap::new(),
            blocks: HashMap::new(),
            telemetry,
            cell_voltages,
            device_info,
            tick: 0,
        }
    }

    fn tick(&mut self) {
        self.tick = self.tick.wrapping_add(1);
        let drift = (self.tick % 20) as i32 - 10;
        let current = -1500 + drift * 5;
        let voltage = 14800 + drift * 3;
        let temperature = 25 + (self.tick % 3) as i32;
        let soc = 75 + (self.tick % 5) as i32;

        self.telemetry.voltage_mv = voltage as u32;
        self.telemetry.current_ma = current;
        self.telemetry.average_current_ma = current - 120;
        self.telemetry.temperature_celsius = temperature;
        self.telemetry.relative_soc_percent = soc as u32;
        self.telemetry.absolute_soc_percent = (soc - 2).max(0) as u32;

        for (index, cell) in self.cell_voltages.iter_mut().enumerate() {
            let tweak = ((self.tick + index as u64) % 3) as i32 - 1;
            *cell = ((*cell as i32) + tweak).max(3300) as u32;
        }
    }

    fn read_register(&self, address: u32, num_bytes: u32) -> BmsRegisterData {
        if let Some(value) = self.registers.get(&address) {
            return BmsRegisterData {
                address,
                value: *value,
                num_bytes,
            };
        }
        create_mock_register(address, num_bytes)
    }

    fn write_register(&mut self, address: u32, value: u32) {
        self.registers.insert(address, value);
    }

    fn read_block(&self, address: u32, max_length: u32) -> BmsBlockData {
        if let Some(data) = self.blocks.get(&address) {
            let mut limited = data.clone();
            limited.truncate(max_length as usize);
            return BmsBlockData { address, data: limited };
        }

        let mut data = vec![0u8; max_length.min(32) as usize];
        for (i, byte) in data.iter_mut().enumerate() {
            *byte = ((address + i as u32) & 0xFF) as u8;
        }
        BmsBlockData { address, data }
    }

    fn write_block(&mut self, address: u32, data: Vec<u8>) {
        self.blocks.insert(address, data);
    }
}

fn handle_mock_request(
    request: &BmsCommandRequest,
    state: &Arc<Mutex<MockDeviceState>>,
) -> Result<BmsCommandResponse, String> {
    let mut state_lock = state.lock().map_err(|_| "Mock state lock poisoned".to_string())?;
    let response = match &request.command {
        Some(bms_command_request::Command::ReadTelemetry(_)) => {
            state_lock.tick();
            bms_command_response::Response::TelemetryData(state_lock.telemetry.clone())
        }
        Some(bms_command_request::Command::ReadCellVoltages(cmd)) => {
            let num_cells = cmd.num_cells.min(state_lock.cell_voltages.len() as u32);
            let cells: Vec<u32> = state_lock.cell_voltages.iter().take(num_cells as usize).copied().collect();
            let pack_mv = cells.iter().sum();
            let min_cell_mv = cells.iter().copied().min().unwrap_or(0);
            let max_cell_mv = cells.iter().copied().max().unwrap_or(0);
            let delta_mv = max_cell_mv.saturating_sub(min_cell_mv);
            bms_command_response::Response::CellVoltagesData(BmsCellVoltagesData {
                cell_mv: cells,
                pack_mv,
                min_cell_mv,
                max_cell_mv,
                delta_mv,
            })
        }
        Some(bms_command_request::Command::ReadDeviceInfo(_)) => {
            bms_command_response::Response::DeviceInfoData(state_lock.device_info.clone())
        }
        Some(bms_command_request::Command::ReadRegister(cmd)) => {
            bms_command_response::Response::RegisterData(state_lock.read_register(
                cmd.address,
                cmd.num_bytes,
            ))
        }
        Some(bms_command_request::Command::WriteRegister(cmd)) => {
            state_lock.write_register(cmd.address, cmd.value);
            bms_command_response::Response::AckData(BmsAckData {
                success: true,
                message: "Mock write successful".to_string(),
            })
        }
        Some(bms_command_request::Command::ReadBlock(cmd)) => {
            bms_command_response::Response::BlockData(state_lock.read_block(cmd.address, cmd.max_length))
        }
        Some(bms_command_request::Command::WriteBlock(cmd)) => {
            state_lock.write_block(cmd.address, cmd.data.clone());
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
            latency_us: 1000,
        }),
        response: Some(response),
    })
}

struct MockPort {
    name: String,
    timeout: Duration,
    read_buffer: VecDeque<u8>,
    write_buffer: Vec<u8>,
    state: Arc<Mutex<MockDeviceState>>,
}

impl MockPort {
    fn new(name: &str, state: Arc<Mutex<MockDeviceState>>) -> Self {
        Self {
            name: name.to_string(),
            timeout: Duration::from_millis(100),
            read_buffer: VecDeque::new(),
            write_buffer: Vec::new(),
            state,
        }
    }

    fn process_writes(&mut self) -> std::io::Result<()> {
        loop {
            let (frame_opt, bytes_consumed, error_opt) =
                frame::read_frame_from_buffer(&self.write_buffer);

            if bytes_consumed > 0 {
                self.write_buffer.drain(..bytes_consumed);
            }

            if let Some(err) = error_opt {
                return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, err.to_string()));
            }

            let frame = match frame_opt {
                Some(frame) => frame,
                None => break,
            };

            if frame.frame_type != FRAME_TYPE_REQUEST {
                continue;
            }

            let request = BmsCommandRequest::decode(frame.payload.as_slice())
                .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e.to_string()))?;
            let (req_type, _) = request_to_json(&request);
            log::info!("[Mock] TX {}", req_type);
            let response = handle_mock_request(&request, &self.state)
                .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e))?;
            let (resp_type, _) = response_to_json(&response);

            let mut response_payload = Vec::new();
            response
                .encode(&mut response_payload)
                .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e.to_string()))?;

            let response_frame = Frame::encode(
                frame.sequence,
                FRAME_TYPE_RESPONSE,
                0,
                &response_payload,
            )
            .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e.to_string()))?;

            for byte in response_frame {
                self.read_buffer.push_back(byte);
            }
            log::info!("[Mock] RX {}", resp_type);
        }

        Ok(())
    }
}

impl Read for MockPort {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        if self.read_buffer.is_empty() {
            return Err(std::io::Error::new(std::io::ErrorKind::TimedOut, "Mock timeout"));
        }

        let mut count = 0usize;
        while count < buf.len() {
            if let Some(byte) = self.read_buffer.pop_front() {
                buf[count] = byte;
                count += 1;
            } else {
                break;
            }
        }

        Ok(count)
    }
}

impl Write for MockPort {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.write_buffer.extend_from_slice(buf);
        self.process_writes()?;
        Ok(buf.len())
    }

    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

impl SerialPort for MockPort {
    fn name(&self) -> Option<String> {
        Some(self.name.clone())
    }

    fn baud_rate(&self) -> serialport::Result<u32> {
        Ok(115200)
    }

    fn data_bits(&self) -> serialport::Result<serialport::DataBits> {
        Ok(serialport::DataBits::Eight)
    }

    fn flow_control(&self) -> serialport::Result<serialport::FlowControl> {
        Ok(serialport::FlowControl::None)
    }

    fn parity(&self) -> serialport::Result<serialport::Parity> {
        Ok(serialport::Parity::None)
    }

    fn stop_bits(&self) -> serialport::Result<serialport::StopBits> {
        Ok(serialport::StopBits::One)
    }

    fn timeout(&self) -> Duration {
        self.timeout
    }

    fn set_baud_rate(&mut self, _baud_rate: u32) -> serialport::Result<()> {
        Ok(())
    }

    fn set_data_bits(&mut self, _data_bits: serialport::DataBits) -> serialport::Result<()> {
        Ok(())
    }

    fn set_flow_control(
        &mut self,
        _flow_control: serialport::FlowControl,
    ) -> serialport::Result<()> {
        Ok(())
    }

    fn set_parity(&mut self, _parity: serialport::Parity) -> serialport::Result<()> {
        Ok(())
    }

    fn set_stop_bits(&mut self, _stop_bits: serialport::StopBits) -> serialport::Result<()> {
        Ok(())
    }

    fn set_timeout(&mut self, timeout: Duration) -> serialport::Result<()> {
        self.timeout = timeout;
        Ok(())
    }

    fn write_request_to_send(&mut self, _level: bool) -> serialport::Result<()> {
        Ok(())
    }

    fn write_data_terminal_ready(&mut self, _level: bool) -> serialport::Result<()> {
        Ok(())
    }

    fn read_clear_to_send(&mut self) -> serialport::Result<bool> {
        Ok(true)
    }

    fn read_data_set_ready(&mut self) -> serialport::Result<bool> {
        Ok(true)
    }

    fn read_ring_indicator(&mut self) -> serialport::Result<bool> {
        Ok(false)
    }

    fn read_carrier_detect(&mut self) -> serialport::Result<bool> {
        Ok(true)
    }

    fn bytes_to_read(&self) -> serialport::Result<u32> {
        Ok(self.read_buffer.len() as u32)
    }

    fn bytes_to_write(&self) -> serialport::Result<u32> {
        Ok(self.write_buffer.len() as u32)
    }

    fn clear(&self, _buffer_to_clear: serialport::ClearBuffer) -> serialport::Result<()> {
        Ok(())
    }

    fn try_clone(&self) -> serialport::Result<Box<dyn SerialPort>> {
        Err(serialport::Error {
            kind: serialport::ErrorKind::Io(std::io::ErrorKind::Other),
            description: "MockPort does not support cloning".to_string(),
        })
    }

    fn set_break(&self) -> serialport::Result<()> {
        Ok(())
    }

    fn clear_break(&self) -> serialport::Result<()> {
        Ok(())
    }
}

impl Read for PtyPort {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        self.file.read(buf)
    }
}

impl Write for PtyPort {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.file.write(buf)
    }

    fn flush(&mut self) -> std::io::Result<()> {
        self.file.flush()
    }
}

impl SerialPort for PtyPort {
    fn name(&self) -> Option<String> {
        Some(self.name.clone())
    }

    fn baud_rate(&self) -> serialport::Result<u32> {
        Ok(115200) // Mock value for PTY
    }

    fn data_bits(&self) -> serialport::Result<serialport::DataBits> {
        Ok(serialport::DataBits::Eight)
    }

    fn flow_control(&self) -> serialport::Result<serialport::FlowControl> {
        Ok(serialport::FlowControl::None)
    }

    fn parity(&self) -> serialport::Result<serialport::Parity> {
        Ok(serialport::Parity::None)
    }

    fn stop_bits(&self) -> serialport::Result<serialport::StopBits> {
        Ok(serialport::StopBits::One)
    }

    fn timeout(&self) -> Duration {
        Duration::from_millis(100) // Short timeout for responsive abort
    }

    fn set_baud_rate(&mut self, _baud_rate: u32) -> serialport::Result<()> {
        Ok(()) // No-op for PTY
    }

    fn set_data_bits(&mut self, _data_bits: serialport::DataBits) -> serialport::Result<()> {
        Ok(())
    }

    fn set_flow_control(
        &mut self,
        _flow_control: serialport::FlowControl,
    ) -> serialport::Result<()> {
        Ok(())
    }

    fn set_parity(&mut self, _parity: serialport::Parity) -> serialport::Result<()> {
        Ok(())
    }

    fn set_stop_bits(&mut self, _stop_bits: serialport::StopBits) -> serialport::Result<()> {
        Ok(())
    }

    fn set_timeout(&mut self, _timeout: Duration) -> serialport::Result<()> {
        Ok(())
    }

    fn write_request_to_send(&mut self, _level: bool) -> serialport::Result<()> {
        Ok(())
    }

    fn write_data_terminal_ready(&mut self, _level: bool) -> serialport::Result<()> {
        Ok(())
    }

    fn read_clear_to_send(&mut self) -> serialport::Result<bool> {
        Ok(true)
    }

    fn read_data_set_ready(&mut self) -> serialport::Result<bool> {
        Ok(true)
    }

    fn read_ring_indicator(&mut self) -> serialport::Result<bool> {
        Ok(false)
    }

    fn read_carrier_detect(&mut self) -> serialport::Result<bool> {
        Ok(true)
    }

    fn bytes_to_read(&self) -> serialport::Result<u32> {
        let mut bytes: libc::c_int = 0;
        let result = unsafe { libc::ioctl(self.file.as_raw_fd(), libc::FIONREAD, &mut bytes) };
        if result == -1 {
            return Err(serialport::Error {
                kind: serialport::ErrorKind::Io(std::io::ErrorKind::Other),
                description: format!(
                    "Failed to query PTY bytes: {}",
                    std::io::Error::last_os_error()
                ),
            });
        }
        Ok(bytes.max(0) as u32)
    }

    fn bytes_to_write(&self) -> serialport::Result<u32> {
        Ok(0)
    }

    fn clear(&self, _buffer_to_clear: serialport::ClearBuffer) -> serialport::Result<()> {
        Ok(())
    }

    fn try_clone(&self) -> serialport::Result<Box<dyn SerialPort>> {
        Err(serialport::Error {
            kind: serialport::ErrorKind::Io(std::io::ErrorKind::Other),
            description: "PTY clone not supported".to_string(),
        })
    }

    fn set_break(&self) -> serialport::Result<()> {
        Ok(())
    }

    fn clear_break(&self) -> serialport::Result<()> {
        Ok(())
    }
}

const DEFAULT_TIMEOUT: Duration = Duration::from_secs(2);
const READ_CHUNK_SIZE: usize = 1024;
const MAX_CELLS_PROTOCOL: u32 = 16;

/// Device state cached from auto-discovery on connection
#[derive(Debug, Clone)]
pub struct DeviceState {
    /// Device information from auto-discovery
    pub info: BmsDeviceInfoData,
    /// Connection timestamp
    pub connected_at: Instant,
}

impl DeviceState {
    pub fn new(info: BmsDeviceInfoData) -> Self {
        Self {
            info,
            connected_at: Instant::now(),
        }
    }

    /// Validate and adjust requested cell count against device capabilities
    pub fn validate_num_cells(&self, requested: u32) -> Result<u32, String> {
        let device_cells = self.info.num_cells;

        // If requested is 0, default to device's actual cell count
        if requested == 0 {
            return Ok(device_cells);
        }

        // Clamp to device capabilities first (auto-adjust to device maximum)
        let clamped = if requested > device_cells {
            device_cells
        } else {
            requested
        };

        // Then check against protocol maximum
        if clamped > MAX_CELLS_PROTOCOL {
            return Err(format!(
                "Device has {} cells which exceeds protocol maximum of {}",
                clamped, MAX_CELLS_PROTOCOL
            ));
        }

        Ok(clamped)
    }
}

/// Maximum number of packets to store in capture buffers

#[derive(Debug, Clone)]
pub struct BmsApp {
    pub port: Arc<Mutex<Option<Box<dyn SerialPort>>>>,
    pub device_state: Arc<Mutex<Option<DeviceState>>>,
    sequence: Arc<Mutex<u8>>,
    read_buffer: Arc<Mutex<Vec<u8>>>,
    /// Signal to abort an in-progress connection attempt
    abort_signal: Arc<AtomicBool>,
    /// Packet capture for debugging RAW and PARSED data views
    pub packet_capture: Arc<PacketCapture>,
    /// Mock device state for DEV:MOCK
    mock_state: Arc<Mutex<MockDeviceState>>,
    /// Mock stream loop toggle
    mock_stream_running: Arc<AtomicBool>,
}

impl Default for BmsApp {
    fn default() -> Self {
        Self::new()
    }
}

impl BmsApp {
    pub fn new() -> Self {
        Self {
            port: Arc::new(Mutex::new(None)),
            device_state: Arc::new(Mutex::new(None)),
            sequence: Arc::new(Mutex::new(0)),
            read_buffer: Arc::new(Mutex::new(Vec::with_capacity(4096))),
            abort_signal: Arc::new(AtomicBool::new(false)),
            packet_capture: Arc::new(PacketCapture::new()),
            mock_state: Arc::new(Mutex::new(MockDeviceState::new())),
            mock_stream_running: Arc::new(AtomicBool::new(false)),
        }
    }

    fn get_next_sequence(&self) -> u8 {
        let mut seq = self.sequence.lock().unwrap();
        let current = *seq;
        *seq = seq.wrapping_add(1);
        current
    }

    fn reset_mock_state(&self) {
        let mut state = self.mock_state.lock().unwrap();
        *state = MockDeviceState::new();
    }

    fn stop_mock_stream(&self) {
        self.mock_stream_running.store(false, Ordering::SeqCst);
    }

    fn start_mock_stream(&self) {
        if self.mock_stream_running.swap(true, Ordering::SeqCst) {
            return;
        }

        let app = self.clone();
        std::thread::spawn(move || loop {
            if !app.mock_stream_running.load(Ordering::SeqCst) {
                break;
            }

            {
                let mut state = app.mock_state.lock().unwrap();
                state.tick();
            }

            let telemetry = {
                let state = app.mock_state.lock().unwrap();
                state.telemetry.clone()
            };

            let response = BmsCommandResponse {
                header: Some(ResponseHeader {
                    request_id: "mock-stream".to_string(),
                    status: Status::Ok as i32,
                    error_message: String::new(),
                    server_timestamp: None,
                    latency_us: 500,
                }),
                response: Some(bms_command_response::Response::TelemetryData(telemetry)),
            };

            let mut payload = Vec::new();
            if response.encode(&mut payload).is_ok() {
                let seq = app.get_next_sequence();
                if let Ok(frame_data) = Frame::encode(seq, FRAME_TYPE_RESPONSE, 0, &payload) {
                    app.packet_capture.capture_raw(
                        PacketDirection::Rx,
                        &frame_data,
                        "RX Mock Telemetry Stream",
                    );
                    let (payload_type, fields) = response_to_json(&response);
                    app.packet_capture.capture_parsed(
                        PacketDirection::Rx,
                        seq,
                        FRAME_TYPE_RESPONSE,
                        &payload_type,
                        fields,
                    );
                    log::info!("[Mock] Stream RX {}", payload_type);
                }
            }

            std::thread::sleep(Duration::from_millis(250));
        });
    }

    /// Internal helper to read device info (used for auto-discovery on connect)
    fn internal_read_device_info(&self) -> Result<BmsDeviceInfoData> {
        let req = BmsCommandRequest {
            header: Some(RequestHeader {
                request_id: format!(
                    "auto-discover-{}",
                    std::time::SystemTime::now()
                        .duration_since(std::time::UNIX_EPOCH)
                        .unwrap()
                        .as_nanos()
                ),
                client_timestamp: None,
                client_version: env!("CARGO_PKG_VERSION").to_string(),
                timeout: None,
            }),
            command: Some(bms_command_request::Command::ReadDeviceInfo(
                ReadDeviceInfoCommand {},
            )),
        };

        let resp = self.send_request(&req, DEFAULT_TIMEOUT)?;

        match resp.response {
            Some(bms_command_response::Response::DeviceInfoData(data)) => Ok(data),
            _ => Err(anyhow::anyhow!("Response does not contain device info")),
        }
    }

    pub fn send_request(
        &self,
        req: &BmsCommandRequest,
        timeout: Duration,
    ) -> Result<BmsCommandResponse> {
        // Marshal protobuf
        let mut payload = Vec::new();
        req.encode(&mut payload)
            .context("Failed to encode request")?;

        // Encode frame
        let seq = self.get_next_sequence();
        let frame_data = Frame::encode(seq, FRAME_TYPE_REQUEST, 0, &payload)
            .map_err(|e| anyhow::anyhow!("Frame encode error: {}", e))?;

        // Capture RAW TX packet (before sending)
        let (payload_type, fields) = request_to_json(req);
        self.packet_capture.capture_raw(
            PacketDirection::Tx,
            &frame_data,
            &format!(
                "TX Frame: seq={} type=0x{:02X} {}",
                seq, FRAME_TYPE_REQUEST, payload_type
            ),
        );

        // Send frame
        {
            let mut port_lock = self.port.lock().unwrap();
            let port = port_lock
                .as_mut()
                .ok_or_else(|| anyhow::anyhow!("Not connected to device"))?;

            port.write_all(&frame_data)
                .context("Failed to write to serial port")?;
            port.flush().context("Failed to flush serial port")?;

            log::debug!("Sent frame: seq={} len={}", seq, frame_data.len());
        }

        // Capture PARSED TX packet (after successful send)
        self.packet_capture.capture_parsed(
            PacketDirection::Tx,
            seq,
            FRAME_TYPE_REQUEST,
            &payload_type,
            fields,
        );

        // Receive response with timeout
        let deadline = Instant::now() + timeout;

        loop {
            // Check if connection was aborted by user
            if self.abort_signal.load(Ordering::SeqCst) {
                log::warn!("Abort signal detected in send_request loop - aborting");
                return Err(anyhow::anyhow!("Connection aborted by user"));
            }

            if Instant::now() > deadline {
                log::warn!("Timeout waiting for response");
                return Err(anyhow::anyhow!("Timeout waiting for response"));
            }

            let available = {
                let mut port_lock = self.port.lock().unwrap();
                let port = port_lock
                    .as_mut()
                    .ok_or_else(|| anyhow::anyhow!("Port disconnected during read"))?;
                port.bytes_to_read()
                    .map_err(|e| anyhow::anyhow!("Read availability error: {}", e))?
            };

            if available == 0 {
                std::thread::sleep(Duration::from_millis(10));
                continue;
            }

            let mut chunk = vec![0u8; READ_CHUNK_SIZE.min(available as usize).max(1)];
            let bytes_read = {
                let mut port_lock = self.port.lock().unwrap();
                let port = port_lock
                    .as_mut()
                    .ok_or_else(|| anyhow::anyhow!("Port disconnected during read"))?;

                match port.read(&mut chunk) {
                    Ok(n) => n,
                    Err(ref e) if e.kind() == std::io::ErrorKind::TimedOut => {
                        std::thread::sleep(Duration::from_millis(10));
                        continue;
                    }
                    Err(e) => return Err(anyhow::anyhow!("Read error: {}", e)),
                }
            };

            if bytes_read > 0 {
                log::debug!("Read {} bytes from serial port", bytes_read);

                // Capture RAW RX packet (partial or complete)
                self.packet_capture.capture_raw(
                    PacketDirection::Rx,
                    &chunk[..bytes_read],
                    &format!("RX Data: {} bytes", bytes_read),
                );

                // Append to read buffer
                {
                    let mut buffer = self.read_buffer.lock().unwrap();
                    buffer.extend_from_slice(&chunk[..bytes_read]);

                    // Try to decode frame from buffer
                    log::debug!(
                        "Trying to decode frame from buffer ({} bytes)",
                        buffer.len()
                    );

                    let (frame_opt, bytes_consumed, error_opt) =
                        frame::read_frame_from_buffer(&buffer);

                    // Remove consumed bytes
                    if bytes_consumed > 0 {
                        buffer.drain(..bytes_consumed);
                    }

                    // Handle decode error
                    if let Some(err) = error_opt {
                        log::debug!(
                            "Frame decode error: {} (consumed {} bytes)",
                            err,
                            bytes_consumed
                        );
                        continue;
                    }

                    // Check if we got a complete frame
                    if let Some(frame) = frame_opt {
                        log::debug!(
                            "Decoded frame: seq={} type=0x{:02X} payload={} bytes",
                            frame.sequence,
                            frame.frame_type,
                            frame.payload.len()
                        );

                        // Verify frame type
                        if frame.frame_type != FRAME_TYPE_RESPONSE {
                            log::warn!("Unexpected frame type: 0x{:02X}", frame.frame_type);
                            continue;
                        }

                        // Verify sequence number matches
                        if frame.sequence != seq {
                            log::debug!(
                                "Sequence mismatch: got {}, expected {} - discarding",
                                frame.sequence,
                                seq
                            );
                            continue;
                        }

                        // Decode protobuf response
                        let resp = BmsCommandResponse::decode(frame.payload.as_slice())
                            .context("Failed to decode protobuf response")?;

                        log::debug!("Successfully decoded response!");

                        // Capture PARSED RX packet (after successful decode)
                        let (resp_payload_type, resp_fields) = response_to_json(&resp);
                        self.packet_capture.capture_parsed(
                            PacketDirection::Rx,
                            frame.sequence,
                            frame.frame_type,
                            &resp_payload_type,
                            resp_fields,
                        );

                        // Check response status
                        if let Some(header) = &resp.header {
                            if header.status != Status::Ok as i32 {
                                return Err(anyhow::anyhow!(
                                    "BMS error: {:?} ({})",
                                    Status::try_from(header.status).unwrap_or(Status::Unknown),
                                    header.error_message
                                ));
                            }
                        }

                        return Ok(resp);
                    } else {
                        log::debug!("Frame incomplete, need more data");
                    }
                }
            }

            // Small delay to avoid busy-waiting
            std::thread::sleep(Duration::from_millis(10));
        }
    }
}

// Tauri commands (exposed to frontend)

#[tauri::command]
pub fn list_serial_ports() -> Result<Vec<String>, String> {
    let mut ports: Vec<String> = serialport::available_ports()
        .map(|ports| {
            ports
                .into_iter()
                .map(|p: SerialPortInfo| p.port_name)
                .collect()
        })
        .map_err(|e| format!("Failed to list serial ports: {}", e))?;

    if cfg!(debug_assertions) {
        ports.push(DEV_MOCK_PORT.to_string());
    }

    let mut extra_ports: Vec<String> = Vec::new();
    if let Ok(extra) = std::env::var("STAR_BMS_VIRTUAL_PORTS") {
        extra_ports.extend(
            extra
                .split(',')
                .map(|port| port.trim().to_string())
                .filter(|port| !port.is_empty()),
        );
    }
    if let Ok(extra) = std::env::var("STAR_BMS_VIRTUAL_PORT") {
        let port = extra.trim();
        if !port.is_empty() {
            extra_ports.push(port.to_string());
        }
    }

    if !extra_ports.is_empty() {
        ports.extend(extra_ports);
        ports.sort();
        ports.dedup();
    }

    if cfg!(debug_assertions) {
        ports.sort();
        ports.dedup();
    }

    Ok(ports)
}

fn connect_to_device_blocking(app: &BmsApp, port_name: String) -> Result<BmsDeviceInfoData, String> {
    log::info!("========== CONNECT_TO_DEVICE START ==========");
    log::info!("Port name: {}", port_name);

    // Reset abort signal at start of new connection attempt
    log::info!("Resetting abort signal to false");
    app.abort_signal.store(false, Ordering::SeqCst);
    log::info!("Abort signal reset");

    log::info!("Acquiring port lock...");
    let mut port_lock = app.port.lock().unwrap();
    log::info!("Port lock acquired");

    if port_lock.is_some() {
        log::warn!("Port already open - returning error");
        return Err("Already connected to a device".to_string());
    }

    // Detect if this is a PTY device (starts with /dev/tty or /tmp/)
    log::info!("Opening port {}...", port_name);
    app.stop_mock_stream();
    let port: Box<dyn SerialPort> = if port_name == DEV_MOCK_PORT {
        log::info!("Using in-process mock device port");
        app.reset_mock_state();
        Box::new(MockPort::new(DEV_MOCK_PORT, app.mock_state.clone()))
    } else if port_name.starts_with("/dev/tty") || port_name.starts_with("/tmp/") {
            log::info!("Detected PTY device, using PtyPort");
            // Use PtyPort for pseudo-terminals (socat, etc.)
            Box::new(PtyPort::open(&port_name).map_err(|e| {
                log::error!("Failed to open PTY: {}", e);
                format!("Failed to open PTY {}: {}", port_name, e)
            })?)
        } else {
            log::info!("Using regular serialport");
            // Use regular serialport for real hardware
            // Short timeout (100ms) allows frequent abort signal checks
            serialport::new(&port_name, 115_200)
                .timeout(Duration::from_millis(100))
                .data_bits(serialport::DataBits::Eight)
                .parity(serialport::Parity::None)
                .stop_bits(serialport::StopBits::One)
                .open()
                .map_err(|e| {
                    log::error!("Failed to open port: {}", e);
                    format!("Failed to open port {}: {}", port_name, e)
                })?
        };

    log::info!("Port opened successfully");
    *port_lock = Some(port);
    drop(port_lock); // Release lock before auto-discovery
    log::info!("Port lock released");

    // Check if connection was aborted before starting auto-discovery
    log::info!("Checking abort signal before auto-discovery...");
    if app.abort_signal.load(Ordering::SeqCst) {
        log::warn!("ABORT DETECTED before auto-discovery! Cleaning up port...");
        let port = {
            let mut port_lock = app.port.lock().unwrap();
            port_lock.take()
        };
        close_port_async(port);
        log::info!("Port cleanup scheduled, returning abort error");
        return Err("Connection aborted by user".to_string());
    }
    log::info!("Abort signal not set, proceeding with auto-discovery");

    // Auto-discover device capabilities
    log::info!("Starting auto-discovery...");
    let device_info = app.internal_read_device_info().map_err(|e| {
        log::error!("Auto-discovery failed: {}", e);
        // If auto-discovery fails, clean up the port
        log::info!("Cleaning up port due to auto-discovery failure");
        let port = {
            let mut port_lock = app.port.lock().unwrap();
            port_lock.take()
        };
        close_port_async(port);
        log::info!("Port cleanup scheduled");
        format!("Failed to auto-discover device info: {}", e)
    })?;

    log::info!(
        "Auto-discovery successful: {} {} ({} cells, {}mAh)",
        device_info.manufacturer,
        device_info.device_name,
        device_info.num_cells,
        device_info.design_capacity_mah
    );

    // Check one final time if connection was aborted
    log::info!("Final abort signal check after auto-discovery...");
    if app.abort_signal.load(Ordering::SeqCst) {
        log::warn!("ABORT DETECTED after auto-discovery! Cleaning up...");
        let port = {
            let mut port_lock = app.port.lock().unwrap();
            port_lock.take()
        };
        close_port_async(port);
        log::info!("Port cleanup scheduled, returning abort error");
        return Err("Connection aborted by user".to_string());
    }
    log::info!("Abort signal not set, finalizing connection");

    // Cache device state
    log::info!("Caching device state");
    let device_state = DeviceState::new(device_info.clone());
    *app.device_state.lock().unwrap() = Some(device_state);
    log::info!("Device state cached");

    if port_name == DEV_MOCK_PORT {
        app.start_mock_stream();
    }

    log::info!("========== CONNECT_TO_DEVICE SUCCESS ==========");
    Ok(device_info)
}

#[tauri::command]
pub async fn connect_to_device(
    app: State<'_, BmsApp>,
    port_name: String,
) -> Result<BmsDeviceInfoData, String> {
    let app = app.inner().clone();
    tauri::async_runtime::spawn_blocking(move || connect_to_device_blocking(&app, port_name))
        .await
        .map_err(|error| format!("Connection task failed: {}", error))?
}

#[tauri::command]
pub fn disconnect_from_device(app: State<BmsApp>) -> Result<(), String> {
    log::info!("========== DISCONNECT_FROM_DEVICE START ==========");
    let port = {
        let mut port_lock = app.port.lock().unwrap();
        if port_lock.is_none() {
            log::warn!("Not connected - returning error");
            return Err("Not connected to any device".to_string());
        }
        log::info!("Closing port...");
        port_lock.take()
    };

    if port.is_some() {
        close_port_async(port);
        log::info!("Port close scheduled");
    }

    app.stop_mock_stream();

    // Clear device state
    log::info!("Clearing device state");
    *app.device_state.lock().unwrap() = None;
    log::info!("========== DISCONNECT_FROM_DEVICE SUCCESS ==========");

    Ok(())
}

#[tauri::command]
pub fn abort_connection(app: State<BmsApp>) -> Result<(), String> {
    log::info!("========== ABORT_CONNECTION START ==========");
    log::info!(
        "Current abort signal value: {}",
        app.abort_signal.load(Ordering::SeqCst)
    );

    // Set abort signal to stop any in-progress connection attempt
    log::info!("Setting abort signal to TRUE");
    app.abort_signal.store(true, Ordering::SeqCst);
    log::info!(
        "Abort signal set to: {}",
        app.abort_signal.load(Ordering::SeqCst)
    );

    // Close the port if it was opened during the connection attempt
    log::info!("Acquiring port lock...");
    let port = {
        let mut port_lock = app.port.lock().unwrap();
        log::info!("Port lock acquired");
        if port_lock.is_some() {
            log::info!("Port is open, closing it now");
        } else {
            log::info!("Port was not open");
        }
        port_lock.take()
    };

    if port.is_some() {
        close_port_async(port);
        log::info!("Port close scheduled");
    }
    log::info!("Port lock released");

    app.stop_mock_stream();

    // Clear any partial device state
    log::info!("Clearing any partial device state");
    *app.device_state.lock().unwrap() = None;
    log::info!("Device state cleared");

    log::info!("========== ABORT_CONNECTION SUCCESS ==========");
    Ok(())
}

#[tauri::command]
pub fn is_connected(app: State<BmsApp>) -> bool {
    app.port.lock().unwrap().is_some()
}

#[tauri::command]
pub fn get_device_state(app: State<BmsApp>) -> Result<BmsDeviceInfoData, String> {
    let state_lock = app.device_state.lock().unwrap();
    let state = state_lock
        .as_ref()
        .ok_or_else(|| "No device connected or state not available".to_string())?;

    Ok(state.info.clone())
}

#[tauri::command]
pub fn read_telemetry(app: State<BmsApp>) -> Result<BmsTelemetryData, String> {
    let req = BmsCommandRequest {
        header: Some(RequestHeader {
            request_id: format!(
                "telemetry-{}",
                std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap()
                    .as_nanos()
            ),
            client_timestamp: None,
            client_version: "1.0.0".to_string(),
            timeout: None,
        }),
        command: Some(bms_command_request::Command::ReadTelemetry(
            ReadTelemetryCommand {},
        )),
    };

    let resp = app
        .send_request(&req, DEFAULT_TIMEOUT)
        .map_err(|e| e.to_string())?;

    match resp.response {
        Some(bms_command_response::Response::TelemetryData(data)) => Ok(data),
        _ => Err("Response does not contain telemetry data".to_string()),
    }
}

#[tauri::command]
pub fn read_cell_voltages(
    app: State<BmsApp>,
    num_cells: u32,
) -> Result<BmsCellVoltagesData, String> {
    // Validate against device capabilities
    let validated_cells = {
        let state_lock = app.device_state.lock().unwrap();
        let state = state_lock
            .as_ref()
            .ok_or_else(|| "Not connected to device".to_string())?;

        state.validate_num_cells(num_cells)?
    };

    let req = BmsCommandRequest {
        header: Some(RequestHeader {
            request_id: format!(
                "cells-{}",
                std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap()
                    .as_nanos()
            ),
            client_timestamp: None,
            client_version: env!("CARGO_PKG_VERSION").to_string(),
            timeout: None,
        }),
        command: Some(bms_command_request::Command::ReadCellVoltages(
            ReadCellVoltagesCommand {
                num_cells: validated_cells,
            },
        )),
    };

    let resp = app
        .send_request(&req, DEFAULT_TIMEOUT)
        .map_err(|e| e.to_string())?;

    match resp.response {
        Some(bms_command_response::Response::CellVoltagesData(data)) => Ok(data),
        _ => Err("Response does not contain cell voltage data".to_string()),
    }
}

#[tauri::command]
pub fn read_device_info(app: State<BmsApp>) -> Result<BmsDeviceInfoData, String> {
    let req = BmsCommandRequest {
        header: Some(RequestHeader {
            request_id: format!(
                "info-{}",
                std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap()
                    .as_nanos()
            ),
            client_timestamp: None,
            client_version: "1.0.0".to_string(),
            timeout: None,
        }),
        command: Some(bms_command_request::Command::ReadDeviceInfo(
            ReadDeviceInfoCommand {},
        )),
    };

    let resp = app
        .send_request(&req, DEFAULT_TIMEOUT)
        .map_err(|e| e.to_string())?;

    match resp.response {
        Some(bms_command_response::Response::DeviceInfoData(data)) => Ok(data),
        _ => Err("Response does not contain device info".to_string()),
    }
}

#[tauri::command]
pub fn read_register(
    app: State<BmsApp>,
    address: u32,
    num_bytes: u32,
) -> Result<BmsRegisterData, String> {
    let req = BmsCommandRequest {
        header: Some(RequestHeader {
            request_id: format!(
                "reg-read-{}",
                std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap()
                    .as_nanos()
            ),
            client_timestamp: None,
            client_version: "1.0.0".to_string(),
            timeout: None,
        }),
        command: Some(bms_command_request::Command::ReadRegister(
            ReadRegisterCommand { address, num_bytes },
        )),
    };

    let resp = app
        .send_request(&req, DEFAULT_TIMEOUT)
        .map_err(|e| e.to_string())?;

    match resp.response {
        Some(bms_command_response::Response::RegisterData(data)) => Ok(data),
        _ => Err("Response does not contain register data".to_string()),
    }
}

#[tauri::command]
pub fn write_register(
    app: State<BmsApp>,
    address: u32,
    value: u32,
    num_bytes: u32,
) -> Result<(), String> {
    let req = BmsCommandRequest {
        header: Some(RequestHeader {
            request_id: format!(
                "reg-write-{}",
                std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap()
                    .as_nanos()
            ),
            client_timestamp: None,
            client_version: "1.0.0".to_string(),
            timeout: None,
        }),
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
        .map_err(|e| e.to_string())?;

    match resp.response {
        Some(bms_command_response::Response::AckData(ack)) => {
            if ack.success {
                Ok(())
            } else {
                Err(format!("Write failed: {}", ack.message))
            }
        }
        _ => Err("Response does not contain acknowledgment".to_string()),
    }
}

#[tauri::command]
pub fn manufacturer_access(
    app: State<BmsApp>,
    subcommand: u32,
    data: Vec<u8>,
) -> Result<BmsManufacturerAccessData, String> {
    let req = BmsCommandRequest {
        header: Some(RequestHeader {
            request_id: format!(
                "mfg-access-{}",
                std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap()
                    .as_nanos()
            ),
            client_timestamp: None,
            client_version: "1.0.0".to_string(),
            timeout: None,
        }),
        command: Some(bms_command_request::Command::ManufacturerAccess(
            ManufacturerAccessCommand { subcommand, data },
        )),
    };

    let resp = app
        .send_request(&req, DEFAULT_TIMEOUT)
        .map_err(|e| e.to_string())?;

    match resp.response {
        Some(bms_command_response::Response::ManufacturerAccessData(data)) => Ok(data),
        _ => Err("Response does not contain manufacturer access data".to_string()),
    }
}

#[tauri::command]
pub fn read_protection_status(app: State<BmsApp>) -> Result<BmsProtectionStatusData, String> {
    let req = BmsCommandRequest {
        header: Some(RequestHeader {
            request_id: format!(
                "protection-status-{}",
                std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap()
                    .as_nanos()
            ),
            client_timestamp: None,
            client_version: "1.0.0".to_string(),
            timeout: None,
        }),
        command: Some(bms_command_request::Command::ReadProtectionStatus(
            ReadProtectionStatusCommand {},
        )),
    };

    let resp = app
        .send_request(&req, DEFAULT_TIMEOUT)
        .map_err(|e| e.to_string())?;

    match resp.response {
        Some(bms_command_response::Response::ProtectionStatusData(data)) => Ok(data),
        _ => Err("Response does not contain protection status data".to_string()),
    }
}

#[tauri::command]
pub fn read_block(
    app: State<BmsApp>,
    address: u32,
    max_length: u32,
) -> Result<BmsBlockData, String> {
    let req = BmsCommandRequest {
        header: Some(RequestHeader {
            request_id: format!(
                "block-read-{}",
                std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap()
                    .as_nanos()
            ),
            client_timestamp: None,
            client_version: "1.0.0".to_string(),
            timeout: None,
        }),
        command: Some(bms_command_request::Command::ReadBlock(ReadBlockCommand {
            address,
            max_length,
        })),
    };

    let resp = app
        .send_request(&req, DEFAULT_TIMEOUT)
        .map_err(|e| e.to_string())?;

    match resp.response {
        Some(bms_command_response::Response::BlockData(data)) => Ok(data),
        _ => Err("Response does not contain block data".to_string()),
    }
}

#[tauri::command]
pub fn write_block(app: State<BmsApp>, address: u32, data: Vec<u8>) -> Result<(), String> {
    let req = BmsCommandRequest {
        header: Some(RequestHeader {
            request_id: format!(
                "block-write-{}",
                std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap()
                    .as_nanos()
            ),
            client_timestamp: None,
            client_version: "1.0.0".to_string(),
            timeout: None,
        }),
        command: Some(bms_command_request::Command::WriteBlock(
            WriteBlockCommand { address, data },
        )),
    };

    let resp = app
        .send_request(&req, DEFAULT_TIMEOUT)
        .map_err(|e| e.to_string())?;

    match resp.response {
        Some(bms_command_response::Response::AckData(ack)) => {
            if ack.success {
                Ok(())
            } else {
                Err(format!("Write failed: {}", ack.message))
            }
        }
        _ => Err("Response does not contain acknowledgment".to_string()),
    }
}

/// Check if experimental features are enabled at compile time
#[tauri::command]
pub fn is_experimental_enabled() -> bool {
    cfg!(feature = "experimental")
}

// Packet capture Tauri commands

/// Get raw packets from the capture buffer with pagination support
#[tauri::command]
pub fn get_raw_packets(app: State<BmsApp>, limit: usize, offset: usize) -> Vec<RawPacket> {
    app.packet_capture.get_raw_packets(limit, offset)
}

/// Get parsed packets from the capture buffer with pagination support
#[tauri::command]
pub fn get_parsed_packets(app: State<BmsApp>, limit: usize, offset: usize) -> Vec<ParsedPacket> {
    app.packet_capture.get_parsed_packets(limit, offset)
}

/// Clear both packet capture buffers
#[tauri::command]
pub fn clear_packet_capture(app: State<BmsApp>) {
    app.packet_capture.clear();
    log::info!("Packet capture buffers cleared");
}

/// Get the count of packets in both buffers
/// Returns (raw_count, parsed_count)
#[tauri::command]
pub fn get_packet_count(app: State<BmsApp>) -> (usize, usize) {
    app.packet_capture.count()
}

/// Enable or disable packet capture
#[tauri::command]
pub fn set_packet_capture_enabled(app: State<BmsApp>, enabled: bool) {
    app.packet_capture.set_enabled(enabled);
    log::info!(
        "Packet capture {}",
        if enabled { "enabled" } else { "disabled" }
    );
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sequence_increment() {
        let app = BmsApp::new();
        assert_eq!(app.get_next_sequence(), 0);
        assert_eq!(app.get_next_sequence(), 1);
        assert_eq!(app.get_next_sequence(), 2);
    }

    #[test]
    fn test_sequence_wraparound() {
        let app = BmsApp::new();
        {
            let mut seq = app.sequence.lock().unwrap();
            *seq = 254;
        }
        assert_eq!(app.get_next_sequence(), 254);
        assert_eq!(app.get_next_sequence(), 255);
        assert_eq!(app.get_next_sequence(), 0); // Should wrap around
    }

    #[test]
    fn test_bms_app_new() {
        let app = BmsApp::new();
        assert!(app.port.lock().unwrap().is_none());
        assert!(app.device_state.lock().unwrap().is_none());
        assert_eq!(*app.sequence.lock().unwrap(), 0);
        assert_eq!(app.read_buffer.lock().unwrap().len(), 0);
        assert_eq!(app.read_buffer.lock().unwrap().capacity(), 4096);
        assert!(!app.abort_signal.load(Ordering::SeqCst));
        // Verify packet capture is initialized and enabled
        assert!(app.packet_capture.is_enabled());
        assert_eq!(app.packet_capture.count(), (0, 0));
    }

    #[test]
    fn test_bms_app_default() {
        let app = BmsApp::default();
        assert!(app.port.lock().unwrap().is_none());
        assert!(app.device_state.lock().unwrap().is_none());
    }

    #[test]
    fn test_device_state_validation() {
        let device_info = BmsDeviceInfoData {
            manufacturer: "Test Manufacturer".to_string(),
            device_name: "Test Device".to_string(),
            chemistry: "LION".to_string(),
            serial_number: 0x12345678,
            firmware_version: "v1.0".to_string(),
            hardware_version: "v1.0".to_string(),
            design_capacity_mah: 3200,
            design_voltage_mv: 14800,
            num_cells: 4,
        };
        let state = DeviceState::new(device_info);

        // Valid requests
        assert_eq!(state.validate_num_cells(4).unwrap(), 4);
        assert_eq!(state.validate_num_cells(2).unwrap(), 2);
        assert_eq!(state.validate_num_cells(0).unwrap(), 4); // Default to device count

        // Requests exceeding device capabilities are clamped to device maximum
        assert_eq!(state.validate_num_cells(5).unwrap(), 4); // Clamped to 4
        assert_eq!(state.validate_num_cells(250).unwrap(), 4); // Clamped to 4
        assert_eq!(state.validate_num_cells(17).unwrap(), 4); // Clamped to 4

        // Test that protocol maximum error is triggered for devices with >16 cells
        // (This is a hypothetical case as BQ78350-R1A supports max 16 cells)
        let large_device_info = BmsDeviceInfoData {
            manufacturer: "Test Manufacturer".to_string(),
            device_name: "Test 20-Cell Device".to_string(),
            chemistry: "LION".to_string(),
            serial_number: 0x12345678,
            firmware_version: "v1.0".to_string(),
            hardware_version: "v1.0".to_string(),
            design_capacity_mah: 6400,
            design_voltage_mv: 74000,
            num_cells: 20, // Hypothetical 20-cell device
        };
        let large_state = DeviceState::new(large_device_info);

        // Requesting full device capacity should fail because device has >16 cells
        let err_protocol = large_state.validate_num_cells(20).unwrap_err();
        assert!(
            err_protocol.contains("exceeds protocol maximum"),
            "Error message was: {}",
            err_protocol
        );

        // But requesting fewer cells should succeed (clamped to request)
        assert_eq!(large_state.validate_num_cells(10).unwrap(), 10);
        assert_eq!(large_state.validate_num_cells(16).unwrap(), 16);
    }

    #[test]
    fn test_request_telemetry_encoding() {
        let req = BmsCommandRequest {
            header: Some(RequestHeader {
                request_id: "test-123".to_string(),
                client_timestamp: None,
                client_version: "1.0.0".to_string(),
                timeout: None,
            }),
            command: Some(bms_command_request::Command::ReadTelemetry(
                ReadTelemetryCommand {},
            )),
        };

        let mut payload = Vec::new();
        let result = req.encode(&mut payload);
        assert!(result.is_ok());
        assert!(!payload.is_empty());

        // Verify we can decode it back
        let decoded = BmsCommandRequest::decode(payload.as_slice());
        assert!(decoded.is_ok());
        let decoded_req = decoded.unwrap();
        assert!(decoded_req.header.is_some());
        assert_eq!(decoded_req.header.unwrap().request_id, "test-123");
    }

    #[test]
    fn test_request_cell_voltages_encoding() {
        let req = BmsCommandRequest {
            header: Some(RequestHeader {
                request_id: "test-456".to_string(),
                client_timestamp: None,
                client_version: "1.0.0".to_string(),
                timeout: None,
            }),
            command: Some(bms_command_request::Command::ReadCellVoltages(
                ReadCellVoltagesCommand { num_cells: 4 },
            )),
        };

        let mut payload = Vec::new();
        let result = req.encode(&mut payload);
        assert!(result.is_ok());

        let decoded = BmsCommandRequest::decode(payload.as_slice()).unwrap();
        if let Some(bms_command_request::Command::ReadCellVoltages(cmd)) = decoded.command {
            assert_eq!(cmd.num_cells, 4);
        } else {
            panic!("Expected ReadCellVoltages command");
        }
    }

    #[test]
    fn test_request_register_read_encoding() {
        let req = BmsCommandRequest {
            header: Some(RequestHeader {
                request_id: "test-reg".to_string(),
                client_timestamp: None,
                client_version: "1.0.0".to_string(),
                timeout: None,
            }),
            command: Some(bms_command_request::Command::ReadRegister(
                ReadRegisterCommand {
                    address: 0x42,
                    num_bytes: 2,
                },
            )),
        };

        let mut payload = Vec::new();
        req.encode(&mut payload).unwrap();

        let decoded = BmsCommandRequest::decode(payload.as_slice()).unwrap();
        if let Some(bms_command_request::Command::ReadRegister(cmd)) = decoded.command {
            assert_eq!(cmd.address, 0x42);
            assert_eq!(cmd.num_bytes, 2);
        } else {
            panic!("Expected ReadRegister command");
        }
    }

    #[test]
    fn test_request_register_write_encoding() {
        let req = BmsCommandRequest {
            header: Some(RequestHeader {
                request_id: "test-write".to_string(),
                client_timestamp: None,
                client_version: "1.0.0".to_string(),
                timeout: None,
            }),
            command: Some(bms_command_request::Command::WriteRegister(
                WriteRegisterCommand {
                    address: 0x10,
                    value: 0xFF,
                    num_bytes: 1,
                },
            )),
        };

        let mut payload = Vec::new();
        req.encode(&mut payload).unwrap();

        let decoded = BmsCommandRequest::decode(payload.as_slice()).unwrap();
        if let Some(bms_command_request::Command::WriteRegister(cmd)) = decoded.command {
            assert_eq!(cmd.address, 0x10);
            assert_eq!(cmd.value, 0xFF);
            assert_eq!(cmd.num_bytes, 1);
        } else {
            panic!("Expected WriteRegister command");
        }
    }

    #[test]
    fn test_response_decoding_telemetry() {
        let resp = BmsCommandResponse {
            header: Some(ResponseHeader {
                request_id: "test-123".to_string(),
                status: Status::Ok as i32,
                error_message: String::new(),
                server_timestamp: None,
                latency_us: 0,
            }),
            response: Some(bms_command_response::Response::TelemetryData(
                BmsTelemetryData {
                    voltage_mv: 14800,
                    current_ma: -1500,
                    average_current_ma: -1400,
                    relative_soc_percent: 75,
                    absolute_soc_percent: 70,
                    temperature_celsius: 25,
                    remaining_capacity_mah: 2250,
                    full_capacity_mah: 3000,
                    cycle_count: 42,
                    design_capacity_mah: 3200,
                    time_to_empty_min: 90,
                    time_to_full_min: 0xFFFF,
                    is_charging: false,
                    is_fully_charged: false,
                    is_fully_discharged: false,
                    is_low_capacity: false,
                },
            )),
        };

        let mut payload = Vec::new();
        resp.encode(&mut payload).unwrap();

        let decoded = BmsCommandResponse::decode(payload.as_slice()).unwrap();
        assert!(decoded.header.is_some());
        assert_eq!(decoded.header.as_ref().unwrap().status, Status::Ok as i32);

        if let Some(bms_command_response::Response::TelemetryData(data)) = decoded.response {
            assert_eq!(data.voltage_mv, 14800);
            assert_eq!(data.current_ma, -1500);
            assert_eq!(data.relative_soc_percent, 75);
        } else {
            panic!("Expected TelemetryData response");
        }
    }

    #[test]
    fn test_response_decoding_error_status() {
        let resp = BmsCommandResponse {
            header: Some(ResponseHeader {
                request_id: "test-error".to_string(),
                status: Status::InvalidRequest as i32,
                error_message: "Invalid command code".to_string(),
                server_timestamp: None,
                latency_us: 100,
            }),
            response: None,
        };

        let mut payload = Vec::new();
        resp.encode(&mut payload).unwrap();

        let decoded = BmsCommandResponse::decode(payload.as_slice()).unwrap();
        let header = decoded.header.unwrap();
        assert_eq!(header.status, Status::InvalidRequest as i32);
        assert_eq!(header.error_message, "Invalid command code");
    }

    #[test]
    fn test_list_serial_ports() {
        // This test just verifies the function doesn't panic
        // Actual available ports depend on the system
        let result = list_serial_ports();
        // Result should be Ok or Err, but shouldn't panic
        assert!(result.is_ok() || result.is_err());
    }
}
