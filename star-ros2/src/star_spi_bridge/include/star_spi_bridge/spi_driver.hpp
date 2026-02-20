// Copyright 2026 Locked Inc.

#ifndef STAR_SPI_BRIDGE__SPI_DRIVER_HPP_
#define STAR_SPI_BRIDGE__SPI_DRIVER_HPP_

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace star_spi_bridge
{

/**
 * @brief Frame type identifiers for SPI protocol messages.
 *
 * @details
 * Each frame carries a one-byte type field at offset 6 in the header.
 * Values match the RX72N firmware rx_frame_type_t enum so both sides
 * agree on the semantics of every message.
 *
 * @see SpiDriver::encode_frame  Frame construction
 * @see SpiDriver::decode_frame  Frame parsing
 *
 * @since Version 1.0.0
 */
enum class FrameType : uint8_t
{
  VelocityCommand = 0x01,  /**< Motor velocity setpoint (RPi5 -> RX72N) */
  TelemetryData = 0x02     /**< Sensor telemetry payload  (RX72N -> RPi5) */
};

/**
 * @brief SPI driver for framed communication with the RX72N peripheral MCU.
 *
 * @details
 * Implements the STAR binary framing protocol over Linux spidev.
 * The wire format is:
 *
 *   SYNC(2, LE) + SEQ(2, LE) + LEN(2, LE) + TYPE(1) + FLAGS(1)
 *   + PAYLOAD(0-1024) + CRC-32 (IEEE 802.3, 4 B, LE)
 *
 * The Raspberry Pi 5 acts as SPI controller at 10 MHz, Mode 0 (CPOL=0, CPHA=0).
 *
 * @par Thread Safety
 * - CRC-32 table initialisation is thread-safe (`std::call_once`).
 * - `encode_frame` / `decode_frame` / `calculate_crc32` are stateless and safe
 *   to call from any thread.
 * - `initialize`, `close_device`, and `transfer` share `spi_fd_` and must
 *   not be called concurrently on the same instance.
 *
 * @invariant After a successful `initialize()`, `spi_fd_ >= 0`.
 *
 * @see star_spi_bridge::FrameType  Supported frame types
 *
 * @since Version 1.0.0
 */
class SpiDriver {
public:
  // -- Frame-structure constants ------------------------------------------
  /// @brief SYNC + SEQ + LEN + TYPE + FLAGS = 8 bytes.
  static constexpr size_t k_header_size = 8;

  /// @brief CRC-32 trailer length in bytes.
  static constexpr size_t k_crc_size = 4;

  /// @brief Maximum application payload per frame (bytes).
  static constexpr size_t k_max_payload_size = 1024;

  /// @brief SYNC word transmitted in every frame header.
  static constexpr uint16_t k_sync_word = 0x55AA;

  /**
   * @brief Construct a new SpiDriver.
   *
   * @param[in] device_path  Path to the Linux spidev node (e.g. "/dev/spidev0.0").
   * @param[in] speed_hz     SPI clock frequency in Hz (default 10 MHz).
   *
   * @pre  device_path is a non-empty, null-terminated string.
   * @post CRC-32 lookup table is initialised (thread-safe, once).
   */
  explicit SpiDriver(
    const std::string & device_path,
    uint32_t speed_hz = 10000000);

  /// @brief Destructor -- calls close_device() if the device is still open.
  virtual ~SpiDriver();

  /**
   * @brief Open and configure the SPI device.
   *
   * @details
   * Opens the spidev file descriptor, sets Mode 0, 8 bits per word, and the
   * requested clock speed via ioctl calls.
   *
   * @return true  Device opened and configured successfully.
   * @return false Open or ioctl failed (error logged via RCLCPP_ERROR).
   *
   * @pre  `device_path_` refers to a valid spidev node.
   * @post On success, `spi_fd_ >= 0` and the device is ready for `transfer`.
   *
   * @note Only supported on Linux; returns false on other platforms.
   */
  bool initialize();

  /**
   * @brief Close the SPI device file descriptor.
   *
   * @post `spi_fd_ == -1`.
   */
  void close_device();

  /**
   * @brief Perform a full-duplex SPI transfer.
   *
   * @param[in]  tx_data  Data to transmit.
   * @param[out] rx_data  Buffer filled with received data (resized to match tx_data).
   *
   * @return true  Transfer completed successfully.
   * @return false Device not open or ioctl failed.
   *
   * @pre  `initialize()` returned true.
   * @post `rx_data.size() == tx_data.size()`.
   *
   * @note Only supported on Linux; returns false on other platforms.
   */
  bool transfer(
    const std::vector<uint8_t> & tx_data,
    std::vector<uint8_t> & rx_data);

  /**
   * @brief Encode an application payload into the STAR wire format.
   *
   * @details
   * Builds the header (little-endian SYNC, SEQ, LEN, TYPE, FLAGS),
   * appends the payload, then computes and appends the CRC-32.
   * The output buffer is cleared and filled in place for reuse.
   *
   * @param[in]  seq        Sequence number (0-65535).
   * @param[in]  type       Frame type (see FrameType enum).
   * @param[in]  flags      Control flags byte.
   * @param[in]  payload    Application data (0-k_max_payload_size bytes).
   * @param[out] out_frame  Encoded wire bytes (header + payload + CRC).
   *
   * @pre  `payload.size() <= k_max_payload_size`.
   * @post `out_frame.size() == k_header_size + payload.size() + k_crc_size`.
   *
   * @throws std::invalid_argument  If `payload.size() > k_max_payload_size`.
   *
   * @see decode_frame  Inverse operation.
   */
  static void encode_frame(
    uint16_t seq, FrameType type, uint8_t flags,
    const std::vector<uint8_t> & payload,
    std::vector<uint8_t> & out_frame);

  /**
   * @brief Decode a STAR wire frame into its constituent fields.
   *
   * @details
   * Validates SYNC word, frame length, and CRC-32.  On success the sequence
   * number, type, flags, and payload are written to the output parameters.
   *
   * @param[in]  frame    Raw wire bytes to parse.
   * @param[out] seq      Decoded sequence number.
   * @param[out] type     Decoded frame type.
   * @param[out] flags    Decoded flags byte.
   * @param[out] payload  Extracted payload bytes.
   *
   * @return true   Frame valid (SYNC, length, and CRC all match).
   * @return false  Frame invalid or corrupted.
   *
   * @pre  `frame.size() >= k_header_size + k_crc_size` (12 bytes minimum).
   * @post On success, all output parameters are populated.
   *
   * @see encode_frame  Inverse operation.
   */
  static bool decode_frame(
    const std::vector<uint8_t> & frame, uint16_t & seq,
    FrameType & type, uint8_t & flags,
    std::vector<uint8_t> & payload);

  /**
   * @brief Compute the IEEE 802.3 CRC-32 of the given byte vector.
   *
   * @param[in] data  Input bytes.
   * @return CRC-32 value.
   *
   * @pre  CRC-32 table has been initialised (guaranteed by the constructor).
   * @post Return value matches `crc32(0, data, len)` from zlib.
   *
   * @note Public for unit-test access.
   */
  static uint32_t calculate_crc32(const std::vector<uint8_t> & data);

private:
  std::string device_path_;
  uint32_t speed_hz_;
  int spi_fd_;

  static const uint32_t k_crc32_polynomial = 0x04C11DB7;
  static constexpr uint8_t k_bits_per_word = 8;
  // [SYNC: 0x55AA (2B, LE) - wire: 0xAA, 0x55]
  // [SEQ: sequence number (2B, LE)]
  // [LEN: payload length (2B, LE)]
  // [TYPE: frame type (1B)]
  // [FLAGS: control flags (1B)]
  // Total header = 2+2+2+1+1 = 8 bytes.

  static uint32_t crc32_table_[256];
  static std::once_flag crc32_table_init_flag_;
  static void init_crc32_table();
};

}  // namespace star_spi_bridge

#endif  // STAR_SPI_BRIDGE__SPI_DRIVER_HPP_
