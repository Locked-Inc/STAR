# IMX219-83 Stereo Camera - Security Hardening & Best Practices Guide

## Executive Summary

This document provides comprehensive security guidelines for implementing IMX219-83 dual-sensor drivers on ESP32-IDF. It addresses critical vulnerabilities found in camera driver implementations and provides practical mitigation strategies.

---

## 1. THREAT MODEL & ATTACK SURFACE

### 1.1 Attack Vectors

```
User Input Validation
├── Resolution Parameters       [HIGH RISK]
├── Exposure/Gain Values        [HIGH RISK]
├── I2C Register Writes         [HIGH RISK]
├── Buffer Sizes                [CRITICAL]
└── DMA Descriptor Construction [CRITICAL]

CSI-2 Interface
├── Malformed Packets           [MEDIUM RISK]
├── DMA Pointer Corruption      [HIGH RISK]
└── Buffer Overflow via CSI     [CRITICAL]

Memory Management
├── Stack Overflow              [HIGH RISK]
├── Heap Fragmentation          [MEDIUM RISK]
├── Integer Overflow            [CRITICAL]
└── Use-After-Free              [HIGH RISK]

I2C Control Channel
├── Unvalidated Register Writes [CRITICAL]
├── Address Spoofing            [MEDIUM RISK]
├── Bus Contention              [LOW RISK]
└── Information Leakage         [MEDIUM RISK]
```

### 1.2 Critical Vulnerabilities

**CVE-2013-4748** - Stack Buffer Overflow in Camera Driver
- Unvalidated user-supplied length values
- Fixed-size buffers on stack
- **Mitigation**: Always validate input sizes before copying

**CVE-2013-4739** - Memory Disclosure in Camera Driver
- Uninitialized kernel memory leaked to userspace
- **Mitigation**: Zero-initialize all buffers before use

**Hardware DMA Attacks**
- Malicious I/O devices can access unprotected memory
- **Mitigation**: Use IOMMU when available, limit DMA ranges

---

## 2. INPUT VALIDATION CHECKLIST

### 2.1 Resolution Parameter Validation

```c
/* BEFORE: Vulnerable Code */
void set_resolution(uint16_t width, uint16_t height) {
    buffer = malloc(width * height * 2);  // OVERFLOW!
    capture_image(buffer);
}

/* AFTER: Secure Implementation */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t valid_flag;  /* Magic value to detect tampering */
} validated_resolution_t;

esp_err_t validate_resolution(uint16_t width, uint16_t height) {
    /* 1. Check individual dimensions */
    if (width == 0 || width > MAX_IMAGE_WIDTH) {
        return ESP_ERR_INVALID_ARG;
    }
    if (height == 0 || height > MAX_IMAGE_HEIGHT) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 2. Check for integer overflow in multiplication */
    uint32_t total_pixels;
    if (__builtin_mul_overflow(width, height, &total_pixels)) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 3. Whitelist approach - only allow known-good modes */
    for (int i = 0; i < IMX219_RES_COUNT; i++) {
        if (g_imx219_modes[i].width == width &&
            g_imx219_modes[i].height == height) {
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}
```

### 2.2 Exposure & Gain Value Validation

```c
/* VULNERABILITY: Arbitrary exposure can damage sensor */
esp_err_t imx219_set_exposure_unsafe(uint16_t exposure) {
    write_register(0x015a, exposure >> 8);      /* No validation! */
    write_register(0x015b, exposure & 0xFF);
    return ESP_OK;
}

/* SECURE: Range checking and limits */
#define IMX219_EXPOSURE_MIN 4       /* Minimum safe exposure */
#define IMX219_EXPOSURE_MAX 65535   /* Hardware maximum */
#define IMX219_EXPOSURE_SAFE_MAX (IMX219_EXPOSURE_MAX * 0.9)  /* Leave headroom */

esp_err_t imx219_set_exposure_safe(uint16_t exposure) {
    /* 1. Validate range */
    if (exposure < IMX219_EXPOSURE_MIN || exposure > IMX219_EXPOSURE_SAFE_MAX) {
        ESP_LOGE(TAG, "Exposure out of safe range: %d", exposure);
        return ESP_ERR_INVALID_ARG;
    }

    /* 2. Split into bytes safely */
    uint8_t exp_high = (exposure >> 8) & 0xFF;
    uint8_t exp_low = exposure & 0xFF;

    /* 3. Write with verification */
    ESP_RETURN_ON_ERROR(imx219_write_reg(port, 0x015a, exp_high), TAG, "Write failed");
    ESP_RETURN_ON_ERROR(imx219_write_reg(port, 0x015b, exp_low), TAG, "Write failed");

    /* 4. Read back to verify */
    uint8_t verify_high, verify_low;
    imx219_read_reg(port, 0x015a, &verify_high);
    imx219_read_reg(port, 0x015b, &verify_low);

    if (verify_high != exp_high || verify_low != exp_low) {
        ESP_LOGW(TAG, "Exposure write verification failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}
```

### 2.3 I2C Register Address Validation

```c
/* VULNERABILITY: Writing to arbitrary registers */
typedef struct {
    uint16_t addr;
    uint8_t min_value;
    uint8_t max_value;
    uint16_t readonly_mask;  /* Which bits are read-only */
    const char *name;
} register_constraint_t;

static const register_constraint_t g_register_whitelist[] = {
    {0x0100, 0x00, 0x01, 0xFF, "MODE_SELECT"},
    {0x0157, 0x00, 232,  0xFF, "ANALOG_GAIN"},
    {0x0114, 0x00, 0x01, 0xFF, "CSI_LANE_MODE"},
    /* ... more registers ... */
    {0, 0, 0, 0, NULL}  /* Terminator */
};

esp_err_t validate_register_write(uint16_t addr, uint8_t value) {
    for (int i = 0; g_register_whitelist[i].name != NULL; i++) {
        const register_constraint_t *reg = &g_register_whitelist[i];

        if (reg->addr == addr) {
            /* Found register in whitelist */
            if (value < reg->min_value || value > reg->max_value) {
                ESP_LOGE(TAG, "Register value out of range");
                return ESP_ERR_INVALID_ARG;
            }
            return ESP_OK;
        }
    }

    /* Register not in whitelist - reject for safety */
    ESP_LOGE(TAG, "Unregistered register access attempt: 0x%04X", addr);
    return ESP_ERR_INVALID_ARG;
}

/* Secure wrapper for register writes */
esp_err_t secure_write_register(i2c_port_t port, uint16_t addr, uint8_t value) {
    /* 1. Validate against whitelist */
    if (validate_register_write(addr, value) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 2. Check I2C bus state */
    if (!i2c_bus_is_healthy(port)) {
        ESP_LOGE(TAG, "I2C bus unhealthy");
        return ESP_FAIL;
    }

    /* 3. Write with timeout */
    if (imx219_write_reg(port, addr, value) != ESP_OK) {
        ESP_LOGE(TAG, "I2C write timeout");
        return ESP_FAIL;
    }

    /* 4. Small delay for register to settle */
    esp_rom_delay_us(100);

    /* 5. Read-back verification */
    uint8_t readback;
    if (imx219_read_reg(port, addr, &readback) != ESP_OK) {
        ESP_LOGW(TAG, "Could not verify register write");
        return ESP_FAIL;
    }

    if (readback != value) {
        ESP_LOGE(TAG, "Register verification failed: 0x%04X", addr);
        return ESP_FAIL;
    }

    return ESP_OK;
}
```

---

## 3. BUFFER OVERFLOW PREVENTION

### 3.1 Safe Buffer Allocation Pattern

```c
/* VULNERABILITY: Stack-based image buffers */
void capture_image_unsafe(uint16_t width, uint16_t height) {
    uint8_t image_data[MAX_IMAGE_WIDTH * MAX_IMAGE_HEIGHT];  /* Stack overflow! */
    capture_to_buffer(image_data, width, height);
}

/* SECURE: Heap allocation with overflow checking */
esp_err_t safe_allocate_image_buffer(image_buffer_t *buf,
                                     uint16_t width, uint16_t height,
                                     uint8_t bytes_per_pixel) {
    /* 1. Validate inputs */
    if (!buf || width == 0 || height == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 2. Check against absolute maximums */
    if (width > MAX_IMAGE_WIDTH || height > MAX_IMAGE_HEIGHT) {
        ESP_LOGE(TAG, "Dimensions exceed limits: %dx%d", width, height);
        return ESP_ERR_INVALID_ARG;
    }

    /* 3. Check for integer overflow in area calculation */
    uint32_t total_pixels;
    if (__builtin_mul_overflow((uint32_t)width, (uint32_t)height, &total_pixels)) {
        ESP_LOGE(TAG, "Pixel count would overflow");
        return ESP_ERR_INVALID_ARG;
    }

    /* 4. Check for overflow in size calculation */
    uint32_t required_bytes;
    if (__builtin_mul_overflow(total_pixels, bytes_per_pixel, &required_bytes)) {
        ESP_LOGE(TAG, "Buffer size would overflow");
        return ESP_ERR_INVALID_ARG;
    }

    /* 5. Check against memory limit */
    if (required_bytes > MAX_IMAGE_BYTES) {
        ESP_LOGE(TAG, "Buffer exceeds memory limit");
        return ESP_ERR_NO_MEM;
    }

    /* 6. Add alignment padding for DMA (must be power of 2) */
    uint32_t stride = ((width * bytes_per_pixel + 31) / 32) * 32;
    uint32_t aligned_size;
    if (__builtin_mul_overflow(stride, height, &aligned_size)) {
        ESP_LOGE(TAG, "Aligned size would overflow");
        return ESP_ERR_INVALID_ARG;
    }

    /* 7. Allocate from DMA heap */
    buf->data = heap_caps_malloc(aligned_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!buf->data) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return ESP_ERR_NO_MEM;
    }

    /* 8. Initialize metadata */
    buf->width = width;
    buf->height = height;
    buf->stride = stride;
    buf->size = aligned_size;

    /* 9. Zero-initialize to prevent information leakage */
    memset(buf->data, 0, aligned_size);

    return ESP_OK;
}
```

### 3.2 Buffer Access Bounds Checking

```c
/* VULNERABILITY: Unchecked pixel access */
void process_pixels_unsafe(image_buffer_t *buf, uint16_t *indices) {
    for (int i = 0; i < 1000; i++) {
        uint16_t idx = indices[i];  /* No validation! */
        process_pixel(buf->data[idx]);  /* Out of bounds! */
    }
}

/* SECURE: Bounds checking before every access */
esp_err_t validate_buffer_access(const image_buffer_t *buf,
                                uint32_t offset, uint32_t size) {
    if (!buf || !buf->data) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Check for overflow in end offset calculation */
    uint32_t end_offset;
    if (__builtin_add_overflow(offset, size, &end_offset)) {
        ESP_LOGE(TAG, "Offset calculation overflow");
        return ESP_ERR_INVALID_ARG;
    }

    /* Check against buffer bounds */
    if (end_offset > buf->size) {
        ESP_LOGE(TAG, "Access exceeds buffer: %d > %d", end_offset, buf->size);
        return ESP_ERR_OUT_OF_RANGE;
    }

    return ESP_OK;
}

void process_pixels_safe(image_buffer_t *buf, uint16_t *indices, uint16_t count) {
    for (int i = 0; i < count; i++) {
        uint16_t idx = indices[i];

        /* Validate before access */
        if (validate_buffer_access(buf, idx, 1) != ESP_OK) {
            ESP_LOGE(TAG, "Invalid pixel access at index %d", idx);
            continue;  /* Skip invalid pixel */
        }

        process_pixel(buf->data[idx]);
    }
}
```

---

## 4. DMA BUFFER SECURITY

### 4.1 Proper DMA Buffer Allocation

```c
/*
 * VULNERABILITY: DMA buffers on stack, misaligned, or in wrong memory
 * - Stack buffers cause stack overflow during DMA operations
 * - Misaligned buffers corrupt adjacent memory
 * - Buffers in IRAM are not DMA-accessible
 */

/* VULNERABLE: Stack-based DMA buffer */
void capture_unsafe() {
    uint8_t image[3280 * 2464];  /* STACK OVERFLOW! */
    dma_capture(image);           /* Corrupts stack! */
}

/* SECURE: Properly allocated DMA buffer */
DMA_ATTR uint8_t g_dma_buffer[3280 * 2464];  /* Global, aligned */

esp_err_t allocate_secure_dma_buffer(dma_buffer_t *buf, uint32_t size) {
    /* 1. Validate size */
    if (size == 0 || size > MAX_DMA_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 2. Allocate from DMA-capable heap with explicit capability flags */
    buf->vaddr = heap_caps_malloc(size,
        MALLOC_CAP_DMA     |  /* DMA-capable memory */
        MALLOC_CAP_INTERNAL |  /* Internal RAM */
        MALLOC_CAP_32BIT      /* 32-bit addressable */
    );

    if (!buf->vaddr) {
        ESP_LOGE(TAG, "DMA allocation failed: %d bytes", size);
        return ESP_ERR_NO_MEM;
    }

    /* 3. Verify alignment (critical for DMA) */
    uintptr_t addr = (uintptr_t)buf->vaddr;
    if ((addr & 31) != 0) {  /* 32-byte alignment */
        ESP_LOGE(TAG, "DMA buffer misaligned: 0x%X", addr);
        free(buf->vaddr);
        return ESP_FAIL;
    }

    /* 4. Get physical address (ESP32-specific) */
    buf->paddr = (uint32_t)buf->vaddr;  /* Identity mapping on ESP32 */

    /* 5. Store allocation info */
    buf->size = size;
    buf->allocated = true;

    ESP_LOGI(TAG, "Allocated secure DMA buffer: vaddr=0x%X, paddr=0x%X, size=%d",
             addr, buf->paddr, size);

    return ESP_OK;
}
```

### 4.2 CSI-2 DMA Descriptor Validation

```c
/* VULNERABILITY: Unchecked DMA descriptors can corrupt memory */
typedef struct {
    uint32_t buffer_addr;   /* Where to write data */
    uint32_t buffer_size;   /* How much data to write */
    uint32_t flags;
} dma_descriptor_t;

/* UNSAFE: No validation of descriptor */
void setup_dma_unsafe(dma_descriptor_t *desc) {
    write_dma_register(DESC_ADDR, (uint32_t)desc);
    /* No checks on buffer_addr or buffer_size! */
}

/* SECURE: Validate descriptor before use */
esp_err_t validate_dma_descriptor(const dma_descriptor_t *desc,
                                 const dma_buffer_t *buf) {
    if (!desc || !buf) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 1. Check buffer address is within allocated buffer */
    if (desc->buffer_addr != buf->paddr) {
        ESP_LOGE(TAG, "Descriptor points to wrong buffer");
        return ESP_ERR_INVALID_ARG;
    }

    /* 2. Check size doesn't exceed buffer */
    if (desc->buffer_size > buf->size) {
        ESP_LOGE(TAG, "Descriptor size exceeds buffer: %d > %d",
                 desc->buffer_size, buf->size);
        return ESP_ERR_INVALID_ARG;
    }

    /* 3. Check buffer is aligned */
    if ((desc->buffer_addr & 31) != 0) {  /* 32-byte alignment */
        ESP_LOGE(TAG, "Buffer not properly aligned");
        return ESP_ERR_INVALID_ARG;
    }

    /* 4. Check flags are valid */
    if (desc->flags & ~(VALID_DMA_FLAGS)) {
        ESP_LOGE(TAG, "Invalid DMA flags: 0x%X", desc->flags);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

void setup_dma_safe(dma_descriptor_t *desc, dma_buffer_t *buf) {
    /* Validate before setup */
    if (validate_dma_descriptor(desc, buf) != ESP_OK) {
        ESP_LOGE(TAG, "DMA descriptor validation failed");
        return;
    }

    /* Safe to setup */
    write_dma_register(DESC_ADDR, (uint32_t)desc);
    write_dma_register(DESC_SIZE, desc->buffer_size);
}
```

---

## 5. STEREO ALIGNMENT SAFETY

### 5.1 Frame Synchronization Validation

```c
/* VULNERABILITY: Misaligned stereo frames produce invalid depth */
typedef struct {
    uint32_t left_frame_id;
    uint32_t right_frame_id;
    uint32_t left_timestamp_us;
    uint32_t right_timestamp_us;
    uint16_t left_width;
    uint16_t right_width;
    uint16_t left_height;
    uint16_t right_height;
} stereo_frame_t;

/* UNSAFE: No synchronization checks */
void compute_depth_unsafe(stereo_frame_t *frames) {
    for (int y = 0; y < frames->left_height; y++) {
        for (int x = 0; x < frames->left_width; x++) {
            /* If frames are misaligned, depth calculation is garbage! */
            compute_disparity(frames->left, frames->right, x, y);
        }
    }
}

/* SECURE: Validate synchronization before processing */
esp_err_t validate_stereo_frames(const stereo_frame_t *frames) {
    if (!frames) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 1. Check frame IDs match (synchronized capture) */
    if (frames->left_frame_id != frames->right_frame_id) {
        ESP_LOGE(TAG, "Frames out of sync: L=%d, R=%d",
                 frames->left_frame_id, frames->right_frame_id);
        return ESP_FAIL;
    }

    /* 2. Check timestamp difference is acceptable (<1ms) */
    int32_t timestamp_diff = (int32_t)(frames->left_timestamp_us -
                                       frames->right_timestamp_us);
    if (timestamp_diff < -1000 || timestamp_diff > 1000) {
        ESP_LOGW(TAG, "Large timestamp difference: %d us", timestamp_diff);
        return ESP_FAIL;
    }

    /* 3. Check dimensions match exactly */
    if (frames->left_width != frames->right_width ||
        frames->left_height != frames->right_height) {
        ESP_LOGE(TAG, "Dimension mismatch: L=%dx%d, R=%dx%d",
                 frames->left_width, frames->left_height,
                 frames->right_width, frames->right_height);
        return ESP_ERR_INVALID_ARG;
    }

    /* 4. Check dimensions are within sensor limits */
    if (frames->left_width > MAX_IMAGE_WIDTH ||
        frames->left_height > MAX_IMAGE_HEIGHT) {
        ESP_LOGE(TAG, "Dimensions exceed sensor limits");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

void compute_depth_safe(stereo_frame_t *frames) {
    /* Validate before processing */
    if (validate_stereo_frames(frames) != ESP_OK) {
        ESP_LOGE(TAG, "Stereo frames not properly synchronized");
        return;
    }

    /* Safe to process */
    for (int y = 0; y < frames->left_height; y++) {
        for (int x = 0; x < frames->left_width; x++) {
            compute_disparity(frames->left, frames->right, x, y);
        }
    }
}
```

---

## 6. MEMORY SAFETY PRACTICES

### 6.1 Integer Overflow Prevention

```c
/* VULNERABILITY: Integer overflow in size calculations */
uint32_t calculate_buffer_size_unsafe(uint16_t width, uint16_t height) {
    /* If width=65535, height=65535: width*height overflows! */
    return width * height * 2;  /* NO OVERFLOW CHECK! */
}

/* SECURE: Using compiler intrinsics for safe arithmetic */
esp_err_t calculate_buffer_size_safe(uint16_t width, uint16_t height,
                                     uint32_t *out_size) {
    if (!out_size) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Step 1: Check width × height */
    uint32_t total_pixels;
    if (__builtin_mul_overflow((uint32_t)width, (uint32_t)height, &total_pixels)) {
        ESP_LOGE(TAG, "Pixel count overflow: %d × %d", width, height);
        return ESP_ERR_INVALID_ARG;
    }

    /* Step 2: Check (width × height) × bytes_per_pixel */
    uint32_t required_bytes;
    if (__builtin_mul_overflow(total_pixels, 2, &required_bytes)) {
        ESP_LOGE(TAG, "Size calculation overflow");
        return ESP_ERR_INVALID_ARG;
    }

    /* Step 3: Check adding alignment padding */
    uint32_t aligned_size;
    uint32_t alignment = 32;
    uint32_t aligned_size_calc = ((required_bytes + alignment - 1) / alignment) * alignment;
    if (aligned_size_calc < required_bytes) {  /* Overflow check */
        ESP_LOGE(TAG, "Alignment calculation overflow");
        return ESP_ERR_INVALID_ARG;
    }

    *out_size = aligned_size_calc;
    return ESP_OK;
}
```

### 6.2 Pointer Safety

```c
/* VULNERABILITY: Use-after-free in image processing */
image_buffer_t *buf;

void process_unsafe() {
    buf = allocate_buffer();
    free_buffer(buf);  /* buf is now invalid */
    process_data(buf->data);  /* USE-AFTER-FREE! */
}

/* SECURE: Explicit state tracking and null checks */
typedef enum {
    BUFFER_STATE_UNINITIALIZED,
    BUFFER_STATE_ALLOCATED,
    BUFFER_STATE_LOCKED,
    BUFFER_STATE_FREED,
} buffer_state_t;

typedef struct {
    image_buffer_t buf;
    buffer_state_t state;
    uint32_t lock_count;
} managed_buffer_t;

esp_err_t acquire_buffer(managed_buffer_t *mbuf) {
    if (!mbuf) return ESP_ERR_INVALID_ARG;

    if (mbuf->state != BUFFER_STATE_ALLOCATED) {
        ESP_LOGE(TAG, "Buffer not in allocated state: %d", mbuf->state);
        return ESP_ERR_INVALID_STATE;
    }

    mbuf->state = BUFFER_STATE_LOCKED;
    mbuf->lock_count++;
    return ESP_OK;
}

esp_err_t release_buffer(managed_buffer_t *mbuf) {
    if (!mbuf) return ESP_ERR_INVALID_ARG;

    if (mbuf->state != BUFFER_STATE_LOCKED) {
        ESP_LOGE(TAG, "Buffer not in locked state: %d", mbuf->state);
        return ESP_ERR_INVALID_STATE;
    }

    if (mbuf->lock_count == 0) {
        ESP_LOGE(TAG, "Buffer lock count underflow");
        return ESP_ERR_INVALID_STATE;
    }

    mbuf->lock_count--;
    if (mbuf->lock_count == 0) {
        mbuf->state = BUFFER_STATE_ALLOCATED;
    }

    return ESP_OK;
}

void process_safe(managed_buffer_t *mbuf) {
    if (acquire_buffer(mbuf) != ESP_OK) {
        return;  /* Buffer not available */
    }

    /* Safe: buffer is locked and valid */
    process_data(mbuf->buf.data);

    release_buffer(mbuf);
}
```

---

## 7. TESTING & VALIDATION FRAMEWORK

### 7.1 Security Test Cases

```c
#include "unity.h"
#include "imx219_stereo.h"

/* Test: Resolution overflow protection */
TEST_CASE("Resolution - Reject oversized dimensions", "[imx219][security]") {
    image_buffer_t buf = {0};

    /* Attempt to allocate oversized buffer */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        image_buffer_allocate(&buf, MAX_IMAGE_WIDTH + 1, 2464, 2));

    TEST_ASSERT_NULL(buf.buffer);
}

/* Test: Integer overflow detection */
TEST_CASE("Buffer allocation - Detect integer overflow", "[imx219][security]") {
    image_buffer_t buf = {0};

    /* These dimensions would cause integer overflow */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        image_buffer_allocate(&buf, 65535, 65535, 2));

    TEST_ASSERT_NULL(buf.buffer);
}

/* Test: Register write validation */
TEST_CASE("Register write - Reject invalid values", "[imx219][security]") {
    /* Mode select should only accept 0 or 1 */
    TEST_ASSERT_EQUAL(ESP_OK, validate_register_write(0x0100, 0x00));
    TEST_ASSERT_EQUAL(ESP_OK, validate_register_write(0x0100, 0x01));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        validate_register_write(0x0100, 0x02));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
        validate_register_write(0x0100, 0xFF));
}

/* Test: Stereo frame sync validation */
TEST_CASE("Stereo - Reject misaligned frames", "[imx219][security]") {
    stereo_frame_t frames = {
        .left_frame_id = 100,
        .right_frame_id = 101,  /* Mismatch! */
        .left_width = 1920,
        .right_width = 1920,
        .left_height = 1080,
        .right_height = 1080,
    };

    TEST_ASSERT_NOT_EQUAL(ESP_OK, validate_stereo_frames(&frames));
}

/* Test: DMA buffer alignment */
TEST_CASE("DMA buffer - Verify alignment", "[imx219][security]") {
    dma_buffer_info_t buf = {0};

    TEST_ASSERT_EQUAL(ESP_OK, allocate_dma_buffer(&buf, 1024));
    TEST_ASSERT_NOT_NULL(buf.vaddr);

    /* Check alignment */
    uintptr_t addr = (uintptr_t)buf.vaddr;
    TEST_ASSERT_EQUAL(0, addr & 31);  /* 32-byte alignment */

    free_dma_buffer(&buf);
}

/* Test: Disparity map memory limit */
TEST_CASE("Disparity - Reject oversized allocation", "[imx219][security]") {
    stereo_disparity_map_t map = {0};

    /* Request more than 2MB limit */
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM,
        allocate_disparity_map(&map, 1920 * 1080, 2048));

    TEST_ASSERT_NULL(map.disparity_map);
}
```

---

## 8. SECURE CONFIGURATION TEMPLATE

```ini
# menuconfig options for security-hardened build

[IMX219 Security]
CONFIG_IMX219_ENABLE_INPUT_VALIDATION=y
CONFIG_IMX219_ENABLE_REGISTER_VERIFY=y
CONFIG_IMX219_ENABLE_BUFFER_GUARD=y
CONFIG_IMX219_ENABLE_FRAME_VALIDATION=y

[Memory Security]
CONFIG_IMX219_MAX_BUFFER_SIZE=2097152
CONFIG_IMX219_MIN_ALIGNMENT=32
CONFIG_IMX219_USE_DMA_MALLOC=y

[I2C Security]
CONFIG_IMX219_I2C_TIMEOUT_MS=1000
CONFIG_IMX219_I2C_RETRIES=3
CONFIG_IMX219_REGISTER_WHITELIST_ONLY=y

[Logging & Debugging]
CONFIG_IMX219_LOG_LEVEL=3
CONFIG_IMX219_SECURITY_AUDIT_LOG=y
CONFIG_IMX219_DEBUG_BUFFERS=y
```

---

## 9. INCIDENT RESPONSE

### 9.1 Detecting Security Issues

```c
/* Monitor for potential exploits */
void security_monitor_task(void *pvParameters) {
    while (1) {
        /* Check for buffer corruption indicators */
        if (is_dma_buffer_corrupted()) {
            ESP_LOGE(TAG, "SECURITY: DMA buffer corruption detected!");
            trigger_emergency_shutdown();
        }

        /* Check for I2C bus attacks */
        if (i2c_bus_has_excessive_errors()) {
            ESP_LOGE(TAG, "SECURITY: I2C bus malfunction detected!");
            reinitialize_i2c();
        }

        /* Check for stack overflow */
        if (is_stack_near_limit()) {
            ESP_LOGE(TAG, "SECURITY: Stack overflow risk!");
            increase_monitoring_frequency();
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
```

---

## 10. COMPLIANCE & STANDARDS

### 10.1 Security Standards Alignment

| Standard | Requirement | Implementation |
|----------|-------------|-----------------|
| CWE-680 | Integer Overflow | `__builtin_mul_overflow()` checks |
| CWE-119 | Buffer Overflow | Size validation, bounds checking |
| CWE-416 | Use-After-Free | Reference counting, state tracking |
| CWE-125 | Out-of-bounds Read | Access validation before read |
| CWE-120 | Unsafe Buffer Copy | Length validation before copy |
| CWE-476 | Null Pointer Deref | Null checks before use |

### 10.2 Secure Development Lifecycle

1. **Threat Modeling** - Identify attack vectors
2. **Secure Design** - Implement defenses by default
3. **Secure Coding** - Follow guidelines and patterns
4. **Security Testing** - Test for common vulnerabilities
5. **Code Review** - Peer security review mandatory
6. **Deployment** - Secure configuration validation
7. **Monitoring** - Runtime anomaly detection
8. **Incident Response** - Rapid failure isolation

---

## 11. REFERENCES & FURTHER READING

- **NIST Cybersecurity Framework**: https://www.nist.gov/cyberframework
- **OWASP Secure Coding Practices**: https://owasp.org/www-project-secure-coding-practices-quick-reference-guide/
- **CWE Top 25**: https://cwe.mitre.org/top25/
- **ESP-IDF Security Guide**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/
- **Linux Kernel Camera Driver Security**: https://www.kernel.org/doc/

---

**Document Version**: 1.0
**Last Updated**: 2025-11-20
**Classification**: Technical Reference
**Security Level**: Internal Use

