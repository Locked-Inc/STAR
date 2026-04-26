/**
 * @file test_spi_driver.cpp
 * @brief Unit tests for SpiDriver frame encoding, decoding, and CRC-32 verification.
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "star_spi_bridge/spi_driver.hpp"

#include <gtest/gtest.h>  // NOLINT

using star_spi_bridge::FrameFlags;
using star_spi_bridge::FrameType;
using star_spi_bridge::SpiDriver;

class SpiDriverTest : public ::testing::Test {
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
  EXPECT_EQ(frame[7], 0x00);                                      // FLAGS: none
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

TEST_F(SpiDriverTest, DecodeRejectsUnknownFrameType)
{
  // Build a frame with a valid TYPE byte, then replace it with a reserved value
  // (0x05 is not a valid FrameType) and recompute the CRC so it is valid.
  // decode_frame must reject the frame because the type is unknown.
  std::vector<uint8_t> payload = {0x01};
  std::vector<uint8_t> frame;
  SpiDriver::encode_frame(1, FrameType::Command, 0x00, payload, frame);

  // Corrupt the TYPE byte (offset 6) to a reserved/invalid value
  frame[6] = 0x05;

  // Recompute CRC over header + payload (all bytes except the last 4)
  std::vector<uint8_t> to_crc(frame.begin(), frame.end() - 4);
  const uint32_t new_crc = SpiDriver::calculate_crc32(to_crc);
  frame[frame.size() - 4] = static_cast<uint8_t>(new_crc & 0xFFu);
  frame[frame.size() - 3] = static_cast<uint8_t>((new_crc >> 8u) & 0xFFu);
  frame[frame.size() - 2] = static_cast<uint8_t>((new_crc >> 16u) & 0xFFu);
  frame[frame.size() - 1] = static_cast<uint8_t>((new_crc >> 24u) & 0xFFu);

  uint16_t seq = 0;
  FrameType type = FrameType::Command;
  uint8_t flags = 0;
  std::vector<uint8_t> decoded_payload;

  EXPECT_FALSE(SpiDriver::decode_frame(frame, seq, type, flags, decoded_payload));
}

TEST_F(SpiDriverTest, EncodeRejectsOversizedPayload)
{
  std::vector<uint8_t> oversized(SpiDriver::MAX_PAYLOAD_SIZE + 1, 0xAA);
  std::vector<uint8_t> frame;

  EXPECT_THROW(SpiDriver::encode_frame(0, FrameType::Command, 0x00, oversized, frame), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Round-11 audit: payload-length bounds check on the decode path
// ---------------------------------------------------------------------------
// The encoder caps payload at MAX_PAYLOAD_SIZE (1024 B), but the decoder
// also has to reject crafted/corrupt frames whose LEN field claims more
// bytes than the protocol allows.  Without this guard, a malicious sender
// could trigger arithmetic on (HEADER_SIZE + len + CRC_SIZE) using an
// out-of-spec len.  See spi_driver.cpp lines 364-369.
// ---------------------------------------------------------------------------

TEST_F(SpiDriverTest, DecodeRejectsLenAboveMaxPayloadSize)
{
  // Build a wire-shaped frame that is large enough to satisfy the
  // size-equality check that runs after the bounds check, but with the LEN
  // field set to MAX_PAYLOAD_SIZE + 1.  decode_frame must reject this on
  // the bounds check before any size-equality math runs.
  const uint16_t bogus_len = static_cast<uint16_t>(SpiDriver::MAX_PAYLOAD_SIZE + 1u);
  std::vector<uint8_t> frame(SpiDriver::HEADER_SIZE + bogus_len + SpiDriver::CRC_SIZE, 0x00);

  // Valid SYNC (0x55AA, little-endian on the wire as 0xAA, 0x55)
  frame[0] = 0xAA;
  frame[1] = 0x55;
  // SEQ = 0
  frame[2] = 0x00;
  frame[3] = 0x00;
  // LEN = MAX_PAYLOAD_SIZE + 1 (little-endian)
  frame[4] = static_cast<uint8_t>(bogus_len & 0xFFu);
  frame[5] = static_cast<uint8_t>((bogus_len >> 8u) & 0xFFu);
  // TYPE / FLAGS
  frame[6] = static_cast<uint8_t>(FrameType::Command);
  frame[7] = 0x00;

  // Even with a valid CRC over header + payload, the decoder must reject
  // because len exceeds MAX_PAYLOAD_SIZE.
  std::vector<uint8_t> to_crc(frame.begin(), frame.end() - SpiDriver::CRC_SIZE);
  const uint32_t crc = SpiDriver::calculate_crc32(to_crc);
  frame[frame.size() - 4] = static_cast<uint8_t>(crc & 0xFFu);
  frame[frame.size() - 3] = static_cast<uint8_t>((crc >> 8u) & 0xFFu);
  frame[frame.size() - 2] = static_cast<uint8_t>((crc >> 16u) & 0xFFu);
  frame[frame.size() - 1] = static_cast<uint8_t>((crc >> 24u) & 0xFFu);

  uint16_t seq = 0;
  FrameType type = FrameType::Response;
  uint8_t flags = 0;
  std::vector<uint8_t> payload;

  // Must return false (no crash, no buffer overflow, no size-mismatch path).
  EXPECT_FALSE(SpiDriver::decode_frame(frame, seq, type, flags, payload));
}

TEST_F(SpiDriverTest, DecodeRejectsMaxLenU16Pathological)
{
  // Worst-case LEN field: 0xFFFF.  Without the round-11 bounds check, the
  // size-equality test (frame.size() == HEADER_SIZE + len + CRC_SIZE) would
  // fail anyway because no realistic frame is 65547 bytes -- but we still
  // exercise the explicit guard with an undersized buffer to make sure the
  // decoder does not read past the frame end while computing the expected
  // size.  This protects against out-of-bounds reads in pathological inputs.
  std::vector<uint8_t> frame(SpiDriver::HEADER_SIZE + SpiDriver::CRC_SIZE, 0x00);
  frame[0] = 0xAA;
  frame[1] = 0x55;
  frame[2] = 0x00;
  frame[3] = 0x00;
  frame[4] = 0xFFu;  // LEN LSB
  frame[5] = 0xFFu;  // LEN MSB -> 0xFFFF = 65535
  frame[6] = static_cast<uint8_t>(FrameType::Command);
  frame[7] = 0x00;
  // CRC bytes left at zero -- decoder must reject before checking CRC.

  uint16_t seq = 0;
  FrameType type = FrameType::Response;
  uint8_t flags = 0;
  std::vector<uint8_t> payload;

  EXPECT_FALSE(SpiDriver::decode_frame(frame, seq, type, flags, payload));
  // Output payload is left untouched (still empty) on failure.
  EXPECT_TRUE(payload.empty());
}

TEST_F(SpiDriverTest, DecodeAcceptsLenAtMaxPayloadBoundary)
{
  // Boundary case: len == MAX_PAYLOAD_SIZE must be accepted (the bounds
  // check uses '>', not '>=').  Round-trip a max-sized payload to confirm.
  std::vector<uint8_t> payload_in(SpiDriver::MAX_PAYLOAD_SIZE, 0x5A);
  std::vector<uint8_t> frame;
  SpiDriver::encode_frame(99, FrameType::Command, 0x00, payload_in, frame);

  uint16_t seq = 0;
  FrameType type = FrameType::Response;
  uint8_t flags = 0;
  std::vector<uint8_t> payload_out;

  ASSERT_TRUE(SpiDriver::decode_frame(frame, seq, type, flags, payload_out));
  EXPECT_EQ(seq, 99u);
  EXPECT_EQ(type, FrameType::Command);
  EXPECT_EQ(payload_out.size(), SpiDriver::MAX_PAYLOAD_SIZE);
  EXPECT_EQ(payload_out, payload_in);
}

// ---------------------------------------------------------------------------
// Decoder negative-path coverage: SYNC word, undersized buffers, length mismatch
// ---------------------------------------------------------------------------

TEST_F(SpiDriverTest, DecodeRejectsTooShortFrame)
{
  // Anything shorter than HEADER_SIZE + CRC_SIZE (12 B) must be rejected.
  std::vector<uint8_t> frame(SpiDriver::HEADER_SIZE + SpiDriver::CRC_SIZE - 1u, 0xAA);

  uint16_t seq = 0;
  FrameType type = FrameType::Response;
  uint8_t flags = 0;
  std::vector<uint8_t> payload;

  EXPECT_FALSE(SpiDriver::decode_frame(frame, seq, type, flags, payload));
}

TEST_F(SpiDriverTest, DecodeRejectsBadSyncWord)
{
  std::vector<uint8_t> payload = {0x42};
  std::vector<uint8_t> frame;
  SpiDriver::encode_frame(1, FrameType::Command, 0x00, payload, frame);

  // Corrupt the SYNC word (offsets 0,1)
  frame[0] = 0xDE;
  frame[1] = 0xAD;

  uint16_t seq = 0;
  FrameType type = FrameType::Response;
  uint8_t flags = 0;
  std::vector<uint8_t> decoded_payload;

  EXPECT_FALSE(SpiDriver::decode_frame(frame, seq, type, flags, decoded_payload));
}

TEST_F(SpiDriverTest, DecodeRejectsLenMismatch)
{
  // LEN field claims 5 bytes but frame is sized for 1-byte payload.  The
  // bounds check passes (5 < MAX_PAYLOAD_SIZE) so the size-equality check
  // at line 373 is what must reject this frame.
  std::vector<uint8_t> payload = {0x99};
  std::vector<uint8_t> frame;
  SpiDriver::encode_frame(2, FrameType::Command, 0x00, payload, frame);

  // Bump LEN from 1 to 5 (LE)
  frame[4] = 0x05;
  frame[5] = 0x00;

  // Recompute CRC so only the size mismatch can cause rejection.
  std::vector<uint8_t> to_crc(frame.begin(), frame.end() - SpiDriver::CRC_SIZE);
  const uint32_t crc = SpiDriver::calculate_crc32(to_crc);
  frame[frame.size() - 4] = static_cast<uint8_t>(crc & 0xFFu);
  frame[frame.size() - 3] = static_cast<uint8_t>((crc >> 8u) & 0xFFu);
  frame[frame.size() - 2] = static_cast<uint8_t>((crc >> 16u) & 0xFFu);
  frame[frame.size() - 1] = static_cast<uint8_t>((crc >> 24u) & 0xFFu);

  uint16_t seq = 0;
  FrameType type = FrameType::Response;
  uint8_t flags = 0;
  std::vector<uint8_t> decoded_payload;

  EXPECT_FALSE(SpiDriver::decode_frame(frame, seq, type, flags, decoded_payload));
}

// ---------------------------------------------------------------------------
// FrameFlags bitwise operator tests
// ---------------------------------------------------------------------------
// All valid flag bits are in [0, 4] (mask 0x1F). Bits 5-7 are reserved and
// must remain 0.  These tests verify combining, masking, toggling, assignment,
// and the unary complement semantics guaranteed by operator~.
// ---------------------------------------------------------------------------

class FrameFlagsTest : public ::testing::Test {};

TEST_F(FrameFlagsTest, OrCombinesFlags)
{
  const FrameFlags combined = FrameFlags::RequiresAck | FrameFlags::Priority;
  const auto raw = static_cast<uint8_t>(combined);

  // RequiresAck=0x01, Priority=0x04 -> 0x05
  EXPECT_EQ(raw, 0x05u);
  // Both source bits are present
  EXPECT_NE(static_cast<uint8_t>(combined & FrameFlags::RequiresAck), 0u);
  EXPECT_NE(static_cast<uint8_t>(combined & FrameFlags::Priority), 0u);
}

TEST_F(FrameFlagsTest, AndMasksFlags)
{
  const FrameFlags combined = FrameFlags::RequiresAck | FrameFlags::Priority;
  // Mask off RequiresAck
  const FrameFlags masked = combined & FrameFlags::RequiresAck;
  EXPECT_EQ(static_cast<uint8_t>(masked), static_cast<uint8_t>(FrameFlags::RequiresAck));
  // Priority bit is not present after AND with RequiresAck
  EXPECT_EQ(static_cast<uint8_t>(masked & FrameFlags::Priority), 0u);
}

TEST_F(FrameFlagsTest, XorTogglesFlags)
{
  // Start with RequiresAck | Priority, toggle RequiresAck off
  const FrameFlags combined = FrameFlags::RequiresAck | FrameFlags::Priority;
  const FrameFlags toggled = combined ^ FrameFlags::RequiresAck;

  // RequiresAck should be cleared, Priority remains
  EXPECT_EQ(static_cast<uint8_t>(toggled), static_cast<uint8_t>(FrameFlags::Priority));
}

TEST_F(FrameFlagsTest, NotComplementsDefinedBitsOnly)
{
  // ~RequiresAck (0x01) should have bits 1-4 set but bit 0 cleared,
  // and reserved bits 5-7 must always be 0.
  const FrameFlags result = ~FrameFlags::RequiresAck;
  const auto raw = static_cast<uint8_t>(result);

  // Bit 0 (RequiresAck) is cleared
  EXPECT_EQ(raw & 0x01u, 0u);
  // Bits 1-4 (Retransmit, Priority, FecEnabled, SoftNack) are set
  EXPECT_EQ(raw & 0x1Eu, 0x1Eu);
  // Reserved bits 5-7 must be zero
  EXPECT_EQ(raw & 0xE0u, 0u);
}

TEST_F(FrameFlagsTest, NotClearsReservedBitsForAllSingleFlags)
{
  // Verify reserved bits are always 0 regardless of input
  constexpr uint8_t kReservedMask = 0xE0u;
  for (const auto flag : {FrameFlags::None, FrameFlags::RequiresAck, FrameFlags::Retransmit, FrameFlags::Priority,
                          FrameFlags::FecEnabled, FrameFlags::SoftNack}) {
    const auto raw = static_cast<uint8_t>(~flag);
    EXPECT_EQ(raw & kReservedMask, 0u) << "Reserved bits 5-7 set for ~flag with underlying value "
                                       << static_cast<int>(static_cast<uint8_t>(flag));
  }
}

TEST_F(FrameFlagsTest, OrAssignmentAccumulatesFlags)
{
  FrameFlags flags = FrameFlags::None;
  flags |= FrameFlags::RequiresAck;
  flags |= FrameFlags::FecEnabled;

  // RequiresAck=0x01, FecEnabled=0x08 -> 0x09
  EXPECT_EQ(static_cast<uint8_t>(flags), 0x09u);
}

TEST_F(FrameFlagsTest, AndAssignmentClearsFlags)
{
  FrameFlags flags = FrameFlags::RequiresAck | FrameFlags::Retransmit | FrameFlags::Priority;
  // Keep only Priority
  flags &= FrameFlags::Priority;
  EXPECT_EQ(static_cast<uint8_t>(flags), static_cast<uint8_t>(FrameFlags::Priority));
}

TEST_F(FrameFlagsTest, XorAssignmentTogglesFlags)
{
  FrameFlags flags = FrameFlags::RequiresAck | FrameFlags::Retransmit;
  // Toggle Retransmit off and SoftNack on
  flags ^= (FrameFlags::Retransmit | FrameFlags::SoftNack);

  // Retransmit cleared, RequiresAck kept, SoftNack added
  const auto raw = static_cast<uint8_t>(flags);
  EXPECT_EQ(raw & static_cast<uint8_t>(FrameFlags::Retransmit), 0u);
  EXPECT_NE(raw & static_cast<uint8_t>(FrameFlags::RequiresAck), 0u);
  EXPECT_NE(raw & static_cast<uint8_t>(FrameFlags::SoftNack), 0u);
}
