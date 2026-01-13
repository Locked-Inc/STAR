// Copyright 2026 Locked Inc.

#include "star_spi_bridge/spi_driver.hpp"

#include <gtest/gtest.h>  // NOLINT

using star_spi_bridge::FrameType;
using star_spi_bridge::SpiDriver;

class SpiDriverTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    // Setup code
  }
};

TEST_F(SpiDriverTest, CRC32Calculation)
{
  // Test vector: "123456789" -> 0xCBF43926
  std::string test_str = "123456789";
  std::vector<uint8_t> data(test_str.begin(), test_str.end());

  // This should fail initially as implementation is empty
  EXPECT_EQ(SpiDriver::calculate_crc32(data), 0xCBF43926);
}

TEST_F(SpiDriverTest, FrameEncoding)
{
  std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
  std::vector<uint8_t> frame;
  SpiDriver::encode_frame(100, FrameType::VelocityCommand, 0x00, payload, frame);

  // Header (8) + Payload (3) + CRC (4) = 15 bytes
  EXPECT_EQ(frame.size(), 15);

  // Check SYNC word (Big Endian)
  EXPECT_EQ(frame[0], 0x55);
  EXPECT_EQ(frame[1], 0xAA);
}

TEST_F(SpiDriverTest, FrameDecoding)
{
  // Construct a valid frame manually
  // SYNC(55AA) SEQ(0001) LEN(0001) TYPE(01) FLAGS(00) PAYLOAD(A5) CRC(...)
  // CRC for header+payload needs to be correct for decode to succeed

  // We can't easily test decode without a working encode or pre-calculated frame
  // For Red Phase, we just assert that decode fails on garbage
  std::vector<uint8_t> garbage = {0x00, 0x00, 0x00};
  uint16_t seq;
  FrameType type;
  uint8_t flags;
  std::vector<uint8_t> payload;

  EXPECT_FALSE(SpiDriver::decode_frame(garbage, seq, type, flags, payload));
}
