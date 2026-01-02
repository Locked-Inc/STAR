/* lib/rx_usb/src/rx_usb_cdc.c */

/**
 * @file rx_usb_cdc.c
 * @brief USB CDC-ACM Class Implementation for RX72N
 * @details
 * This file implements the USB Communications Device Class (CDC)
 * with Abstract Control Model (ACM) for virtual COM port functionality.
 *
 * CDC-ACM Endpoints:
 * - EP0: Control (enumeration, class requests)
 * - EP1: Bulk IN (data to host)
 * - EP2: Bulk OUT (data from host)
 * - EP3: Interrupt IN (serial state notifications)
 *
 * Class Requests Supported:
 * - SET_LINE_CODING (0x20)
 * - GET_LINE_CODING (0x21)
 * - SET_CONTROL_LINE_STATE (0x22)
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <string.h>

#include "rx72n_regs.h"
#include "rx_log.h"
#include "rx_usb.h"

/* =============================================================================
 * Private Definitions
 * =============================================================================
 */

static const char* s_tag = "USB_CDC";

/** @brief USB Descriptor Types */
typedef enum {
  k_usb_desc_type_device        = (0x01), /**< Device descriptor */
  k_usb_desc_type_configuration = (0x02), /**< Configuration descriptor */
  k_usb_desc_type_string        = (0x03), /**< String descriptor */
  k_usb_desc_type_interface     = (0x04), /**< Interface descriptor */
  k_usb_desc_type_endpoint      = (0x05), /**< Endpoint descriptor */
  k_usb_desc_type_cs_interface  = (0x24), /**< Class-specific interface descriptor */
} usb_descriptor_type_t;

/** @brief USB Class Codes */
typedef enum {
  k_usb_class_cdc      = (0x02), /**< Communications Device Class */
  k_usb_class_cdc_data = (0x0A), /**< CDC Data Class */
  k_usb_subclass_acm   = (0x02), /**< Abstract Control Model subclass */
  k_usb_protocol_at    = (0x01), /**< AT command protocol */
} usb_class_code_t;

/** @brief CDC Class Request Codes */
typedef enum {
  k_cdc_set_line_coding        = (0x20), /**< Set line coding (baud rate, parity, etc.) */
  k_cdc_get_line_coding        = (0x21), /**< Get current line coding */
  k_cdc_set_control_line_state = (0x22), /**< Set control line state (DTR/RTS) */
} cdc_request_code_t;

/** @brief USB Standard Request Codes */
typedef enum {
  k_usb_req_get_status        = (0x00), /**< Get device/interface/endpoint status */
  k_usb_req_clear_feature     = (0x01), /**< Clear feature */
  k_usb_req_set_feature       = (0x03), /**< Set feature */
  k_usb_req_set_address       = (0x05), /**< Set device address */
  k_usb_req_get_descriptor    = (0x06), /**< Get descriptor */
  k_usb_req_set_descriptor    = (0x07), /**< Set descriptor */
  k_usb_req_get_configuration = (0x08), /**< Get configuration */
  k_usb_req_set_configuration = (0x09), /**< Set configuration */
  k_usb_req_get_interface     = (0x0A), /**< Get interface */
  k_usb_req_set_interface     = (0x0B), /**< Set interface */
} usb_request_code_t;

/** @brief Vendor and Product IDs */
typedef enum {
  k_usb_vid = (0x045B), /**< Vendor ID: Renesas Electronics (test VID) */
  k_usb_pid = (0x0234), /**< Product ID: CDC-ACM device (test PID) */
} usb_device_id_t;

/** @brief USB Version Numbers (BCD format) */
typedef enum {
  k_usb_version_2_0  = (0x0200), /**< USB 2.0 specification */
  k_usb_version_1_1  = (0x0110), /**< USB 1.1 specification (for CDC) */
  k_device_version   = (0x0100), /**< Device release version 1.0 */
  k_usb_langid_en_us = (0x0409), /**< Language ID: English (United States) */
} usb_version_t;

/** @brief CDC Functional Descriptor Subtypes */
typedef enum {
  k_cdc_subtype_header          = (0x00), /**< Header functional descriptor */
  k_cdc_subtype_call_management = (0x01), /**< Call management functional descriptor */
  k_cdc_subtype_acm             = (0x02), /**< ACM functional descriptor */
  k_cdc_subtype_union           = (0x06), /**< Union functional descriptor */
} cdc_descriptor_subtype_t;

/** @brief USB Endpoint Addresses */
typedef enum {
  k_usb_ep0_out      = (0x00), /**< Control endpoint 0 OUT */
  k_usb_ep0_in       = (0x80), /**< Control endpoint 0 IN */
  k_usb_ep1_bulk_in  = (0x81), /**< Bulk IN endpoint 1 (data to host) */
  k_usb_ep2_bulk_out = (0x02), /**< Bulk OUT endpoint 2 (data from host) */
  k_usb_ep3_int_in   = (0x83), /**< Interrupt IN endpoint 3 (notifications) */
} usb_endpoint_address_t;

/** @brief USB Endpoint Transfer Types (bmAttributes) */
typedef enum {
  k_usb_ep_type_control     = (0x00), /**< Control transfer */
  k_usb_ep_type_isochronous = (0x01), /**< Isochronous transfer */
  k_usb_ep_type_bulk        = (0x02), /**< Bulk transfer */
  k_usb_ep_type_interrupt   = (0x03), /**< Interrupt transfer */
} usb_endpoint_type_t;

/** @brief USB Configuration Attributes */
typedef enum {
  k_usb_cfg_attr_bus_powered  = (0x80), /**< Bus-powered device */
  k_usb_cfg_attr_self_powered = (0xC0), /**< Self-powered device */
} usb_config_attributes_t;

/** @brief USB Power Consumption (in 2mA units) */
typedef enum {
  k_usb_max_power_100ma = (50),  /**< 100mA (50 * 2mA) */
  k_usb_max_power_500ma = (250), /**< 500mA (250 * 2mA) */
} usb_max_power_t;

/** @brief CDC Capabilities */
typedef enum {
  k_cdc_acm_cap_line_coding = (0x02), /**< Supports SET/GET_LINE_CODING and serial state */
} cdc_acm_capabilities_t;

/** @brief USB Descriptor Field Indices */
typedef enum {
  k_usb_string_index_langid        = (0), /**< Language ID string index */
  k_usb_string_index_manufacturer  = (1), /**< Manufacturer string index */
  k_usb_string_index_product       = (2), /**< Product string index */
  k_usb_string_index_serial_number = (3), /**< Serial number string index */
} usb_string_index_t;

/** @brief USB Endpoint Packet Sizes */
typedef enum {
  k_usb_ep0_packet_size       = (64), /**< Control endpoint max packet size */
  k_usb_bulk_packet_size      = (64), /**< Bulk endpoint max packet size (Full-Speed) */
  k_usb_interrupt_packet_size = (8),  /**< Interrupt endpoint max packet size */
} usb_packet_size_t;

/** @brief USB Polling Intervals */
typedef enum {
  k_usb_bulk_interval      = (0),  /**< Bulk endpoints don't use polling */
  k_usb_interrupt_interval = (10), /**< Interrupt endpoint polling interval (10ms) */
} usb_polling_interval_t;

/** @brief USB String Descriptor Sizes */
typedef enum {
  k_usb_string_header_size   = (2),  /**< bLength + bDescriptorType */
  k_usb_string_char_size     = (2),  /**< UTF-16LE character size (2 bytes) */
  k_usb_string_langid_chars  = (1),  /**< Language ID is 1 uint16_t */
  k_usb_string_mfr_chars     = (7),  /**< "Renesas" = 7 characters */
  k_usb_string_product_chars = (14), /**< "STAR RX72N CDC" = 14 characters */
  k_usb_string_serial_chars  = (8),  /**< "00000001" = 8 characters */
} usb_string_size_t;

/** @brief Bit Shift Values for Multi-Byte Fields */
typedef enum {
  k_bit_shift_byte_0 = (0),  /**< Shift for byte 0 (LSB) */
  k_bit_shift_byte_1 = (8),  /**< Shift for byte 1 */
  k_bit_shift_byte_2 = (16), /**< Shift for byte 2 */
  k_bit_shift_byte_3 = (24), /**< Shift for byte 3 (MSB for 32-bit) */
} bit_shift_t;

/** @brief Byte Masks */
typedef enum {
  k_byte_mask        = (0xFF), /**< Mask for extracting a single byte */
  k_usb_address_mask = (0x7F), /**< USB address mask (7 bits, 0-127) */
} byte_mask_t;

/** @brief USB Request Type Field Masks (bmRequestType) */
typedef enum {
  k_usb_req_type_mask     = (0x60), /**< Mask for bits 5-6 (request type) */
  k_usb_req_type_standard = (0x00), /**< Standard request */
  k_usb_req_type_class    = (0x20), /**< Class-specific request */
  k_usb_req_type_vendor   = (0x40), /**< Vendor-specific request */
} usb_request_type_mask_t;

/** @brief CDC Line Coding Structure Byte Indices */
typedef enum {
  k_line_coding_baud_rate_byte_0 = (0), /**< Baud rate byte 0 (LSB) */
  k_line_coding_baud_rate_byte_1 = (1), /**< Baud rate byte 1 */
  k_line_coding_baud_rate_byte_2 = (2), /**< Baud rate byte 2 */
  k_line_coding_baud_rate_byte_3 = (3), /**< Baud rate byte 3 (MSB) */
  k_line_coding_stop_bits_index  = (4), /**< Stop bits byte index */
  k_line_coding_parity_index     = (5), /**< Parity byte index */
  k_line_coding_data_bits_index  = (6), /**< Data bits byte index */
  k_line_coding_size             = (7), /**< Total line coding structure size */
} cdc_line_coding_index_t;

/** @brief USB Pipe Numbers */
typedef enum {
  k_usb_pipe_0 = (0), /**< Control pipe (DCP) */
  k_usb_pipe_1 = (1), /**< Pipe 1 (Bulk IN - EP1) */
  k_usb_pipe_2 = (2), /**< Pipe 2 (Bulk OUT - EP2) */
  k_usb_pipe_3 = (3), /**< Pipe 3 (Interrupt IN - EP3) */
} usb_pipe_number_t;

/** @brief USB Configuration Values */
typedef enum {
  k_usb_config_unconfigured = (0), /**< Device not configured */
  k_usb_config_value_1      = (1), /**< Configuration 1 */
} usb_config_value_t;

/* =============================================================================
 * USB Descriptor Type Definitions
 * =============================================================================
 */

/**
 * @brief USB Device Descriptor (18 bytes)
 *
 * USB spec field names (camelCase) are documented in comments for reference.
 */
typedef struct __attribute__((packed)) {
  uint8_t  length;              /**< bLength: Size of this descriptor in bytes */
  uint8_t  descriptor_type;     /**< bDescriptorType: DEVICE descriptor type */
  uint16_t usb_version;         /**< bcdUSB: USB Specification Release Number (BCD) */
  uint8_t  device_class;        /**< bDeviceClass: Class code (assigned by USB-IF) */
  uint8_t  device_sub_class;    /**< bDeviceSubClass: Subclass code */
  uint8_t  device_protocol;     /**< bDeviceProtocol: Protocol code */
  uint8_t  max_packet_size_ep0; /**< bMaxPacketSize0: Max packet size for EP0 */
  uint16_t vendor_id;           /**< idVendor: Vendor ID (assigned by USB-IF) */
  uint16_t product_id;          /**< idProduct: Product ID (assigned by manufacturer) */
  uint16_t device_version;      /**< bcdDevice: Device release number (BCD) */
  uint8_t  manufacturer_index;  /**< iManufacturer: Manufacturer string index */
  uint8_t  product_index;       /**< iProduct: Product string index */
  uint8_t  serial_number_index; /**< iSerialNumber: Serial number string index */
  uint8_t  num_configurations;  /**< bNumConfigurations: Number of configurations */
} usb_device_descriptor_t;

/**
 * @brief USB Configuration Descriptor (9 bytes)
 */
typedef struct __attribute__((packed)) {
  uint8_t  length;              /**< bLength: Size of this descriptor */
  uint8_t  descriptor_type;     /**< bDescriptorType: CONFIGURATION type */
  uint16_t total_length;        /**< wTotalLength: Total data length */
  uint8_t  num_interfaces;      /**< bNumInterfaces: Number of interfaces */
  uint8_t  configuration_value; /**< bConfigurationValue: Config select value */
  uint8_t  configuration_index; /**< iConfiguration: String index */
  uint8_t  attributes;          /**< bmAttributes: Configuration characteristics */
  uint8_t  max_power;           /**< bMaxPower: Power consumption (2mA units) */
} usb_configuration_descriptor_t;

/**
 * @brief USB Interface Descriptor (9 bytes)
 */
typedef struct __attribute__((packed)) {
  uint8_t length;             /**< bLength: Size of this descriptor */
  uint8_t descriptor_type;    /**< bDescriptorType: INTERFACE type */
  uint8_t interface_number;   /**< bInterfaceNumber: Interface number */
  uint8_t alternate_setting;  /**< bAlternateSetting: Alternate setting */
  uint8_t num_endpoints;      /**< bNumEndpoints: Number of endpoints */
  uint8_t interface_class;    /**< bInterfaceClass: Class code */
  uint8_t interface_subclass; /**< bInterfaceSubClass: Subclass code */
  uint8_t interface_protocol; /**< bInterfaceProtocol: Protocol code */
  uint8_t interface_index;    /**< iInterface: String index */
} usb_interface_descriptor_t;

/**
 * @brief USB Endpoint Descriptor (7 bytes)
 */
typedef struct __attribute__((packed)) {
  uint8_t  length;           /**< bLength: Size of this descriptor */
  uint8_t  descriptor_type;  /**< bDescriptorType: ENDPOINT type */
  uint8_t  endpoint_address; /**< bEndpointAddress: Endpoint address */
  uint8_t  attributes;       /**< bmAttributes: Endpoint attributes */
  uint16_t max_packet_size;  /**< wMaxPacketSize: Maximum packet size */
  uint8_t  interval;         /**< bInterval: Polling interval */
} usb_endpoint_descriptor_t;

/**
 * @brief CDC Header Functional Descriptor (5 bytes)
 */
typedef struct __attribute__((packed)) {
  uint8_t  length;             /**< bFunctionLength: Size of descriptor */
  uint8_t  descriptor_type;    /**< bDescriptorType: CS_INTERFACE type */
  uint8_t  descriptor_subtype; /**< bDescriptorSubtype: Header subtype */
  uint16_t cdc_version;        /**< bcdCDC: CDC specification release (BCD) */
} cdc_header_descriptor_t;

/**
 * @brief CDC Call Management Functional Descriptor (5 bytes)
 */
typedef struct __attribute__((packed)) {
  uint8_t length;             /**< bFunctionLength: Size of descriptor */
  uint8_t descriptor_type;    /**< bDescriptorType: CS_INTERFACE type */
  uint8_t descriptor_subtype; /**< bDescriptorSubtype: Call Management */
  uint8_t capabilities;       /**< bmCapabilities: Call management capabilities */
  uint8_t data_interface;     /**< bDataInterface: Data class interface number */
} cdc_call_management_descriptor_t;

/**
 * @brief CDC ACM Functional Descriptor (4 bytes)
 */
typedef struct __attribute__((packed)) {
  uint8_t length;             /**< bFunctionLength: Size of descriptor */
  uint8_t descriptor_type;    /**< bDescriptorType: CS_INTERFACE type */
  uint8_t descriptor_subtype; /**< bDescriptorSubtype: ACM subtype */
  uint8_t capabilities;       /**< bmCapabilities: ACM capabilities */
} cdc_acm_descriptor_t;

/**
 * @brief CDC Union Functional Descriptor (5 bytes)
 */
typedef struct __attribute__((packed)) {
  uint8_t length;                /**< bFunctionLength: Size of descriptor */
  uint8_t descriptor_type;       /**< bDescriptorType: CS_INTERFACE type */
  uint8_t descriptor_subtype;    /**< bDescriptorSubtype: Union subtype */
  uint8_t control_interface;     /**< bControlInterface: Control interface number */
  uint8_t subordinate_interface; /**< bSubordinateInterface: Subordinate interface */
} cdc_union_descriptor_t;

/**
 * @brief USB String Descriptor Header (variable length)
 */
typedef struct __attribute__((packed)) {
  uint8_t  length;          /**< bLength: Size of descriptor */
  uint8_t  descriptor_type; /**< bDescriptorType: STRING type */
  uint16_t data[];          /**< wData: UTF-16LE string data */
} usb_string_descriptor_t;

/* Compile-time size verification */
_Static_assert(sizeof(usb_device_descriptor_t) == 18, "Device descriptor must be 18 bytes");
_Static_assert(sizeof(usb_configuration_descriptor_t) == 9,
               "Configuration descriptor must be 9 bytes");
_Static_assert(sizeof(usb_interface_descriptor_t) == 9, "Interface descriptor must be 9 bytes");
_Static_assert(sizeof(usb_endpoint_descriptor_t) == 7, "Endpoint descriptor must be 7 bytes");
_Static_assert(sizeof(cdc_header_descriptor_t) == 5, "CDC Header descriptor must be 5 bytes");
_Static_assert(sizeof(cdc_call_management_descriptor_t) == 5,
               "CDC Call Management descriptor must be 5 bytes");
_Static_assert(sizeof(cdc_acm_descriptor_t) == 4, "CDC ACM descriptor must be 4 bytes");
_Static_assert(sizeof(cdc_union_descriptor_t) == 5, "CDC Union descriptor must be 5 bytes");

/* =============================================================================
 * USB Descriptor Instances
 * =============================================================================
 */

/** @brief Device Descriptor */
static const usb_device_descriptor_t s_device_desc = {
  .length              = sizeof(usb_device_descriptor_t),
  .descriptor_type     = k_usb_desc_type_device,
  .usb_version         = k_usb_version_2_0,
  .device_class        = k_usb_class_cdc,
  .device_sub_class    = 0x00,
  .device_protocol     = 0x00,
  .max_packet_size_ep0 = k_usb_ep0_packet_size,
  .vendor_id           = k_usb_vid,
  .product_id          = k_usb_pid,
  .device_version      = k_device_version,
  .manufacturer_index  = k_usb_string_index_manufacturer,
  .product_index       = k_usb_string_index_product,
  .serial_number_index = k_usb_string_index_serial_number,
  .num_configurations  = 1,
};

/**
 * @brief Complete Configuration Descriptor
 *
 * USB configuration descriptors are composite structures containing:
 * - Configuration descriptor (9 bytes)
 * - Interface descriptors (9 bytes each)
 * - CDC functional descriptors (4-5 bytes each)
 * - Endpoint descriptors (7 bytes each)
 *
 * Total: 67 bytes for CDC-ACM with 2 interfaces and 3 endpoints
 */
typedef struct __attribute__((packed)) {
  usb_configuration_descriptor_t   config;
  usb_interface_descriptor_t       cdc_interface;
  cdc_header_descriptor_t          cdc_header;
  cdc_call_management_descriptor_t cdc_call_mgmt;
  cdc_acm_descriptor_t             cdc_acm;
  cdc_union_descriptor_t           cdc_union;
  usb_endpoint_descriptor_t        ep3_interrupt_in;
  usb_interface_descriptor_t       data_interface;
  usb_endpoint_descriptor_t        ep1_bulk_in;
  usb_endpoint_descriptor_t        ep2_bulk_out;
} usb_cdc_config_descriptor_t;

_Static_assert(sizeof(usb_cdc_config_descriptor_t) == 67, "CDC config descriptor must be 67 bytes");

/** @brief Configuration Descriptor Instance */
static const usb_cdc_config_descriptor_t s_config_desc = {
  .config =
    {
      .length              = sizeof(usb_configuration_descriptor_t),
      .descriptor_type     = k_usb_desc_type_configuration,
      .total_length        = sizeof(usb_cdc_config_descriptor_t),
      .num_interfaces      = 2,
      .configuration_value = 1,
      .configuration_index = 0,
      .attributes          = k_usb_cfg_attr_bus_powered,
      .max_power           = k_usb_max_power_100ma,
    },
  .cdc_interface =
    {
      .length             = sizeof(usb_interface_descriptor_t),
      .descriptor_type    = k_usb_desc_type_interface,
      .interface_number   = 0,
      .alternate_setting  = 0,
      .num_endpoints      = 1,
      .interface_class    = k_usb_class_cdc,
      .interface_subclass = k_usb_subclass_acm,
      .interface_protocol = k_usb_protocol_at,
      .interface_index    = 0,
    },
  .cdc_header =
    {
      .length             = sizeof(cdc_header_descriptor_t),
      .descriptor_type    = k_usb_desc_type_cs_interface,
      .descriptor_subtype = k_cdc_subtype_header,
      .cdc_version        = k_usb_version_1_1,
    },
  .cdc_call_mgmt =
    {
      .length             = sizeof(cdc_call_management_descriptor_t),
      .descriptor_type    = k_usb_desc_type_cs_interface,
      .descriptor_subtype = k_cdc_subtype_call_management,
      .capabilities       = 0x00,
      .data_interface     = 1,
    },
  .cdc_acm =
    {
      .length             = sizeof(cdc_acm_descriptor_t),
      .descriptor_type    = k_usb_desc_type_cs_interface,
      .descriptor_subtype = k_cdc_subtype_acm,
      .capabilities       = k_cdc_acm_cap_line_coding,
    },
  .cdc_union =
    {
      .length                = sizeof(cdc_union_descriptor_t),
      .descriptor_type       = k_usb_desc_type_cs_interface,
      .descriptor_subtype    = k_cdc_subtype_union,
      .control_interface     = 0,
      .subordinate_interface = 1,
    },
  .ep3_interrupt_in =
    {
      .length           = sizeof(usb_endpoint_descriptor_t),
      .descriptor_type  = k_usb_desc_type_endpoint,
      .endpoint_address = k_usb_ep3_int_in,
      .attributes       = k_usb_ep_type_interrupt,
      .max_packet_size  = k_usb_interrupt_packet_size,
      .interval         = k_usb_interrupt_interval,
    },
  .data_interface =
    {
      .length             = sizeof(usb_interface_descriptor_t),
      .descriptor_type    = k_usb_desc_type_interface,
      .interface_number   = 1,
      .alternate_setting  = 0,
      .num_endpoints      = 2,
      .interface_class    = k_usb_class_cdc_data,
      .interface_subclass = 0,
      .interface_protocol = 0,
      .interface_index    = 0,
    },
  .ep1_bulk_in =
    {
      .length           = sizeof(usb_endpoint_descriptor_t),
      .descriptor_type  = k_usb_desc_type_endpoint,
      .endpoint_address = k_usb_ep1_bulk_in,
      .attributes       = k_usb_ep_type_bulk,
      .max_packet_size  = k_usb_bulk_packet_size,
      .interval         = k_usb_bulk_interval,
    },
  .ep2_bulk_out =
    {
      .length           = sizeof(usb_endpoint_descriptor_t),
      .descriptor_type  = k_usb_desc_type_endpoint,
      .endpoint_address = k_usb_ep2_bulk_out,
      .attributes       = k_usb_ep_type_bulk,
      .max_packet_size  = k_usb_bulk_packet_size,
      .interval         = k_usb_bulk_interval,
    },
};

/** @brief String Descriptor 0: Language ID (English US) */
static const struct __attribute__((packed)) {
  uint8_t  length;
  uint8_t  descriptor_type;
  uint16_t langid;
} s_string_langid = {
  .length = k_usb_string_header_size + (k_usb_string_langid_chars * k_usb_string_char_size),
  .descriptor_type = k_usb_desc_type_string,
  .langid          = k_usb_langid_en_us,
};

/** @brief String Descriptor 1: Manufacturer ("Renesas") */
static const struct __attribute__((packed)) {
  uint8_t  length;
  uint8_t  descriptor_type;
  uint16_t string[k_usb_string_mfr_chars]; /* "Renesas" in UTF-16LE */
} s_string_manufacturer = {
  .length          = k_usb_string_header_size + (k_usb_string_mfr_chars * k_usb_string_char_size),
  .descriptor_type = k_usb_desc_type_string,
  .string          = {'R', 'e', 'n', 'e', 's', 'a', 's'},
};

/** @brief String Descriptor 2: Product ("STAR RX72N CDC") */
static const struct __attribute__((packed)) {
  uint8_t  length;
  uint8_t  descriptor_type;
  uint16_t string[k_usb_string_product_chars]; /* "STAR RX72N CDC" in UTF-16LE */
} s_string_product = {
  .length = k_usb_string_header_size + (k_usb_string_product_chars * k_usb_string_char_size),
  .descriptor_type = k_usb_desc_type_string,
  .string          = {'S', 'T', 'A', 'R', ' ', 'R', 'X', '7', '2', 'N', ' ', 'C', 'D', 'C'},
};

/** @brief String Descriptor 3: Serial Number ("00000001") */
static const struct __attribute__((packed)) {
  uint8_t  length;
  uint8_t  descriptor_type;
  uint16_t string[k_usb_string_serial_chars]; /* "00000001" in UTF-16LE */
} s_string_serial = {
  .length = k_usb_string_header_size + (k_usb_string_serial_chars * k_usb_string_char_size),
  .descriptor_type = k_usb_desc_type_string,
  .string          = {'0', '0', '0', '0', '0', '0', '0', '1'},
};

/* =============================================================================
 * Private Variables
 * =============================================================================
 */

static bool s_cdc_initialized = false;

/* Current line coding from host */
static rx_usb_line_coding_t s_line_coding = {.baud_rate = k_usb_default_baud_rate,
                                             .stop_bits = k_usb_default_stop_bits,
                                             .parity    = k_usb_default_parity,
                                             .data_bits = k_usb_default_data_bits};

/* Control line state from host */
static uint16_t s_control_line_state = 0;

/* =============================================================================
 * Forward Declarations
 * =============================================================================
 */

/* From rx_usb_hw.c */
extern uint32_t rx_usb_hw_fifo_read(uint8_t pipe, uint8_t* data, uint32_t max_len);
extern uint32_t rx_usb_hw_fifo_write(uint8_t pipe, const uint8_t* data, uint32_t len);
extern void     rx_usb_hw_set_address(uint8_t address);
extern rx_err_t rx_usb_hw_configure_pipe(uint8_t  pipe,
                                         uint8_t  endpoint,
                                         bool     is_in,
                                         uint16_t type,
                                         uint16_t max_packet);

/* From rx_usb.c */
extern void     rx_usb_set_state(rx_usb_state_t state);
extern void     rx_usb_set_line_coding(const rx_usb_line_coding_t* coding);
extern uint32_t rx_usb_rx_push(const uint8_t* data, uint32_t len);
extern uint32_t rx_usb_tx_pop(uint8_t* data, uint32_t max_len);

/* =============================================================================
 * Private Functions
 * =============================================================================
 */

/**
 * @brief Send descriptor in response to GET_DESCRIPTOR request
 */
static void internal_send_descriptor(const uint8_t* desc, uint16_t desc_len, uint16_t requested_len)
{
  uint16_t len = (desc_len < requested_len) ? desc_len : requested_len;

  rx_usb_hw_fifo_write(k_usb_pipe_0, desc, len);
}

/**
 * @brief Handle GET_DESCRIPTOR request
 */
static void internal_handle_get_descriptor(uint16_t w_value, uint16_t w_length)
{
  uint8_t desc_type  = (w_value >> k_bit_shift_byte_1) & k_byte_mask;
  uint8_t desc_index = w_value & k_byte_mask;

  switch (desc_type) {
    case k_usb_desc_type_device:
      internal_send_descriptor((const uint8_t*)&s_device_desc, sizeof(s_device_desc), w_length);
      break;

    case k_usb_desc_type_configuration:
      internal_send_descriptor((const uint8_t*)&s_config_desc, sizeof(s_config_desc), w_length);
      break;

    case k_usb_desc_type_string:
      switch (desc_index) {
        case k_usb_string_index_langid:
          internal_send_descriptor((const uint8_t*)&s_string_langid,
                                   sizeof(s_string_langid),
                                   w_length);
          break;
        case k_usb_string_index_manufacturer:
          internal_send_descriptor((const uint8_t*)&s_string_manufacturer,
                                   sizeof(s_string_manufacturer),
                                   w_length);
          break;
        case k_usb_string_index_product:
          internal_send_descriptor((const uint8_t*)&s_string_product,
                                   sizeof(s_string_product),
                                   w_length);
          break;
        case k_usb_string_index_serial_number:
          internal_send_descriptor((const uint8_t*)&s_string_serial,
                                   sizeof(s_string_serial),
                                   w_length);
          break;
        default:
          /* STALL for unknown string index */
          USB0.dcpctr |= k_usb_dcpctr_pid_stall;
          break;
      }
      break;

    default:
      /* STALL for unknown descriptor type */
      USB0.dcpctr |= k_usb_dcpctr_pid_stall;
      break;
  }
}

/**
 * @brief Handle SET_ADDRESS request
 */
static void internal_handle_set_address(uint16_t w_value)
{
  uint8_t address = w_value & k_usb_address_mask;

  /* Send zero-length status packet first */
  USB0.dcpctr |= k_usb_dcpctr_ccpl;

  /* Then set the address */
  rx_usb_hw_set_address(address);

  if (address != k_usb_config_unconfigured) {
    rx_usb_set_state(k_usb_state_addressed);
  }

  rx_log_debug(s_tag, "Address set");
}

/**
 * @brief Handle SET_CONFIGURATION request
 */
static void internal_handle_set_configuration(uint16_t w_value)
{
  uint8_t config = w_value & k_byte_mask;

  if (config == k_usb_config_value_1) {
    /* Configure data endpoints */
    /* Pipe 1: Bulk IN (EP1) */
    rx_usb_hw_configure_pipe(k_usb_pipe_1,
                             k_usb_pipe_1,
                             true,
                             k_usb_pipecfg_type_bulk,
                             k_usb_bulk_packet_size);

    /* Pipe 2: Bulk OUT (EP2) */
    rx_usb_hw_configure_pipe(k_usb_pipe_2,
                             k_usb_pipe_2,
                             false,
                             k_usb_pipecfg_type_bulk,
                             k_usb_bulk_packet_size);

    /* Pipe 3: Interrupt IN (EP3) */
    rx_usb_hw_configure_pipe(k_usb_pipe_3,
                             k_usb_pipe_3,
                             true,
                             k_usb_pipecfg_type_int,
                             k_usb_interrupt_packet_size);

    /* Enable BRDY interrupt for Pipe 2 (receive from host) */
    USB0.brdyenb |= (1 << k_usb_pipe_2);

    /* Enable BEMP interrupt for Pipe 1 (transmit complete) */
    USB0.bempenb |= (1 << k_usb_pipe_1);

    rx_usb_set_state(k_usb_state_configured);
    rx_log_info(s_tag, "Device configured");
  } else if (config == k_usb_config_unconfigured) {
    rx_usb_set_state(k_usb_state_addressed);
  }

  /* Send zero-length status packet */
  USB0.dcpctr |= k_usb_dcpctr_ccpl;
}

/**
 * @brief Handle CDC SET_LINE_CODING request
 */
static void internal_handle_set_line_coding(void)
{
  uint8_t data[k_line_coding_size];

  /* Read line coding data from FIFO */
  uint32_t len = rx_usb_hw_fifo_read(k_usb_pipe_0, data, k_line_coding_size);

  if (len == k_line_coding_size) {
    s_line_coding.baud_rate =
      (uint32_t)data[k_line_coding_baud_rate_byte_0] |
      ((uint32_t)data[k_line_coding_baud_rate_byte_1] << k_bit_shift_byte_1) |
      ((uint32_t)data[k_line_coding_baud_rate_byte_2] << k_bit_shift_byte_2) |
      ((uint32_t)data[k_line_coding_baud_rate_byte_3] << k_bit_shift_byte_3);
    s_line_coding.stop_bits = data[k_line_coding_stop_bits_index];
    s_line_coding.parity    = data[k_line_coding_parity_index];
    s_line_coding.data_bits = data[k_line_coding_data_bits_index];

    rx_usb_set_line_coding(&s_line_coding);

    rx_log_debug(s_tag, "Line coding set");
  }

  /* Send zero-length status packet */
  USB0.dcpctr |= k_usb_dcpctr_ccpl;
}

/**
 * @brief Handle CDC GET_LINE_CODING request
 */
static void internal_handle_get_line_coding(void)
{
  uint8_t data[k_line_coding_size];

  data[k_line_coding_baud_rate_byte_0] =
    (s_line_coding.baud_rate >> k_bit_shift_byte_0) & k_byte_mask;
  data[k_line_coding_baud_rate_byte_1] =
    (s_line_coding.baud_rate >> k_bit_shift_byte_1) & k_byte_mask;
  data[k_line_coding_baud_rate_byte_2] =
    (s_line_coding.baud_rate >> k_bit_shift_byte_2) & k_byte_mask;
  data[k_line_coding_baud_rate_byte_3] =
    (s_line_coding.baud_rate >> k_bit_shift_byte_3) & k_byte_mask;
  data[k_line_coding_stop_bits_index] = s_line_coding.stop_bits;
  data[k_line_coding_parity_index]    = s_line_coding.parity;
  data[k_line_coding_data_bits_index] = s_line_coding.data_bits;

  rx_usb_hw_fifo_write(k_usb_pipe_0, data, k_line_coding_size);
}

/**
 * @brief Handle CDC SET_CONTROL_LINE_STATE request
 */
static void internal_handle_set_control_line_state(uint16_t w_value)
{
  s_control_line_state = w_value;

  /* DTR (bit 0) and RTS (bit 1) */
  rx_log_debug(s_tag, "Control line state set");

  /* Send zero-length status packet */
  USB0.dcpctr |= k_usb_dcpctr_ccpl;
}

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Initialize CDC class
 */
rx_err_t rx_usb_cdc_init(void)
{
  if (s_cdc_initialized) {
    return k_rx_ok;
  }

  rx_log_debug(s_tag, "Initializing USB CDC class");

  /* Reset line coding to defaults */
  s_line_coding.baud_rate = k_usb_default_baud_rate;
  s_line_coding.stop_bits = k_usb_default_stop_bits;
  s_line_coding.parity    = k_usb_default_parity;
  s_line_coding.data_bits = k_usb_default_data_bits;

  s_control_line_state = k_usb_config_unconfigured;
  s_cdc_initialized    = true;

  return k_rx_ok;
}

/**
 * @brief Handle control transfer (SETUP packet)
 *
 * Called by ISR when a SETUP packet is received on EP0.
 */
void rx_usb_cdc_handle_setup(void)
{
  /* Read SETUP packet from registers */
  uint16_t bm_request_type_b_request = USB0.usbreq;
  uint16_t w_value                   = USB0.usbval;
  uint16_t w_index                   = USB0.usbindx;
  uint16_t w_length                  = USB0.usbleng;

  uint8_t bm_request_type = bm_request_type_b_request & k_byte_mask;
  uint8_t b_request       = (bm_request_type_b_request >> k_bit_shift_byte_1) & k_byte_mask;

  (void)w_index; /* Currently unused */

  /* Determine request type */
  uint8_t type = bm_request_type & k_usb_req_type_mask;

  if (type == k_usb_req_type_standard) {
    /* Standard request */
    switch (b_request) {
      case k_usb_req_get_descriptor:
        internal_handle_get_descriptor(w_value, w_length);
        break;

      case k_usb_req_set_address:
        internal_handle_set_address(w_value);
        break;

      case k_usb_req_set_configuration:
        internal_handle_set_configuration(w_value);
        break;

      case k_usb_req_get_configuration: {
        /* Return current configuration (1 = configured, 0 = not) */
        uint8_t cfg = (rx_usb_get_state() == k_usb_state_configured) ? k_usb_config_value_1
                                                                     : k_usb_config_unconfigured;
        rx_usb_hw_fifo_write(k_usb_pipe_0, &cfg, sizeof(cfg));
      } break;

      default:
        /* STALL unknown standard requests */
        USB0.dcpctr |= k_usb_dcpctr_pid_stall;
        break;
    }
  } else if (type == k_usb_req_type_class) {
    /* Class request (CDC) */
    switch (b_request) {
      case k_cdc_set_line_coding:
        internal_handle_set_line_coding();
        break;

      case k_cdc_get_line_coding:
        internal_handle_get_line_coding();
        break;

      case k_cdc_set_control_line_state:
        internal_handle_set_control_line_state(w_value);
        break;

      default:
        /* STALL unknown class requests */
        USB0.dcpctr |= k_usb_dcpctr_pid_stall;
        break;
    }
  } else {
    /* STALL vendor and reserved requests */
    USB0.dcpctr |= k_usb_dcpctr_pid_stall;
  }
}

/**
 * @brief Handle bulk OUT data (received from host)
 *
 * Called by ISR when data is received on Pipe 2 (EP2 OUT).
 */
void rx_usb_cdc_handle_bulk_out(void)
{
  uint8_t  data[k_usb_bulk_packet_size];
  uint32_t len = rx_usb_hw_fifo_read(k_usb_pipe_2, data, k_usb_bulk_packet_size);

  if (len > k_usb_config_unconfigured) {
    rx_usb_rx_push(data, len);
  }
}

/**
 * @brief Handle bulk IN (transmit to host)
 *
 * Called by ISR when Pipe 1 (EP1 IN) buffer is empty.
 */
void rx_usb_cdc_handle_bulk_in(void)
{
  uint8_t  data[k_usb_bulk_packet_size];
  uint32_t len = rx_usb_tx_pop(data, k_usb_bulk_packet_size);

  if (len > k_usb_config_unconfigured) {
    rx_usb_hw_fifo_write(k_usb_pipe_1, data, len);
  }
}
