/**
 * @file rx_usb_cdc.c
 * @brief USB CDC-ACM Class Implementation for RX72N
 *
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

/* USB Descriptor Types */
#define USB_DESC_TYPE_DEVICE        0x01
#define USB_DESC_TYPE_CONFIGURATION 0x02
#define USB_DESC_TYPE_STRING        0x03
#define USB_DESC_TYPE_INTERFACE     0x04
#define USB_DESC_TYPE_ENDPOINT      0x05
#define USB_DESC_TYPE_CS_INTERFACE  0x24

/* USB Class Codes */
#define USB_CLASS_CDC      0x02
#define USB_CLASS_CDC_DATA 0x0A
#define USB_SUBCLASS_ACM   0x02
#define USB_PROTOCOL_AT    0x01

/* CDC Class Request Codes */
#define CDC_SET_LINE_CODING        0x20
#define CDC_GET_LINE_CODING        0x21
#define CDC_SET_CONTROL_LINE_STATE 0x22

/* USB Standard Request Codes */
#define USB_REQ_GET_STATUS        0x00
#define USB_REQ_CLEAR_FEATURE     0x01
#define USB_REQ_SET_FEATURE       0x03
#define USB_REQ_SET_ADDRESS       0x05
#define USB_REQ_GET_DESCRIPTOR    0x06
#define USB_REQ_SET_DESCRIPTOR    0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09
#define USB_REQ_GET_INTERFACE     0x0A
#define USB_REQ_SET_INTERFACE     0x0B

/* Vendor ID and Product ID */
/* Using Renesas test VID/PID - should be changed for production */
#define USB_VID 0x045B /* Renesas Electronics */
#define USB_PID 0x0234 /* CDC-ACM device */

/* =============================================================================
 * USB Descriptors
 * =============================================================================
 */

/* Device Descriptor */
static const uint8_t s_device_desc[] = {
  18,                   /* bLength */
  USB_DESC_TYPE_DEVICE, /* bDescriptorType */
  0x00,
  0x02,             /* bcdUSB = 2.00 */
  USB_CLASS_CDC,    /* bDeviceClass */
  0x00,             /* bDeviceSubClass */
  0x00,             /* bDeviceProtocol */
  64,               /* bMaxPacketSize0 */
  (USB_VID & 0xFF), /* idVendor (low) */
  (USB_VID >> 8),   /* idVendor (high) */
  (USB_PID & 0xFF), /* idProduct (low) */
  (USB_PID >> 8),   /* idProduct (high) */
  0x00,
  0x01, /* bcdDevice = 1.00 */
  1,    /* iManufacturer */
  2,    /* iProduct */
  3,    /* iSerialNumber */
  1     /* bNumConfigurations */
};

/* Configuration Descriptor (includes interface, CDC, and endpoint descriptors) */
static const uint8_t s_config_desc[] = {
  /* Configuration Descriptor */
  9,                           /* bLength */
  USB_DESC_TYPE_CONFIGURATION, /* bDescriptorType */
  67,
  0,    /* wTotalLength (67 bytes) */
  2,    /* bNumInterfaces */
  1,    /* bConfigurationValue */
  0,    /* iConfiguration */
  0x80, /* bmAttributes (bus powered) */
  50,   /* bMaxPower (100mA) */

  /* Interface 0: CDC Control Interface */
  9,                       /* bLength */
  USB_DESC_TYPE_INTERFACE, /* bDescriptorType */
  0,                       /* bInterfaceNumber */
  0,                       /* bAlternateSetting */
  1,                       /* bNumEndpoints */
  USB_CLASS_CDC,           /* bInterfaceClass */
  USB_SUBCLASS_ACM,        /* bInterfaceSubClass */
  USB_PROTOCOL_AT,         /* bInterfaceProtocol */
  0,                       /* iInterface */

  /* CDC Header Functional Descriptor */
  5,                          /* bLength */
  USB_DESC_TYPE_CS_INTERFACE, /* bDescriptorType */
  0x00,                       /* bDescriptorSubtype (Header) */
  0x10,
  0x01, /* bcdCDC = 1.10 */

  /* CDC Call Management Functional Descriptor */
  5,                          /* bLength */
  USB_DESC_TYPE_CS_INTERFACE, /* bDescriptorType */
  0x01,                       /* bDescriptorSubtype (Call Management) */
  0x00,                       /* bmCapabilities */
  1,                          /* bDataInterface */

  /* CDC ACM Functional Descriptor */
  4,                          /* bLength */
  USB_DESC_TYPE_CS_INTERFACE, /* bDescriptorType */
  0x02,                       /* bDescriptorSubtype (ACM) */
  0x02,                       /* bmCapabilities (line coding, serial state) */

  /* CDC Union Functional Descriptor */
  5,                          /* bLength */
  USB_DESC_TYPE_CS_INTERFACE, /* bDescriptorType */
  0x06,                       /* bDescriptorSubtype (Union) */
  0,                          /* bControlInterface */
  1,                          /* bSubordinateInterface0 */

  /* Endpoint 3: Interrupt IN (notifications) */
  7,                      /* bLength */
  USB_DESC_TYPE_ENDPOINT, /* bDescriptorType */
  0x83,                   /* bEndpointAddress (EP3 IN) */
  0x03,                   /* bmAttributes (Interrupt) */
  8,
  0,  /* wMaxPacketSize */
  10, /* bInterval (10ms) */

  /* Interface 1: CDC Data Interface */
  9,                       /* bLength */
  USB_DESC_TYPE_INTERFACE, /* bDescriptorType */
  1,                       /* bInterfaceNumber */
  0,                       /* bAlternateSetting */
  2,                       /* bNumEndpoints */
  USB_CLASS_CDC_DATA,      /* bInterfaceClass */
  0,                       /* bInterfaceSubClass */
  0,                       /* bInterfaceProtocol */
  0,                       /* iInterface */

  /* Endpoint 1: Bulk IN (data to host) */
  7,                      /* bLength */
  USB_DESC_TYPE_ENDPOINT, /* bDescriptorType */
  0x81,                   /* bEndpointAddress (EP1 IN) */
  0x02,                   /* bmAttributes (Bulk) */
  64,
  0, /* wMaxPacketSize */
  0, /* bInterval */

  /* Endpoint 2: Bulk OUT (data from host) */
  7,                      /* bLength */
  USB_DESC_TYPE_ENDPOINT, /* bDescriptorType */
  0x02,                   /* bEndpointAddress (EP2 OUT) */
  0x02,                   /* bmAttributes (Bulk) */
  64,
  0, /* wMaxPacketSize */
  0  /* bInterval */
};

/* String Descriptor 0: Language ID */
static const uint8_t s_string_langid[] = {
  4,                    /* bLength */
  USB_DESC_TYPE_STRING, /* bDescriptorType */
  0x09,
  0x04 /* wLANGID (English US) */
};

/* String Descriptor 1: Manufacturer */
static const uint8_t s_string_manufacturer[] = {16,                   /* bLength */
                                                USB_DESC_TYPE_STRING, /* bDescriptorType */
                                                'R',
                                                0,
                                                'e',
                                                0,
                                                'n',
                                                0,
                                                'e',
                                                0,
                                                's',
                                                0,
                                                'a',
                                                0,
                                                's',
                                                0};

/* String Descriptor 2: Product */
static const uint8_t s_string_product[] = {28,                   /* bLength */
                                           USB_DESC_TYPE_STRING, /* bDescriptorType */
                                           'S',
                                           0,
                                           'T',
                                           0,
                                           'A',
                                           0,
                                           'R',
                                           0,
                                           ' ',
                                           0,
                                           'R',
                                           0,
                                           'X',
                                           0,
                                           '7',
                                           0,
                                           '2',
                                           0,
                                           'N',
                                           0,
                                           ' ',
                                           0,
                                           'C',
                                           0,
                                           'D',
                                           0,
                                           'C',
                                           0};

/* String Descriptor 3: Serial Number */
static const uint8_t s_string_serial[] = {18,                   /* bLength */
                                          USB_DESC_TYPE_STRING, /* bDescriptorType */
                                          '0',
                                          0,
                                          '0',
                                          0,
                                          '0',
                                          0,
                                          '0',
                                          0,
                                          '0',
                                          0,
                                          '0',
                                          0,
                                          '0',
                                          0,
                                          '1',
                                          0};

/* =============================================================================
 * Private Variables
 * =============================================================================
 */

static bool s_cdc_initialized = false;

/* Current line coding from host */
static rx_usb_line_coding_t s_line_coding = {.baud_rate = 115200,
                                             .stop_bits = 0,
                                             .parity    = 0,
                                             .data_bits = 8};

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

  rx_usb_hw_fifo_write(0, desc, len);
}

/**
 * @brief Handle GET_DESCRIPTOR request
 */
static void internal_handle_get_descriptor(uint16_t wValue, uint16_t wLength)
{
  uint8_t desc_type  = (wValue >> 8) & 0xFF;
  uint8_t desc_index = wValue & 0xFF;

  switch (desc_type) {
    case USB_DESC_TYPE_DEVICE:
      internal_send_descriptor(s_device_desc, sizeof(s_device_desc), wLength);
      break;

    case USB_DESC_TYPE_CONFIGURATION:
      internal_send_descriptor(s_config_desc, sizeof(s_config_desc), wLength);
      break;

    case USB_DESC_TYPE_STRING:
      switch (desc_index) {
        case 0:
          internal_send_descriptor(s_string_langid, sizeof(s_string_langid), wLength);
          break;
        case 1:
          internal_send_descriptor(s_string_manufacturer, sizeof(s_string_manufacturer), wLength);
          break;
        case 2:
          internal_send_descriptor(s_string_product, sizeof(s_string_product), wLength);
          break;
        case 3:
          internal_send_descriptor(s_string_serial, sizeof(s_string_serial), wLength);
          break;
        default:
          /* STALL for unknown string index */
          USB0.DCPCTR |= k_usb_dcpctr_pid_stall;
          break;
      }
      break;

    default:
      /* STALL for unknown descriptor type */
      USB0.DCPCTR |= k_usb_dcpctr_pid_stall;
      break;
  }
}

/**
 * @brief Handle SET_ADDRESS request
 */
static void internal_handle_set_address(uint16_t wValue)
{
  uint8_t address = wValue & 0x7F;

  /* Send zero-length status packet first */
  USB0.DCPCTR |= k_usb_dcpctr_ccpl;

  /* Then set the address */
  rx_usb_hw_set_address(address);

  if (address != 0) {
    rx_usb_set_state(k_usb_state_addressed);
  }

  RX_LOG_DEBUG(s_tag, "Address set");
}

/**
 * @brief Handle SET_CONFIGURATION request
 */
static void internal_handle_set_configuration(uint16_t wValue)
{
  uint8_t config = wValue & 0xFF;

  if (config == 1) {
    /* Configure data endpoints */
    /* Pipe 1: Bulk IN (EP1) */
    rx_usb_hw_configure_pipe(1, 1, true, k_usb_pipecfg_type_bulk, 64);

    /* Pipe 2: Bulk OUT (EP2) */
    rx_usb_hw_configure_pipe(2, 2, false, k_usb_pipecfg_type_bulk, 64);

    /* Pipe 3: Interrupt IN (EP3) */
    rx_usb_hw_configure_pipe(3, 3, true, k_usb_pipecfg_type_int, 8);

    /* Enable BRDY interrupt for Pipe 2 (receive from host) */
    USB0.BRDYENB |= (1 << 2);

    /* Enable BEMP interrupt for Pipe 1 (transmit complete) */
    USB0.BEMPENB |= (1 << 1);

    rx_usb_set_state(k_usb_state_configured);
    RX_LOG_INFO(s_tag, "Device configured");
  } else if (config == 0) {
    rx_usb_set_state(k_usb_state_addressed);
  }

  /* Send zero-length status packet */
  USB0.DCPCTR |= k_usb_dcpctr_ccpl;
}

/**
 * @brief Handle CDC SET_LINE_CODING request
 */
static void internal_handle_set_line_coding(void)
{
  uint8_t data[7];

  /* Read 7 bytes of line coding data from FIFO */
  uint32_t len = rx_usb_hw_fifo_read(0, data, 7);

  if (len == 7) {
    s_line_coding.baud_rate = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
                              ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    s_line_coding.stop_bits = data[4];
    s_line_coding.parity    = data[5];
    s_line_coding.data_bits = data[6];

    rx_usb_set_line_coding(&s_line_coding);

    RX_LOG_DEBUG(s_tag, "Line coding set");
  }

  /* Send zero-length status packet */
  USB0.DCPCTR |= k_usb_dcpctr_ccpl;
}

/**
 * @brief Handle CDC GET_LINE_CODING request
 */
static void internal_handle_get_line_coding(void)
{
  uint8_t data[7];

  data[0] = (s_line_coding.baud_rate >> 0) & 0xFF;
  data[1] = (s_line_coding.baud_rate >> 8) & 0xFF;
  data[2] = (s_line_coding.baud_rate >> 16) & 0xFF;
  data[3] = (s_line_coding.baud_rate >> 24) & 0xFF;
  data[4] = s_line_coding.stop_bits;
  data[5] = s_line_coding.parity;
  data[6] = s_line_coding.data_bits;

  rx_usb_hw_fifo_write(0, data, 7);
}

/**
 * @brief Handle CDC SET_CONTROL_LINE_STATE request
 */
static void internal_handle_set_control_line_state(uint16_t wValue)
{
  s_control_line_state = wValue;

  /* DTR (bit 0) and RTS (bit 1) */
  RX_LOG_DEBUG(s_tag, "Control line state set");

  /* Send zero-length status packet */
  USB0.DCPCTR |= k_usb_dcpctr_ccpl;
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

  RX_LOG_DEBUG(s_tag, "Initializing USB CDC class");

  /* Reset line coding to defaults */
  s_line_coding.baud_rate = 115200;
  s_line_coding.stop_bits = 0;
  s_line_coding.parity    = 0;
  s_line_coding.data_bits = 8;

  s_control_line_state = 0;
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
  uint16_t bmRequestType_bRequest = USB0.USBREQ;
  uint16_t wValue                 = USB0.USBVAL;
  uint16_t wIndex                 = USB0.USBINDX;
  uint16_t wLength                = USB0.USBLENG;

  uint8_t bmRequestType = bmRequestType_bRequest & 0xFF;
  uint8_t bRequest      = (bmRequestType_bRequest >> 8) & 0xFF;

  (void)wIndex; /* Currently unused */

  /* Determine request type */
  uint8_t type = bmRequestType & 0x60; /* Bits 5-6: type */

  if (type == 0x00) {
    /* Standard request */
    switch (bRequest) {
      case USB_REQ_GET_DESCRIPTOR:
        internal_handle_get_descriptor(wValue, wLength);
        break;

      case USB_REQ_SET_ADDRESS:
        internal_handle_set_address(wValue);
        break;

      case USB_REQ_SET_CONFIGURATION:
        internal_handle_set_configuration(wValue);
        break;

      case USB_REQ_GET_CONFIGURATION:
        /* Return current configuration (1 = configured, 0 = not) */
        {
          uint8_t cfg = (rx_usb_get_state() == k_usb_state_configured) ? 1 : 0;
          rx_usb_hw_fifo_write(0, &cfg, 1);
        }
        break;

      default:
        /* STALL unknown standard requests */
        USB0.DCPCTR |= k_usb_dcpctr_pid_stall;
        break;
    }
  } else if (type == 0x20) {
    /* Class request (CDC) */
    switch (bRequest) {
      case CDC_SET_LINE_CODING:
        internal_handle_set_line_coding();
        break;

      case CDC_GET_LINE_CODING:
        internal_handle_get_line_coding();
        break;

      case CDC_SET_CONTROL_LINE_STATE:
        internal_handle_set_control_line_state(wValue);
        break;

      default:
        /* STALL unknown class requests */
        USB0.DCPCTR |= k_usb_dcpctr_pid_stall;
        break;
    }
  } else {
    /* STALL vendor and reserved requests */
    USB0.DCPCTR |= k_usb_dcpctr_pid_stall;
  }
}

/**
 * @brief Handle bulk OUT data (received from host)
 *
 * Called by ISR when data is received on Pipe 2 (EP2 OUT).
 */
void rx_usb_cdc_handle_bulk_out(void)
{
  uint8_t  data[64];
  uint32_t len = rx_usb_hw_fifo_read(2, data, 64);

  if (len > 0) {
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
  uint8_t  data[64];
  uint32_t len = rx_usb_tx_pop(data, 64);

  if (len > 0) {
    rx_usb_hw_fifo_write(1, data, len);
  }
}
