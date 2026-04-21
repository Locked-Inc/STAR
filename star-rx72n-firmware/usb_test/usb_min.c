/**
 * @file usb_min.c
 * @brief Minimal hand-coded USB device for RX72N to isolate rx_usb bugs.
 *
 * @details
 * Bypasses libs/rx_usb entirely.  Implements just enough to enumerate as a
 * vendor-class device (class 0xFF) with one dummy interface and only the
 * Default Control Pipe (EP0).  The descriptors are hardcoded const arrays
 * so we can rule out any struct-packing or descriptor-table bugs.
 *
 * Goal: get past GET_DESCRIPTOR(DEVICE), GET_DESCRIPTOR(CONFIG), SET_ADDRESS,
 * SET_CONFIGURATION on a real Linux host.  If this works we know the RX72N
 * hardware bring-up is correct and the bug is somewhere in libs/rx_usb.
 *
 * The four USB0 ICU vectors (34/35/36/90) are wired in vectors.S to:
 *   usb0_d0fifo_isr   (34)  -- empty stub
 *   usb0_d1fifo_isr   (35)  -- empty stub
 *   usb0_usbi_isr     (36)  -- the real handler in this file
 *   usb0_usbr_isr     (90)  -- empty stub
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

#include "rx72n_regs.h"
#include "rx_register_protection.h"

/* ==========================================================================
 * USB0 register field constants used here.  Names mirror the RX72N HW manual.
 * ========================================================================== */
typedef enum : uint16_t {
    k_syscfg_usbe  = (1U << 0),
    k_syscfg_dprpu = (1U << 4),
    k_syscfg_dcfm  = (1U << 6),
    k_syscfg_scke  = (1U << 10),
} syscfg_bits_t;

typedef enum : uint16_t {
    k_intenb0_brdy = (1U << 8),
    k_intenb0_bemp = (1U << 10),
    k_intenb0_ctre = (1U << 11),
    k_intenb0_dvse = (1U << 12),
} intenb0_bits_t;

typedef enum : uint16_t {
    k_intsts0_ctsq_mask    = 0x0007,
    k_intsts0_ctsq_idle    = 0x0000,
    k_intsts0_ctsq_rd_data = 0x0001,
    k_intsts0_ctsq_wr_data = 0x0003,
    k_intsts0_ctsq_wr_nd   = 0x0005,
    k_intsts0_valid        = (1U << 3),
    k_intsts0_brdy         = (1U << 8),
    k_intsts0_nrdy         = (1U << 9),
    k_intsts0_bemp         = (1U << 10),
    k_intsts0_ctrt         = (1U << 11),
    k_intsts0_dvst         = (1U << 12),
} intsts0_bits_t;

typedef enum : uint16_t {
    k_dcpctr_pid_buf   = 0x0001,
    k_dcpctr_pid_stall = 0x0002,
    k_dcpctr_ccpl      = (1U << 2),
} dcpctr_bits_t;

typedef enum : uint16_t {
    k_cfifosel_isel    = (1U << 5),
    k_cfifosel_mbw_16  = (1U << 10),
    k_fifoctr_frdy     = (1U << 13),
    k_fifoctr_bclr     = (1U << 14),
    k_fifoctr_bval     = (1U << 15),
} fifo_bits_t;

typedef enum : uint8_t {
    k_min_pipe_dcp   = 0,
    k_min_max_pkt_fs = 64,
} usb_min_const_t;

typedef enum : uint8_t {
    k_bm_request_type_in = 0x80,
    k_breq_get_status    = 0x00,
    k_breq_set_address   = 0x05,
    k_breq_get_descriptor = 0x06,
    k_breq_set_config    = 0x09,
    k_desc_type_device   = 0x01,
    k_desc_type_config   = 0x02,
    k_desc_type_string   = 0x03,
} usb_req_t;

typedef enum : uint16_t {
    k_settle_loops = 10000U,
} settle_t;

/* ==========================================================================
 * Hardcoded device + config descriptors.  Vendor-class device, one dummy
 * interface, no extra endpoints, bus-powered, 100 mA.  pid.codes test VID/PID.
 * ========================================================================== */
/* Real USB 2.0 device descriptor.  pid.codes test VID/PID (1209:0001).
 * Vendor-class device, 64-byte max packet on EP0 (full-speed). */
static const uint8_t s_device_desc[18] = {
    0x12,           /* bLength = 18                                      */
    0x01,           /* bDescriptorType = DEVICE                          */
    0x00, 0x02,     /* bcdUSB = 2.00                                     */
    0xFF,           /* bDeviceClass = vendor-specific                    */
    0x00,           /* bDeviceSubClass                                   */
    0x00,           /* bDeviceProtocol                                   */
    0x40,           /* bMaxPacketSize0 = 64                              */
    0x09, 0x12,     /* idVendor = 0x1209 (pid.codes)                    */
    0x01, 0x00,     /* idProduct = 0x0001 (pid.codes test PID)           */
    0x00, 0x01,     /* bcdDevice = 1.00                                  */
    0x00,           /* iManufacturer = 0 (no string)                     */
    0x00,           /* iProduct = 0 (no string)                          */
    0x00,           /* iSerialNumber = 0 (no string)                     */
    0x01,           /* bNumConfigurations = 1                            */
};

static const uint8_t s_config_desc[18] = {
    /* Configuration descriptor (9 bytes) */
    0x09,           /* bLength = 9                                       */
    0x02,           /* bDescriptorType = CONFIGURATION                   */
    0x12, 0x00,     /* wTotalLength = 18                                 */
    0x01,           /* bNumInterfaces = 1                                */
    0x01,           /* bConfigurationValue = 1                           */
    0x00,           /* iConfiguration                                    */
    0x80,           /* bmAttributes = bus-powered                        */
    0x32,           /* bMaxPower = 100 mA (50 * 2 mA)                    */

    /* Interface descriptor (9 bytes) -- one dummy vendor interface */
    0x09,           /* bLength = 9                                       */
    0x04,           /* bDescriptorType = INTERFACE                       */
    0x00,           /* bInterfaceNumber = 0                              */
    0x00,           /* bAlternateSetting = 0                             */
    0x00,           /* bNumEndpoints = 0 (only EP0)                      */
    0xFF,           /* bInterfaceClass = vendor                          */
    0xFF,           /* bInterfaceSubClass                                */
    0xFF,           /* bInterfaceProtocol                                */
    0x00,           /* iInterface                                        */
};

/* ==========================================================================
 * Diagnostic counters -- bumped by the ISR so we could read them via a
 * debugger if we had one.  Kept volatile so the compiler can't optimise
 * them away.
 * ========================================================================== */
volatile uint32_t g_usb_isr_count   = 0U;
volatile uint32_t g_usb_setup_count = 0U;
volatile uint32_t g_usb_dvst_count  = 0U;
volatile uint16_t g_usb_last_intsts = 0U;
volatile uint16_t g_usb_last_breq   = 0U;

/* ==========================================================================
 * Helper: write a buffer to the DCP CFIFO and mark it valid for transmit.
 * ========================================================================== */
static void cfifo_write_dcp(const uint8_t *data, uint16_t len)
{
    /* Select DCP for write (ISEL=1), 16-bit access width. */
    usb0()->cfifosel = ((uint16_t)k_min_pipe_dcp & 0x000FU) | k_cfifosel_isel | k_cfifosel_mbw_16;

    /* Wait for FIFO ready */
    while ((usb0()->cfifoctr & (uint16_t)k_fifoctr_frdy) == 0U) {
        /* spin */
    }

    /* Clear any stale buffer contents */
    usb0()->cfifoctr |= (uint16_t)k_fifoctr_bclr;
    while ((usb0()->cfifoctr & (uint16_t)k_fifoctr_bclr) != 0U) {
        /* spin -- hardware self-clears BCLR when done */
    }

    /* Write data as 16-bit words (CFIFO is 16-bit). */
    for (uint16_t i = 0U; i < len; i += 2U) {
        uint16_t w = (uint16_t)data[i];
        if ((uint16_t)(i + 1U) < len) {
            w |= ((uint16_t)data[i + 1U]) << 8U;
        }
        usb0()->cfifo = w;
    }

    /* Mark buffer valid -- hardware sends on next IN token */
    usb0()->cfifoctr |= (uint16_t)k_fifoctr_bval;
}

/* ==========================================================================
 * SETUP handler -- called from the ISR after CTSQ transitions to a data
 * or no-data stage and we've cleared VALID.
 * ========================================================================== */
static void handle_setup(void)
{
    const uint16_t bmreq_breq = usb0()->usbreq;
    const uint16_t wval       = usb0()->usbval;
    const uint16_t windex     = usb0()->usbindx;
    const uint16_t wlen       = usb0()->usbleng;

    (void)windex;

    const uint8_t bmRequestType = (uint8_t)(bmreq_breq & 0xFFU);
    const uint8_t bRequest      = (uint8_t)((bmreq_breq >> 8U) & 0xFFU);

    g_usb_setup_count++;
    g_usb_last_breq = bmreq_breq;

    (void)bmRequestType;

    switch (bRequest) {
        case (uint8_t)k_breq_get_descriptor: {
            const uint8_t desc_type = (uint8_t)((wval >> 8U) & 0xFFU);
            if (desc_type == (uint8_t)k_desc_type_device) {
                uint16_t send = (wlen < (uint16_t)sizeof s_device_desc)
                                  ? wlen
                                  : (uint16_t)sizeof s_device_desc;
                cfifo_write_dcp(s_device_desc, send);
            }
            else if (desc_type == (uint8_t)k_desc_type_config) {
                uint16_t send = (wlen < (uint16_t)sizeof s_config_desc)
                                  ? wlen
                                  : (uint16_t)sizeof s_config_desc;
                cfifo_write_dcp(s_config_desc, send);
            }
            else {
                /* String descriptors etc. -- STALL */
                usb0()->dcpctr = (uint16_t)((usb0()->dcpctr & ~(uint16_t)0x0003U)
                                            | (uint16_t)k_dcpctr_pid_stall);
            }
            break;
        }

        case (uint8_t)k_breq_set_address: {
            const uint8_t address = (uint8_t)(wval & 0x7FU);
            usb0()->usbaddr = (uint16_t)address;
            usb0()->dcpctr |= (uint16_t)k_dcpctr_ccpl;
            break;
        }

        case (uint8_t)k_breq_set_config: {
            /* No data stage, just complete the status stage. */
            usb0()->dcpctr |= (uint16_t)k_dcpctr_ccpl;
            break;
        }

        case (uint8_t)k_breq_get_status: {
            /* Return two bytes of zeros: not self-powered, no remote wakeup. */
            static const uint8_t zeros[2] = { 0x00U, 0x00U };
            cfifo_write_dcp(zeros, 2U);
            break;
        }

        default:
            usb0()->dcpctr = (uint16_t)((usb0()->dcpctr & ~(uint16_t)0x0003U)
                                        | (uint16_t)k_dcpctr_pid_stall);
            break;
    }
}

/* ==========================================================================
 * Public init -- call from a ThreadX task (uses no sleeps so safe from main()
 * too).
 * ========================================================================== */
void usb_min_init(void)
{
    /* 1. Release USB0 from MSTP. */
    *prcr_reg() = k_rx_prcr_unlock_prc1;
    system_regs()->mstpcrb &= ~(uint32_t)(1UL << (uint32_t)k_mstpb_usb0);
    *prcr_reg() = k_rx_prcr_lock;

    /* 2. Disable everything in SYSCFG so we start from a clean state. */
    usb0()->syscfg = 0x0000U;

    for (volatile uint32_t i = 0U; i < (uint32_t)k_settle_loops; i++) {
        __asm__ volatile("nop");
    }

    /* 3. Enable USBE first, then SCKE.  Both orders enumerate on this
     *    silicon (manual Ch40 and tinyusb prefer SCKE-first; our proven
     *    hoco_pid_fix.c reference uses USBE-first).  Keep USBE-first so
     *    this minimal path exactly mirrors the known-good polling repro. */
    usb0()->syscfg |= (uint16_t)k_syscfg_usbe;
    usb0()->syscfg |= (uint16_t)k_syscfg_scke;

    for (volatile uint32_t i = 0U; i < (uint32_t)k_settle_loops; i++) {
        __asm__ volatile("nop");
    }

    /* 3b. RX72N PHY housekeeping (tinyusb renesas/usba dcd_usba.c:626-628).
     *     Must run between USBE=1 and INTENB0 -- see
     *     libs/rx_usb/src/rx_usb_hw.c internal_usb_configure_phy(). */
    *usb_dpusr0r() &= ~(uint32_t)k_usb_dpusr0r_fixphy0;
    usb0()->physlew = (uint32_t)k_usb_physlew_rx72n;

    /* 4. DCP setup: max packet 64, default config, PID=BUF so the HW will
     *               actually answer SETUP packets. */
    usb0()->dcpcfg  = 0x0000U;
    usb0()->dcpmaxp = (uint16_t)k_min_max_pkt_fs;
    usb0()->dcpctr  = (uint16_t)k_dcpctr_pid_buf;

    /* 5. Enable DCP-pipe BRDY/BEMP and the high-level CTRT/DVST events. */
    usb0()->brdyenb = 0x0001U;
    usb0()->bempenb = 0x0001U;
    usb0()->intenb0 = (uint16_t)(k_intenb0_ctre | k_intenb0_dvse |
                                 k_intenb0_brdy | k_intenb0_bemp);

    /* 6. Route USBI0 onto its SELECTB vector slot, then priority + enable.
     *    k_vect_usb0_usbi is 144 (a software-configurable slot) on RX72N;
     *    without the SLIBR write the slot stays inert no matter what
     *    IER/IPR/IR contain.  See rx72n_usb_regs.h. */
    *icu_slibr(k_vect_usb0_usbi) = (uint8_t)k_usb0_usbi_sli_src;
    icu()->ir[k_vect_usb0_usbi]  = 0;
    icu()->ipr[k_vect_usb0_usbi] = 6;
    icu()->ier[k_vect_usb0_usbi / 8U] |= (uint8_t)(1U << (k_vect_usb0_usbi % 8U));

    /* 7. Soft-attach: D+ pull-up. */
    usb0()->syscfg |= (uint16_t)k_syscfg_dprpu;
}

/* ==========================================================================
 * Interrupt handlers wired in vectors.S.
 * ========================================================================== */
void __attribute__((interrupt)) usb0_usbi_isr(void)
{
    icu()->ir[k_vect_usb0_usbi] = 0;
    g_usb_isr_count++;

    const uint16_t intsts = usb0()->intsts0;
    g_usb_last_intsts = intsts;

    /* DVST: device state change (bus reset, etc.) */
    if ((intsts & (uint16_t)k_intsts0_dvst) != 0U) {
        g_usb_dvst_count++;
        usb0()->intsts0 = (uint16_t)~(uint16_t)k_intsts0_dvst;
    }

    /* CTRT: control-transfer stage transition */
    if ((intsts & (uint16_t)k_intsts0_ctrt) != 0U) {
        const uint16_t ctsq = intsts & (uint16_t)k_intsts0_ctsq_mask;
        if (ctsq == (uint16_t)k_intsts0_ctsq_rd_data
            || ctsq == (uint16_t)k_intsts0_ctsq_wr_data
            || ctsq == (uint16_t)k_intsts0_ctsq_wr_nd) {
            /* Clear VALID before reading the SETUP regs. */
            usb0()->intsts0 = (uint16_t)~(uint16_t)k_intsts0_valid;
            handle_setup();
        }
        usb0()->intsts0 = (uint16_t)~(uint16_t)k_intsts0_ctrt;
    }

    /* BRDY: data ready on a pipe. */
    if ((intsts & (uint16_t)k_intsts0_brdy) != 0U) {
        usb0()->brdysts = 0U;
        usb0()->intsts0 = (uint16_t)~(uint16_t)k_intsts0_brdy;
    }

    /* BEMP: buffer emptied (transmit complete). */
    if ((intsts & (uint16_t)k_intsts0_bemp) != 0U) {
        usb0()->bempsts = 0U;
        usb0()->intsts0 = (uint16_t)~(uint16_t)k_intsts0_bemp;
    }
}

void __attribute__((interrupt)) usb0_d0fifo_isr(void)
{
    icu()->ir[k_vect_usb0_d0fifo] = 0;
}

void __attribute__((interrupt)) usb0_d1fifo_isr(void)
{
    icu()->ir[k_vect_usb0_d1fifo] = 0;
}

void __attribute__((interrupt)) usb0_usbr_isr(void)
{
    icu()->ir[k_vect_usb0_usbr] = 0;
}
