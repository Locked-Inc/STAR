# SICK TIM561-2050101 LiDAR Communication Guide

Complete guide for implementing TCP/IP communication with the SICK TIM561 LiDAR using C++ without ROS2.

## Overview

The SICK TIM561-2050101 uses COLA (Command Language) protocol over TCP/IP Ethernet for communication.

### Key Specifications

- **Connection**: TCP/IP over Ethernet
- **Default IP**: 192.168.0.1 (configurable via SOPAS)
- **Subnet**: 255.255.255.0
- **Port**: 2111 (COLA-A ASCII) or 2112 (COLA-B Binary)
- **Protocol**: COLA-A (human-readable) or COLA-B (binary)
- **Data Rate**: Up to 15 Hz scan frequency
- **Range**: 0.05 m to 10 m
- **Angular Resolution**: 0.33° (270° scanning angle)

## COLA Protocol

SICK's COLA (Command Language) protocol comes in two variants:

### COLA-A (ASCII Protocol)

**Format**: Human-readable ASCII text commands  
**Port**: TCP 2111  
**Use Case**: Configuration, debugging, diagnostic commands

**Message Structure**:
```
STX (0x02) + Command + ETX (0x03)
```

**Example Commands**:
```
\x02sRN DeviceIdent\x03        # Read device identification
\x02sMN SetAccessMode 3 F4724744\x03  # Set authorized client access mode
\x02sMN mSC stopmeas\x03       # Stop measurement
\x02sMN mSC startmeas\x03      # Start measurement
\x02sEN LMDscandata 1\x03      # Enable continuous scan data output
\x02sEN LMDscandata 0\x03      # Disable continuous scan data output
\x02sRN LMDscandata\x03        # Request single scan dataset
```

### COLA-B (Binary Protocol)

**Format**: Binary/hexadecimal values  
**Port**: TCP 2112  
**Use Case**: High-frequency scan data streaming (faster, less network traffic)

**For this guide, we'll focus on COLA-A** as it's easier to debug and sufficient for most applications.

## C++ TCP Socket Implementation

### Basic TCP Connection Class

```cpp
// lidar_connection.h
#ifndef LIDAR_CONNECTION_H
#define LIDAR_CONNECTION_H

#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

class LidarConnection {
public:
    LidarConnection(const std::string& ip, int port = 2111);
    ~LidarConnection();
    
    bool connect();
    void disconnect();
    bool isConnected() const { return connected_; }
    
    bool sendCommand(const std::string& command);
    std::string receiveResponse(int timeout_ms = 5000);
    
private:
    std::string ip_;
    int port_;
    int sockfd_;
    bool connected_;
    
    static constexpr char STX = 0x02;
    static constexpr char ETX = 0x03;
    static constexpr size_t BUFFER_SIZE = 65536;  // 64KB buffer
};

#endif // LIDAR_CONNECTION_H
```

```cpp
// lidar_connection.cpp
#include "lidar_connection.h"
#include <iostream>
#include <sys/select.h>
#include <cerrno>

LidarConnection::LidarConnection(const std::string& ip, int port)
    : ip_(ip), port_(port), sockfd_(-1), connected_(false) {}

LidarConnection::~LidarConnection() {
    disconnect();
}

bool LidarConnection::connect() {
    // Create socket
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0) {
        std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Set socket address
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    
    if (inet_pton(AF_INET, ip_.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid IP address: " << ip_ << std::endl;
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }
    
    // Connect to server
    if (::connect(sockfd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed: " << strerror(errno) << std::endl;
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }
    
    connected_ = true;
    std::cout << "Connected to SICK LiDAR at " << ip_ << ":" << port_ << std::endl;
    return true;
}

void LidarConnection::disconnect() {
    if (sockfd_ >= 0) {
        close(sockfd_);
        sockfd_ = -1;
    }
    connected_ = false;
}

bool LidarConnection::sendCommand(const std::string& command) {
    if (!connected_) {
        std::cerr << "Not connected to LiDAR" << std::endl;
        return false;
    }
    
    // Build message: STX + command + ETX
    std::string message;
    message += STX;
    message += command;
    message += ETX;
    
    ssize_t bytes_sent = send(sockfd_, message.c_str(), message.length(), 0);
    if (bytes_sent < 0) {
        std::cerr << "Send failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

std::string LidarConnection::receiveResponse(int timeout_ms) {
    if (!connected_) {
        throw std::runtime_error("Not connected to LiDAR");
    }
    
    char buffer[BUFFER_SIZE];
    std::string response;
    
    // Set timeout
    fd_set read_fds;
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    FD_ZERO(&read_fds);
    FD_SET(sockfd_, &read_fds);
    
    int select_result = select(sockfd_ + 1, &read_fds, nullptr, nullptr, &timeout);
    
    if (select_result < 0) {
        throw std::runtime_error(std::string("select() failed: ") + strerror(errno));
    } else if (select_result == 0) {
        throw std::runtime_error("Receive timeout");
    }
    
    ssize_t bytes_received = recv(sockfd_, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received < 0) {
        throw std::runtime_error(std::string("Receive failed: ") + strerror(errno));
    }
    
    buffer[bytes_received] = '\0';
    response = std::string(buffer, bytes_received);
    
    // Strip STX and ETX
    if (!response.empty() && response[0] == STX) {
        response = response.substr(1);
    }
    if (!response.empty() && response[response.length() - 1] == ETX) {
        response = response.substr(0, response.length() - 1);
    }
    
    return response;
}
```

### Usage Example

```cpp
// main.cpp
#include "lidar_connection.h"
#include <iostream>

int main() {
    // Connect to LiDAR
    LidarConnection lidar("192.168.0.1", 2111);
    
    if (!lidar.connect()) {
        std::cerr << "Failed to connect to LiDAR" << std::endl;
        return 1;
    }
    
    try {
        // Read device identification
        lidar.sendCommand("sRN DeviceIdent");
        std::string response = lidar.receiveResponse();
        std::cout << "Device ID: " << response << std::endl;
        
        // Set authorized client mode
        lidar.sendCommand("sMN SetAccessMode 3 F4724744");
        response = lidar.receiveResponse();
        std::cout << "Access mode response: " << response << std::endl;
        
        // Start measurement
        lidar.sendCommand("sMN mSC startmeas");
        response = lidar.receiveResponse();
        std::cout << "Start measurement response: " << response << std::endl;
        
        // Enable continuous scan data
        lidar.sendCommand("sEN LMDscandata 1");
        response = lidar.receiveResponse();
        std::cout << "Enable scan data response: " << response << std::endl;
        
        // Receive scan data continuously
        for (int i = 0; i < 10; ++i) {
            std::string scan_data = lidar.receiveResponse(10000);  // 10 second timeout
            std::cout << "Scan " << i << " received, length: " << scan_data.length() << std::endl;
            // Parse scan_data here (see parsing section below)
        }
        
        // Stop measurement
        lidar.sendCommand("sEN LMDscandata 0");
        lidar.sendCommand("sMN mSC stopmeas");
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    lidar.disconnect();
    return 0;
}
```

## Scan Data Format

The LMDscandata response follows this structure:

```
sRA LMDscandata <StatusCode> <TelegramCounter> <ScanCounter> 
<TimeStamp> <TimeTransmission> <DigitalInputs> <DigitalOutputs> 
<ScanFrequency> <MeasurementFrequency> <NumberEncoders> 
<NumberChannels> <ContentType> <ScaleFactor> <ScaleOffset> 
<StartAngle> <AngularStepWidth> <NumberData> <Data1> <Data2> ...
```

### Key Fields

- **StatusCode**: 0 = OK, 7 = error
- **TelegramCounter**: Incremental message counter
- **ScanCounter**: Scan number
- **ScaleFactor**: Multiplier for distance values (typically 1)
- **ScaleOffset**: Offset for distance values (typically 0)
- **StartAngle**: Starting angle in 1/10000 degree (e.g., -450000 = -45°)
- **AngularStepWidth**: Step width in 1/10000 degree (e.g., 3333 = 0.333°)
- **NumberData**: Number of distance measurements
- **DataN**: Distance values in millimeters (16-bit hex)

### Parsing Example

```cpp
// scan_data_parser.h
#ifndef SCAN_DATA_PARSER_H
#define SCAN_DATA_PARSER_H

#include <vector>
#include <string>
#include <sstream>

struct ScanPoint {
    float angle;      // degrees
    float distance;   // meters
};

struct ScanData {
    uint32_t timestamp;
    uint16_t scan_counter;
    float start_angle;
    float angular_step;
    std::vector<ScanPoint> points;
};

class ScanDataParser {
public:
    static ScanData parse(const std::string& telegram);
    
private:
    static std::vector<std::string> split(const std::string& str, char delim = ' ');
    static uint32_t hexToInt(const std::string& hex);
    static float hexToFloat(const std::string& hex);
};

#endif // SCAN_DATA_PARSER_H
```

```cpp
// scan_data_parser.cpp
#include "scan_data_parser.h"
#include <stdexcept>
#include <iomanip>

std::vector<std::string> ScanDataParser::split(const std::string& str, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

uint32_t ScanDataParser::hexToInt(const std::string& hex) {
    return std::stoul(hex, nullptr, 16);
}

float ScanDataParser::hexToFloat(const std::string& hex) {
    return static_cast<float>(hexToInt(hex));
}

ScanData ScanDataParser::parse(const std::string& telegram) {
    ScanData data;
    std::vector<std::string> tokens = split(telegram);
    
    if (tokens.size() < 26) {
        throw std::runtime_error("Invalid scan data telegram: too few tokens");
    }
    
    // Parse header (indices approximate - verify with actual data)
    size_t idx = 0;
    if (tokens[idx++] != "sRA") throw std::runtime_error("Not a scan data response");
    if (tokens[idx++] != "LMDscandata") throw std::runtime_error("Not LMDscandata");
    
    uint16_t status_code = hexToInt(tokens[idx++]);
    if (status_code != 0 && status_code != 7) {
        throw std::runtime_error("LiDAR error status: " + std::to_string(status_code));
    }
    
    idx++;  // Telegram counter
    data.scan_counter = hexToInt(tokens[idx++]);
    data.timestamp = hexToInt(tokens[idx++]);
    idx++;  // Time transmission
    idx++;  // Digital inputs
    idx++;  // Digital outputs
    idx += 3;  // Skip scan frequency, measurement frequency, number of encoders
    
    uint16_t num_channels = hexToInt(tokens[idx++]);
    if (num_channels != 1) {
        throw std::runtime_error("Expected 1 channel, got " + std::to_string(num_channels));
    }
    
    idx++;  // Skip content type
    float scale_factor = hexToFloat(tokens[idx++]);
    float scale_offset = hexToFloat(tokens[idx++]);
    
    int32_t start_angle_raw = static_cast<int32_t>(hexToInt(tokens[idx++]));
    int32_t angular_step_raw = hexToInt(tokens[idx++]);
    
    data.start_angle = start_angle_raw / 10000.0f;  // Convert to degrees
    data.angular_step = angular_step_raw / 10000.0f;
    
    uint16_t num_data = hexToInt(tokens[idx++]);
    
    // Parse distance measurements
    data.points.reserve(num_data);
    for (uint16_t i = 0; i < num_data && idx < tokens.size(); ++i) {
        ScanPoint point;
        point.angle = data.start_angle + i * data.angular_step;
        
        uint16_t distance_mm = hexToInt(tokens[idx++]);
        point.distance = (distance_mm * scale_factor + scale_offset) / 1000.0f;  // Convert to meters
        
        data.points.push_back(point);
    }
    
    return data;
}
```

## Extracting Code from sick_scan_xd

The `sick_scan_xd` repository contains production-quality COLA protocol implementation:

### Key Files to Reference

**TCP Communication**:
- `sick_scan_xd/driver/src/tcp/tcp.cpp` - Core TCP socket operations
- `sick_scan_xd/driver/src/tcp/Hostname.cpp` - Network address handling

**COLA Protocol**:
- `sick_scan_xd/driver/src/tcp/colaa.cpp` - COLA-A parser
- `sick_scan_xd/driver/src/tcp/colaa.hpp` - COLA-A header

### Building sick_scan_xd Without ROS

```bash
git clone https://github.com/SICKAG/sick_scan_xd.git
cd sick_scan_xd
mkdir build && cd build
cmake -DROS_VERSION=0 -DLDMRS=0 -DSCANSEGMENT_XD=0 -G "Unix Makefiles" ..
make -j4
```

This produces `sick_generic_caller` and `libsick_scan_xd_shared_lib.so` without ROS dependencies.

## Complete Example Application

```cpp
// star_lidar.cpp - Complete example
#include "lidar_connection.h"
#include "scan_data_parser.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

std::atomic<bool> running(true);

void signal_handler(int) {
    running = false;
}

int main(int argc, char** argv) {
    std::string lidar_ip = "192.168.0.1";
    if (argc > 1) {
        lidar_ip = argv[1];
    }
    
    std::signal(SIGINT, signal_handler);
    
    LidarConnection lidar(lidar_ip);
    
    if (!lidar.connect()) {
        return 1;
    }
    
    try {
        // Initialize LiDAR
        lidar.sendCommand("sMN SetAccessMode 3 F4724744");
        lidar.receiveResponse();
        
        lidar.sendCommand("sMN mSC startmeas");
        lidar.receiveResponse();
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        lidar.sendCommand("sEN LMDscandata 1");
        lidar.receiveResponse();
        
        std::cout << "Receiving scan data... (Ctrl+C to stop)" << std::endl;
        
        // Main scanning loop
        while (running) {
            try {
                std::string telegram = lidar.receiveResponse(1000);
                ScanData scan = ScanDataParser::parse(telegram);
                
                std::cout << "Scan " << scan.scan_counter 
                          << ": " << scan.points.size() << " points"
                          << std::endl;
                
                // Process scan data here
                for (const auto& point : scan.points) {
                    if (point.distance > 0.1f && point.distance < 10.0f) {
                        // Valid measurement
                        // TODO: Add your robot logic here
                    }
                }
                
            } catch (const std::exception& e) {
                if (running) {
                    std::cerr << "Error: " << e.what() << std::endl;
                }
            }
        }
        
        // Shutdown
        lidar.sendCommand("sEN LMDscandata 0");
        lidar.sendCommand("sMN mSC stopmeas");
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    lidar.disconnect();
    return 0;
}
```

## CMakeLists.txt for LiDAR Project

```cmake
cmake_minimum_required(VERSION 3.16)
project(star_lidar CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(star_lidar
    src/main.cpp
    src/lidar_connection.cpp
    src/scan_data_parser.cpp
)

target_link_libraries(star_lidar pthread)

install(TARGETS star_lidar DESTINATION bin)
```

## Testing

### Local Testing (Simulation)

You can test your code without the physical LiDAR by creating a simple TCP server that responds with mock data.

### Hardware Testing Checklist

1. Connect LiDAR to same network as PYNQ-Z2
2. Configure LiDAR IP (default 192.168.0.1)
3. Verify connectivity: `ping 192.168.0.1`
4. Test with SOPAS ET software first (optional)
5. Run your application

## Troubleshooting

### Connection Refused

- Check LiDAR is powered on
- Verify network configuration
- Test with `telnet 192.168.0.1 2111`

### No Scan Data

- Ensure measurement is started (`sMN mSC startmeas`)
- Enable scan output (`sEN LMDscandata 1`)
- Check access mode is set

### Incomplete Telegrams

- Increase buffer size if needed
- Handle multiple recv() calls for large telegrams

## Performance Considerations

- Use non-blocking sockets for production
- Consider a dedicated thread for LiDAR communication
- Implement proper timeout handling
- Buffer scan data for processing thread

## Next Steps

- [Build & Deploy Workflow](./build-deploy-workflow.md) - Deploy to PYNQ-Z2
- [Embedded C++ Best Practices](./embedded-cpp-best-practices.md) - Optimize performance
- [Remote Debugging](./remote-debugging.md) - Debug on target
