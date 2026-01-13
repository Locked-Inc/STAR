#ifndef STAR_SPI_BRIDGE_SPI_DRIVER_HPP_
#define STAR_SPI_BRIDGE_SPI_DRIVER_HPP_

#include <vector>
#include <cstdint>
#include <string>

namespace star_spi_bridge {

class SpiDriver {
public:
    explicit SpiDriver(const std::string& device_path, uint32_t speed_hz = 10000000);
    virtual ~SpiDriver();

    bool initialize();
    void close_device();

    // Full duplex transfer
    // Returns true on success, false on failure
    bool transfer(const std::vector<uint8_t>& tx_data, std::vector<uint8_t>& rx_data);

    // Frame encoding/decoding helpers
    static std::vector<uint8_t> encode_frame(uint16_t seq, uint8_t type, uint8_t flags, const std::vector<uint8_t>& payload);
    static bool decode_frame(const std::vector<uint8_t>& frame, uint16_t& seq, uint8_t& type, uint8_t& flags, std::vector<uint8_t>& payload);

    // CRC-32 helper (public for testing)
    static uint32_t calculate_crc32(const std::vector<uint8_t>& data);

private:
    std::string device_path_;
    uint32_t speed_hz_;
    int spi_fd_;

    static const uint32_t k_crc32_polynomial = 0x04C11DB7;
    static const uint16_t k_sync_word = 0x55AA;
    static const size_t k_header_size = 6; // SYNC(2) + SEQ(2) + LEN(2) + TYPE(1) + FLAGS(1) -> Wait, spec says:
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

} // namespace star_spi_bridge

#endif // STAR_SPI_BRIDGE_SPI_DRIVER_HPP_
