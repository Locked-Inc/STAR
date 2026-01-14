// Copyright 2026 Locked Inc.

#include "star_spi_bridge/spi_driver.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstring>
#include <iomanip>
#include <iostream>

#ifdef __linux__
#include <linux/spi/spidev.h>
#else
// Mock spidev headers if on macOS/non-Linux to allow compilation (for
// development/analysis only)
// Ideally this code runs on Linux/RPi5. On macOS, these headers likely
// don't exist or are different.
#ifndef SPI_IOC_MESSAGE
#define SPI_IOC_MESSAGE(N)       0
#define SPI_MODE_0               0
#define SPI_IOC_WR_MODE          0
#define SPI_IOC_WR_BITS_PER_WORD 0
#define SPI_IOC_WR_MAX_SPEED_HZ  0

struct spi_ioc_transfer
{
  uint64_t tx_buf;
  uint64_t rx_buf;
  uint32_t len;
  uint32_t speed_hz;
  uint16_t delay_usecs;
  uint8_t bits_per_word;
  uint8_t cs_change;
  uint32_t pad;
};
#endif
#endif

namespace star_spi_bridge
{

uint32_t SpiDriver::crc32_table_[256];
bool SpiDriver::crc32_table_initialized_ = false;

SpiDriver::SpiDriver(const std::string & device_path, uint32_t speed_hz)
: device_path_(device_path), speed_hz_(speed_hz), spi_fd_(-1)
{
  if (!crc32_table_initialized_) {
    init_crc32_table();
    crc32_table_initialized_ = true;
  }
}

SpiDriver::~SpiDriver()
{
  close_device();
}

bool SpiDriver::initialize()
{
  // Open SPI device
  spi_fd_ = open(device_path_.c_str(), O_RDWR);
  if (spi_fd_ < 0) {
    std::cerr << "Failed to open SPI device: " << device_path_ << " (" << strerror(errno) << ")" <<
      std::endl;
    return false;
  }

  // Configure SPI mode (Mode 0: CPOL=0, CPHA=0)
  uint8_t mode = SPI_MODE_0;
  if (ioctl(spi_fd_, SPI_IOC_WR_MODE, &mode) < 0) {
    std::cerr << "Failed to set SPI mode" << std::endl;
    close_device();
    return false;
  }

  // Configure bits per word (8 bits)
  uint8_t bits = 8;
  if (ioctl(spi_fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
    std::cerr << "Failed to set SPI bits per word" << std::endl;
    close_device();
    return false;
  }

  // Configure max speed
  if (ioctl(spi_fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz_) < 0) {
    std::cerr << "Failed to set SPI max speed" << std::endl;
    close_device();
    return false;
  }

  return true;
}

void SpiDriver::close_device()
{
  if (spi_fd_ >= 0) {
    close(spi_fd_);
    spi_fd_ = -1;
  }
}

bool SpiDriver::transfer(const std::vector<uint8_t> & tx_data, std::vector<uint8_t> & rx_data)
{
  if (spi_fd_ < 0) {
    return false;
  }

  if (rx_data.size() != tx_data.size()) {
    rx_data.resize(tx_data.size());
  }

  struct spi_ioc_transfer xfer;
  std::memset(&xfer, 0, sizeof(xfer));

  xfer.tx_buf = reinterpret_cast<uint64_t>(tx_data.data());
  xfer.rx_buf = reinterpret_cast<uint64_t>(rx_data.data());
  xfer.len = static_cast<uint32_t>(tx_data.size());
  xfer.speed_hz = speed_hz_;
  xfer.bits_per_word = 8;

  int ret = ioctl(spi_fd_, SPI_IOC_MESSAGE(1), &xfer);
  if (ret < 0) {
    std::cerr << "SPI transfer failed: " << strerror(errno) << std::endl;
    return false;
  }

  return true;
}

void SpiDriver::encode_frame(
  uint16_t seq, FrameType type, uint8_t flags, const std::vector<uint8_t> & payload,
  std::vector<uint8_t> & out_frame)
{
  // [SYNC(2)][SEQ(2)][LEN(2)][TYPE(1)][FLAGS(1)][PAYLOAD(N)][CRC(4)]
  size_t frame_size = 8 + payload.size() + 4;
  out_frame.clear();
  out_frame.reserve(frame_size);

  // SYNC: 0x55AA (Big Endian)
  // Note: Header fields (SYNC, SEQ, LEN) are Big Endian per RFC 1700
  out_frame.push_back((k_sync_word >> 8) & 0xFF);
  out_frame.push_back(k_sync_word & 0xFF);

  // SEQ (Big Endian)
  out_frame.push_back((seq >> 8) & 0xFF);
  out_frame.push_back(seq & 0xFF);

  // LEN (Big Endian)
  uint16_t len = static_cast<uint16_t>(payload.size());
  out_frame.push_back((len >> 8) & 0xFF);
  out_frame.push_back(len & 0xFF);

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
}

bool SpiDriver::decode_frame(
  const std::vector<uint8_t> & frame, uint16_t & seq, FrameType & type, uint8_t & flags,
  std::vector<uint8_t> & payload)
{
  if (frame.size() < k_header_size + 4) {  // Header (8) + CRC (4) = 12 bytes min
    return false;
  }

  // Check SYNC
  if (frame[0] != ((k_sync_word >> 8) & 0xFF) || frame[1] != (k_sync_word & 0xFF)) {
    return false;
  }

  // Extract Length
  uint16_t len = (static_cast<uint16_t>(frame[4]) << 8) | frame[5];

  // Validate Frame Size
  // Header(8) + Payload(len) + CRC(4)
  if (frame.size() != static_cast<size_t>(8 + len + 4)) {
    return false;
  }

  // Verify CRC
  // Calculate CRC over Header + Payload (bytes 0 to end-4)
  std::vector<uint8_t> data_to_check(frame.begin(), frame.end() - 4);
  uint32_t calculated_crc = calculate_crc32(data_to_check);

  uint32_t received_crc =
    static_cast<uint32_t>(frame[frame.size() - 4]) |
    (static_cast<uint32_t>(frame[frame.size() - 3]) << 8) |
    (static_cast<uint32_t>(frame[frame.size() - 2]) <<
    16) | (static_cast<uint32_t>(frame[frame.size() - 1]) << 24);

  if (calculated_crc != received_crc) {
    return false;
  }

  // Extract fields
  seq = (static_cast<uint16_t>(frame[2]) << 8) | frame[3];
  type = static_cast<FrameType>(frame[6]);
  flags = frame[7];

  payload.assign(frame.begin() + 8, frame.end() - 4);

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
  if (!crc32_table_initialized_) {
    init_crc32_table();
    crc32_table_initialized_ = true;
  }

  uint32_t crc = 0xFFFFFFFF;
  for (uint8_t byte : data) {
    uint8_t lookup_index = (crc ^ byte) & 0xFF;
    crc = (crc >> 8) ^ crc32_table_[lookup_index];
  }
  return crc ^ 0xFFFFFFFF;
}

}  // namespace star_spi_bridge
