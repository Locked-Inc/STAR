// Packet capture infrastructure for debugging RAW and PARSED data views
// STAR Project - Texas A&M University
//
// This module provides packet capture functionality that works DURING connection
// attempts to help users debug communication issues.

use serde::{Deserialize, Serialize};
use std::collections::VecDeque;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Mutex;
use std::time::{SystemTime, UNIX_EPOCH};


/// Direction of packet transmission
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum PacketDirection {
    /// Transmitted from host to device
    Tx,
    /// Received from device to host
    Rx,
}

/// Raw packet data with timestamp and description
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RawPacket {
    /// Timestamp in microseconds since Unix epoch
    pub timestamp_us: u64,
    /// Direction of transmission
    pub direction: PacketDirection,
    /// Raw byte data
    pub data: Vec<u8>,
    /// Human-readable description of the packet
    pub description: String,
}

/// Parsed packet with decoded protobuf information
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ParsedPacket {
    /// Timestamp in microseconds since Unix epoch
    pub timestamp_us: u64,
    /// Direction of transmission
    pub direction: PacketDirection,
    /// Frame sequence number
    pub frame_seq: u8,
    /// Frame type (request=0x01, response=0x02)
    pub frame_type: u8,
    /// Type of protobuf payload (e.g., "ReadTelemetry", "TelemetryData")
    pub payload_type: String,
    /// Decoded protobuf fields as JSON
    pub fields: serde_json::Value,
}

/// Packet capture buffer with ring buffer behavior
#[derive(Debug)]
pub struct PacketCapture {
    /// Buffer for raw packets
    raw_buffer: Mutex<VecDeque<RawPacket>>,
    /// Buffer for parsed packets
    parsed_buffer: Mutex<VecDeque<ParsedPacket>>,
    /// Whether capture is enabled
    enabled: AtomicBool,
}

impl PacketCapture {
    /// Create a new packet capture instance
    pub fn new() -> Self {
        Self {
            raw_buffer: Mutex::new(VecDeque::new()),
            parsed_buffer: Mutex::new(VecDeque::new()),
            enabled: AtomicBool::new(true), // Enabled by default for debugging
        }
    }

    /// Capture a raw packet with timestamp and description
    pub fn capture_raw(&self, direction: PacketDirection, data: &[u8], description: &str) {
        if !self.enabled.load(Ordering::SeqCst) {
            return;
        }

        let packet = RawPacket {
            timestamp_us: get_current_timestamp_us(),
            direction,
            data: data.to_vec(),
            description: description.to_string(),
        };

        let mut buffer = self.raw_buffer.lock().unwrap();
        buffer.push_back(packet);
    }

    /// Capture a parsed packet from decoded protobuf
    pub fn capture_parsed(
        &self,
        direction: PacketDirection,
        frame_seq: u8,
        frame_type: u8,
        payload_type: &str,
        fields: serde_json::Value,
    ) {
        if !self.enabled.load(Ordering::SeqCst) {
            return;
        }

        let packet = ParsedPacket {
            timestamp_us: get_current_timestamp_us(),
            direction,
            frame_seq,
            frame_type,
            payload_type: payload_type.to_string(),
            fields,
        };

        let mut buffer = self.parsed_buffer.lock().unwrap();
        buffer.push_back(packet);
    }

    /// Get raw packets with pagination support
    /// Returns packets in chronological order (oldest first)
    pub fn get_raw_packets(&self, limit: usize, offset: usize) -> Vec<RawPacket> {
        let buffer = self.raw_buffer.lock().unwrap();
        buffer.iter().skip(offset).take(limit).cloned().collect()
    }

    /// Get parsed packets with pagination support
    /// Returns packets in chronological order (oldest first)
    pub fn get_parsed_packets(&self, limit: usize, offset: usize) -> Vec<ParsedPacket> {
        let buffer = self.parsed_buffer.lock().unwrap();
        buffer.iter().skip(offset).take(limit).cloned().collect()
    }

    /// Clear both buffers
    pub fn clear(&self) {
        let mut raw_buf = self.raw_buffer.lock().unwrap();
        let mut parsed_buf = self.parsed_buffer.lock().unwrap();
        raw_buf.clear();
        parsed_buf.clear();
    }

    /// Enable or disable packet capture
    pub fn set_enabled(&self, enabled: bool) {
        self.enabled.store(enabled, Ordering::SeqCst);
    }

    /// Check if packet capture is enabled
    pub fn is_enabled(&self) -> bool {
        self.enabled.load(Ordering::SeqCst)
    }

    /// Get the count of packets in both buffers
    /// Returns (raw_count, parsed_count)
    pub fn count(&self) -> (usize, usize) {
        let raw_count = self.raw_buffer.lock().unwrap().len();
        let parsed_count = self.parsed_buffer.lock().unwrap().len();
        (raw_count, parsed_count)
    }
}

impl Default for PacketCapture {
    fn default() -> Self {
        Self::new()
    }
}

/// Get the current timestamp in microseconds since Unix epoch
pub fn get_current_timestamp_us() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_micros() as u64)
        .unwrap_or(0)
}

/// Convert a protobuf request to a JSON value for capture
/// Returns (payload_type, fields)
pub fn request_to_json(req: &star_proto::BmsCommandRequest) -> (String, serde_json::Value) {
    use serde_json::json;
    use star_proto::bms_command_request::Command;

    let (payload_type, fields) = match &req.command {
        Some(Command::ReadTelemetry(_)) => ("ReadTelemetry", json!({})),
        Some(Command::ReadCellVoltages(cmd)) => {
            ("ReadCellVoltages", json!({ "num_cells": cmd.num_cells }))
        }
        Some(Command::ReadDeviceInfo(_)) => ("ReadDeviceInfo", json!({})),
        Some(Command::ReadRegister(cmd)) => (
            "ReadRegister",
            json!({
                "address": format!("0x{:02X}", cmd.address),
                "num_bytes": cmd.num_bytes
            }),
        ),
        Some(Command::WriteRegister(cmd)) => (
            "WriteRegister",
            json!({
                "address": format!("0x{:02X}", cmd.address),
                "value": cmd.value,
                "num_bytes": cmd.num_bytes
            }),
        ),
        Some(Command::ReadBlock(cmd)) => (
            "ReadBlock",
            json!({
                "address": format!("0x{:02X}", cmd.address),
                "max_length": cmd.max_length
            }),
        ),
        Some(Command::WriteBlock(cmd)) => (
            "WriteBlock",
            json!({
                "address": format!("0x{:02X}", cmd.address),
                "data_len": cmd.data.len()
            }),
        ),
        Some(Command::ManufacturerAccess(cmd)) => (
            "ManufacturerAccess",
            json!({
                "subcommand": format!("0x{:04X}", cmd.subcommand),
                "data_len": cmd.data.len()
            }),
        ),
        Some(Command::ReadProtectionStatus(_)) => ("ReadProtectionStatus", json!({})),
        Some(Command::ResetDevice(_)) => ("ResetDevice", json!({})),
        None => ("Unknown", json!({})),
    };

    // Include header info if present
    let mut result = serde_json::Map::new();
    if let Some(header) = &req.header {
        result.insert("request_id".to_string(), json!(header.request_id));
        result.insert("client_version".to_string(), json!(header.client_version));
    }
    result.insert("command".to_string(), fields);

    (payload_type.to_string(), serde_json::Value::Object(result))
}

/// Convert a protobuf response to a JSON value for capture
/// Returns (payload_type, fields)
pub fn response_to_json(resp: &star_proto::BmsCommandResponse) -> (String, serde_json::Value) {
    use serde_json::json;
    use star_proto::bms_command_response::Response;

    let (payload_type, response_fields) = match &resp.response {
        Some(Response::TelemetryData(data)) => (
            "TelemetryData",
            json!({
                "voltage_mv": data.voltage_mv,
                "current_ma": data.current_ma,
                "average_current_ma": data.average_current_ma,
                "relative_soc_percent": data.relative_soc_percent,
                "absolute_soc_percent": data.absolute_soc_percent,
                "temperature_celsius": data.temperature_celsius,
                "remaining_capacity_mah": data.remaining_capacity_mah,
                "full_capacity_mah": data.full_capacity_mah,
                "cycle_count": data.cycle_count,
                "is_charging": data.is_charging,
                "is_fully_charged": data.is_fully_charged
            }),
        ),
        Some(Response::CellVoltagesData(data)) => {
            let voltages: Vec<u32> = data.cell_mv.clone();
            (
                "CellVoltagesData",
                json!({
                    "cell_mv": voltages,
                    "pack_mv": data.pack_mv,
                    "min_cell_mv": data.min_cell_mv,
                    "max_cell_mv": data.max_cell_mv,
                    "delta_mv": data.delta_mv
                }),
            )
        }
        Some(Response::DeviceInfoData(data)) => (
            "DeviceInfoData",
            json!({
                "manufacturer": data.manufacturer,
                "device_name": data.device_name,
                "chemistry": data.chemistry,
                "serial_number": format!("0x{:08X}", data.serial_number),
                "firmware_version": data.firmware_version,
                "hardware_version": data.hardware_version,
                "design_capacity_mah": data.design_capacity_mah,
                "design_voltage_mv": data.design_voltage_mv,
                "num_cells": data.num_cells
            }),
        ),
        Some(Response::RegisterData(data)) => (
            "RegisterData",
            json!({
                "address": format!("0x{:02X}", data.address),
                "value": data.value,
                "num_bytes": data.num_bytes
            }),
        ),
        Some(Response::BlockData(data)) => (
            "BlockData",
            json!({
                "address": format!("0x{:02X}", data.address),
                "data_len": data.data.len(),
                "data_hex": hex_encode(&data.data)
            }),
        ),
        Some(Response::AckData(data)) => (
            "AckData",
            json!({
                "success": data.success,
                "message": data.message
            }),
        ),
        Some(Response::ManufacturerAccessData(data)) => (
            "ManufacturerAccessData",
            json!({
                "subcommand": format!("0x{:04X}", data.subcommand),
                "data_len": data.data.len(),
                "data_hex": hex_encode(&data.data)
            }),
        ),
        Some(Response::ProtectionStatusData(data)) => (
            "ProtectionStatusData",
            json!({
                "cell_overvoltage": data.cell_overvoltage,
                "pack_overvoltage": data.pack_overvoltage,
                "cell_undervoltage": data.cell_undervoltage,
                "pack_undervoltage": data.pack_undervoltage,
                "charge_overcurrent": data.charge_overcurrent,
                "discharge_overcurrent": data.discharge_overcurrent,
                "short_circuit": data.short_circuit,
                "overtemperature_charge": data.overtemperature_charge,
                "overtemperature_discharge": data.overtemperature_discharge,
                "undertemperature_charge": data.undertemperature_charge,
                "undertemperature_discharge": data.undertemperature_discharge,
                "cell_balancing_active": data.cell_balancing_active,
                "permanent_failure": data.permanent_failure,
                "safety_status_alert": data.safety_status_alert
            }),
        ),
        None => ("NoResponse", json!({})),
    };

    // Include header info if present
    let mut result = serde_json::Map::new();
    if let Some(header) = &resp.header {
        result.insert("request_id".to_string(), json!(header.request_id));
        result.insert("status".to_string(), json!(header.status));
        if !header.error_message.is_empty() {
            result.insert("error_message".to_string(), json!(header.error_message));
        }
        result.insert("latency_us".to_string(), json!(header.latency_us));
    }
    result.insert("response".to_string(), response_fields);

    (payload_type.to_string(), serde_json::Value::Object(result))
}

/// Helper function to encode bytes as hex string
fn hex_encode(data: &[u8]) -> String {
    data.iter()
        .map(|b| format!("{:02X}", b))
        .collect::<Vec<_>>()
        .join(" ")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_packet_capture_new() {
        let capture = PacketCapture::new();
        assert!(capture.is_enabled());
        assert_eq!(capture.count(), (0, 0));
    }

    #[test]
    fn test_packet_capture_default() {
        let _capture = PacketCapture::default();
    }

    #[test]
    fn test_capture_raw_packet() {
        let capture = PacketCapture::new();
        let data = vec![0x55, 0xAA, 0x01, 0x02, 0x03];

        capture.capture_raw(PacketDirection::Tx, &data, "Test TX packet");

        let (raw_count, parsed_count) = capture.count();
        assert_eq!(raw_count, 1);
        assert_eq!(parsed_count, 0);

        let packets = capture.get_raw_packets(10, 0);
        assert_eq!(packets.len(), 1);
        assert_eq!(packets[0].direction, PacketDirection::Tx);
        assert_eq!(packets[0].data, data);
        assert_eq!(packets[0].description, "Test TX packet");
    }

    #[test]
    fn test_capture_parsed_packet() {
        let capture = PacketCapture::new();
        let fields = serde_json::json!({"voltage_mv": 14800});

        capture.capture_parsed(
            PacketDirection::Rx,
            42,
            0x02,
            "TelemetryData",
            fields.clone(),
        );

        let (raw_count, parsed_count) = capture.count();
        assert_eq!(raw_count, 0);
        assert_eq!(parsed_count, 1);

        let packets = capture.get_parsed_packets(10, 0);
        assert_eq!(packets.len(), 1);
        assert_eq!(packets[0].direction, PacketDirection::Rx);
        assert_eq!(packets[0].frame_seq, 42);
        assert_eq!(packets[0].frame_type, 0x02);
        assert_eq!(packets[0].payload_type, "TelemetryData");
        assert_eq!(packets[0].fields, fields);
    }

    #[test]
    #[test]
    fn test_clear_buffers() {
        let capture = PacketCapture::new();

        capture.capture_raw(PacketDirection::Tx, &[0x01], "Test");
        capture.capture_parsed(PacketDirection::Rx, 1, 0x02, "Test", serde_json::json!({}));

        assert_eq!(capture.count(), (1, 1));

        capture.clear();

        assert_eq!(capture.count(), (0, 0));
    }

    #[test]
    fn test_enable_disable_capture() {
        let capture = PacketCapture::new();

        // Capture while enabled
        capture.capture_raw(PacketDirection::Tx, &[0x01], "Enabled");
        assert_eq!(capture.count().0, 1);

        // Disable and try to capture
        capture.set_enabled(false);
        assert!(!capture.is_enabled());
        capture.capture_raw(PacketDirection::Tx, &[0x02], "Disabled");
        assert_eq!(capture.count().0, 1); // Still 1, not captured

        // Re-enable and capture
        capture.set_enabled(true);
        capture.capture_raw(PacketDirection::Tx, &[0x03], "Re-enabled");
        assert_eq!(capture.count().0, 2);
    }

    #[test]
    fn test_pagination() {
        let capture = PacketCapture::new();

        // Add 10 packets
        for i in 0..10 {
            capture.capture_raw(PacketDirection::Tx, &[i], &format!("Packet {}", i));
        }

        // Get first 3
        let first_page = capture.get_raw_packets(3, 0);
        assert_eq!(first_page.len(), 3);
        assert_eq!(first_page[0].data, vec![0u8]);
        assert_eq!(first_page[2].data, vec![2u8]);

        // Get next 3 (offset 3)
        let second_page = capture.get_raw_packets(3, 3);
        assert_eq!(second_page.len(), 3);
        assert_eq!(second_page[0].data, vec![3u8]);
        assert_eq!(second_page[2].data, vec![5u8]);

        // Get last 4 (offset 6)
        let last_page = capture.get_raw_packets(10, 6);
        assert_eq!(last_page.len(), 4);
        assert_eq!(last_page[0].data, vec![6u8]);
        assert_eq!(last_page[3].data, vec![9u8]);

        // Offset beyond buffer
        let empty_page = capture.get_raw_packets(10, 100);
        assert!(empty_page.is_empty());
    }

    #[test]
    fn test_get_current_timestamp_us() {
        let ts1 = get_current_timestamp_us();
        std::thread::sleep(std::time::Duration::from_millis(1));
        let ts2 = get_current_timestamp_us();

        assert!(ts2 > ts1);
        assert!(ts2 - ts1 >= 1000); // At least 1000 microseconds (1ms)
    }

    #[test]
    fn test_hex_encode() {
        assert_eq!(hex_encode(&[0x00, 0xFF, 0x55, 0xAA]), "00 FF 55 AA");
        assert_eq!(hex_encode(&[]), "");
        assert_eq!(hex_encode(&[0x42]), "42");
    }

    #[test]
    fn test_packet_direction_serialization() {
        let tx = PacketDirection::Tx;
        let rx = PacketDirection::Rx;

        assert_eq!(serde_json::to_string(&tx).unwrap(), "\"tx\"");
        assert_eq!(serde_json::to_string(&rx).unwrap(), "\"rx\"");

        let tx_deser: PacketDirection = serde_json::from_str("\"tx\"").unwrap();
        let rx_deser: PacketDirection = serde_json::from_str("\"rx\"").unwrap();

        assert_eq!(tx_deser, PacketDirection::Tx);
        assert_eq!(rx_deser, PacketDirection::Rx);
    }

    #[test]
    fn test_raw_packet_serialization() {
        let packet = RawPacket {
            timestamp_us: 1234567890,
            direction: PacketDirection::Tx,
            data: vec![0x55, 0xAA, 0x01],
            description: "Test packet".to_string(),
        };

        let json = serde_json::to_string(&packet).unwrap();
        let deser: RawPacket = serde_json::from_str(&json).unwrap();

        assert_eq!(deser.timestamp_us, packet.timestamp_us);
        assert_eq!(deser.direction, packet.direction);
        assert_eq!(deser.data, packet.data);
        assert_eq!(deser.description, packet.description);
    }

    #[test]
    fn test_parsed_packet_serialization() {
        let packet = ParsedPacket {
            timestamp_us: 1234567890,
            direction: PacketDirection::Rx,
            frame_seq: 42,
            frame_type: 0x02,
            payload_type: "TelemetryData".to_string(),
            fields: serde_json::json!({"voltage_mv": 14800}),
        };

        let json = serde_json::to_string(&packet).unwrap();
        let deser: ParsedPacket = serde_json::from_str(&json).unwrap();

        assert_eq!(deser.timestamp_us, packet.timestamp_us);
        assert_eq!(deser.direction, packet.direction);
        assert_eq!(deser.frame_seq, packet.frame_seq);
        assert_eq!(deser.frame_type, packet.frame_type);
        assert_eq!(deser.payload_type, packet.payload_type);
        assert_eq!(deser.fields, packet.fields);
    }
}
