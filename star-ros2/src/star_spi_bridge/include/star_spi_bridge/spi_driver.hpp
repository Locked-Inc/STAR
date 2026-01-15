// Copyright 2026 Locked Inc.

#ifndef STAR_SPI_BRIDGE_INCLUDE_STAR_SPI_BRIDGE_SPI_DRIVER_HPP_
#define STAR_SPI_BRIDGE_INCLUDE_STAR_SPI_BRIDGE_SPI_DRIVER_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace star_spi_bridge
{

enum class FrameType : uint8_t { VelocityCommand = 0x01, TelemetryData = 0x02 };

class SpiDriver
{
public:
  explicit SpiDriver(const std::string & device_path, uint32_t speed_hz = 10000000);
  virtual ~SpiDriver();

  bool initialize();
  void close_device();

  // Full duplex transfer
  // Returns true on success, false on failure
  bool transfer(const std::vector<uint8_t> & tx_data, std::vector<uint8_t> & rx_data);

  // Frame encoding/decoding helpers
  // Optimized: uses output parameter to allow buffer reuse
  static void encode_frame(
    uint16_t seq, FrameType type, uint8_t flags, const std::vector<uint8_t> & payload,
    std::vector<uint8_t> & out_frame);
  static bool decode_frame(
    const std::vector<uint8_t> & frame, uint16_t & seq, FrameType & type, uint8_t & flags,
    std::vector<uint8_t> & payload);

  // CRC-32 helper (public for testing)
  static uint32_t calculate_crc32(const std::vector<uint8_t> & data);

private:
  std::string device_path_;
  uint32_t speed_hz_;
  int spi_fd_;

  static const uint32_t k_crc32_polynomial = 0x04C11DB7;
  static const uint16_t k_sync_word = 0x55AA;
  static const size_t k_header_size = 8;  // SYNC(2) + SEQ(2) + LEN(2) + TYPE(1) + FLAGS(1)
  static constexpr uint8_t k_bits_per_word = 8;
  // [SYNC: 0x55AA (2B, BE)]
  // [SEQ: sequence number (2B, BE)]
  // [LEN: payload length (2B, BE)]
  // [TYPE: frame type (1B)]
  // [FLAGS: control flags (1B)]
  // Total header = 2+2+2+1+1 = 8 bytes.

  static uint32_t crc32_table_[256];
  static bool crc32_table_initialized_;
  static void init_crc32_table();
};

}  // namespace star_spi_bridge

#endif  // STAR_SPI_BRIDGE_INCLUDE_STAR_SPI_BRIDGE_SPI_DRIVER_HPP_
