# A7670G CAT1 4G LTE Module - Comprehensive Technical Guide for ESP32-IDF C Driver Development

## Table of Contents
1. [Module Overview](#module-overview)
2. [Hardware Specifications](#hardware-specifications)
3. [UART Protocol & Communication](#uart-protocol--communication)
4. [AT Command Set](#at-command-set)
5. [Network Registration & Connectivity](#network-registration--connectivity)
6. [Socket Communication (TCP/UDP)](#socket-communication-tcpudp)
7. [HTTP/HTTPS/MQTT Support](#httphttpsmqtt-support)
8. [SMS Functionality](#sms-functionality)
9. [GPS/GNSS Positioning](#gpsgnss-positioning)
10. [Power Modes & Energy Management](#power-modes--energy-management)
11. [SSL/TLS Configuration](#ssltls-configuration)
12. [Security Considerations](#security-considerations)
13. [Memory Safety in Drivers](#memory-safety-in-drivers)
14. [ESP32-IDF Implementation](#esp32-idf-implementation)

---

## Module Overview

### Key Characteristics
- **Manufacturer**: SIMcom Wireless Solutions
- **Module Type**: Integrated wireless communication module
- **Cellular Standards**: LTE CAT1, LTE-FDD, LTE-TDD, GSM, GPRS, EDGE
- **Downlink Speed**: 10 Mbps
- **Uplink Speed**: 5 Mbps
- **Supported LTE Bands**:
  - LTE-TDD: B38, B39, B40, B41
  - LTE-FDD: B1, B2, B3, B4, B5, B7, B8, B12, B13, B18, B19, B20, B25, B26, B28, B66
- **GSM Bands**: 850/900/1800/1900 MHz
- **Form Factor**: Compact module designed for IoT applications

### Network Protocols Supported
- TCP/IP, IPv4
- Multi-PDP (Packet Data Protocol)
- FTP, FTPS
- HTTP, HTTPS
- MQTT, MQTTs
- DNS
- SSL/TLS

### Interfaces
- **UART**: Primary control interface (3x UART: full-function, standard, debug)
- **USB 2.0**: Alternative interface, can function as network card
- **I2C**: Secondary interface for configuration
- **GPIO**: General purpose I/O pins for control signals

---

## Hardware Specifications

### Pin Configuration

#### Power & Control Pins
| Pin | Function | Voltage | Notes |
|-----|----------|---------|-------|
| VBAT | Power Supply | 3.6V - 4.2V | Primary power input |
| GND | Ground | 0V | Common ground |
| PWRKEY | Power Control | 1.8V/0V | Toggle to power on/off (>1sec low pulse) |
| RESET | Reset Signal | 1.8V/0V | Hard reset (>100ms low pulse) |

#### UART Pins (Standard Configuration)
| Pin | Function | Voltage | Typical Connection |
|-----|----------|---------|-------------------|
| TXD | Transmit Data | 1.8V | ESP32 RX via level shifter |
| RXD | Receive Data | 1.8V | ESP32 TX via level shifter |
| GND | Ground | 0V | Common ground |

**Critical Voltage Consideration**: Module operates at 1.8V logic levels
- **Must use level shifter** when connecting to ESP32 (3.3V logic)
- Recommended approach: Dedicated 3.3V to 1.8V level shifter IC

#### Standard ESP32-A7670G Wiring
```
ESP32 GPIO17 (TX) -> Level Shifter -> A7670G RXD (1.8V)
ESP32 GPIO16 (RX) -> Level Shifter -> A7670G TXD (1.8V)
ESP32 GND -> A7670G GND (common reference)
ESP32 GPIO4 -> A7670G PWRKEY (power control)
ESP32 GPIO5 -> A7670G RESET (optional, hard reset)
```

#### Alternative Pin Configuration (T-A7670G Board)
- **TX**: GPIO 26
- **RX**: GPIO 27
- **Power Enable**: GPIO 12
- **Reset**: GPIO 5
- **Power Key**: GPIO 4
- **Baud Rate**: 115200 bps

---

## UART Protocol & Communication

### UART Configuration

**Standard Parameters**:
```
Baud Rate:    115200 bps (standard), can support lower rates
Data Bits:    8
Stop Bits:    1
Parity:       None
Flow Control: None (optional RTS/CTS support)
```

### Frame Structure

**Standard AT Command Frame**:
```
[CR][LF] + Command + [CR][LF]
```

Example:
```
AT[CR][LF]
```

Where:
- `[CR]` = Carriage Return (0x0D)
- `[LF]` = Line Feed (0x0A)

### Response Format

**Standard Response**:
```
[CR][LF] + <response> + [CR][LF][OK][CR][LF]
```

**Error Response**:
```
[CR][LF][ERROR][CR][LF]
```

**Unsolicited Result Code (URC)**:
```
[CR][LF]<URC message>[CR][LF]
```

### Timeout Specifications

| Operation | Timeout | Notes |
|-----------|---------|-------|
| Standard AT Command | 1-3 seconds | Network operations may require longer |
| TCP Connection | 30-120 seconds | Depends on network conditions |
| SSL/TLS Negotiation | 10-30 seconds | Certificate validation time |
| SMS Send/Receive | 5-10 seconds | Depends on network |
| GPS Fix | 30-180 seconds | Cold start vs warm start |

---

## AT Command Set

### Command Classification & Examples

#### 1. Basic AT Commands

**Syntax**: `AT` (Attention - connection test)
**Response**: `OK` or `ERROR`

```
AT                          // Test module communication
AT+GMM                      // Get module model (A7670G)
AT+GMR                      // Get firmware revision
AT+GSMR                     // Get software release date
AT+GSIR                     // Get software revision information
AT+GSCR                     // Get software customization release
```

#### 2. SIM & Network Registration Commands

```
AT+CPIN?                    // Check SIM card status
// Response: +CPIN: READY or +CPIN: SIM PIN (needs PIN)

AT+CPIN="1234"              // Enter SIM PIN
AT+CLCK="SC",0,"1234"       // Disable SIM lock

AT+CGREG?                   // Check GPRS registration status
// Response: +CGREG: <n>,<stat>[,<lac>,<ci>[,<AcT>]]
// <stat>: 0=not registered, 1=home, 2=searching, 5=roaming

AT+CREG?                    // Check network registration (voice)
// Response: +CREG: <n>,<stat>[,<lac>,<ci>[,<AcT>]]

AT+COPS?                    // Get current operator
// Response: +COPS: <mode>,<format>,<oper>[,<AcT>]

AT+COPS=0                   // Automatic operator selection
AT+COPS=1,2,"46000"         // Manual operator selection (MCC-MNC)
```

**Network Registration States**:
- `0`: Not registered, not searching
- `1`: Registered, home network
- `2`: Not registered, searching
- `3`: Registration denied
- `5`: Registered, roaming

#### 3. Signal Quality Commands

```
AT+CSQ                      // Get signal quality
// Response: +CSQ: <rssi>,<ber>
// <rssi>: 0-31 (31=highest), 99=not known
// <ber>: 0-7 (0=lowest), 99=not known

AT+CIND?                    // Get indicator status
AT+CIND="AC",4              // Set indicator value

AT+CSDH=1                   // Enable detailed error reporting
AT+CMEE=2                   // Enable verbose error messages
```

#### 4. IMSI/IMEI Commands

```
AT+CIMI                     // Get IMSI (International Mobile Subscriber Identity)
// Response: 460020123456789

AT+CGSN                     // Get IMEI (International Mobile Equipment Identity)
// Response: 866666666666666

AT+CNUM                     // Get subscriber phone number
// Response: +CNUM: "","+12025550199",129
```

#### 5. Band Selection

```
AT+QCFG="bands/lte"                    // Query available LTE bands
AT+QCFG="bands/lte",0x400A0E5,0x2010B5 // Set specific LTE bands
```

---

## Network Registration & Connectivity

### PDP Context Configuration

**Step 1: Define PDP Context**
```
AT+CGDCONT=1,"IP","3gnet","0.0.0.0",0,0
//          |   |    |    |           |  |
//          |   |    |    |           |  +-- GGSN address type (0=IPv4)
//          |   |    |    +-------------------PDP address
//          |   |    +------------------------PDP type (IP/PPP/IPv6/etc)
//          |   +----------------------------Primary PDP type indicator
//          +-------------------------------Context ID (1-3 typical)
```

**Example for common APNs**:
```
// T-Mobile USA
AT+CGDCONT=1,"IP","fast.t-mobile.com"

// Verizon
AT+CGDCONT=1,"IP","vzwinternet"

// AT&T
AT+CGDCONT=1,"IP","broadband"

// China Mobile
AT+CGDCONT=1,"IP","cmnet"

// China Unicom
AT+CGDCONT=1,"IP","3gnet"

// China Telecom
AT+CGDCONT=1,"IP","ctnet"
```

**Step 2: Attach to GPRS**
```
AT+CGATT=1                  // Attach to GPRS/LTE network
AT+CGATT?                   // Check attachment status (1=attached)
```

**Step 3: Activate PDP Context**
```
AT+CGACT=1,1                // Activate context ID 1
//        | |
//        | +-- Context ID
//        +---- 1=activate, 0=deactivate

AT+CGACT?                   // Query active contexts
// Response: +CGACT: 1,1 (context 1 is active)
```

**Step 4: Get IP Address**
```
AT+CGPADDR=1                // Get address for context 1
// Response: +CGPADDR: 1,"10.191.208.124"
```

### Multiple PDP Context Management

```
// Define secondary PDP context
AT+CGDCONT=2,"IP","secondary.apn.com"

// Different context IDs can have different APNs
AT+CGDCONT=3,"IP","mqtt.provider.com"

// Query all defined contexts
AT+CGDCONT?
// Response: +CGDCONT: 1,"IP","3gnet","0.0.0.0",0,0
//           +CGDCONT: 2,"IP","secondary.apn.com","0.0.0.0",0,0

// Activate specific context for socket operations
AT+CSOCKSSETPN=1            // Set active context for TCP/IP sockets
```

### Network Disconnection & Cleanup

```
AT+CGACT=0,1                // Deactivate context 1
AT+CGATT=0                  // Detach from GPRS/LTE network
```

---

## Socket Communication (TCP/UDP)

### TCP Socket Operations

#### TCP Connection (Non-Transparent Mode)

**Step 1: Open Socket**
```
AT+CAOPEN=1,0,"TCP","your.server.com",80
//         | |  |    |                |
//         | |  |    |                +-- Port
//         | |  |    +--------------------Server address (domain or IP)
//         | |  +------------------------Protocol (TCP or UDP)
//         | +---------------------------Bearer (0=PDP context 1)
//         +-----------------------------Socket index (0-10 typical)

// Response: +CAOPEN: 1,0
//           (socket handle, context)
```

**Step 2: Send Data**
```
AT+CASEND=1,5
// (prompts for 5 bytes)
Hello
// Response: +CASEND: 1,5
//           (socket handle, bytes sent)
```

**Step 3: Receive Data**
```
AT+CARECV=1,1024             // Receive up to 1024 bytes from socket 1
// Response: +CARECV: 1,100,"data..."
//           (socket handle, bytes received, data)
```

**Step 4: Close Socket**
```
AT+CACLOSE=1                 // Close socket 1
// Response: +CACLOSE: 1
```

#### TCP Connection (Alternative - QIOPEN Method)

```
// Alternative socket API (Quectel-compatible)
AT+QISRVC=0                  // Disable TCP/UDP service first if needed
AT+QIOPEN=1,0,"TCP","www.example.com",80,0
//        | |  |    |                 |  |
//        | |  |    |                 |  +-- Access mode (0=buffer/1=direct)
//        | |  |    |                 +-----Port
//        | |  |    +----------------------Server
//        | |  +---------------------------Protocol
//        | +------------------------------Context ID
//        +-------------------------------Number of context IDs

// Response: +QIOPEN: 0,0  (connection ID, result)
```

### UDP Socket Operations

```
AT+CAOPEN=1,0,"UDP","192.168.1.100",9999
//        |            |      |
//        |            |      +-- Server address
//        |            +--------- Protocol: UDP
//        +---------------------- Socket index

// Send UDP data
AT+CASEND=1,10
// (send 10 bytes)

// Receive UDP data
AT+CARECV=1,1024
```

### Socket State Management

```
AT+CASTATE?                  // Query state of all sockets
// Response: +CASTATE: 1,1   (socket 1, state 1=connected)
//           +CASTATE: 2,0   (socket 2, state 0=not connected)

// States: 0=initial, 1=connected, 2=listening, 3=closing
```

### TCP Keep-Alive Configuration

```
AT+CACONFIG=<socketId>,<paramTag>,<paramValue>

// Set keep-alive interval
AT+CACONFIG=1,20,60          // Keep-alive every 60 seconds
```

### Error Handling for Sockets

```
AT+CAONERR?                  // Query socket error handling mode
AT+CAONERR=0,1               // Close socket on error (recommended)
```

---

## HTTP/HTTPS/MQTT Support

### HTTP/HTTPS Client Operations

#### HTTP GET Request

```
AT+HTTPINIT                  // Initialize HTTP client
// Response: OK or +HTTPINIT: 1 (1=initialized)

AT+HTTPPARA="URL","http://www.example.com/api/data"
//                                              ^
//                                 Optional path and query

AT+HTTPPARA="CONTENT","application/json"
AT+HTTPACTION=0              // 0=GET, 1=POST, 2=HEAD
// Response: +HTTPACTION: 0,200,1234
//           (method, status code, response length)

AT+HTTPREAD=0,1024           // Read response (offset=0, length=1024)
// Response: +HTTPREAD: 200,"JSON response data..."

AT+HTTPTERM                  // Close HTTP connection
```

#### HTTP POST Request with Data

```
AT+HTTPINIT
AT+HTTPPARA="URL","http://api.example.com/data"

// Set POST data
AT+HTTPDATA=50,30000         // 50 bytes, 30-sec timeout
// (module responds with DOWNLOAD and waits for data)
{"temperature":25.5,"humidity":65}
// (send exactly 50 bytes, then Ctrl+Z or EOF)

AT+HTTPACTION=1              // 1=POST
// Response: +HTTPACTION: 1,201,15
//           (method, status code, response length)

AT+HTTPREAD                  // Read server response
AT+HTTPTERM
```

#### HTTPS GET (SSL/TLS)

```
AT+HTTPINIT
AT+HTTPPARA="URL","https://secure.example.com/data"

// Configure SSL context (see SSL/TLS section)
AT+CSSLCFG="sslversion",0,3  // Use TLS 1.2

AT+HTTPACTION=0              // GET request over HTTPS
// Response: +HTTPACTION: 0,200,1234

AT+HTTPREAD
AT+HTTPTERM
```

### MQTT Client Operations

#### MQTT Connection Setup

```
// Enable MQTT service
AT+CMQTTSTART                // Start MQTT service
// Response: OK

// Set up client ID
AT+CMQTTACCQ=0,"ClientID",0  // Context 0, client ID, version
//             |   |         |
//             |   |         +-- MQTT version (0=3.1, 1=3.1.1)
//             |   +----------- Client identifier
//             +-------------- Client index

// Connect to broker
AT+CMQTTCONNECT=0,"tcp://mqtt.eclipse.org",20,1
//               |  |                      |  |
//               |  |                      |  +-- Keep-alive interval (seconds)
//               |  |                      +--------- Broker address and protocol
//               |  +-- Protocol: tcp:// or ssl://
//               +------ Client index

// Response: +CMQTTCONNECT: 0,0  (client index, result 0=success)
```

#### MQTT Publish

```
AT+CMQTTTOPIC=0,15           // Set topic length (15 bytes)
// Responses with ">" indicating ready for input
home/sensor/temp
// (exactly 15 bytes)

// Set payload
AT+CMQTTPAYLOAD=0,6          // Payload length (6 bytes)
// Responds with ">"
25.50C
// (exactly 6 bytes)

// Publish message
AT+CMQTTPUB=0,0,60           // Client 0, QoS 0, timeout 60 seconds
// Response: +CMQTTPUB: 0,0,0  (client, packet ID, result)
```

#### MQTT Subscribe

```
AT+CMQTTSUB=0,18,1           // Subscribe to topic (client, length, QoS)
// Responds with ">"
home/control/light
// (exactly 18 bytes)

// Response: +CMQTTSUB: 0,0,0  (client, packet ID, result)

// Unsolicited response when message arrives:
// +CMQTTRECV: 0,4,11
// (client, topic length, payload length)
// home/light{"on":true}
```

#### MQTT Disconnect

```
AT+CMQTTDISCONNECT=0         // Disconnect client 0
// Response: OK

AT+CMQTTSTOP                 // Stop MQTT service
```

### MQTT Keep-Alive Configuration

```
AT+CMQTTCONNECT=0,"tcp://broker",30,1
//                                  |
//                                  +-- Keep-alive (30 seconds)
```

---

## SMS Functionality

### SMS Configuration

```
// Set SMS mode to TEXT (easier to read)
AT+CMGF=1                    // 1=text mode, 0=PDU mode
// Response: OK

// Set SMS message center (optional, usually auto-detected)
AT+CSCA?                     // Query SMS center
AT+CSCA="+1234567890"        // Set SMS center number
```

### Send SMS

```
AT+CMGS="5551234567"         // Send to recipient number
// Response: >
Hello World
// Send Ctrl+Z or ^Z to complete

// Response: +CMGS: 123,0  (message reference, error code)
```

### Read SMS

```
AT+CMGL="ALL"                // List all SMS messages
// Response: +CMGL: 0,0,"Sender","2024-01-15,10:30:45+00",
//           "Message text here"
//           +CMGL: 1,1,"Another","2024-01-15,11:00:00+00",
//           "Another message"
//
// Message status: 0=received unread, 1=received read,
//                 2=stored unsent, 3=stored sent

AT+CMGR=0                    // Read specific message (index 0)
// Response: +CMGR: 0,"Sender","2024-01-15,10:30:45+00"
//           "Full message text"
```

### Delete SMS

```
AT+CMGD=0                    // Delete message at index 0
AT+CMGD=1,4                  // Delete all messages
//        | |
//        | +-- 4=delete all messages
//        +---- Message index (ignored when deleting all)
```

### SMS Incoming Notification

```
// Enable unsolicited result codes for incoming SMS
AT+CNMI=2,1,0,1              // Mode 2, SMS to TE, no delivery, all storage

// When SMS arrives, unsolicited message:
+CMT: "Sender","2024-01-15,10:30:45+00"
"Message text"
```

### SMS Memory Management

```
AT+CPMS?                     // Query SMS storage status
// Response: +CPMS: 12,100,0,100,0,100
//           (SM used/total, ME used/total, MT used/total)

AT+CPMS="SM","SM","SR"       // Set preferred storage to SIM
//        |   |   |
//        |   |   +-- SMS deliver storage (not often used)
//        |   +------ SMS send storage (not often used)
//        +---------- SMS read/unread storage (SIM card)
```

---

## GPS/GNSS Positioning

### GPS/GNSS Initialization

```
AT+CGNSSPWR=1                // Power on GNSS
AT+CGNSSPWR=0                // Power off GNSS

AT+CGNSSPWR?                 // Check GNSS power status
// Response: +CGNSSPWR: 1  (1=on, 0=off)
```

### GNSS Data Output Configuration

```
// Enable GNSS sentence output
AT+CGNSSTST=1                // Enable GNSS test mode (continuous output)
AT+CGNSSTST=0                // Disable test mode

// Switch output port (if multiple UART interfaces available)
AT+CGNSSPORTSWITCH=0,1       // UART 0, enable NMEA output

// NMEA sentence output format:
// $GPGGA,timestamp,latitude,N/S,longitude,E/W,fix,satellites,HDOP,altitude,...
// $GPGSA,A/M,fix_type,satellite_PRNs...,PDOP,HDOP,VDOP,...
// $GPRMC,timestamp,status,latitude,N/S,longitude,E/W,speed,heading,date,...
```

### GPS Position Retrieval

```
AT+CGPSINFO                  // Get current GPS position
// Response: +CGPSINFO: lat,N/S,lon,E/W,date,UTC_time,altitude,speed,course
//           +CGPSINFO: 3723.2475,N,12158.3416,W,131024,092949.927,800.5,10.0,170.0

// Example: 37° 23' 24.75" N, 121° 58' 34.16" W, 800.5m altitude
```

### GPS Positioning Timing

```
// GPS fix acquisition times (approximate)
Cold Start (no ephemeris):     30-180 seconds (TTFF)
Warm Start (old ephemeris):    15-30 seconds
Hot Start (recent fix):        5-10 seconds
Aiding Data Available (AGPS):  3-5 seconds (if assisted GNSS available)

// Recommendations for IoT applications:
1. Power on GPS during initialization
2. Wait 30-60 seconds for cold start
3. Cache almanac/ephemeris data
4. Use assisted GPS if network available
5. Implement location caching for faster re-fixes
```

### AGPS (Assisted GPS) Support

```
// Some applications support A-GPS data injection
// This requires downloading ephemeris from external server
// Not directly supported via AT commands - requires:
// 1. Download assistance data from online service
// 2. Store in module file system
// 3. Enable auto-acquisition on startup
```

---

## Power Modes & Energy Management

### Power Supply Specifications

```
Operating Voltage (VBAT):  3.6V - 4.2V (nominal 3.8V)
Typical Current Consumption:
  - Active Mode (LTE):     600-800 mA average
  - Sleep Mode:            <10 mA
  - Deep Sleep:            <1 mA

Power-On Duration:         1.2 seconds (PWRKEY pulse >1.0 sec)
Power-Off Duration:        >3.0 seconds (normal shutdown)
```

### Sleep Mode Control

```
// Enable low power mode
AT+CFUN=1                    // Set phone functionality (1=full)
AT+CFUN=0                    // Set phone functionality (0=minimum)

// Enter sleep/power-saving mode
// Module automatically enters sleep when idle

// Query power mode
AT+CPSMS?                    // Query PSM (Power Saving Mode) status
// Response: +CPSMS: <enabled>,<Requested_periodic_TAU>,<Requested_active_time>

// Enable PSM (Power Saving Mode)
AT+CPSMS=1,"01000010","00011110"
//        |  |         |
//        |  |         +-- Active Time (10 seconds)
//        |  +----------- Periodic TAU (2560 seconds = ~42 minutes)
//        +-------------- Enable PSM (1=enable, 0=disable)
```

### DRX and eDRX Modes

```
// Discontinuous Reception (DRX)
AT+CEDRXS=1,4,1234           // Enable eDRX, LTE Cat-M, cycle 1234
//         | | |
//         | | +-- eDRX cycle (in 100ms units, e.g., 1234 = 123.4 seconds)
//         | +---- RAT (4=LTE Cat-M, 5=NB-IoT)
//         +------ Enable/disable (1=enable, 0=disable)

AT+CEDRXS?                   // Query eDRX status
```

### PWRKEY Control for Power Management

```
// Hardware Power-On:
// 1. Ensure VBAT = 3.8V
// 2. Pull PWRKEY LOW for >1 second
// 3. Release PWRKEY
// 4. Module powers on (takes ~1.2 seconds)

// Hardware Power-Off:
// 1. Pull PWRKEY LOW for >3 seconds
// 2. Release PWRKEY
// 3. Module shuts down gracefully

// In ESP32-IDF code:
// gpio_set_level(PWRKEY_GPIO, 0);    // Pull low
// vTaskDelay(pdMS_TO_TICKS(1200));   // Wait 1.2 seconds
// gpio_set_level(PWRKEY_GPIO, 1);    // Release high

// For graceful shutdown:
// AT+CPOWD=0                 // Software power-off
// (then wait for module to power off completely)
```

---

## SSL/TLS Configuration

### SSL/TLS Context Setup

#### Step 1: Configure Authentication Mode

```
// Mode 0: No server verification (less secure, useful for testing)
AT+CSSLCFG="authmode",0,0    // Context 0, no authentication
//                       |  |
//                       |  +-- 0=off, 1=server verified, 2=mutual auth
//                       +------ Context ID (0-3 typical)

// Mode 1: Verify server certificate
AT+CSSLCFG="authmode",0,1    // Context 0, server verification
// Requires CA certificate to be uploaded

// Mode 2: Mutual authentication (client + server)
AT+CSSLCFG="authmode",0,2    // Context 0, mutual authentication
// Requires both CA certificate and client certificate/key
```

#### Step 2: Configure Ignore RTC Time (Important!)

```
// If module RTC is not set correctly, SSL will fail
AT+CSSLCFG="ignorertctime",0,1
//                          |  |
//                          |  +-- 1=ignore RTC, 0=use RTC
//                          +------ Context ID

// Alternative: Set system time manually
AT+CCLK="24/01/15,10:30:45+00"
//       |
//       +-- Date/time format: yy/MM/dd,hh:mm:ss+tz
```

#### Step 3: Configure TLS Version

```
AT+CSSLCFG="sslversion",0,3  // Context 0, TLS 1.2
//                       |  |
//                       |  +-- 0=SSL3.0, 2=TLS1.0, 3=TLS1.2, 4=TLS1.3
//                       +------ Context ID

// Recommended: Use TLS 1.2 or higher for security
```

#### Step 4: Configure Server Name Indication (SNI)

```
// For virtual-hosted domains (highly recommended)
AT+CSSLCFG="sni",0,"api.example.com"
//                |  |
//                |  +-- Domain name for SNI
//                +------ Context ID
```

### Certificate Installation

#### Upload CA Certificate (for server verification)

```
// Step 1: Open file system
AT+CFSINIT                   // Initialize file system
// Response: OK

// Step 2: Upload certificate file
AT+CCERTDOWN="ca.crt",2048   // File name, length in bytes
// Module responds with: DOWNLOAD
// (send binary certificate data, 2048 bytes)

// Example: Send PEM-formatted CA certificate
// -----BEGIN CERTIFICATE-----
// MIIDXTCCAkWgAwIBAgIJALz3G5+...
// ...
// -----END CERTIFICATE-----

// Step 3: Configure certificate for SSL context
AT+CSSLCFG="cacert",0,"ca.crt"
//                    |  |
//                    |  +-- File name
//                    +------ Context ID

// Step 4: Close file system
AT+CFSTERM                   // Terminate file system
```

#### Upload Client Certificate (for mutual authentication)

```
AT+CFSINIT
AT+CCERTDOWN="client.crt",2048
// (send client certificate)

AT+CSSLCFG="clientcert",0,"client.crt"
//                          |  |
//                          |  +-- Certificate file
//                          +------ Context ID
```

#### Upload Client Key (for mutual authentication)

```
AT+CFSINIT
AT+CCERTDOWN="client.key",2048
// (send client private key - should be unencrypted for AT interface)

AT+CSSLCFG="clientkey",0,"client.key"
//                        |  |
//                        |  +-- Key file
//                        +------ Context ID
```

### Complete HTTPS Configuration Example

```
// Configure for HTTPS GET to https://secure.example.com

// 1. Set time (if RTC not set)
AT+CCLK="24/01/15,14:30:00+00"

// 2. Initialize SSL context
AT+CSSLCFG="authmode",0,1          // Server verification
AT+CSSLCFG="sslversion",0,3        // TLS 1.2
AT+CSSLCFG="ignorertctime",0,1     // Ignore RTC time
AT+CSSLCFG="sni",0,"secure.example.com"  // SNI

// 3. Perform HTTPS request
AT+HTTPINIT
AT+HTTPPARA="URL","https://secure.example.com/api/data"
AT+HTTPACTION=0                    // GET request
// Response: +HTTPACTION: 0,200,1234  (success)

AT+HTTPREAD
AT+HTTPTERM
```

### Certificate Conversion for AT Interface

```
// Most certificates are in PEM format (text-based)
// To send via AT commands, they must be:

// 1. Already in PEM text format (base64-encoded)
// 2. Can be sent as-is for some modules
// 3. For binary DER format, requires special handling

// Important: Include certificate markers:
-----BEGIN CERTIFICATE-----
(base64 data, 64 chars per line)
-----END CERTIFICATE-----

// Command reference:
AT+CCERTDOWN="cert.pem",2048
// Wait for DOWNLOAD prompt
// Send entire certificate (including BEGIN/END markers)
```

### Error Handling for SSL/TLS

```
// Common SSL errors:
// +CME ERROR 506: Uninitialized profile
// +CME ERROR 507: SSL negotiation failed
// +CME ERROR 508: SSL certificate not found
// +CME ERROR 509: RTC not set
// +CME ERROR 510: Certificate format error

// Troubleshooting:
// 1. Verify RTC is set: AT+CCLK?
// 2. Check certificate upload: AT+CCERTLIST (if supported)
// 3. Verify domain name in SNI matches certificate CN
// 4. Use TLS 1.2 minimum
// 5. For public certificates, authentication mode 1 is typical
```

---

## Security Considerations

### AT Command Injection Vulnerabilities

**Risk Assessment**: HIGH

AT commands from user input could lead to:
- Modification of network settings
- Connection to malicious servers
- SMS message interception/sending
- Unauthorized data transmission

**Mitigation Strategies**:

1. **Input Validation**:
   ```c
   // Only allow specific expected formats
   int validate_phone_number(const char *num) {
       if (strlen(num) < 7 || strlen(num) > 15) return 0;
       for (int i = 0; i < strlen(num); i++) {
           if (!isdigit(num[i]) && num[i] != '+') return 0;
       }
       return 1;
   }

   // Only allow alphanumeric for URLs
   int validate_url(const char *url) {
       if (strstr(url, "AT+") != NULL) return 0;  // Block AT commands
       if (strstr(url, "\n") != NULL) return 0;   // Block newlines
       if (strstr(url, "\r") != NULL) return 0;   // Block carriage returns
       // Verify URL format (https://, proper domain, etc.)
       return 1;
   }
   ```

2. **Command Whitelisting**:
   - Only send pre-approved AT commands
   - Use enumeration for command types
   - Never concatenate user input directly

3. **Input Sanitization**:
   ```c
   // Remove/escape dangerous characters
   void sanitize_input(char *input) {
       for (int i = 0; input[i]; i++) {
           if (input[i] == '\r' || input[i] == '\n') {
               input[i] = '\0';  // Null-terminate at first CR/LF
               break;
           }
       }
   }
   ```

### Response Buffer Overflow Vulnerabilities

**Risk Assessment**: HIGH

Oversized AT command responses could overflow fixed buffers.

**Vulnerable Pattern**:
```c
// DANGEROUS!
char response[256];
while (get_char_from_uart(&ch)) {
    response[index++] = ch;  // No bounds checking!
}
```

**Safe Implementation**:
```c
// SAFE
#define MAX_RESPONSE 4096
typedef struct {
    char buffer[MAX_RESPONSE];
    size_t length;
    size_t max_length;
} ResponseBuffer;

int append_to_response(ResponseBuffer *buf, char ch) {
    if (buf->length >= buf->max_length - 1) {
        // Buffer full, error condition
        return -1;
    }
    buf->buffer[buf->length++] = ch;
    return 0;
}
```

### SMS Parsing Injection Attacks

**Risk Assessment**: MEDIUM-HIGH

SMS messages can contain:
- Malicious AT command sequences
- Buffer overflow payloads
- State-machine breaking patterns

**Mitigation**:

1. **Parse SMS safely**:
   ```c
   typedef struct {
       char sender[20];
       char content[160];
       size_t content_len;
   } SMSMessage;

   // Strict parsing with length limits
   int parse_sms_safe(const char *raw, SMSMessage *msg) {
       // Extract sender (limited to 20 chars)
       // Extract content (max 160 chars typical)
       // Validate both fields
       // Return error if any field exceeds limits
   }
   ```

2. **Disable SMS processing if not needed**:
   ```c
   // Disable unsolicited SMS notifications
   AT+CNMI=0,0,0,0  // Disable all notifications
   ```

3. **Handle SMS as data, not commands**:
   - Never execute SMS content as AT commands
   - Validate SMS format strictly
   - Log SMS reception for debugging

### Credential Protection

**Risk Assessment**: CRITICAL

Credentials stored/transmitted insecurely lead to service compromise.

**Secure Practices**:

1. **APN Credentials**:
   ```c
   // Store APN in flash/secure storage, not hardcoded
   // Never transmit APN over unencrypted connection
   typedef struct {
       const char apn[64];        // Read from secure config
       const char username[64];   // May be NULL
       const char password[64];   // May be NULL
   } APNConfig;

   // Use encrypted storage (ESP32 NVS with encryption)
   esp_err_t store_apn_encrypted(const APNConfig *config) {
       nvs_handle_t handle;
       esp_err_t ret = nvs_open("cellular", NVS_READWRITE, &handle);
       // Store encrypted...
   }
   ```

2. **MQTT Credentials**:
   ```c
   // Always use TLS for MQTT connection
   AT+CMQTTCONNECT=0,"ssl://mqtt.broker",1883,...

   // Store username/password securely
   // Consider using certificate-based authentication instead
   ```

3. **API Keys/Tokens**:
   ```c
   // Never hardcode API keys in source
   // Store in encrypted NVS
   // Rotate periodically
   // Use HTTPS only
   ```

### Connection State TOCTOU (Time-of-Check-Time-of-Use) Vulnerabilities

**Risk Assessment**: MEDIUM

Race condition between checking connection state and using it.

**Vulnerable Pattern**:
```c
// DANGEROUS - TOCTOU vulnerability
if (is_socket_connected(socket_id)) {  // Check
    // ... delay/interrupt here ...
    send_data(socket_id, data);         // Use (may fail if disconnected)
}
```

**Safe Implementation**:
```c
// SAFE - Atomic check-and-use
typedef struct {
    int socket_id;
    volatile bool connected;
    SemaphoreHandle_t state_lock;
} SocketState;

int send_data_safe(SocketState *sock, const uint8_t *data, size_t len) {
    xSemaphoreTake(sock->state_lock, portMAX_DELAY);

    if (!sock->connected) {
        xSemaphoreGive(sock->state_lock);
        return -1;
    }

    // Perform send while holding lock
    int ret = send_data_impl(sock->socket_id, data, len);

    // Update state if send failed
    if (ret < 0) {
        sock->connected = false;
    }

    xSemaphoreGive(sock->state_lock);
    return ret;
}
```

### Unsolicited Result Code (URC) Parsing Security

**Risk Assessment**: MEDIUM

URCs can arrive at any time, potentially corrupting parser state.

**Vulnerable Pattern**:
```c
// DANGEROUS - URC in middle of command response
// Expect: +CGPADDR: 1,"10.0.0.1"[CR][LF]
// Actual:  +CMTI: "MT",1[CR][LF]          <- Incoming SMS URC
//          +CGPADDR: 1,"10.0.0.1"[CR][LF]
```

**Safe Implementation**:
```c
typedef struct {
    enum {
        STATE_IDLE,
        STATE_WAITING_RESPONSE,
        STATE_IN_URC
    } state;
    char line_buffer[256];
    size_t line_pos;
} ParserState;

// Handle each line independently
void process_uart_line(ParserState *parser, const char *line) {
    // Determine if line is URC or response
    if (is_urc(line)) {
        // Handle URC separately, don't interfere with command response
        handle_urc(line);
    } else if (parser->state == STATE_WAITING_RESPONSE) {
        // This is command response
        handle_response(line);
    }
}

int is_urc(const char *line) {
    // URCs start with +, but aren't expected responses
    // Examples: +CMT, +CMTI, +CRING, +CREG, etc.
    if (line[0] != '+') return 0;

    // Check against list of expected responses
    // If not in list, it's a URC
    return !is_expected_response(line);
}
```

---

## Memory Safety in Drivers

### Response Parser Buffer Management

**Key Principles**:
1. Always check buffer length before writing
2. Use consistent, bounded buffers
3. Validate before parsing
4. Handle incomplete/malformed data gracefully

**Safe Response Parser Pattern**:

```c
#define MAX_RESPONSE_LINE 1024
#define MAX_RESPONSE_LINES 10

typedef struct {
    char lines[MAX_RESPONSE_LINES][MAX_RESPONSE_LINE];
    size_t line_count;
    size_t current_line_idx;
    size_t current_line_len;
} ResponseBuffer;

// Bounded line accumulation
int append_response_char(ResponseBuffer *buf, char ch) {
    size_t cur_idx = buf->current_line_idx;

    // Check line bounds
    if (buf->current_line_len >= MAX_RESPONSE_LINE - 1) {
        return -1;  // Line too long
    }

    // Check line array bounds
    if (cur_idx >= MAX_RESPONSE_LINES) {
        return -2;  // Too many response lines
    }

    // Safe append
    buf->lines[cur_idx][buf->current_line_len++] = ch;
    buf->lines[cur_idx][buf->current_line_len] = '\0';

    return 0;
}

// Process complete line
int process_response_line(ResponseBuffer *buf) {
    const char *line = buf->lines[buf->current_line_idx];
    size_t len = buf->current_line_len;

    // Validate line before parsing
    if (len > 0 && line[0] == '+') {
        // Parse structured response
        return parse_plusline(line, len);
    } else if (len > 0 && strncmp(line, "OK", 2) == 0) {
        return RESPONSE_OK;
    } else if (len > 0 && strncmp(line, "ERROR", 5) == 0) {
        return RESPONSE_ERROR;
    }

    return RESPONSE_UNKNOWN;
}
```

### Socket Buffer Safety

**Considerations**:
1. TCP sockets can send/receive in chunks
2. Multiple URCs can interleave with data
3. Application buffers must have bounds

**Safe Socket Implementation**:

```c
#define MAX_SOCKET_RX_BUFFER 4096
#define NUM_SOCKETS 10

typedef struct {
    int socket_id;
    uint8_t rx_buffer[MAX_SOCKET_RX_BUFFER];
    size_t rx_write_pos;
    size_t rx_read_pos;
    size_t rx_count;
    SemaphoreHandle_t rx_lock;
} SocketBuffer;

// Thread-safe socket receive
int socket_read_safe(SocketBuffer *sock, uint8_t *data, size_t len) {
    xSemaphoreTake(sock->rx_lock, portMAX_DELAY);

    // Calculate available data
    size_t available = sock->rx_count;
    size_t to_read = (len < available) ? len : available;

    // Copy data from circular buffer
    for (size_t i = 0; i < to_read; i++) {
        data[i] = sock->rx_buffer[sock->rx_read_pos];
        sock->rx_read_pos = (sock->rx_read_pos + 1) % MAX_SOCKET_RX_BUFFER;
    }

    sock->rx_count -= to_read;

    xSemaphoreGive(sock->rx_lock);
    return to_read;
}

// Receive data from modem safely
int socket_receive_handler(SocketBuffer *sock, const uint8_t *data, size_t len) {
    xSemaphoreTake(sock->rx_lock, portMAX_DELAY);

    // Check buffer space
    if (sock->rx_count + len > MAX_SOCKET_RX_BUFFER) {
        // Buffer overflow - handle gracefully
        xSemaphoreGive(sock->rx_lock);
        return -1;  // Drop data, signal error
    }

    // Copy data into circular buffer
    for (size_t i = 0; i < len; i++) {
        sock->rx_buffer[sock->rx_write_pos] = data[i];
        sock->rx_write_pos = (sock->rx_write_pos + 1) % MAX_SOCKET_RX_BUFFER;
    }

    sock->rx_count += len;

    xSemaphoreGive(sock->rx_lock);
    return len;
}
```

### UART Circular Buffer Implementation

**Requirements**:
1. DMA-safe circular buffer
2. Atomic pointer updates
3. No data loss during ISR transitions
4. Thread-safe read/write

**Robust Circular Buffer**:

```c
#define UART_RX_BUFFER_SIZE 4096

typedef struct {
    uint8_t buffer[UART_RX_BUFFER_SIZE];
    volatile size_t write_pos;
    volatile size_t read_pos;
    volatile size_t count;
} UARTCircularBuffer;

// Add byte from ISR (atomic update)
void uart_buffer_write_isr(UARTCircularBuffer *buf, uint8_t byte) {
    size_t next_write = (buf->write_pos + 1) % UART_RX_BUFFER_SIZE;

    // Check overflow
    if (next_write == buf->read_pos) {
        // Buffer full - drop oldest byte
        buf->read_pos = (buf->read_pos + 1) % UART_RX_BUFFER_SIZE;
    } else {
        buf->count++;
    }

    buf->buffer[buf->write_pos] = byte;
    buf->write_pos = next_write;
}

// Read from main task (disable interrupts for atomic read)
size_t uart_buffer_read(UARTCircularBuffer *buf, uint8_t *data, size_t max_len) {
    portDISABLE_INTERRUPTS();

    size_t available = buf->count;
    size_t to_read = (max_len < available) ? max_len : available;

    for (size_t i = 0; i < to_read; i++) {
        data[i] = buf->buffer[buf->read_pos];
        buf->read_pos = (buf->read_pos + 1) % UART_RX_BUFFER_SIZE;
    }

    buf->count -= to_read;

    portENABLE_INTERRUPTS();
    return to_read;
}
```

### URC (Unsolicited Result Code) Handling

**Safety Considerations**:
1. URCs can arrive anytime, even during command response parsing
2. Must not corrupt command response state
3. Need separate processing path

**Safe URC Handling**:

```c
#define URC_BUFFER_SIZE 256
#define URC_QUEUE_SIZE 16

typedef struct {
    char content[URC_BUFFER_SIZE];
    size_t length;
} URC;

typedef struct {
    URC queue[URC_QUEUE_SIZE];
    volatile int write_idx;
    volatile int read_idx;
    volatile int count;
    SemaphoreHandle_t available_sem;
} URCQueue;

// Line received from UART - determine if URC or response
void process_uart_line(const char *line, size_t len,
                      URCQueue *urc_queue, ResponseBuffer *response) {
    if (is_urc_pattern(line)) {
        // Queue URC separately
        enqueue_urc(urc_queue, line, len);
    } else {
        // Add to command response
        append_response_buffer(response, line, len);
    }
}

// Detect URC pattern (starts with + but isn't expected response)
int is_urc_pattern(const char *line) {
    if (line[0] != '+') return 0;

    // List of expected response patterns
    const char *expected[] = {
        "+CGPADDR", "+CGACT", "+CGREG", "+CREG", "+CIND", "+CSQ", NULL
    };

    for (int i = 0; expected[i]; i++) {
        if (strncmp(line, expected[i], strlen(expected[i])) == 0) {
            return 0;  // It's expected, not a URC
        }
    }

    return 1;  // Likely URC
}

// Process URCs in separate task
void urc_handler_task(void *arg) {
    URCQueue *queue = (URCQueue *)arg;

    while (1) {
        // Wait for URC available
        xSemaphoreTake(queue->available_sem, portMAX_DELAY);

        // Dequeue and process
        if (queue->count > 0) {
            URC urc;
            dequeue_urc(queue, &urc);

            // Parse and handle specific URC
            if (strncmp(urc.content, "+CMT", 4) == 0) {
                handle_incoming_sms(&urc);
            } else if (strncmp(urc.content, "+CMTI", 5) == 0) {
                handle_sms_notification(&urc);
            } else if (strncmp(urc.content, "+CRING", 6) == 0) {
                handle_incoming_call(&urc);
            }
            // ... other URC types
        }
    }
}
```

---

## ESP32-IDF Implementation

### Project Structure

```
esp32-a7670g-driver/
├── CMakeLists.txt
├── README.md
├── main/
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild
│   └── main.c
├── components/
│   └── a7670g/
│       ├── CMakeLists.txt
│       ├── include/
│       │   ├── a7670g.h
│       │   ├── a7670g_network.h
│       │   ├── a7670g_socket.h
│       │   ├── a7670g_sms.h
│       │   ├── a7670g_gps.h
│       │   └── a7670g_ssl.h
│       └── src/
│           ├── a7670g.c
│           ├── a7670g_uart.c
│           ├── a7670g_parser.c
│           ├── a7670g_network.c
│           ├── a7670g_socket.c
│           ├── a7670g_sms.c
│           ├── a7670g_gps.c
│           └── a7670g_ssl.c
└── .gitignore
```

### Header Files

#### a7670g/include/a7670g.h

```c
#ifndef A7670G_H
#define A7670G_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/uart.h"

#define A7670G_UART_NUM           UART_NUM_1
#define A7670G_UART_BAUD          115200
#define A7670G_UART_TX_PIN        GPIO_NUM_17
#define A7670G_UART_RX_PIN        GPIO_NUM_16
#define A7670G_UART_BUFFER_SIZE   4096

#define A7670G_PWRKEY_PIN         GPIO_NUM_4
#define A7670G_RESET_PIN          GPIO_NUM_5

#define MAX_AT_RESPONSE            4096
#define MAX_SOCKET_ID             10
#define MAX_APN_LENGTH            64

// Error codes
typedef enum {
    A7670G_OK                  = 0,
    A7670G_ERR_INVALID_PARAM   = -1,
    A7670G_ERR_NOT_INITIALIZED = -2,
    A7670G_ERR_TIMEOUT         = -3,
    A7670G_ERR_NO_MEMORY       = -4,
    A7670G_ERR_UART            = -5,
    A7670G_ERR_AT_COMMAND      = -6,
    A7670G_ERR_NETWORK         = -7,
    A7670G_ERR_SOCKET          = -8,
    A7670G_ERR_UNKNOWN         = -99,
} a7670g_error_t;

// Connection states
typedef enum {
    A7670G_STATE_UNINITIALIZED,
    A7670G_STATE_IDLE,
    A7670G_STATE_POWERED_ON,
    A7670G_STATE_SIM_READY,
    A7670G_STATE_NETWORK_SEARCHING,
    A7670G_STATE_NETWORK_REGISTERED,
    A7670G_STATE_PDP_ACTIVE,
    A7670G_STATE_ERROR,
} a7670g_state_t;

// Module instance
typedef struct {
    a7670g_state_t state;
    SemaphoreHandle_t state_lock;
    uart_event_queue_t *uart_queue;

    // Network info
    char imei[20];
    char imsi[20];
    char operator[32];
    int signal_quality;

    // PDP context
    char apn[MAX_APN_LENGTH];
    char ip_address[16];

    // Socket management
    SemaphoreHandle_t socket_locks[MAX_SOCKET_ID];

    // Response handling
    QueueHandle_t response_queue;
} a7670g_t;

// Initialization
a7670g_error_t a7670g_init(a7670g_t *module);
a7670g_error_t a7670g_deinit(a7670g_t *module);

// Power management
a7670g_error_t a7670g_power_on(a7670g_t *module);
a7670g_error_t a7670g_power_off(a7670g_t *module);

// State management
a7670g_state_t a7670g_get_state(a7670g_t *module);
a7670g_error_t a7670g_wait_for_state(a7670g_t *module,
                                     a7670g_state_t state,
                                     uint32_t timeout_ms);

#endif // A7670G_H
```

#### a7670g/include/a7670g_network.h

```c
#ifndef A7670G_NETWORK_H
#define A7670G_NETWORK_H

#include "a7670g.h"

// Network registration status
typedef enum {
    NET_NOT_REGISTERED = 0,
    NET_REGISTERED_HOME = 1,
    NET_SEARCHING = 2,
    NET_DENIED = 3,
    NET_REGISTERED_ROAMING = 5,
} a7670g_net_status_t;

// Network registration info
typedef struct {
    a7670g_net_status_t status;
    int signal_rssi;  // 0-31, 99=unknown
    int signal_ber;   // 0-7, 99=unknown
} a7670g_net_info_t;

// PDP context states
typedef enum {
    PDP_DEACTIVATED = 0,
    PDP_ACTIVATED = 1,
} a7670g_pdp_state_t;

// Network functions
a7670g_error_t a7670g_net_init(a7670g_t *module);
a7670g_error_t a7670g_net_get_status(a7670g_t *module, a7670g_net_info_t *info);
a7670g_error_t a7670g_net_attach(a7670g_t *module);
a7670g_error_t a7670g_net_detach(a7670g_t *module);

// PDP context
a7670g_error_t a7670g_pdp_define(a7670g_t *module, int cid, const char *apn);
a7670g_error_t a7670g_pdp_activate(a7670g_t *module, int cid);
a7670g_error_t a7670g_pdp_deactivate(a7670g_t *module, int cid);
a7670g_error_t a7670g_pdp_get_ip(a7670g_t *module, int cid, char *ip);

#endif // A7670G_NETWORK_H
```

### Implementation Examples

#### a7670g/src/a7670g_uart.c

```c
#include "a7670g.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "A7670G_UART";

#define UART_BUFFER_SIZE 1024

// UART data structure
typedef struct {
    uint8_t rx_buffer[A7670G_UART_BUFFER_SIZE];
    volatile size_t write_pos;
    volatile size_t read_pos;
    volatile size_t count;
} uart_circular_buffer_t;

static uart_circular_buffer_t uart_buf = {0};
static QueueHandle_t uart_queue = NULL;

// UART interrupt handler
static void uart_event_task(void *pvParameters) {
    uart_event_t event;
    uint8_t dtmp[512];

    while (1) {
        if (xQueueReceive(uart_queue, (void *)&event, portMAX_DELAY)) {
            switch (event.type) {
            case UART_DATA:
                if (event.size > 0) {
                    // Read data from UART into circular buffer
                    size_t len = uart_read_bytes(A7670G_UART_NUM, dtmp,
                                                 event.size, 0);
                    for (size_t i = 0; i < len; i++) {
                        // Bounded write to circular buffer
                        if (uart_buf.count < A7670G_UART_BUFFER_SIZE) {
                            uart_buf.rx_buffer[uart_buf.write_pos] = dtmp[i];
                            uart_buf.write_pos =
                                (uart_buf.write_pos + 1) % A7670G_UART_BUFFER_SIZE;
                            uart_buf.count++;
                        }
                    }
                }
                break;

            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "UART FIFO overflow");
                uart_flush_input(A7670G_UART_NUM);
                break;

            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "UART ring buffer full");
                uart_flush_input(A7670G_UART_NUM);
                break;

            default:
                break;
            }
        }
    }
}

// Initialize UART
a7670g_error_t uart_init(void) {
    uart_config_t uart_config = {
        .baud_rate = A7670G_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 120,
    };

    // Configure UART
    ESP_RETURN_ON_ERROR(uart_param_config(A7670G_UART_NUM, &uart_config),
                        TAG, "Failed to configure UART");

    // Set pins
    ESP_RETURN_ON_ERROR(uart_set_pin(A7670G_UART_NUM,
                                     A7670G_UART_TX_PIN,
                                     A7670G_UART_RX_PIN,
                                     UART_PIN_NO_CHANGE,
                                     UART_PIN_NO_CHANGE),
                        TAG, "Failed to set UART pins");

    // Install driver
    ESP_RETURN_ON_ERROR(uart_driver_install(A7670G_UART_NUM,
                                            A7670G_UART_BUFFER_SIZE,
                                            A7670G_UART_BUFFER_SIZE,
                                            20,
                                            &uart_queue,
                                            0),
                        TAG, "Failed to install UART driver");

    // Create UART event task
    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 10, NULL);

    return A7670G_OK;
}

// Send AT command (with bounds checking)
a7670g_error_t uart_send_at_command(const char *cmd) {
    if (!cmd || strlen(cmd) > 1024) {
        return A7670G_ERR_INVALID_PARAM;
    }

    // Send command
    int len = uart_write_bytes(A7670G_UART_NUM, (const uint8_t *)cmd,
                               strlen(cmd));

    // Send CR+LF terminator
    uart_write_bytes(A7670G_UART_NUM, (const uint8_t *)"\r\n", 2);

    if (len < 0) {
        return A7670G_ERR_UART;
    }

    return A7670G_OK;
}

// Read from UART buffer (safe, bounded)
size_t uart_read_buffer(uint8_t *data, size_t max_len, uint32_t timeout_ms) {
    if (!data || max_len == 0) {
        return 0;
    }

    // Wait for data with timeout
    TickType_t ticks = timeout_ms / portTICK_PERIOD_MS;
    uint32_t start_time = xTaskGetTickCount();

    size_t total_read = 0;

    while (total_read < max_len) {
        // Check timeout
        uint32_t elapsed = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;
        if (elapsed > timeout_ms) break;

        // Check if data available
        if (uart_buf.count > 0) {
            // Safe read from circular buffer
            uint8_t ch = uart_buf.rx_buffer[uart_buf.read_pos];
            uart_buf.read_pos = (uart_buf.read_pos + 1) % A7670G_UART_BUFFER_SIZE;
            uart_buf.count--;

            data[total_read++] = ch;
        } else {
            // Small delay to avoid busy waiting
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    return total_read;
}

// Deinitialize UART
void uart_deinit(void) {
    uart_driver_delete(A7670G_UART_NUM);
}
```

#### a7670g/src/a7670g_parser.c

```c
#include "a7670g.h"
#include "esp_log.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "A7670G_PARSER";

// Safe string parsing with length limits
int parse_at_response_line(const char *line, size_t max_len) {
    if (!line || max_len == 0 || max_len > A7670G_UART_BUFFER_SIZE) {
        return -1;
    }

    size_t len = strnlen(line, max_len);

    // Parse different response types
    if (strncmp(line, "OK", 2) == 0 && len == 2) {
        return RESPONSE_OK;
    } else if (strncmp(line, "ERROR", 5) == 0 && len == 5) {
        return RESPONSE_ERROR;
    } else if (line[0] == '+' && len > 1) {
        // Structured response: +CMD:param1,param2,...
        return RESPONSE_STRUCTURED;
    }

    return RESPONSE_UNKNOWN;
}

// Safe parsing of +CGPADDR response
int parse_cgpaddr_response(const char *line, char *ip_addr, size_t ip_len) {
    if (!line || !ip_addr || ip_len < 16) {
        return -1;
    }

    // Expected format: +CGPADDR: <cid>,"<ip>"
    const char *prefix = "+CGPADDR: ";
    if (strncmp(line, prefix, strlen(prefix)) != 0) {
        return -1;
    }

    // Find quoted IP address
    const char *ip_start = strchr(line, '"');
    if (!ip_start) return -1;
    ip_start++;

    const char *ip_end = strchr(ip_start, '"');
    if (!ip_end) return -1;

    size_t ip_str_len = ip_end - ip_start;
    if (ip_str_len >= ip_len) {
        return -2;  // Buffer too small
    }

    // Safe copy
    strncpy(ip_addr, ip_start, ip_str_len);
    ip_addr[ip_str_len] = '\0';

    return 0;
}

// Safe parsing of +CSQ response (signal quality)
int parse_csq_response(const char *line, int *rssi, int *ber) {
    if (!line || !rssi || !ber) {
        return -1;
    }

    // Expected format: +CSQ: <rssi>,<ber>
    const char *prefix = "+CSQ: ";
    if (strncmp(line, prefix, strlen(prefix)) != 0) {
        return -1;
    }

    int parsed = sscanf(line, "+CSQ: %d,%d", rssi, ber);
    if (parsed != 2) {
        return -2;
    }

    // Validate ranges
    if (*rssi < 0 || *rssi > 31) return -3;
    if (*ber < 0 || *ber > 7) return -4;

    return 0;
}

// Accumulate response lines safely
typedef struct {
    char lines[10][256];
    int count;
    int max_lines;
} ResponseLines;

int append_response_line(ResponseLines *resp, const char *line) {
    if (!resp || !line) {
        return -1;
    }

    if (resp->count >= resp->max_lines) {
        return -2;  // Too many lines
    }

    size_t len = strlen(line);
    if (len >= 256) {
        return -3;  // Line too long
    }

    strcpy(resp->lines[resp->count], line);
    resp->count++;

    return 0;
}
```

#### a7670g/src/a7670g_socket.c

```c
#include "a7670g.h"
#include "esp_log.h"

static const char *TAG = "A7670G_SOCKET";

typedef struct {
    int socket_id;
    uint8_t rx_buffer[4096];
    size_t rx_write_idx;
    size_t rx_read_idx;
    size_t rx_count;
    SemaphoreHandle_t rx_lock;
    bool connected;
} socket_context_t;

static socket_context_t sockets[MAX_SOCKET_ID] = {0};

// Thread-safe socket write
a7670g_error_t socket_send_safe(int socket_id, const uint8_t *data,
                                size_t len) {
    if (socket_id < 0 || socket_id >= MAX_SOCKET_ID) {
        return A7670G_ERR_INVALID_PARAM;
    }

    socket_context_t *sock = &sockets[socket_id];

    // Lock socket state
    xSemaphoreTake(sock->rx_lock, portMAX_DELAY);

    // Check connection state
    if (!sock->connected) {
        xSemaphoreGive(sock->rx_lock);
        return A7670G_ERR_SOCKET;
    }

    // Build AT command
    char cmd[256];
    int ret = snprintf(cmd, sizeof(cmd), "AT+CASEND=%d,%zu",
                       socket_id, len);
    if (ret < 0 || ret >= sizeof(cmd)) {
        xSemaphoreGive(sock->rx_lock);
        return A7670G_ERR_INVALID_PARAM;
    }

    // Send command
    a7670g_error_t err = uart_send_at_command(cmd);

    xSemaphoreGive(sock->rx_lock);
    return err;
}

// Bounded socket receive buffer
a7670g_error_t socket_receive_safe(int socket_id, uint8_t *data,
                                   size_t max_len, size_t *bytes_read) {
    if (socket_id < 0 || socket_id >= MAX_SOCKET_ID) {
        return A7670G_ERR_INVALID_PARAM;
    }

    if (!data || max_len == 0 || !bytes_read) {
        return A7670G_ERR_INVALID_PARAM;
    }

    socket_context_t *sock = &sockets[socket_id];

    xSemaphoreTake(sock->rx_lock, portMAX_DELAY);

    // Calculate available data
    size_t available = sock->rx_count;
    *bytes_read = (max_len < available) ? max_len : available;

    // Safe copy from circular buffer
    for (size_t i = 0; i < *bytes_read; i++) {
        data[i] = sock->rx_buffer[sock->rx_read_idx];
        sock->rx_read_idx = (sock->rx_read_idx + 1) % sizeof(sock->rx_buffer);
    }

    sock->rx_count -= *bytes_read;

    xSemaphoreGive(sock->rx_lock);
    return A7670G_OK;
}

// Close socket with state verification
a7670g_error_t socket_close_safe(int socket_id) {
    if (socket_id < 0 || socket_id >= MAX_SOCKET_ID) {
        return A7670G_ERR_INVALID_PARAM;
    }

    socket_context_t *sock = &sockets[socket_id];

    xSemaphoreTake(sock->rx_lock, portMAX_DELAY);

    // Mark as disconnected before closing
    sock->connected = false;

    // Clear receive buffer
    sock->rx_count = 0;
    sock->rx_read_idx = 0;
    sock->rx_write_idx = 0;

    xSemaphoreGive(sock->rx_lock);

    // Send close command
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CACLOSE=%d", socket_id);

    return uart_send_at_command(cmd);
}
```

### Building and Flashing

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.16)
project(esp32-a7670g-driver)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project_info(EXAMPLE "A7670G LTE CAT1 Driver for ESP32")

project(esp32-a7670g-driver)
```

**Build and Flash**:
```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash
idf.py -p /dev/ttyUSB0 monitor
```

---

## References & Documentation

### Official Documentation
- SIMcom A7670 Series Hardware Design V1.03
- A76XX Series AT Command Manual V1.06-4
- A76XX Series SSL Application Note V1.02
- A76XX Series HTTP(S) Application Note V1.03
- A76XX Series Sleep Mode Application Note V1.02
- A76XX Series Low Power Mode Application Note V1.01

### Key Resources
- [SIMcom Official Product Page](https://www.simcom.com/product/A7670X.html)
- [LilyGo Modem Series GitHub](https://github.com/Xinyuan-LilyGO/LilyGO-T-A76XX)
- [TinyGSM Library (A7670G Fork)](https://github.com/Xinyuan-LilyGO/TinyGSM)
- [ESP-IDF UART Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/uart.html)
- [ESP-IDF Drivers Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/index.html)

### Security Resources
- SEC Consult SIMCom Modem Security Vulnerabilities
- BlackHat USA 2019: "All The 4G Modules Could Be Hacked"
- OWASP: Mobile Security Testing Guide (AT Command Testing)

---

## Notes for Developers

1. **Always use level shifters** between ESP32 (3.3V) and A7670G (1.8V)
2. **Test with proper power supply** - underpowering causes intermittent issues
3. **Keep firmware updated** - SIMcom regularly releases updates for stability/security
4. **Buffer sizes matter** - larger buffers help with network variability
5. **Error recovery is critical** - modems can enter bad states; have recovery paths
6. **Log everything** during development - AT command sequencing is subtle
7. **Use timeouts consistently** - network operations are inherently asynchronous
8. **Handle URCs properly** - they can appear anytime, not just responses
9. **Test edge cases** - low signal, network changes, power loss, etc.
10. **Security review** - especially if handling user input in AT commands

