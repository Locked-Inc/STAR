/**
 * @file spi_driver.cpp
 * @brief SPI driver implementation for framed communication with the RX72N
 * peripheral MCU.
 *
 * @details
 * Implements SpiDriver: Linux spidev ioctl handling, STAR binary frame
 * encoding/decoding, and IEEE 802.3 CRC-32 computation using a lookup table
 * initialised once via std::call_once.
 *
 * @see star_spi_bridge/spi_driver.hpp
 *
 * @author Locked Inc.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "star_spi_bridge/spi_driver.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

#include <rclcpp/rclcpp.hpp>

#ifdef __linux__
#include <linux/spi/spidev.h>
#else
// Mock spidev headers if on macOS/non-Linux to allow compilation (for
// development/analysis only)
// Ideally this code runs on Linux/RPi5. On macOS, these headers likely
// don't exist or are different.
#ifndef SPI_IOC_MESSAGE
#define SPI_IOC_MESSAGE(N) 0
constexpr uint8_t SPI_MODE_0 = 0;
constexpr int SPI_IOC_WR_MODE = 0;
constexpr int SPI_IOC_WR_BITS_PER_WORD = 0;
constexpr int SPI_IOC_WR_MAX_SPEED_HZ = 0;

struct spi_ioc_transfer
{
  uint64_t tx_buf_;
  uint64_t rx_buf_;
  uint32_t len_;
  uint32_t speed_hz_;
  uint16_t delay_usecs_;
  uint8_t bits_per_word_;
  uint8_t cs_change_;
  uint32_t pad_;
};
#endif
#endif

namespace star_spi_bridge
{

namespace
{
/** @brief ROS2 logger for SPI driver diagnostics. */
auto logger()
{
  return rclcpp::get_logger("star_spi_bridge.spi_driver");
}

inline void set_spi_xfer_tx_buf(spi_ioc_transfer & xfer, uint64_t value)
{
#ifdef __linux__
  xfer.tx_buf = value;
#else
  xfer.tx_buf_ = value;
#endif
}

inline void set_spi_xfer_rx_buf(spi_ioc_transfer & xfer, uint64_t value)
{
#ifdef __linux__
  xfer.rx_buf = value;
#else
  xfer.rx_buf_ = value;
#endif
}

inline void set_spi_xfer_len(spi_ioc_transfer & xfer, uint32_t value)
{
#ifdef __linux__
  xfer.len = value;
#else
  xfer.len_ = value;
#endif
}

inline void set_spi_xfer_speed_hz(spi_ioc_transfer & xfer, uint32_t value)
{
#ifdef __linux__
  xfer.speed_hz = value;
#else
  xfer.speed_hz_ = value;
#endif
}

inline void set_spi_xfer_bits_per_word(spi_ioc_transfer & xfer, uint8_t value)
{
#ifdef __linux__
  xfer.bits_per_word = value;
#else
  xfer.bits_per_word_ = value;
#endif
}

/**
 * @brief Check whether a FrameType value is one of the eight valid protocol types.
 *
 * @details
 * Validates the frame type field decoded from an incoming SPI frame against the
 * eight explicit values defined in the STAR SPI protocol (rx_frame.h on the
 * RX72N firmware side).  A contiguous range check is intentionally avoided
 * because valid codes are sparse: 0x00-0x01, 0x10-0x13, 0xFE-0xFF.  Any byte
 * not in this enumerated set is treated as an unknown or corrupted frame type.
 *
 * @param[in] frame_type  FrameType value to validate.  May be any uint8_t
 *                        bit-pattern cast to FrameType.
 *
 * @return true   The value is one of: Ping, Pong, Command, Response, Ack,
 *                Nack, ResetAck, or Reset.
 * @return false  The value does not match any known FrameType enumerator.
 *
 * @throw None  This function is noexcept and does not throw.
 *
 * @see star_spi_bridge::FrameType  Enumeration of all valid frame types.
 * @see SpiDriver::decode_frame  Primary caller; rejects frames with unknown types.
 *
 * @since Version 1.0.0
 */
inline bool is_valid_frame_type(FrameType frame_type) noexcept
{
  // Accept exactly the 8 frame type values defined in rx_frame.h.
  // A contiguous range check would accept invalid bytes between 0x02 and 0x0F
  // and between 0x14 and 0xFD, so we enumerate valid values explicitly.
  switch (frame_type) {
    case FrameType::Ping:
    case FrameType::Pong:
    case FrameType::Command:
    case FrameType::Response:
    case FrameType::Ack:
    case FrameType::Nack:
    case FrameType::ResetAck:
    case FrameType::Reset:
      return true;
    default:
      return false;
  }
}
}  // namespace

uint32_t SpiDriver::crc32_table_[256];
std::once_flag SpiDriver::crc32_table_init_flag_;

/**
 * @brief Construct a SpiDriver for the given spidev node.
 * @param[in] device_path  Path to the Linux spidev node (e.g. "/dev/spidev0.0");
 *                         must be non-empty.
 * @param[in] speed_hz     SPI clock frequency in Hz; must be > 0.
 * @pre  device_path is non-empty.
 * @pre  speed_hz > 0; zero-Hz SPI clock is rejected by the kernel.
 * @post spi_fd_ is -1 until initialize() is called.
 * @post CRC-32 lookup table is initialised (thread-safe, once).
 */
SpiDriver::SpiDriver(const std::string & device_path, uint32_t speed_hz)
: device_path_(device_path), speed_hz_(speed_hz), spi_fd_(-1)
{
  assert(!device_path_.empty());
  assert(speed_hz_ > 0);
  std::call_once(crc32_table_init_flag_, init_crc32_table);
}

/**
 * @brief Destroy the SpiDriver, releasing the SPI device.
 * @details Calls close_device() to close spi_fd_ if the device is still open.
 * @post spi_fd_ is closed and set to -1.
 * @post All instance resources are freed.
 */
SpiDriver::~SpiDriver()
{
  close_device();
}

bool SpiDriver::initialize()
{
#ifndef __linux__
  RCLCPP_ERROR(logger(), "SPI driver is only supported on Linux platforms");
  return false;
#endif
  // Open SPI device
  spi_fd_ = open(device_path_.c_str(), O_RDWR);
  if (spi_fd_ < 0) {
    RCLCPP_ERROR(logger(), "Failed to open SPI device: %s (%s)",
                 device_path_.c_str(), strerror(errno));
    return false;
  }

  // Configure SPI mode (Mode 0: CPOL=0, CPHA=0)
  uint8_t mode = SPI_MODE_0;
  if (ioctl(spi_fd_, SPI_IOC_WR_MODE, &mode) < 0) {
    RCLCPP_ERROR(logger(), "Failed to set SPI mode");
    close_device();
    return false;
  }

  // Configure bits per word (8 bits)
  uint8_t bits = BITS_PER_WORD;
  if (ioctl(spi_fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
    RCLCPP_ERROR(logger(), "Failed to set SPI bits per word");
    close_device();
    return false;
  }

  // Configure max speed
  if (ioctl(spi_fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz_) < 0) {
    RCLCPP_ERROR(logger(), "Failed to set SPI max speed");
    close_device();
    return false;
  }

  return true;
}

/**
 * @brief Close the SPI device file descriptor.
 *
 * @details
 * Closes spi_fd_ and resets it to -1.  Safe to call multiple times; if
 * the device is already closed (spi_fd_ < 0) this is a no-op.  Called
 * automatically by the destructor to guarantee resource release.
 *
 * @pre  None; may be called regardless of whether initialize() succeeded.
 * @post spi_fd_ == -1.
 * @post The underlying file descriptor (if it was open) is released to the OS.
 *
 * @note Thread-safe only when no concurrent transfer() calls are in progress;
 *       callers must quiesce I/O before calling close_device().
 * @since Version 1.0.0
 */
void SpiDriver::close_device()
{
  if (spi_fd_ >= 0) {
    close(spi_fd_);
    spi_fd_ = -1;
  }
}

bool SpiDriver::transfer(
  const std::vector<uint8_t> & tx_data,
  std::vector<uint8_t> & rx_data)
{
#ifndef __linux__
  RCLCPP_ERROR(logger(), "SPI driver is only supported on Linux platforms");
  return false;
#endif
  if (spi_fd_ < 0) {
    return false;
  }

  if (rx_data.size() != tx_data.size()) {
    rx_data.resize(tx_data.size());
  }

  struct spi_ioc_transfer xfer;
  std::memset(&xfer, 0, sizeof(xfer));

  set_spi_xfer_tx_buf(
      xfer, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tx_data.data())));
  set_spi_xfer_rx_buf(
      xfer, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(rx_data.data())));
  set_spi_xfer_len(xfer, static_cast<uint32_t>(tx_data.size()));
  set_spi_xfer_speed_hz(xfer, speed_hz_);
  set_spi_xfer_bits_per_word(xfer, BITS_PER_WORD);

  int ret = ioctl(spi_fd_, SPI_IOC_MESSAGE(1), &xfer);
  if (ret < 0) {
    RCLCPP_ERROR(logger(), "SPI transfer failed: %s", strerror(errno));
    return false;
  }

  return true;
}

void SpiDriver::encode_frame(
  uint16_t seq, FrameType type, uint8_t flags,
  const std::vector<uint8_t> & payload,
  std::vector<uint8_t> & out_frame)
{
  if (payload.size() > MAX_PAYLOAD_SIZE) {
    throw std::invalid_argument("payload exceeds maximum size of 1024 bytes");
  }
  if (!is_valid_frame_type(type)) {
    throw std::invalid_argument("frame type is outside valid enum range");
  }

  // [SYNC(2)][SEQ(2)][LEN(2)][TYPE(1)][FLAGS(1)][PAYLOAD(N)][CRC(4)]
  size_t frame_size = HEADER_SIZE + payload.size() + CRC_SIZE;
  out_frame.clear();
  out_frame.reserve(frame_size);

  // SYNC: 0x55AA (Little Endian)
  // Note: Header fields (SYNC, SEQ, LEN) use little-endian byte order
  // Wire format: [0xAA, 0x55] for 0x55AA
  out_frame.push_back(SYNC_WORD & 0xFF);        // LSB first (0xAA)
  out_frame.push_back((SYNC_WORD >> 8) & 0xFF); // MSB second (0x55)

  // SEQ (Little Endian)
  out_frame.push_back(seq & 0xFF);        // LSB first
  out_frame.push_back((seq >> 8) & 0xFF); // MSB second

  // LEN (Little Endian)
  uint16_t len = static_cast<uint16_t>(payload.size());
  out_frame.push_back(len & 0xFF);        // LSB first
  out_frame.push_back((len >> 8) & 0xFF); // MSB second

  // TYPE
  out_frame.push_back(static_cast<uint8_t>(type));

  // FLAGS
  out_frame.push_back(flags);

  // PAYLOAD
  out_frame.insert(out_frame.end(), payload.begin(), payload.end());

  // CRC-32 (IEEE 802.3) - Calculated over Header + Payload
  uint32_t crc = calculate_crc32(out_frame);

  // Append CRC (Little Endian as per spec "CRC-32: IEEE 802.3 (4B, LE)")
  out_frame.push_back(crc & 0xFF);
  out_frame.push_back((crc >> 8) & 0xFF);
  out_frame.push_back((crc >> 16) & 0xFF);
  out_frame.push_back((crc >> 24) & 0xFF);

  if (out_frame.size() != frame_size) {
    throw std::logic_error("encoded frame size does not match expected layout");
  }
  if (out_frame[0] != (SYNC_WORD & 0xFF) ||
    out_frame[1] != ((SYNC_WORD >> 8) & 0xFF))
  {
    throw std::logic_error("encoded frame sync word mismatch");
  }
}

bool SpiDriver::decode_frame(
  const std::vector<uint8_t> & frame, uint16_t & seq,
  FrameType & type, uint8_t & flags,
  std::vector<uint8_t> & payload)
{
  if (frame.size() < HEADER_SIZE + CRC_SIZE) {
    return false;
  }

  // Check SYNC (Little Endian: [0xAA, 0x55] for 0x55AA)
  if (frame[0] != (SYNC_WORD & 0xFF) || frame[1] != ((SYNC_WORD >> 8) & 0xFF)) {
    return false;
  }

  // Extract Length (Little Endian: LSB first)
  uint16_t len = frame[4] | (static_cast<uint16_t>(frame[5]) << 8);

  // Audit F-03: reject frames that claim a payload longer than the
  // protocol maximum BEFORE arithmetic on (HEADER_SIZE + len + CRC_SIZE).
  // The firmware (rx_frame.c) and Go gateway (decoder.go) already do this;
  // mirror it here so a malicious or corrupted SYNC+LEN cannot trick us
  // into accepting a frame just because the wire-byte count happens to
  // match HEADER_SIZE + len + CRC_SIZE for some absurd len.
  if (len > MAX_PAYLOAD_SIZE) {
    static rclcpp::Clock throttle_clock{RCL_STEADY_TIME};
    RCLCPP_WARN_THROTTLE(logger(), throttle_clock, 1000,
                         "decode_frame: payload length %u exceeds MAX_PAYLOAD_SIZE %zu",
                         static_cast<unsigned>(len), MAX_PAYLOAD_SIZE);
    return false;
  }

  // Validate Frame Size
  // Header(8) + Payload(len) + CRC(4)
  if (frame.size() != HEADER_SIZE + len + CRC_SIZE) {
    return false;
  }

  // Verify CRC
  // Calculate CRC over Header + Payload (bytes 0 to end-4)
  std::vector<uint8_t> data_to_check(frame.begin(), frame.end() - CRC_SIZE);
  uint32_t calculated_crc = calculate_crc32(data_to_check);

  uint32_t received_crc =
    static_cast<uint32_t>(frame[frame.size() - 4]) |
    (static_cast<uint32_t>(frame[frame.size() - 3]) << 8) |
    (static_cast<uint32_t>(frame[frame.size() - 2]) << 16) |
    (static_cast<uint32_t>(frame[frame.size() - 1]) << 24);

  if (calculated_crc != received_crc) {
    return false;
  }

  // Extract fields (Little Endian: LSB first)
  seq = frame[2] | (static_cast<uint16_t>(frame[3]) << 8);
  type = static_cast<FrameType>(frame[6]);

  // Reject frames carrying an unknown or reserved frame type byte
  if (!is_valid_frame_type(type)) {
    static rclcpp::Clock throttle_clock{RCL_STEADY_TIME};
    RCLCPP_WARN_THROTTLE(logger(), throttle_clock, 1000,
                         "decode_frame: unknown frame type 0x%02X",
                         static_cast<uint8_t>(type));
    return false;
  }

  flags = frame[7];

  payload.assign(frame.begin() + HEADER_SIZE, frame.end() - CRC_SIZE);

  return true;
}

void SpiDriver::init_crc32_table()
{
  // Generate CRC-32 lookup table using IEEE 802.3 polynomial (reflected form)
  // Polynomial 0xEDB88320 is the bit-reversed form of 0x04C11DB7
  // This produces standard CRC-32 values (e.g., "123456789" -> 0xCBF43926)
  constexpr uint32_t polynomial = 0xEDB88320;
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t crc = i;
    for (uint32_t j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ polynomial;
      } else {
        crc >>= 1;
      }
    }
    crc32_table_[i] = crc;
  }
}

uint32_t SpiDriver::calculate_crc32(const std::vector<uint8_t> & data)
{
  std::call_once(crc32_table_init_flag_, init_crc32_table);
  uint32_t crc = 0xFFFFFFFF;
  for (uint8_t byte : data) {
    uint8_t lookup_index = (crc ^ byte) & 0xFF;
    crc = (crc >> 8) ^ crc32_table_[lookup_index];
  }
  return crc ^ 0xFFFFFFFF;
}

}  // namespace star_spi_bridge
