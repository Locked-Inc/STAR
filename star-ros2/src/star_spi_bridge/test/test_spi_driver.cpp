// Copyright 2026 Locked Inc.

#include "star_spi_bridge/spi_driver.hpp"

#include <gtest/gtest.h>  // NOLINT

using star_spi_bridge::FrameFlags;
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
  SpiDriver::encode_frame(100, FrameType::Command, 0x00, payload, frame);

  // Header (8) + Payload (3) + CRC (4) = 15 bytes
  EXPECT_EQ(frame.size(), 15u);

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
  EXPECT_EQ(frame[6], static_cast<uint8_t>(FrameType::Command));  // TYPE: Command
  EXPECT_EQ(frame[7], 0x00);  // FLAGS: none
}

TEST_F(SpiDriverTest, FrameEncodingAck)
{
  std::vector<uint8_t> empty_payload;
  std::vector<uint8_t> frame;
  SpiDriver::encode_frame(7, FrameType::Ack, 0x00, empty_payload, frame);

  // Header (8) + Payload (0) + CRC (4) = 12 bytes
  EXPECT_EQ(frame.size(), 12u);
  EXPECT_EQ(frame[6], static_cast<uint8_t>(FrameType::Ack));  // TYPE: Ack
}

TEST_F(SpiDriverTest, FrameEncodingPing)
{
  std::vector<uint8_t> empty_payload;
  std::vector<uint8_t> frame;
  SpiDriver::encode_frame(0, FrameType::Ping, 0x00, empty_payload, frame);

  EXPECT_EQ(frame.size(), 12u);
  EXPECT_EQ(frame[6], static_cast<uint8_t>(FrameType::Ping));  // TYPE: Ping
}

TEST_F(SpiDriverTest, FrameDecoding)
{
  // Verify that decode fails on garbage
  std::vector<uint8_t> garbage = {0x00, 0x00, 0x00};
  uint16_t seq = 0;
  FrameType type = FrameType::Response;
  uint8_t flags = 0x00;
  std::vector<uint8_t> payload = {};

  EXPECT_FALSE(SpiDriver::decode_frame(garbage, seq, type, flags, payload));
}

TEST_F(SpiDriverTest, EncodeDecodeRoundTrip)
{
  std::vector<uint8_t> payload_in = {0xDE, 0xAD, 0xBE, 0xEF};
  std::vector<uint8_t> frame;
  SpiDriver::encode_frame(42, FrameType::Response, 0x03, payload_in, frame);

  uint16_t seq = 0;
  FrameType type = FrameType::Command;
  uint8_t flags = 0;
  std::vector<uint8_t> payload_out;

  ASSERT_TRUE(SpiDriver::decode_frame(frame, seq, type, flags, payload_out));
  EXPECT_EQ(seq, 42u);
  EXPECT_EQ(type, FrameType::Response);
  EXPECT_EQ(flags, 0x03);
  EXPECT_EQ(payload_out, payload_in);
}

TEST_F(SpiDriverTest, EncodeDecodeRoundTripCommand)
{
  std::vector<uint8_t> payload_in = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> frame;
  const uint8_t flags = static_cast<uint8_t>(FrameFlags::RequiresAck);
  SpiDriver::encode_frame(10, FrameType::Command, flags, payload_in, frame);

  uint16_t seq = 0;
  FrameType type = FrameType::Ping;
  uint8_t decoded_flags = 0;
  std::vector<uint8_t> payload_out;

  ASSERT_TRUE(SpiDriver::decode_frame(frame, seq, type, decoded_flags, payload_out));
  EXPECT_EQ(seq, 10u);
  EXPECT_EQ(type, FrameType::Command);
  EXPECT_EQ(decoded_flags, flags);
  EXPECT_EQ(payload_out, payload_in);
}

TEST_F(SpiDriverTest, DecodeRejectsCRCCorruption)
{
  std::vector<uint8_t> payload = {0x01};
  std::vector<uint8_t> frame;
  SpiDriver::encode_frame(1, FrameType::Command, 0x00, payload, frame);

  // Corrupt the last byte (CRC)
  frame.back() ^= 0xFF;

  uint16_t seq = 0;
  FrameType type = FrameType::Response;
  uint8_t flags = 0;
  std::vector<uint8_t> decoded_payload;

  EXPECT_FALSE(SpiDriver::decode_frame(frame, seq, type, flags, decoded_payload));
}

TEST_F(SpiDriverTest, EncodeRejectsOversizedPayload)
{
  std::vector<uint8_t> oversized(SpiDriver::MAX_PAYLOAD_SIZE + 1, 0xAA);
  std::vector<uint8_t> frame;

  EXPECT_THROW(
    SpiDriver::encode_frame(0, FrameType::Command, 0x00, oversized, frame),
    std::invalid_argument);
}
