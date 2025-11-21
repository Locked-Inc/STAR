# A7670G CAT1 4G LTE - Quick Reference Guide

## Pin Configuration

| Function | GPIO | Voltage | Notes |
|----------|------|---------|-------|
| TX | GPIO17 | 1.8V via LS | To ESP32 RX |
| RX | GPIO16 | 1.8V via LS | From ESP32 TX |
| PWRKEY | GPIO4 | 1.8V/0V | >1sec pulse to power on |
| RESET | GPIO5 | 1.8V/0V | Hard reset (optional) |
| VBAT | - | 3.6-4.2V | Main power, 500mA typical |
| GND | - | 0V | Common ground |

**LS = Level Shifter (3.3V → 1.8V)**

## UART Configuration

```
Baud: 115200 bps
Data: 8 bits
Stop: 1 bit
Parity: None
Flow Control: None
```

## Essential AT Commands

### Connection Test
```
AT                    → OK (test connection)
AT+GMM                → A7670G (get model)
AT+CPIN?              → +CPIN: READY (SIM ready)
```

### Network Registration
```
AT+CGREG?             → +CGREG: 0,1 (registered home)
AT+CREG?              → +CREG: 0,1 (voice registered)
AT+COPS?              → +COPS: 0,0,"46000" (operator)
AT+CSQ                → +CSQ: 20,99 (signal quality)
```

### PDP Context (Internet Connection)
```
AT+CGDCONT=1,"IP","3gnet"        (set APN)
AT+CGATT=1                        (attach network)
AT+CGACT=1,1                      (activate context)
AT+CGPADDR=1                      → +CGPADDR: 1,"10.1.1.1"
AT+CGACT=0,1                      (deactivate context)
```

### TCP Socket
```
AT+CAOPEN=1,0,"TCP","example.com",80
AT+CASEND=1,5                     (send 5 bytes)
Hello                             (actual data)
AT+CARECV=1,1024                  (receive up to 1024 bytes)
AT+CACLOSE=1                      (close socket)
```

### HTTP GET
```
AT+HTTPINIT
AT+HTTPPARA="URL","https://api.example.com/data"
AT+HTTPACTION=0                   (0=GET, 1=POST)
AT+HTTPREAD
AT+HTTPTERM
```

### MQTT
```
AT+CMQTTSTART
AT+CMQTTACCQ=0,"ClientID",0
AT+CMQTTCONNECT=0,"tcp://broker.example.com",20,1
AT+CMQTTTOPIC=0,4
home
AT+CMQTTPAYLOAD=0,3
ABC
AT+CMQTTPUB=0,0,60
AT+CMQTTDISCONNECT=0
AT+CMQTTSTOP
```

### SMS
```
AT+CMGF=1                         (text mode)
AT+CMGS="5551234567"              (send SMS)
Hello World                       (message text)
[Ctrl+Z]                          (send)
AT+CMGL="ALL"                     (list all SMS)
AT+CMGR=0                         (read SMS index 0)
AT+CMGD=0                         (delete SMS index 0)
```

### GPS/GNSS
```
AT+CGNSSPWR=1                     (enable GPS)
AT+CGNSSTST=1                     (continuous output)
AT+CGPSINFO                       → GPS position data
```

### Power Management
```
AT+CFUN=1                         (full functionality)
AT+CFUN=0                         (minimum functionality)
AT+CPSMS=1,"01000010","00011010"  (enable PSM)
AT+CPOWD=0                        (software power off)
```

### SSL/TLS Configuration
```
AT+CSSLCFG="authmode",0,1         (server verification)
AT+CSSLCFG="sslversion",0,3       (TLS 1.2)
AT+CSSLCFG="sni",0,"example.com"  (SNI)
AT+CSSLCFG="ignorertctime",0,1    (ignore RTC)
```

## Common Responses

| Response | Meaning |
|----------|---------|
| OK | Command successful |
| ERROR | Command failed |
| +CME ERROR 100 | Unknown error |
| +CME ERROR 506 | Uninitialized profile |
| +CME ERROR 509 | RTC not set |
| +CMTI: "MT",1 | SMS received (index 1) |
| +CMT: ... | Direct SMS delivery |
| +CRING | Incoming call |
| +CREG: 0,1 | Registered, home network |
| +CREG: 0,5 | Registered, roaming |

## Network Registration States

| Value | Meaning |
|-------|---------|
| 0 | Not registered, not searching |
| 1 | Registered, home network |
| 2 | Not registered, searching |
| 3 | Registration denied |
| 5 | Registered, roaming |

## Power Modes

| Mode | Current | Wake Time | Use Case |
|------|---------|-----------|----------|
| Active | 600-800mA | - | Data transmission |
| Sleep | <10mA | <1sec | Idle with active network |
| PSM | <1mA | 30-60min | Battery-powered devices |
| DRX | 5-50mA | 1-40min | Periodic updates |
| eDRX | <5mA | 10min-40min | Extended idle |

## Baud Rate Test Sequence

```
1. Start at 115200 (default)
2. If no response, try 9600
3. If still no response, try 19200
4. Send: AT[CR][LF]
5. Should receive: [CR][LF]OK[CR][LF]
```

## Hardware Troubleshooting

| Issue | Solution |
|-------|----------|
| No response to AT | Check UART pins, baud rate, PWRKEY pulse |
| Intermittent connection | Improve power supply (500mA+), check antenna |
| SMS not received | Enable notifications: AT+CNMI=2,1,0,1 |
| GPS not fixing | Wait 30-60sec cold start, check antenna |
| SSL errors | Set time: AT+CCLK="24/01/15,10:30:00+00" |
| High current consumption | Check sleep mode, disable GPS if unused |

## ESP32-IDF UART Setup

```c
// Configure UART
uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
};

uart_param_config(UART_NUM_1, &uart_config);
uart_set_pin(UART_NUM_1, GPIO_NUM_17, GPIO_NUM_16,
             UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
uart_driver_install(UART_NUM_1, 4096, 4096, 20, &uart_queue, 0);

// Send AT command
uart_write_bytes(UART_NUM_1, "AT\r\n", 4);

// Read response (with timeout)
uint8_t data[256];
int len = uart_read_bytes(UART_NUM_1, data, sizeof(data), 100 / portTICK_PERIOD_MS);
```

## Initialization Sequence

```
1. Power on (PWRKEY pulse 1+ seconds)
2. Wait 1-2 seconds for module to start
3. Send: AT (connection test)
4. Send: AT+CPIN? (check SIM)
5. Wait for network: AT+CGREG? (repeat until 1 or 5)
6. Send: AT+CGDCONT=1,"IP","apn.name"
7. Send: AT+CGATT=1 (attach network)
8. Send: AT+CGACT=1,1 (activate context)
9. Send: AT+CGPADDR=1 (get IP address)
10. Ready for TCP/UDP/HTTP/MQTT

Complete in ~10-30 seconds typically
```

## APN Configurations by Region

| Provider | Country | APN |
|----------|---------|-----|
| China Mobile | China | cmnet |
| China Unicom | China | 3gnet |
| China Telecom | China | ctnet |
| T-Mobile | USA | fast.t-mobile.com |
| Verizon | USA | vzwinternet |
| AT&T | USA | broadband |
| Vodafone | Europe | www.vodafone.com |
| Orange | Europe | orange |
| Deutsche Telekom | Germany | internet.t-mobile |
| Swisscom | Switzerland | gprs.swisscom.com |

## Performance Metrics

| Operation | Time |
|-----------|------|
| Cold start GPS | 30-180sec |
| Warm start GPS | 15-30sec |
| Hot start GPS | 5-10sec |
| Network registration | 5-30sec |
| PDP activation | 3-10sec |
| TCP connection | 5-30sec (network dependent) |
| SSL/TLS handshake | 10-30sec |
| SMS send | 5-10sec |
| Power on | ~1.2sec |
| Power off | ~3.0sec |

## Memory Allocation Recommendations

```c
// UART buffer (circular)
#define UART_BUFFER_SIZE 4096

// Response buffer
#define MAX_RESPONSE_LINE 1024
#define MAX_RESPONSE_LINES 10

// Socket buffers
#define MAX_SOCKET_RX_BUFFER 4096
#define NUM_SOCKETS 10

// SMS storage
#define MAX_SMS_CONTENT 160
#define MAX_PHONE_NUMBER 20

// Credential storage (encrypted)
#define MAX_APN_LENGTH 64
#define MAX_USERNAME_LENGTH 64
#define MAX_PASSWORD_LENGTH 64

// Total: ~50KB minimum recommended
```

## Security Checklist

- [ ] Use level shifter for voltage (3.3V → 1.8V)
- [ ] Validate all user input (phone, URL, APN)
- [ ] Sanitize SMS content
- [ ] Use bounded buffers
- [ ] Implement timeouts on all operations
- [ ] Enable TLS for MQTT/HTTPS
- [ ] Store credentials encrypted
- [ ] Check command responses for errors
- [ ] Implement proper URC handling
- [ ] Test error recovery paths
- [ ] Monitor connection state
- [ ] Log AT commands for debugging
- [ ] Disable debug features in production
- [ ] Update firmware periodically

## Common Issues & Solutions

### Issue: "No module response"
```
→ Check PWRKEY pulse (>1 second)
→ Check power supply (3.8V, 500mA)
→ Verify UART pins (TX/RX swapped?)
→ Check baud rate (try 9600)
```

### Issue: "Network not registered"
```
→ Check SIM card has data plan
→ Try AT+COPS=0 (auto operator)
→ Check AT+CSQ (signal quality)
→ Wait longer (cold start: 30sec+)
```

### Issue: "PDP activation fails"
```
→ Verify APN correct for carrier
→ Check network registration first
→ Try: AT+CGACT=0,1 (deactivate, retry)
→ Check error code: AT+CSDH=1; AT+CMEE=2
```

### Issue: "SSL/TLS fails"
```
→ Set time: AT+CCLK="24/01/15,14:30:00+00"
→ Or: AT+CSSLCFG="ignorertctime",0,1
→ Verify domain: AT+CSSLCFG="sni",0,"domain.com"
→ Check certificate: AT+CCERTLIST
```

### Issue: "GPS not getting fix"
```
→ Wait 30+ seconds for cold start
→ Check GPS antenna connection
→ Verify AT+CGNSSPWR=1
→ Move to outdoor location
→ Check AT+CGPSINFO for "$GPGGA" format
```

### Issue: "High current consumption"
```
→ Check if GPS is always on
→ Disable PSM if not needed
→ Check for data retransmissions
→ Verify proper power mode: AT+CFUN?
→ Reduce HTTP polling frequency
```

---

## Resources

- **Official Documentation**: https://www.simcom.com/product/A7670X.html
- **GitHub Reference Implementation**: https://github.com/Xinyuan-LilyGO/LilyGO-T-A76XX
- **TinyGSM Library**: https://github.com/vshymanskyy/TinyGSM
- **ESP-IDF Docs**: https://docs.espressif.com/projects/esp-idf

---

**Version**: 1.0 | **Last Updated**: 2024-01-15 | **Author**: Technical Research

