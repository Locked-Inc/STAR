/**
 * @file spi_driver.hpp
 * @brief SPI driver declarations for framed communication with the RX72N
 * peripheral MCU.
 *
 * @details
 * Declares SpiDriver and FrameType for the STAR binary framing protocol over
 * Linux spidev. The Raspberry Pi 5 acts as SPI controller at 10 MHz, Mode 0
 * (CPOL=0, CPHA=0). Frame format: SYNC(2) + SEQ(2) + LEN(2) + TYPE(1) +
 * FLAGS(1) + PAYLOAD(N) + CRC-32(4), all multi-byte fields little-endian.
 *
 * @author Locked Inc.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

namespace star_spi_bridge
{

/**
 * @brief Frame type identifiers for SPI protocol messages.
 *
 * @details
 * Each frame carries a one-byte type field at offset 6 in the header.
 * Values match the RX72N firmware rx_frame_type_t enum (rx_frame.h) and
 * the Go gateway frame.go so all three sides agree on message semantics.
 *
 * @see SpiDriver::encode_frame  Frame construction
 * @see SpiDriver::decode_frame  Frame parsing
 * @see star_spi_bridge::FrameFlags  Control flags companion enum
 *
 * @since Version 1.0.0
 */
enum class FrameType : uint8_t
{
  Ping = 0x00,        /**< Keepalive request (RPi5 <-> RX72N) */
  Pong = 0x01,        /**< Keepalive response echoing the PING payload */
  Command = 0x10,     /**< Protobuf-encoded command (RPi5 -> RX72N) */
  Response = 0x11,    /**< Protobuf-encoded response (RX72N -> RPi5) */
  Ack = 0x12,         /**< Positive acknowledgment, empty payload (SPI only) */
  Nack = 0x13,        /**< Negative acknowledgment, empty payload (SPI only) */
  /* 0x20 (LogMessage) intentionally absent: firmware sends log frames
   * over UART only; comm_task.c sets channel=k_comm_channel_uart for
   * log frames, and is_valid_frame_type() rejects 0x20 on SPI. */
  ResetAck = 0xFE,    /**< Confirms reset completed; seq matches reset request */
  Reset = 0xFF,       /**< Request to reset communication state */
};

/**
 * @brief Frame control flags for SPI protocol messages.
 *
 * @details
 * Bit flags that modify frame handling. Multiple flags may be combined with
 * bitwise OR. Values match the RX72N firmware rx_frame_flags_t enum
 * (rx_frame.h) and the Go gateway flags.go.
 *
 * @see star_spi_bridge::FrameType  Frame type companion enum
 *
 * @since Version 1.0.0
 */
enum class FrameFlags : uint8_t
{
  None = 0x00,        /**< No flags set; default for frames needing no special handling */
  RequiresAck = 0x01, /**< Sender expects an ACK or NACK response (bit 0) */
  Retransmit = 0x02,  /**< This is a retry after timeout or NACK (bit 1) */
  Priority = 0x04,    /**< Process before normal-priority frames (bit 2) */
  FecEnabled = 0x08,  /**< Payload uses forward error correction (bit 3) */
  SoftNack = 0x10,    /**< NACK with recoverable soft-error info (bit 4) */
};

/** @brief Bitmask covering all valid FrameFlags bits (bits 0-4); bits 5-7 are reserved. */
constexpr uint8_t FRAME_FLAGS_MASK = 0x1Fu;

/**
 * @defgroup FrameFlagsOperators FrameFlags Bitwise Operators
 * @brief Bitwise operator overloads for combining FrameFlags values.
 *
 * @details
 * These constexpr operators allow FrameFlags enumerators to be combined
 * without verbose static_cast boilerplate.  All operators work by casting to
 * and from the underlying type (uint8_t via std::underlying_type_t<FrameFlags>).
 *
 * Example:
 * @code
 * const uint8_t flags = static_cast<uint8_t>(
 *     FrameFlags::RequiresAck | FrameFlags::Priority);
 * @endcode
 *
 * @since Version 1.0.0
 * @{
 */

/**
 * @brief Bitwise OR for FrameFlags.
 * @param[in] lhs  Left-hand operand.
 * @param[in] rhs  Right-hand operand.
 * @return FrameFlags result of lhs | rhs.
 * @pre  lhs is a valid FrameFlags value or combination of FrameFlags enumerators.
 * @pre  rhs is a valid FrameFlags value or combination of FrameFlags enumerators.
 * @post The returned value has exactly the union of bits set in lhs and rhs.
 * @post No bits outside the FrameFlags underlying type (uint8_t) are affected.
 * @note Constexpr and thread-safe; has no side effects.
 * @since Version 1.0.0
 */
constexpr inline FrameFlags operator|(FrameFlags lhs, FrameFlags rhs)
{
  return static_cast<FrameFlags>(
    static_cast<std::underlying_type_t<FrameFlags>>(lhs) |
    static_cast<std::underlying_type_t<FrameFlags>>(rhs));
}
/**
 * @brief Bitwise AND for FrameFlags.
 * @param[in] lhs  Left-hand operand.
 * @param[in] rhs  Right-hand operand.
 * @return FrameFlags result of lhs & rhs.
 * @pre  lhs is a valid FrameFlags value or combination of FrameFlags enumerators.
 * @pre  rhs is a valid FrameFlags value or combination of FrameFlags enumerators.
 * @post The returned value has only the bits set that are present in both lhs and rhs.
 * @post No bits outside the FrameFlags underlying type (uint8_t) are affected.
 * @note Constexpr and thread-safe; has no side effects.
 * @since Version 1.0.0
 */
constexpr inline FrameFlags operator&(FrameFlags lhs, FrameFlags rhs)
{
  return static_cast<FrameFlags>(
    static_cast<std::underlying_type_t<FrameFlags>>(lhs) &
    static_cast<std::underlying_type_t<FrameFlags>>(rhs));
}
/**
 * @brief Bitwise XOR for FrameFlags.
 * @param[in] lhs  Left-hand operand.
 * @param[in] rhs  Right-hand operand.
 * @return FrameFlags result of lhs ^ rhs.
 * @pre  lhs is a valid FrameFlags value or combination of FrameFlags enumerators.
 * @pre  rhs is a valid FrameFlags value or combination of FrameFlags enumerators.
 * @post The returned value has bits set where lhs and rhs differ.
 * @post No bits outside the FrameFlags underlying type (uint8_t) are affected.
 * @note Constexpr and thread-safe; has no side effects.
 * @since Version 1.0.0
 */
constexpr inline FrameFlags operator^(FrameFlags lhs, FrameFlags rhs)
{
  return static_cast<FrameFlags>(
    static_cast<std::underlying_type_t<FrameFlags>>(lhs) ^
    static_cast<std::underlying_type_t<FrameFlags>>(rhs));
}
/**
 * @brief Bitwise NOT for FrameFlags.
 * @param[in] val  Operand to complement.
 * @return FrameFlags bitwise complement of val, masked to the valid flag range (bits 0-4).
 * @pre  val is a valid FrameFlags value or combination of FrameFlags enumerators.
 * @pre  The caller expects a mask-complement result; bits 5-7 of the underlying byte
 *       are always cleared because they have no defined meaning in the protocol.
 * @post Bits 0-4 of the result are the bitwise complement of the corresponding bits in val.
 * @post Bits 5-7 of the result are always 0 (mask 0x1F applied).
 * @note Constexpr and thread-safe; has no side effects.
 * @since Version 1.0.0
 */
constexpr inline FrameFlags operator~(FrameFlags val)
{
  return static_cast<FrameFlags>(
    (~static_cast<std::underlying_type_t<FrameFlags>>(val)) & FRAME_FLAGS_MASK);
}
/**
 * @brief Bitwise OR-assignment for FrameFlags.
 * @param[in,out] lhs  Left-hand operand; receives the result.
 * @param[in]     rhs  Right-hand operand.
 * @return Reference to lhs after applying lhs |= rhs.
 * @pre  lhs is a valid FrameFlags value or combination of FrameFlags enumerators.
 * @pre  rhs is a valid FrameFlags value or combination of FrameFlags enumerators.
 * @post lhs contains the union of bits from the original lhs and rhs.
 * @post No bits outside the FrameFlags underlying type (uint8_t) are modified.
 * @note Constexpr; thread-safe only when no other thread concurrently writes lhs.
 * @since Version 1.0.0
 */
constexpr inline FrameFlags & operator|=(FrameFlags & lhs, FrameFlags rhs)
{
  lhs = lhs | rhs;
  return lhs;
}
/**
 * @brief Bitwise AND-assignment for FrameFlags.
 * @param[in,out] lhs  Left-hand operand; receives the result.
 * @param[in]     rhs  Right-hand operand.
 * @return Reference to lhs after applying lhs &= rhs.
 * @pre  lhs is a valid FrameFlags value or combination of FrameFlags enumerators.
 * @pre  rhs is a valid FrameFlags value or combination of FrameFlags enumerators.
 * @post lhs contains only the bits that were set in both the original lhs and rhs.
 * @post No bits outside the FrameFlags underlying type (uint8_t) are modified.
 * @note Constexpr; thread-safe only when no other thread concurrently writes lhs.
 * @since Version 1.0.0
 */
constexpr inline FrameFlags & operator&=(FrameFlags & lhs, FrameFlags rhs)
{
  lhs = lhs & rhs;
  return lhs;
}
/**
 * @brief Bitwise XOR-assignment for FrameFlags.
 * @param[in,out] lhs  Left-hand operand; receives the result.
 * @param[in]     rhs  Right-hand operand.
 * @return Reference to lhs after applying lhs ^= rhs.
 * @pre  lhs is a valid FrameFlags value or combination of FrameFlags enumerators.
 * @pre  rhs is a valid FrameFlags value or combination of FrameFlags enumerators.
 * @post lhs contains bits set where the original lhs and rhs differ.
 * @post No bits outside the FrameFlags underlying type (uint8_t) are modified.
 * @note Constexpr; thread-safe only when no other thread concurrently writes lhs.
 * @since Version 1.0.0
 */
constexpr inline FrameFlags & operator^=(FrameFlags & lhs, FrameFlags rhs)
{
  lhs = lhs ^ rhs;
  return lhs;
}
/** @} */  // end of FrameFlagsOperators group

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
  static constexpr size_t HEADER_SIZE =
    8;   /**< SYNC + SEQ + LEN + TYPE + FLAGS = 8 bytes. */
  static constexpr size_t CRC_SIZE = 4; /**< CRC-32 trailer length in bytes. */
  static constexpr size_t MAX_PAYLOAD_SIZE =
    1024;   /**< Maximum application payload per frame (bytes). */
  static constexpr uint16_t SYNC_WORD =
    0x55AA;   /**< SYNC word transmitted in every frame header. */

  /**
   * @brief Construct a new SpiDriver.
   *
   * @param[in] device_path  Path to the Linux spidev node (e.g.
   * "/dev/spidev0.0").
   * @param[in] speed_hz     SPI clock frequency in Hz (default 10 MHz).
   *
   * @pre  device_path is a non-empty, null-terminated string.
   * @pre  speed_hz > 0; the kernel rejects a zero-Hz SPI clock request.
   * @post CRC-32 lookup table is initialised (thread-safe, once).
   * @post spi_fd_ is set to -1 (invalid); call initialize() to open and
   *       configure the SPI device before calling transfer().
   */
  explicit SpiDriver(
    const std::string & device_path,
    uint32_t speed_hz = 10000000);

  /**
   * @brief Destructor -- calls close_device() if the device is still open.
   *
   * @pre  No other threads are concurrently accessing this SpiDriver instance.
   * @pre  Ownership has been quiesced before destruction (no concurrent
   * close_device()/transfer()).
   * @post close_device() has been called.
   * @post The SPI device file descriptor is closed (spi_fd_ == -1).
   * @post All resources owned by this instance are released.
   */
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
   * @param[out] rx_data  Buffer filled with received data (resized to match
   * tx_data).
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
   * @param[in]  payload    Application data (0-MAX_PAYLOAD_SIZE bytes).
   * @param[out] out_frame  Encoded wire bytes (header + payload + CRC).
   *
   * @pre  `payload.size() <= MAX_PAYLOAD_SIZE`.
   * @post `out_frame.size() == HEADER_SIZE + payload.size() + CRC_SIZE`.
   *
   * @throws std::invalid_argument  If `payload.size() > MAX_PAYLOAD_SIZE`.
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
   * @pre  `frame.size() >= HEADER_SIZE + CRC_SIZE` (12 bytes minimum).
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

  static const uint32_t CRC32_POLYNOMIAL = 0x04C11DB7;
  static constexpr uint8_t BITS_PER_WORD = 8;
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
