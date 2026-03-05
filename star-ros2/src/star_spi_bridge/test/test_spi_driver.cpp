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

  EXPECT_EQ(SpiDriver::calculate_crc32(data), 0xCBF43926);
}

TEST_F(SpiDriverTest, FrameEncoding)
{
  std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
  std::vector<uint8_t> frame;
  SpiDriver::encode_frame(100, FrameType::VelocityCommand, 0x00, payload, frame);

  // Header (8) + Payload (3) + CRC (4) = 15 bytes
  EXPECT_EQ(frame.size(), 15);

  // Check SYNC word (Little Endian: [0xAA, 0x55] for 0x55AA)
  EXPECT_EQ(frame[0], 0xAA);  // LSB first
  EXPECT_EQ(frame[1], 0x55);  // MSB second

  // Check SEQ field (Little Endian: seq=100=0x0064 -> [0x64, 0x00])
  EXPECT_EQ(frame[2], 0x64);  // SEQ LSB
  EXPECT_EQ(frame[3], 0x00);  // SEQ MSB

  // Check LEN field (Little Endian: len=3=0x0003 -> [0x03, 0x00])
  EXPECT_EQ(frame[4], 0x03);  // LEN LSB
  EXPECT_EQ(frame[5], 0x00);  // LEN MSB

  // Check TYPE and FLAGS
  EXPECT_EQ(frame[6], 0x01);  // TYPE: VelocityCommand
  EXPECT_EQ(frame[7], 0x00);  // FLAGS: none
}

TEST_F(SpiDriverTest, FrameDecoding)
{
  // Construct a valid frame manually
  // SYNC(55AA) SEQ(0001) LEN(0001) TYPE(01) FLAGS(00) PAYLOAD(A5) CRC(...)
  // CRC for header+payload needs to be correct for decode to succeed

  // We can't easily test decode without a working encode or pre-calculated frame
  // For Red Phase, we just assert that decode fails on garbage
  std::vector<uint8_t> garbage = {0x00, 0x00, 0x00};
  uint16_t seq = 0;
  FrameType type = FrameType::TelemetryData;
  uint8_t flags = 0x00;
  std::vector<uint8_t> payload = {};

  EXPECT_FALSE(SpiDriver::decode_frame(garbage, seq, type, flags, payload));
}

TEST_F(SpiDriverTest, EncodeDecodeRoundTrip)
{
  std::vector<uint8_t> payload_in = {0xDE, 0xAD, 0xBE, 0xEF};
  std::vector<uint8_t> frame;
  SpiDriver::encode_frame(42, FrameType::TelemetryData, 0x03, payload_in, frame);

  uint16_t seq = 0;
  FrameType type = FrameType::VelocityCommand;
  uint8_t flags = 0;
  std::vector<uint8_t> payload_out;

  ASSERT_TRUE(SpiDriver::decode_frame(frame, seq, type, flags, payload_out));
  EXPECT_EQ(seq, 42);
  EXPECT_EQ(type, FrameType::TelemetryData);
  EXPECT_EQ(flags, 0x03);
  EXPECT_EQ(payload_out, payload_in);
}

TEST_F(SpiDriverTest, DecodeRejectsCRCCorruption)
{
  std::vector<uint8_t> payload = {0x01};
  std::vector<uint8_t> frame;
  SpiDriver::encode_frame(1, FrameType::VelocityCommand, 0x00, payload, frame);

  // Corrupt the last byte (CRC)
  frame.back() ^= 0xFF;

  uint16_t seq = 0;
  FrameType type = FrameType::TelemetryData;
  uint8_t flags = 0;
  std::vector<uint8_t> decoded_payload;

  EXPECT_FALSE(SpiDriver::decode_frame(frame, seq, type, flags, decoded_payload));
}

TEST_F(SpiDriverTest, EncodeRejectsOversizedPayload)
{
  std::vector<uint8_t> oversized(SpiDriver::MAX_PAYLOAD_SIZE + 1, 0xAA);
  std::vector<uint8_t> frame;

  EXPECT_THROW(
    SpiDriver::encode_frame(0, FrameType::VelocityCommand, 0x00, oversized, frame),
    std::invalid_argument);
}
