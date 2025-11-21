# A7670G Security Vulnerabilities & Implementation Best Practices

## Executive Summary

The A7670G is a production-grade CAT1 4G LTE modem widely used in IoT applications. While generally reliable, implementations must account for:

1. **AT Command Injection Risks** - Unsanitized user input can lead to command execution
2. **Response Buffer Overflows** - Malformed modem responses can overflow fixed buffers
3. **Memory Safety Issues** - Circular buffers, socket buffers, and parsers need careful management
4. **State Machine Vulnerabilities** - URCs can corrupt parser state
5. **Credential Exposure** - Improperly stored credentials compromise security
6. **TOCTOU Race Conditions** - Connection state changes between check and use

This document provides mitigation strategies and secure implementation patterns.

---

## Known Security Issues

### CVE-2025-26412: Undocumented Root Shell Access on SIMCom Modems

**Affected Devices**: SIM7600G and related SIMcom modules
**Severity**: CRITICAL
**Vector**: Requires either physical access or remote shell access to device with direct modem connection

**Description**:
SIMcom modems support an undocumented AT command that allows execution of system commands with root privilege on the modem. An attacker with physical access to the UART interface or with access to a device that communicates directly with the modem can execute arbitrary commands.

**Mitigation**:
1. Disable UART debug port in production
2. Use firmware level protection if available
3. Monitor for unauthorized AT command patterns
4. Implement command whitelisting strictly

**Status**: SIMcom did not respond to disclosure attempts; patch status unknown.

---

## Vulnerability Classes & Mitigation

### 1. AT Command Injection Attacks

#### Risk Level: HIGH

An attacker who can control data sent to the modem can inject AT commands to:
- Modify network settings
- Establish unauthorized connections
- Intercept SMS messages
- Exfiltrate IMEI/IMSI
- Disconnect existing connections

#### Attack Vectors

**SMS-Based Injection**:
```
Attacker sends SMS: "Device password\r\nAT+COPS=0,1,2\r\nOK"
If parsed unsafely:
  Device processes: "password" (expected)
  Modem executes: AT+COPS=0,1,2 (operator change - malicious!)
```

**HTTP Response Injection**:
```
Server response: 200 OK
Set-APN: evil.com\r\nAT+CGDCONT=1,"IP","evil.com"
If used to configure: Subsequent traffic routed through attacker's APN
```

**User Input Injection**:
```
User provides APN: "3gnet"; AT+CMGF=0"
If not sanitized, command becomes:
AT+CGDCONT=1,"IP","3gnet"; AT+CMGF=0"  <- PDU mode enabled unexpectedly
```

#### Mitigation Strategies

**1. Input Validation & Sanitization**:

```c
#include <ctype.h>
#include <string.h>

// Validate APN format strictly
int validate_apn(const char *apn) {
    if (!apn || strlen(apn) > 63) return 0;

    // Only allow: alphanumeric, dot, hyphen, underscore
    for (const char *p = apn; *p; p++) {
        if (!isalnum(*p) && *p != '.' && *p != '-' && *p != '_') {
            return 0;
        }
    }
    return 1;
}

// Validate phone number format
int validate_phone_number(const char *number) {
    if (!number || strlen(number) > 15 || strlen(number) < 7) return 0;

    // First char may be +, rest must be digits
    int i = 0;
    if (number[0] == '+') i = 1;

    for (; number[i]; i++) {
        if (!isdigit(number[i])) return 0;
    }
    return 1;
}

// Validate URL format
int validate_url(const char *url) {
    if (!url || strlen(url) > 512) return 0;

    // Block AT command injection patterns
    if (strchr(url, '\r') || strchr(url, '\n')) return 0;
    if (strstr(url, "AT+") != NULL) return 0;
    if (strstr(url, "AT ") != NULL) return 0;

    // Require http:// or https://
    if (strncmp(url, "http://", 7) != 0 &&
        strncmp(url, "https://", 8) != 0) {
        return 0;
    }

    return 1;
}

// Sanitize SMS content (remove CR/LF)
void sanitize_sms_content(char *content, size_t max_len) {
    if (!content) return;

    for (size_t i = 0; i < max_len && content[i]; i++) {
        if (content[i] == '\r' || content[i] == '\n') {
            content[i] = ' ';  // Replace with space or truncate
        }
    }
}
```

**2. Command Whitelisting**:

```c
typedef enum {
    CMD_GET_SIGNAL = 0,
    CMD_GET_REGISTRATION,
    CMD_GET_IP,
    CMD_SEND_SMS,
    CMD_READ_SMS,
    CMD_SOCKET_CONNECT,
    CMD_SOCKET_SEND,
    CMD_SOCKET_RECV,
    // ... other safe commands
    CMD_INVALID = -1,
} allowed_command_t;

// Only execute pre-approved commands
a7670g_error_t execute_safe_command(allowed_command_t cmd, const char *param) {
    switch (cmd) {
    case CMD_GET_SIGNAL:
        return execute_get_signal();
    case CMD_SEND_SMS:
        if (validate_phone_number(param)) {
            return execute_send_sms(param);
        }
        return A7670G_ERR_INVALID_PARAM;
    // ... other cases
    default:
        return A7670G_ERR_INVALID_PARAM;
    }
}

// Never do this:
// uart_send_at_command(user_provided_string);  // DANGEROUS!
```

**3. Parameter Encoding**:

```c
// Escape special characters in parameters
void encode_at_parameter(const char *input, char *output, size_t output_size) {
    if (!input || !output || output_size < strlen(input) + 1) {
        return;
    }

    size_t out_idx = 0;
    for (size_t i = 0; input[i] && out_idx < output_size - 1; i++) {
        char ch = input[i];

        // Escape quotes
        if (ch == '"') {
            if (out_idx + 2 < output_size) {
                output[out_idx++] = '\\';
                output[out_idx++] = '"';
            }
        } else if (ch == '\\') {
            if (out_idx + 2 < output_size) {
                output[out_idx++] = '\\';
                output[out_idx++] = '\\';
            }
        } else if (ch == '\r' || ch == '\n') {
            // Reject these characters entirely
            return;
        } else {
            output[out_idx++] = ch;
        }
    }

    output[out_idx] = '\0';
}

// Use encoded parameter in command
char safe_apn[256];
encode_at_parameter(user_apn, safe_apn, sizeof(safe_apn));

char cmd[512];
snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", safe_apn);
uart_send_at_command(cmd);
```

---

### 2. Response Buffer Overflow Vulnerabilities

#### Risk Level: HIGH

The modem can send unexpectedly large or malformed responses that overflow fixed-size buffers.

#### Attack Vectors

**Oversized Response**:
```
// VULNERABLE CODE:
char response[256];
int i = 0;
while ((ch = read_uart()) != '\n') {
    response[i++] = ch;  // No bounds checking!
    // If response > 256 bytes, overflow occurs
}
```

**Malformed Response**:
```
// Modem sends response without terminator
// Parser waits indefinitely or until buffer full
// Meanwhile, data continues arriving -> overflow
```

**Multiple Lines**:
```
+CGPADDR: 1,"10.0.0.1"
+CMT: "Sender","timestamp"
"SMS content that is very long and causes buffer overflow..."
```

#### Mitigation Strategies

**1. Bounded Buffer with Overflow Detection**:

```c
#define MAX_RESPONSE_LINE 1024
#define MAX_RESPONSE_LINES 10

typedef struct {
    char lines[MAX_RESPONSE_LINES][MAX_RESPONSE_LINE];
    int line_count;
    int current_line_idx;
    size_t current_line_len;
    bool overflow_detected;
} ResponseBuffer;

int append_response_char(ResponseBuffer *buf, char ch) {
    // Validate state
    if (!buf || buf->overflow_detected) {
        return -1;
    }

    // Check line bounds
    if (buf->current_line_idx >= MAX_RESPONSE_LINES) {
        buf->overflow_detected = true;
        return -1;
    }

    // Check current line length
    if (buf->current_line_len >= MAX_RESPONSE_LINE - 1) {
        // Line too long
        if (ch == '\n') {
            // Force new line
            buf->lines[buf->current_line_idx][buf->current_line_len] = '\0';
            buf->current_line_idx++;
            buf->current_line_len = 0;
            return 0;
        } else {
            // Line still growing beyond limit - truncate
            return 0;  // Drop character silently
        }
    }

    // Safe append
    buf->lines[buf->current_line_idx][buf->current_line_len++] = ch;

    // Handle line termination
    if (ch == '\n') {
        buf->lines[buf->current_line_idx][buf->current_line_len - 1] = '\0';
        buf->current_line_idx++;
        buf->current_line_len = 0;
        buf->line_count = buf->current_line_idx;
    }

    return 0;
}

// Process bounded responses
int process_response(ResponseBuffer *buf) {
    if (buf->overflow_detected) {
        ESP_LOGE(TAG, "Response buffer overflow detected");
        return -1;
    }

    for (int i = 0; i < buf->line_count; i++) {
        // Process each line safely
        process_line(buf->lines[i]);
    }

    return 0;
}
```

**2. Timeout-Based Line Reading**:

```c
#define LINE_TIMEOUT_MS 5000

size_t read_at_response_line(char *line, size_t max_len, uint32_t timeout_ms) {
    if (!line || max_len == 0) {
        return 0;
    }

    size_t pos = 0;
    uint32_t start = get_time_ms();

    while (pos < max_len - 1) {
        // Check timeout
        if (get_time_ms() - start > timeout_ms) {
            ESP_LOGW(TAG, "Response line timeout after %d bytes", pos);
            break;
        }

        // Read single byte
        uint8_t ch;
        if (uart_read_char(&ch, 100) < 0) {
            continue;  // No data available
        }

        // Check for line terminator
        if (ch == '\n') {
            line[pos] = '\0';
            return pos;
        }

        // Accept character (strip CR)
        if (ch != '\r') {
            line[pos++] = ch;
        }
    }

    // Timeout or buffer full
    line[pos] = '\0';
    return pos;
}
```

**3. Length-Prefixed Parsing**:

```c
// For responses that include length, use it
// Example: +HTTPREAD: 200,"<length>"
// Then read exactly <length> bytes

int parse_length_prefixed_response(const char *line, size_t *data_len) {
    // Expected: +HTTPREAD: <status>,<length>,"<data>"
    int status;
    size_t len;

    int parsed = sscanf(line, "+HTTPREAD: %d,%zu", &status, &len);
    if (parsed != 2) {
        return -1;
    }

    if (len > MAX_RESPONSE_DATA) {
        return -2;  // Data too large
    }

    *data_len = len;
    return 0;
}

// Read exactly N bytes
size_t read_bounded_data(uint8_t *data, size_t expected_len) {
    size_t read_count = 0;

    while (read_count < expected_len) {
        uint8_t ch;
        if (uart_read_char(&ch, 100) < 0) {
            continue;
        }

        data[read_count++] = ch;
    }

    return read_count;
}
```

---

### 3. SMS Parsing Vulnerabilities

#### Risk Level: MEDIUM-HIGH

SMS messages are less trusted than AT responses but still need safe parsing.

#### Attack Vectors

**SMS Command Injection**:
```
Attacker sends SMS with content:
"Check balance\r\nAT+COPS=0,1,2\r\nOK"

If SMS parser doesn't validate, could inject AT commands
```

**SMS Buffer Overflow**:
```
SMS content: 160+ characters of padding + payload
If parser assumes max 160 bytes, overflow occurs
```

**URC Mixing**:
```
+CMT: "Sender","timestamp"
"Very long SMS message that never terminates..."
+CRING (incoming call URC arrives here)
"Rest of SMS"
```

#### Mitigation Strategies

**1. Strict SMS Structure Parsing**:

```c
#define MAX_SMS_LENGTH 160
#define MAX_PHONE_NUMBER_LENGTH 20

typedef struct {
    char sender[MAX_PHONE_NUMBER_LENGTH];
    char timestamp[20];
    uint8_t content[MAX_SMS_LENGTH];
    size_t content_len;
    bool valid;
} ParsedSMS;

// Parse SMS safely with bounds checking
int parse_incoming_sms(const char *raw, ParsedSMS *sms) {
    if (!raw || !sms) {
        return -1;
    }

    // Expected format: +CMT: "sender","timestamp"
    // message_content

    // Extract sender (first quoted string)
    const char *sender_start = strchr(raw, '"');
    if (!sender_start) return -1;
    sender_start++;

    const char *sender_end = strchr(sender_start, '"');
    if (!sender_end) return -1;

    size_t sender_len = sender_end - sender_start;
    if (sender_len >= MAX_PHONE_NUMBER_LENGTH) {
        return -2;  // Sender too long
    }

    strncpy(sms->sender, sender_start, sender_len);
    sms->sender[sender_len] = '\0';

    // Validate sender is phone number
    if (!validate_phone_number(sms->sender)) {
        return -3;
    }

    // Extract timestamp (second quoted string)
    const char *ts_start = strchr(sender_end + 1, '"');
    if (!ts_start) return -1;
    ts_start++;

    const char *ts_end = strchr(ts_start, '"');
    if (!ts_end) return -1;

    size_t ts_len = ts_end - ts_start;
    if (ts_len >= 20) {
        return -4;  // Timestamp too long
    }

    strncpy(sms->timestamp, ts_start, ts_len);
    sms->timestamp[ts_len] = '\0';

    // SMS content comes after: [CR][LF]content[CR][LF]
    const char *content_start = ts_end + 1;
    while (*content_start && (*content_start == '"' || *content_start == '\r' ||
           *content_start == '\n')) {
        content_start++;
    }

    // Content is remaining text (up to newline or end)
    size_t content_len = 0;
    const char *p = content_start;

    while (*p && *p != '\r' && *p != '\n' && content_len < MAX_SMS_LENGTH) {
        sms->content[content_len++] = *p++;
    }

    if (content_len >= MAX_SMS_LENGTH) {
        ESP_LOGW(TAG, "SMS content truncated (exceeded %d bytes)",
                 MAX_SMS_LENGTH);
    }

    sms->content_len = content_len;
    sms->valid = true;

    return 0;
}

// Disable SMS notifications if not needed
int disable_sms_notifications(a7670g_t *module) {
    // AT+CNMI=0,0,0,0 disables all unsolicited SMS notifications
    return uart_send_at_command("AT+CNMI=0,0,0,0");
}
```

**2. SMS Content Validation**:

```c
// Check SMS for suspicious patterns
int validate_sms_content(const uint8_t *content, size_t len) {
    if (!content || len == 0) return 0;

    // Block AT command patterns
    if (len > 2) {
        if ((content[0] == 'A' || content[0] == 'a') &&
            (content[1] == 'T' || content[1] == 't')) {
            // Looks like AT command
            return 0;
        }
    }

    // Block null bytes
    for (size_t i = 0; i < len; i++) {
        if (content[i] == '\0') {
            return 0;  // Null byte in middle of content
        }
    }

    // Block control characters (except space, tab)
    for (size_t i = 0; i < len; i++) {
        uint8_t ch = content[i];
        if (ch < 0x20 && ch != '\t') {
            return 0;  // Invalid control character
        }
    }

    return 1;  // Content looks valid
}
```

**3. SMS Processing Isolation**:

```c
// Process SMS in isolated context
typedef struct {
    ParsedSMS sms;
    a7670g_error_t error_code;
} SMSProcessResult;

SMSProcessResult process_sms_isolated(const char *raw) {
    SMSProcessResult result = {0};

    // Parse
    int parse_ret = parse_incoming_sms(raw, &result.sms);
    if (parse_ret < 0) {
        result.error_code = A7670G_ERR_INVALID_PARAM;
        return result;
    }

    // Validate
    if (!validate_sms_content(result.sms.content, result.sms.content_len)) {
        result.error_code = A7670G_ERR_INVALID_PARAM;
        return result;
    }

    // Process (never execute as commands)
    result.error_code = handle_sms_as_data(&result.sms);
    return result;
}

// Never do this:
// uart_send_at_command((const char *)sms.content);  // DANGEROUS!
```

---

### 4. Memory Safety Issues

#### Risk Level: MEDIUM-HIGH

C-based firmware has inherent memory safety risks without proper discipline.

#### Risk Areas

1. **Circular Buffers**: Head/tail pointer corruption
2. **Socket Buffers**: Race conditions in multi-threaded access
3. **Response Parsers**: Off-by-one errors, strcpy overflows
4. **State Machines**: Invalid state transitions
5. **Free Memory**: Use-after-free, double-free

#### Mitigation Strategies

**1. Safe Circular Buffer Implementation**:

```c
#define UART_BUFFER_SIZE 4096

typedef struct {
    uint8_t data[UART_BUFFER_SIZE];
    volatile size_t head;
    volatile size_t tail;
    volatile size_t count;
    portMUX_TYPE lock;
} CircularBuffer;

// Initialize buffer
void circular_buffer_init(CircularBuffer *buf) {
    if (buf) {
        buf->head = 0;
        buf->tail = 0;
        buf->count = 0;
        portMUX_INITIALIZE(&buf->lock);
    }
}

// Write byte (from ISR)
int circular_buffer_write(CircularBuffer *buf, uint8_t byte) {
    if (!buf) return -1;

    portENTER_CRITICAL_ISR(&buf->lock);

    // Check for overflow
    if (buf->count >= UART_BUFFER_SIZE) {
        // Buffer full - decide on policy:
        // Option 1: Drop oldest byte
        buf->tail = (buf->tail + 1) % UART_BUFFER_SIZE;
        // Option 2: Drop new byte (uncomment instead)
        // portEXIT_CRITICAL_ISR(&buf->lock);
        // return -1;
    } else {
        buf->count++;
    }

    // Write byte
    buf->data[buf->head] = byte;
    buf->head = (buf->head + 1) % UART_BUFFER_SIZE;

    portEXIT_CRITICAL_ISR(&buf->lock);
    return 0;
}

// Read bytes (from task)
size_t circular_buffer_read(CircularBuffer *buf, uint8_t *out,
                            size_t max_len) {
    if (!buf || !out || max_len == 0) return 0;

    portENTER_CRITICAL(&buf->lock);

    size_t to_read = (buf->count < max_len) ? buf->count : max_len;

    for (size_t i = 0; i < to_read; i++) {
        out[i] = buf->data[buf->tail];
        buf->tail = (buf->tail + 1) % UART_BUFFER_SIZE;
        buf->count--;
    }

    portEXIT_CRITICAL(&buf->lock);
    return to_read;
}
```

**2. Safe String Operations**:

```c
#include <string.h>

// Never use strcpy - use strncpy with bounds
void copy_string_safe(char *dest, size_t dest_size, const char *src) {
    if (!dest || dest_size == 0 || !src) {
        return;
    }

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';  // Guarantee null termination
}

// Safe string concatenation
int concat_string_safe(char *dest, size_t dest_size,
                       const char *src1, const char *src2) {
    if (!dest || dest_size == 0) return -1;

    size_t len1 = strlen(src1);
    size_t len2 = strlen(src2);

    if (len1 + len2 + 1 > dest_size) {
        return -2;  // Insufficient space
    }

    strcpy(dest, src1);
    strcat(dest, src2);
    return 0;
}

// Safe snprintf
int safe_format(char *buf, size_t size, const char *fmt, ...) {
    if (!buf || size == 0) return -1;

    va_list args;
    va_start(args, fmt);

    int ret = vsnprintf(buf, size, fmt, args);

    va_end(args);

    if (ret < 0 || (size_t)ret >= size) {
        // Truncation occurred
        return -2;
    }

    return ret;
}
```

**3. Safe Pointer Management**:

```c
// Use opaque handles instead of raw pointers
typedef int socket_handle_t;  // Instead of passing socket_t *

#define INVALID_SOCKET ((socket_handle_t)-1)

// Socket state management with validation
socket_handle_t socket_open(a7670g_t *module, const char *address,
                            uint16_t port) {
    if (!module || !address) {
        return INVALID_SOCKET;
    }

    // Validate module is in valid state
    a7670g_state_t state = a7670g_get_state(module);
    if (state != A7670G_STATE_PDP_ACTIVE) {
        return INVALID_SOCKET;
    }

    // Allocate and initialize socket
    // ... implementation ...

    return 0;  // Valid handle
}

// Validate handle before use
int socket_send(socket_handle_t handle, const uint8_t *data, size_t len) {
    if (handle < 0 || handle >= MAX_SOCKET_ID) {
        return -1;  // Invalid handle
    }

    if (!data || len == 0) {
        return -2;  // Invalid data
    }

    // ... implementation ...
}
```

**4. Reference Counting for Resources**:

```c
typedef struct {
    int ref_count;
    a7670g_state_t state;
    SemaphoreHandle_t lock;
} SocketResource;

int socket_add_ref(SocketResource *res) {
    if (!res) return -1;

    xSemaphoreTake(res->lock, portMAX_DELAY);
    res->ref_count++;
    int count = res->ref_count;
    xSemaphoreGive(res->lock);

    return count;
}

int socket_release_ref(SocketResource *res) {
    if (!res) return -1;

    xSemaphoreTake(res->lock, portMAX_DELAY);
    res->ref_count--;

    int count = res->ref_count;

    if (count == 0) {
        // Free resource
        cleanup_socket_resource(res);
    }

    xSemaphoreGive(res->lock);
    return count;
}
```

---

### 5. Credential Protection

#### Risk Level: CRITICAL

Credentials stored/transmitted insecurely lead to account compromise.

#### Vulnerable Patterns

**Hardcoded Credentials**:
```c
// DANGEROUS!
const char *APN = "3gnet";
const char *MQTT_USER = "iot_user";
const char *MQTT_PASS = "password123";
const char *API_KEY = "sk-abcdef123456";
```

**Plaintext Storage**:
```c
// DANGEROUS!
struct {
    char apn[64];
    char username[64];
    char password[64];
} credentials;  // Stored in NVS unencrypted
```

**Unencrypted Transmission**:
```c
// DANGEROUS!
AT+CMQTTCONNECT=0,"tcp://broker",20,1   // Plain TCP, no TLS
```

#### Mitigation Strategies

**1. Encrypted Credential Storage**:

```c
#include "nvs.h"
#include "nvs_flash.h"
#include "mbedtls/aes.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#define NVS_NAMESPACE "secure_creds"
#define CREDENTIAL_SALT "a7670g_default_salt_changeme"

typedef struct {
    uint8_t iv[16];
    uint8_t ciphertext[256];
    size_t ciphertext_len;
} EncryptedCredential;

// Derive key from device ID
int derive_key_from_device_id(uint8_t *key, size_t key_len) {
    // Get device ID (MAC address, etc)
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // Use KDF to derive key
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);

    // PBKDF2
    mbedtls_pkcs5_pbkdf2_hmac(&ctx, mac, sizeof(mac),
                              (const uint8_t *)CREDENTIAL_SALT,
                              strlen(CREDENTIAL_SALT),
                              10000,  // iterations
                              key_len, key);

    mbedtls_md_free(&ctx);
    return 0;
}

// Store credential encrypted
int store_credential_encrypted(const char *key_name, const char *value) {
    if (!key_name || !value) return -1;

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return -1;

    // Derive encryption key
    uint8_t key[32];
    derive_key_from_device_id(key, sizeof(key));

    // Generate random IV
    uint8_t iv[16];
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                          (const uint8_t *)key_name, strlen(key_name));

    mbedtls_ctr_drbg_random(&ctr_drbg, iv, sizeof(iv));

    // Encrypt value
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 256);

    uint8_t ciphertext[256];
    size_t cipher_len = 0;
    size_t value_len = strlen(value);

    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT,
                          (value_len + 15) & ~15,  // Round up to AES block size
                          iv, (uint8_t *)value, ciphertext);

    cipher_len = (value_len + 15) & ~15;

    // Store IV + ciphertext
    EncryptedCredential enc = {0};
    memcpy(enc.iv, iv, sizeof(iv));
    memcpy(enc.ciphertext, ciphertext, cipher_len);
    enc.ciphertext_len = cipher_len;

    ret = nvs_set_blob(handle, key_name, &enc, sizeof(enc));

    nvs_close(handle);
    mbedtls_aes_free(&aes);
    mbedtls_entropy_free(&entropy);

    return (ret == ESP_OK) ? 0 : -1;
}

// Retrieve credential decrypted
int retrieve_credential_decrypted(const char *key_name, char *value,
                                  size_t value_size) {
    if (!key_name || !value || value_size == 0) return -1;

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) return -1;

    // Read encrypted value
    EncryptedCredential enc;
    size_t enc_size = sizeof(enc);

    ret = nvs_get_blob(handle, key_name, &enc, &enc_size);
    if (ret != ESP_OK) {
        nvs_close(handle);
        return -1;
    }

    // Derive same key
    uint8_t key[32];
    derive_key_from_device_id(key, sizeof(key));

    // Decrypt
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, key, 256);

    uint8_t plaintext[256] = {0};
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT,
                          enc.ciphertext_len,
                          enc.iv, enc.ciphertext, plaintext);

    // Copy to output (trim padding)
    size_t plain_len = strlen((const char *)plaintext);
    if (plain_len >= value_size) {
        plain_len = value_size - 1;
    }

    memcpy(value, plaintext, plain_len);
    value[plain_len] = '\0';

    // Secure wipe
    memset(plaintext, 0, sizeof(plaintext));

    nvs_close(handle);
    mbedtls_aes_free(&aes);

    return 0;
}
```

**2. Use TLS for All Network Communication**:

```c
// Always use SSL:// for connections
AT+CMQTTCONNECT=0,"ssl://mqtt.broker",1883,...  // TLS
// NOT: "tcp://mqtt.broker"  // Plaintext!

// For HTTPS
AT+HTTPPARA="URL","https://api.example.com/data"
// NOT: "http://api.example.com/data"  // Plaintext!

// Verify SSL context is properly configured
AT+CSSLCFG="authmode",0,1                // Server verification
AT+CSSLCFG="sslversion",0,3              // TLS 1.2
AT+CSSLCFG="sni",0,"api.example.com"     // SNI for virtual hosts
```

**3. Certificate Pinning for Critical Connections**:

```c
// Store certificate hash or public key
typedef struct {
    uint8_t cert_hash[32];  // SHA256 hash of certificate
    bool verify_required;
} CertificatePin;

// Verify certificate during SSL negotiation
int verify_certificate_pin(const uint8_t *cert_data, size_t cert_len,
                          const CertificatePin *pin) {
    if (!cert_data || !pin) return -1;

    // Hash certificate
    mbedtls_sha256_context ctx;
    uint8_t hash[32];

    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, cert_data, cert_len);
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    // Compare to pinned hash
    if (memcmp(hash, pin->cert_hash, 32) != 0) {
        ESP_LOGE(TAG, "Certificate pin mismatch!");
        return -1;
    }

    return 0;
}
```

**4. Secure Credential Wipe**:

```c
// Clear sensitive data from memory
void secure_memzero(void *s, size_t n) {
    volatile uint8_t *v = (volatile uint8_t *)s;
    for (size_t i = 0; i < n; i++) {
        v[i] = 0;
    }
}

// After use, always clear credentials
void cleanup_credentials(void) {
    char credentials[256];

    // Get credentials from storage
    retrieve_credential_decrypted("mqtt_pass", credentials, sizeof(credentials));

    // Use credentials
    // ... AT commands ...

    // Secure wipe
    secure_memzero(credentials, sizeof(credentials));
}
```

---

### 6. TOCTOU (Time-of-Check-Time-of-Use) Vulnerabilities

#### Risk Level: MEDIUM

Race condition between checking socket state and using it.

#### Vulnerable Pattern

```c
// TOCTOU VULNERABILITY:
if (sockets[id].is_connected) {          // CHECK (time T1)
    // ... some delay here (interrupt, context switch) ...
    send_data(sockets[id], data);        // USE (time T2)
    // If socket disconnected between T1 and T2, crash/undefined behavior
}
```

#### Mitigation Strategies

**1. Atomic Check-and-Use with Locking**:

```c
// SAFE: Perform check and use while holding lock
a7670g_error_t send_data_safe(int socket_id, const uint8_t *data,
                              size_t len) {
    if (socket_id < 0 || socket_id >= MAX_SOCKET_ID) {
        return A7670G_ERR_INVALID_PARAM;
    }

    SocketContext *sock = &sockets[socket_id];

    // Lock socket state
    xSemaphoreTake(sock->state_lock, portMAX_DELAY);

    // Check state
    if (sock->state != SOCKET_CONNECTED) {
        xSemaphoreGive(sock->state_lock);
        return A7670G_ERR_SOCKET;
    }

    // Use socket while locked
    a7670g_error_t ret = send_data_impl(socket_id, data, len);

    // Update state if send failed
    if (ret != A7670G_OK) {
        sock->state = SOCKET_ERROR;
    }

    // Unlock
    xSemaphoreGive(sock->state_lock);

    return ret;
}
```

**2. Version-Based State Checking**:

```c
typedef struct {
    uint32_t version;  // Increment on state change
    socket_state_t state;
    SemaphoreHandle_t lock;
} SocketStateV;

// Snapshot state with version
typedef struct {
    uint32_t version_snapshot;
    socket_state_t state_snapshot;
} SocketSnapshot;

SocketSnapshot get_socket_state(int socket_id) {
    SocketSnapshot snap = {0};

    xSemaphoreTake(sockets[socket_id].lock, portMAX_DELAY);
    snap.version_snapshot = sockets[socket_id].version;
    snap.state_snapshot = sockets[socket_id].state;
    xSemaphoreGive(sockets[socket_id].lock);

    return snap;
}

// Later, verify state hasn't changed
int verify_state_unchanged(int socket_id, const SocketSnapshot *snap) {
    xSemaphoreTake(sockets[socket_id].lock, portMAX_DELAY);

    int unchanged = (sockets[socket_id].version == snap->version_snapshot);

    xSemaphoreGive(sockets[socket_id].lock);

    return unchanged;
}
```

**3. Compound Operations**:

```c
// Provide atomic combined operations
typedef struct {
    int socket_id;
    const uint8_t *data;
    size_t len;
    a7670g_error_t result;
} SendOperation;

// Enqueue operation to be executed atomically
a7670g_error_t enqueue_send_operation(const SendOperation *op) {
    if (!op || op->socket_id < 0) {
        return A7670G_ERR_INVALID_PARAM;
    }

    // Operation will be executed in dedicated task with proper locking
    xQueueSend(operation_queue, op, portMAX_DELAY);
    return A7670G_OK;
}

// Dedicated task executes operations atomically
void socket_operation_task(void *arg) {
    SendOperation op;

    while (1) {
        if (xQueueReceive(operation_queue, &op, portMAX_DELAY)) {
            SocketContext *sock = &sockets[op.socket_id];

            xSemaphoreTake(sock->state_lock, portMAX_DELAY);

            if (sock->state == SOCKET_CONNECTED) {
                op.result = send_data_impl(op.socket_id, op.data, op.len);

                if (op.result != A7670G_OK) {
                    sock->state = SOCKET_ERROR;
                }
            } else {
                op.result = A7670G_ERR_SOCKET;
            }

            xSemaphoreGive(sock->state_lock);

            // Signal completion
            xEventGroupSetBits(operation_complete_event, 1 << op.socket_id);
        }
    }
}
```

---

## Testing & Validation

### Fuzzing AT Command Parser

```c
// Test parser with random/malformed inputs
void fuzz_at_parser(void) {
    char test_input[256];

    for (int i = 0; i < 10000; i++) {
        // Generate random input
        generate_random_bytes((uint8_t *)test_input, sizeof(test_input));

        // Parse (should not crash)
        ResponseBuffer buf = {0};

        for (size_t j = 0; j < sizeof(test_input); j++) {
            int ret = append_response_char(&buf, test_input[j]);
            if (ret < 0) {
                // Expected - buffer full or invalid
                break;
            }
        }

        // Verify parser didn't corrupt memory
        verify_memory_integrity();
    }
}
```

### Security Test Cases

```
Test: AT Command Injection
├─ Input: "apn\r\nAT+COPS=0"
└─ Expected: Validation error, sanitized

Test: Buffer Overflow
├─ Input: 4KB of 'A' characters
└─ Expected: Graceful truncation/error

Test: SMS Parsing with Malformed Data
├─ Input: "+CMT: \"sender\",\"timestamp\n\n\n\nmalformed..."
└─ Expected: Parse error, no buffer overflow

Test: TOCTOU Race Condition
├─ Input: Close socket while sending
└─ Expected: Atomic operation, no crash

Test: Credential Extraction
├─ Attempt: Read NVS directly
└─ Expected: Encrypted, unreadable

Test: Invalid States
├─ Input: Send on disconnected socket
└─ Expected: Error return, no crash
```

---

## Compliance & Standards

### Best Practices Applied

- **CWE-78**: Command Injection - Input validation, whitelisting
- **CWE-120**: Buffer Copy without Checking Size - Bounded buffers
- **CWE-362**: Concurrent Execution - Atomic operations with locks
- **CWE-327**: Use of Broken Cryptography - TLS 1.2+, encrypted storage
- **CWE-522**: Insufficiently Protected Credentials - Encrypted NVS
- **CWE-829**: Inclusion of Functionality from Untrusted Control Sphere - Strict validation

### Security Checklist

```
[ ] Input validation for all user-provided data
[ ] Bounded buffers with overflow detection
[ ] Atomic operations for critical state changes
[ ] Encrypted credential storage
[ ] TLS for all network communication
[ ] Certificate validation/pinning
[ ] Secure memory wiping for sensitive data
[ ] Disable debug/test AT commands in production
[ ] Monitor for suspicious AT command patterns
[ ] Regular security updates from SIMcom
[ ] Penetration testing of AT command parser
[ ] Code review of parser and state machines
[ ] Fuzz testing of all input handlers
[ ] Rate limiting on failed operations
[ ] Logging of security events
```

---

## References

1. SEC Consult: "Undocumented Root Shell Access on SIMCom Modems"
2. BlackHat USA 2019: "All The 4G Modules Could Be Hacked"
3. OWASP: Mobile Security Testing Guide
4. CWE: Common Weakness Enumeration (MITRE)
5. ESP-IDF Security Best Practices
6. MbedTLS Documentation
7. FreeRTOS Thread Safety Guidelines
8. NIST Cybersecurity Framework

