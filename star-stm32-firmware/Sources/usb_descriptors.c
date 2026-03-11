/**
 * @file usb_descriptors.c
 * @brief TinyUSB USB device, configuration, and string descriptors for STM32F7.
 *
 * @details
 * Provides the three descriptor callbacks required by TinyUSB for a single
 * CDC ACM interface. These are invoked by the stack during USB enumeration:
 *
 *  - tud_descriptor_device_cb()        -- 18-byte Device Descriptor
 *  - tud_descriptor_configuration_cb() -- Configuration + CDC Interface descriptors
 *  - tud_descriptor_string_cb()        -- Language ID and UTF-16LE string descriptors
 *
 * The device enumerates as a USB CDC ACM serial port (/dev/ttyACM0 on Linux).
 *
 * VID/PID are placeholders -- assign a real VID from usb.org before production.
 *
 * String indices (must match iManufacturer/iProduct/iSerialNumber in device descriptor):
 *   0 = Language ID (English US, 0x0409)
 *   1 = Manufacturer ("Locked Inc")
 *   2 = Product      ("STAR STM32 CDC")
 *   3 = Serial       ("000001")
 *   4 = CDC interface string (empty)
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "tusb.h"

#include <stdint.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * USB identifiers
 * NOTE: k_usb_vid 0xCafe is a test VID. Obtain a real VID before production.
 * ---------------------------------------------------------------------------*/

/**
 * @enum usb_ids_t
 * @brief USB vendor, product, and device release identifiers.
 *
 * @details
 * All values are 16-bit BCD or raw IDs as required by the USB 2.0 spec.
 * Replace k_usb_vid and k_usb_pid with values from usb.org before shipping.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
    k_usb_vid = 0xCafeU, /**< Placeholder vendor ID -- NOT for production use */
    k_usb_pid = 0x4001U, /**< Product ID */
    k_usb_bcd = 0x0100U, /**< Device release: 1.00 in BCD */
} usb_ids_t;

/* ---------------------------------------------------------------------------
 * String descriptor table indices
 * ---------------------------------------------------------------------------*/

/**
 * @enum string_id_t
 * @brief Indices into the USB string descriptor table.
 *
 * @details
 * Index 0 is always the language ID descriptor (mandatory). Indices 1-3 map
 * to iManufacturer, iProduct, and iSerialNumber in the device descriptor.
 * Index 4 is the CDC interface string (empty, referenced by TUD_CDC_DESCRIPTOR).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_strid_langid       = 0U, /**< Language ID (English US 0x0409) */
    k_strid_manufacturer = 1U, /**< Manufacturer string */
    k_strid_product      = 2U, /**< Product string */
    k_strid_serial       = 3U, /**< Serial number string */
    k_strid_cdc_iface    = 4U, /**< CDC interface string (not displayed) */
    k_strid_count        = 5U, /**< Total number of string descriptors */
} string_id_t;

/**
 * @brief UTF-8 string table for string descriptor callbacks.
 *
 * @details
 * Index 0 is the raw 2-byte language ID (English US = 0x09, 0x04) and is
 * handled specially in tud_descriptor_string_cb(). Indices 1+ are ASCII
 * strings converted to UTF-16LE during the callback.
 *
 * @note All strings must be pure 7-bit ASCII (STAR project encoding policy).
 * @warning Direct access is forbidden; only tud_descriptor_string_cb() reads this.
 * @since Version 1.0.0
 */
static const char* const s_string_desc[k_strid_count] = {
    [k_strid_langid]       = "\x09\x04", /**< English (US) language ID bytes */
    [k_strid_manufacturer] = "Locked Inc",
    [k_strid_product]      = "STAR STM32 CDC",
    [k_strid_serial]       = "000001",
    [k_strid_cdc_iface]    = "",
};

/* ---------------------------------------------------------------------------
 * Interface and endpoint numbering
 * ---------------------------------------------------------------------------*/

/**
 * @enum itf_num_t
 * @brief USB interface numbers for the CDC ACM device.
 *
 * @details
 * CDC requires two consecutive interfaces: a Communication Interface (control)
 * and a Data Interface. These must match the order in the configuration
 * descriptor produced by TUD_CDC_DESCRIPTOR().
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_itf_cdc_cmd  = 0U, /**< CDC Communication Interface (control + notifications) */
    k_itf_cdc_data = 1U, /**< CDC Data Interface (bulk IN/OUT) */
    k_itf_total    = 2U, /**< Total interface count */
} itf_num_t;

/**
 * @enum endpoint_num_t
 * @brief USB endpoint addresses (number | direction bit).
 *
 * @details
 * Bit 7 set = IN (device to host). EP 0x81 carries CDC line-state
 * notifications at low rate; EP 0x02/0x82 carry the bulk data stream.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_epnum_cdc_notif = 0x81U, /**< CDC notification IN (interrupt transfer) */
    k_epnum_cdc_out   = 0x02U, /**< CDC data OUT (bulk, host to device) */
    k_epnum_cdc_in    = 0x82U, /**< CDC data IN  (bulk, device to host) */
} endpoint_num_t;

/**
 * @enum cdc_ep_size_t
 * @brief Endpoint packet sizes for full-speed CDC.
 *
 * @details
 * USB 2.0 full-speed bulk max packet size is 64 bytes. Notification endpoint
 * uses 8 bytes (CDC spec minimum for interrupt EP).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_ep_notif_size = 8U,  /**< CDC notification interrupt endpoint size (bytes) */
    k_ep_bulk_size  = 64U, /**< CDC bulk data endpoint size for full-speed (bytes) */
} cdc_ep_size_t;

/**
 * @enum config_desc_sizes_t
 * @brief Configuration descriptor sizes and power.
 *
 * @details
 * Total length is the sum of the standard config descriptor and one CDC
 * descriptor blob (IAD + 2 interfaces + 3 endpoints).
 * Power is in mA; 100 mA is the USB default for bus-powered devices.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
    k_config_total_len = TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN, /**< Total bytes in config blob */
    k_config_power_ma  = 100U, /**< Max bus current draw in mA */
} config_desc_sizes_t;

/**
 * @enum usb_string_limits_t
 * @brief Limits for the UTF-16LE string descriptor conversion buffer.
 *
 * @details
 * The static buffer index 0 holds the descriptor header word; indices 1..N
 * hold the UTF-16LE character payload.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_max_str_chars     = 31U, /**< Max payload characters in one string descriptor */
    k_str_header_words  = 1U,  /**< Buffer slots reserved for descriptor header */
    k_langid_word_count = 1U,  /**< Language ID encodes as 1 UTF-16LE word */
    k_desc_hdr_bytes    = 2U,  /**< Byte length of descriptor header (type + length) */
    k_utf16_bytes_per   = 2U,  /**< Bytes per UTF-16LE character */
} usb_string_limits_t;

/* ---------------------------------------------------------------------------
 * USB version constant
 * ---------------------------------------------------------------------------*/

/**
 * @enum usb_version_t
 * @brief USB specification version in BCD format.
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
    k_usb_ver_bcd = 0x0200U, /**< USB 2.0 in BCD */
} usb_version_t;

/**
 * @enum device_desc_counts_t
 * @brief Device descriptor scalar counts.
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
    k_num_configurations = 1U, /**< Single USB configuration supported */
} device_desc_counts_t;

/* ---------------------------------------------------------------------------
 * Configuration descriptor blob
 * ---------------------------------------------------------------------------*/

/**
 * @brief Full-speed configuration descriptor for a single CDC ACM interface.
 *
 * @details
 * Static byte array consumed by tud_descriptor_configuration_cb(). Built
 * with TinyUSB helper macros:
 *   TUD_CONFIG_DESCRIPTOR -- standard USB configuration descriptor header
 *   TUD_CDC_DESCRIPTOR    -- IAD + Communication Interface + Data Interface +
 *                            notification (interrupt IN) + data (bulk IN/OUT)
 *
 * @note Configuration number is 1 (USB spec: first config = 1, not 0).
 * @note String index 0 for the configuration means no configuration string.
 * @note bmAttributes 0x00 = bus-powered, no remote wakeup.
 * @since Version 1.0.0
 */
static uint8_t const s_desc_fs_config[k_config_total_len] = {
    TUD_CONFIG_DESCRIPTOR(1,                  /* bConfigurationValue */
                          k_itf_total,         /* bNumInterfaces */
                          0,                   /* iConfiguration string index (none) */
                          k_config_total_len,
                          0x00,                /* bmAttributes: bus-powered */
                          k_config_power_ma),

    TUD_CDC_DESCRIPTOR(k_itf_cdc_cmd,
                       k_strid_cdc_iface,
                       k_epnum_cdc_notif,
                       k_ep_notif_size,
                       k_epnum_cdc_out,
                       k_epnum_cdc_in,
                       k_ep_bulk_size),
};

/* ---------------------------------------------------------------------------
 * Descriptor callbacks (called by TinyUSB during enumeration)
 * ---------------------------------------------------------------------------*/

/**
 * @brief Return pointer to the USB Device Descriptor.
 *
 * @details
 * Called by TinyUSB in response to GET_DESCRIPTOR(Device). Returns an
 * 18-byte descriptor that tells the host the USB version, device class,
 * VID/PID, and number of configurations.
 *
 * The class triple MISC + COMMON + IAD is mandatory when CDC requires an
 * Interface Association Descriptor to group its two interfaces under one
 * logical device.
 *
 * @return uint8_t const* Pointer to the 18-byte device descriptor.
 *
 * @pre TinyUSB stack initialized via tusb_init()
 * @post No state modified; descriptor is read-only static data
 *
 * @note Called from tud_task() context, not from ISR
 * @since Version 1.0.0
 */
uint8_t const* tud_descriptor_device_cb(void)
{
    static tusb_desc_device_t const desc_device = {
        .bLength            = sizeof(tusb_desc_device_t),
        .bDescriptorType    = TUSB_DESC_DEVICE,
        .bcdUSB             = k_usb_ver_bcd,

        /* MISC + COMMON + IAD: required for composite CDC device */
        .bDeviceClass       = TUSB_CLASS_MISC,
        .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
        .bDeviceProtocol    = MISC_PROTOCOL_IAD,

        .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

        .idVendor           = k_usb_vid,
        .idProduct          = k_usb_pid,
        .bcdDevice          = k_usb_bcd,

        .iManufacturer      = k_strid_manufacturer,
        .iProduct           = k_strid_product,
        .iSerialNumber      = k_strid_serial,

        .bNumConfigurations = k_num_configurations,
    };

    return (uint8_t const*)&desc_device;
}

/**
 * @brief Return pointer to the USB Configuration Descriptor.
 *
 * @details
 * Called by TinyUSB in response to GET_DESCRIPTOR(Configuration). Returns
 * the full descriptor blob including the CDC IAD, interface descriptors, and
 * endpoint descriptors. The host uses this to load the CDC driver.
 *
 * @param[in] index Configuration index (0-based). Only index 0 is valid.
 *
 * @return uint8_t const* Pointer to configuration descriptor blob.
 *
 * @pre TinyUSB stack initialized via tusb_init()
 * @post No state modified; descriptor is read-only static data
 *
 * @note Called from tud_task() context, not from ISR
 * @since Version 1.0.0
 */
uint8_t const* tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return s_desc_fs_config;
}

/**
 * @brief Build and return a USB String Descriptor for the given index.
 *
 * @details
 * Called by TinyUSB in response to GET_DESCRIPTOR(String). Converts the
 * ASCII string at s_string_desc[index] to a UTF-16LE USB string descriptor
 * stored in a static buffer. Index 0 (language ID) is handled specially --
 * the raw 2-byte language ID is copied rather than converted.
 *
 * The returned pointer is valid until the next call to this function.
 *
 * @param[in] index  String descriptor index (0 = language ID, 1-4 = strings)
 * @param[in] langid BCP 47 language tag from the host (ignored)
 *
 * @return uint16_t const* Pointer to UTF-16LE string descriptor.
 * @retval NULL index >= k_strid_count (out of range)
 *
 * @pre TinyUSB stack initialized via tusb_init()
 * @pre s_string_desc populated with valid ASCII strings
 * @post Static buffer overwritten with new descriptor content
 *
 * @note Not re-entrant; called from tud_task() context only
 * @note Strings longer than k_max_str_chars are silently truncated
 * @since Version 1.0.0
 */
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;

    /* Index 0 = language ID header + 1 payload word; rest = header + string chars */
    static uint16_t s_str_buf[k_str_header_words + k_max_str_chars];

    uint8_t chr_count;

    if (index == k_strid_langid) {
        /* Language ID: copy the 2 raw bytes (0x09, 0x04) as one UTF-16LE word */
        memcpy(&s_str_buf[k_str_header_words], s_string_desc[k_strid_langid],
               k_utf16_bytes_per);
        chr_count = k_langid_word_count;
    } else {
        if (index >= k_strid_count) {
            return NULL;
        }

        const char* str = s_string_desc[index];
        chr_count = (uint8_t)strlen(str);
        if (chr_count > k_max_str_chars) {
            chr_count = k_max_str_chars;
        }

        /* ASCII to UTF-16LE: ASCII is a subset, so high byte is always 0 */
        for (uint8_t i = 0U; i < chr_count; i++) {
            s_str_buf[k_str_header_words + i] = (uint16_t)str[i];
        }
    }

    /* Header: descriptor type in high byte, total byte length in low byte */
    s_str_buf[0] = (uint16_t)(((uint16_t)TUSB_DESC_STRING << 8U) |
                               (uint16_t)(k_utf16_bytes_per * chr_count + k_desc_hdr_bytes));

    return s_str_buf;
}
