# RX72N Register Structure Verification Checklist

**Purpose:** Verify all hardware register struct definitions match the RX72N hardware manual exactly.

**Background:** Issue #43 revealed a critical bug in MPC registers where PWPR was at offset 0 instead of 0x1F, causing all MPC writes to access wrong hardware addresses. This checklist prevents similar bugs in other peripherals.

**How to Use:**
1. For each peripheral, consult the RX72N Hardware Manual
2. Fill in the requested information from the manual
3. Compare with our struct definitions in `lib/rx_hal/inc/rx72n_*_regs.h`
4. Add compile-time static assertions to verify critical offsets

---

## ✅ Completed Verifications

### MPC (Multi-Function Pin Controller)
**Status:** ✅ VERIFIED (Issue #43 - Critical bug fixed)
- **Base Address:** 0x0008C100 ✓
- **PWPR Offset:** 0x1F (was incorrectly at 0x00) ✓
- **P00PFS Offset:** 0x40 ✓
- **Issues Found:**
  - Missing bus control registers (PFCSE, PFCSS, PFAOE, PFBCR, PFENET)
  - Wrong reserved space (had 32 bytes, needed 1+4+16+8)
  - Missing DSCR2 registers (24 bytes)
  - Missing reserved bytes for ports with <8 pins
- **Verification:** Static assertions added, build passes

---

## 🔴 Priority 1: Critical for System Operation

### 1. PORT (GPIO Registers)
**File:** `lib/rx_hal/inc/rx72n_port_regs.h`
**Priority:** CRITICAL - Similar structure to MPC, likely has same issues!

#### Questions to Answer:

**Base Addresses:**
- [ ] What is the base address of PORT0?
- [ ] What is the base address of PORT1?
- [ ] What is the base address of PORT2?
- [ ] What is the base address of PORT3?
- [ ] What is the base address of PORT4?
- [ ] What is the base address of PORT5?
- [ ] What is the base address of PORTA?
- [ ] What is the base address of PORTB?
- [ ] What is the base address of PORTC?
- [ ] What is the base address of PORTD?
- [ ] What is the base address of PORTE?
- [ ] What is the base address of PORTJ?

**Register Offsets (from each port's base):**
- [ ] What is the offset of PDR (Port Direction Register)?
- [ ] What is the offset of PODR (Port Output Data Register)?
- [ ] What is the offset of PIDR (Port Input Data Register)?
- [ ] What is the offset of PMR (Port Mode Register)?
- [ ] What is the offset of ODR0 (Open Drain Control 0)?
- [ ] What is the offset of ODR1 (Open Drain Control 1)?
- [ ] What is the offset of PCR (Pull-up Control Register)?
- [ ] What is the offset of DSCR (Drive Capacity Control)?

**Memory Layout:**
- [ ] How many bytes are in a complete PORT register block?
- [ ] Are there reserved bytes between registers within a port?
- [ ] Are there reserved bytes between different ports?
- [ ] Do all ports (0-9, A-J) have identical register layouts?
- [ ] Which ports exist on 100-pin LFQFP package?

**Specific Register Sizes:**
- [ ] Are PDR, PODR, PIDR, PMR all 8-bit registers?
- [ ] What size are ODR0/ODR1 registers?
- [ ] What size is PCR register?
- [ ] What size is DSCR register?

---

### 2. CMT (Compare Match Timer)
**File:** `lib/rx_hal/inc/rx72n_cmt_regs.h`
**Priority:** CRITICAL - Used for ThreadX system tick!

#### Questions to Answer:

**Base Addresses:**
- [ ] What is the base address of CMT unit (CMSTR)?
- [ ] What is the base address of CMT0?
- [ ] What is the base address of CMT1?
- [ ] What is the base address of CMT2?
- [ ] What is the base address of CMT3?

**CMSTR Register (Start Register):**
- [ ] What is the absolute address of CMSTR0 (CMT0/1 start)?
- [ ] What is the absolute address of CMSTR1 (CMT2/3 start)?
- [ ] What is the offset between CMSTR0 and CMSTR1?

**CMT Channel Registers (offsets from channel base):**
- [ ] What is the offset of CMCR (Control Register)?
- [ ] What is the offset of CMCNT (Counter)?
- [ ] What is the offset of CMCOR (Compare Match Register)?
- [ ] What size is CMCR (8-bit or 16-bit)?
- [ ] What size is CMCNT (16-bit or 32-bit)?
- [ ] What size is CMCOR (16-bit or 32-bit)?

**Memory Layout:**
- [ ] Are there reserved bytes between CMCR and CMCNT?
- [ ] Are there reserved bytes between CMCNT and CMCOR?
- [ ] What is the total size of a CMT channel register block?
- [ ] How many bytes between CMT0 base and CMT1 base?

---

### 3. ICU (Interrupt Controller)
**File:** `lib/rx_hal/inc/rx72n_icu_regs.h`
**Priority:** CRITICAL - All interrupts depend on this!

#### Questions to Answer:

**Base Address:**
- [ ] What is the base address of ICU?

**Register Arrays:**
- [ ] What is the offset of IR (Interrupt Request flags array)?
- [ ] What is the size of the IR array? (How many interrupt vectors?)
- [ ] What is the offset of DTCER (DTC Enable Register array)?
- [ ] What is the size of the DTCER array?
- [ ] What is the offset of IER (Interrupt Enable Register array)?
- [ ] What is the size of the IER array?
- [ ] What is the offset of IPR (Interrupt Priority Register array)?
- [ ] What is the size of the IPR array?

**Other ICU Registers:**
- [ ] What is the offset of DMRSR (DMA Route Select)?
- [ ] What is the offset of IRQCR (IRQ Control)?
- [ ] What is the offset of IRQFLTE (IRQ Filter Enable)?
- [ ] What is the offset of IRQFLTC (IRQ Filter Clock)?
- [ ] What is the offset of NMISR (NMI Status)?
- [ ] What is the offset of NMIER (NMI Enable)?
- [ ] What is the offset of NMICLR (NMI Clear)?
- [ ] What is the offset of NMICR (NMI Control)?

**Memory Layout:**
- [ ] Are there reserved bytes between these register arrays?
- [ ] What is the total size of the ICU register block?
- [ ] Are all IR registers 8-bit?
- [ ] Are all IPR registers 8-bit?

---

### 4. SYSTEM (Clock and Module Control)
**File:** `lib/rx_hal/inc/rx72n_system_regs.h`
**Priority:** CRITICAL - Clock configuration and module power!

#### Questions to Answer:

**Base Address:**
- [ ] What is the base address of SYSTEM registers?

**Clock Control Registers:**
- [ ] What is the offset of SCKCR (System Clock Control)?
- [ ] What is the offset of SCKCR2 (System Clock Control 2)?
- [ ] What is the offset of SCKCR3 (System Clock Control 3)?
- [ ] What is the offset of PLLCR (PLL Control)?
- [ ] What is the offset of PLLCR2 (PLL Control 2)?
- [ ] What is the offset of BCKCR (External Bus Clock Control)?
- [ ] What is the offset of MOSCCR (Main Clock Oscillator Control)?
- [ ] What is the offset of SOSCCR (Sub-Clock Oscillator Control)?
- [ ] What is the offset of LOCOCR (Low-Speed On-Chip Oscillator Control)?
- [ ] What is the offset of ILOCOCR (High-Speed On-Chip Oscillator Control)?
- [ ] What is the offset of HOCOCR (High-Speed On-Chip Oscillator Control)?
- [ ] What is the offset of HOCOCR2 (High-Speed On-Chip Oscillator Control 2)?
- [ ] What is the offset of OSCOVFSR (Oscillation Stabilization Flag)?

**Module Stop Control:**
- [ ] What is the offset of MSTPCRA (Module Stop Control A)?
- [ ] What is the offset of MSTPCRB (Module Stop Control B)?
- [ ] What is the offset of MSTPCRC (Module Stop Control C)?
- [ ] What is the offset of MSTPCRD (Module Stop Control D)?

**Protection Register:**
- [ ] What is the offset of PRCR (Protect Register)?

**Memory Layout:**
- [ ] Are there reserved bytes between these registers?
- [ ] What register sizes are used (8-bit, 16-bit, 32-bit)?
- [ ] What is the total size of the SYSTEM register block?

---

## 🟡 Priority 2: Critical for Motor Control

### 5. GPTW (General PWM Timer)
**File:** `lib/rx_hal/inc/rx72n_gptw_regs.h`
**Priority:** HIGH - Motor PWM generation!

#### Questions to Answer:

**Base Addresses:**
- [ ] What is the base address of GPTW common registers?
- [ ] What is the base address of GPTW0?
- [ ] What is the base address of GPTW1?
- [ ] What is the base address of GPTW2?
- [ ] What is the base address of GPTW3?

**GPTW Common Registers:**
- [ ] What is the offset of GTSTR (Start Register)?
- [ ] What is the offset of GTSTP (Stop Register)?
- [ ] What is the offset of GTCLR (Clear Register)?
- [ ] What is the offset of GTSSR (Start Source Select)?
- [ ] What is the offset of GTPSR (Stop Source Select)?
- [ ] What is the offset of GTCSR (Clear Source Select)?

**GPTW Channel Registers (offsets from channel base):**
- [ ] What is the offset of GTWP (Write Protect)?
- [ ] What is the offset of GTSTR (Start)?
- [ ] What is the offset of GTSTP (Stop)?
- [ ] What is the offset of GTCLR (Clear)?
- [ ] What is the offset of GTSSR (Start Source)?
- [ ] What is the offset of GTPSR (Stop Source)?
- [ ] What is the offset of GTCSR (Clear Source)?
- [ ] What is the offset of GTUPSR (Up Count Source)?
- [ ] What is the offset of GTDNSR (Down Count Source)?
- [ ] What is the offset of GTICASR (Input Capture A Source)?
- [ ] What is the offset of GTICBSR (Input Capture B Source)?
- [ ] What is the offset of GTCR (Control)?
- [ ] What is the offset of GTUDDTYC (Up/Down Count Control)?
- [ ] What is the offset of GTIOR (I/O Control)?
- [ ] What is the offset of GTINTAD (Interrupt ADC Start)?
- [ ] What is the offset of GTST (Status)?
- [ ] What is the offset of GTBER (Buffer Enable)?
- [ ] What is the offset of GTITC (Interrupt Control)?
- [ ] What is the offset of GTCNT (Counter)?
- [ ] What is the offset of GTCCRA (Compare Capture A)?
- [ ] What is the offset of GTCCRB (Compare Capture B)?
- [ ] What is the offset of GTCCRC (Compare Capture C)?
- [ ] What is the offset of GTCCRE (Compare Capture E)?
- [ ] What is the offset of GTCCRD (Compare Capture D)?
- [ ] What is the offset of GTCCRF (Compare Capture F)?
- [ ] What is the offset of GTPR (Period)?
- [ ] What is the offset of GTPBR (Period Buffer)?
- [ ] What is the offset of GTPDBR (Period Double Buffer)?
- [ ] What is the offset of GTADTRA (ADC Trigger A)?
- [ ] What is the offset of GTADTBRA (ADC Trigger A Buffer)?
- [ ] What is the offset of GTADTDBRA (ADC Trigger A Double Buffer)?
- [ ] What is the offset of GTADTRB (ADC Trigger B)?
- [ ] What is the offset of GTADTBRB (ADC Trigger B Buffer)?
- [ ] What is the offset of GTADTDBRB (ADC Trigger B Double Buffer)?

**Memory Layout:**
- [ ] What is the total size of a GPTW channel register block?
- [ ] How many bytes between GPTW0 base and GPTW1 base?
- [ ] Are there reserved bytes within the channel block?
- [ ] What register sizes are used (8-bit, 16-bit, 32-bit)?

---

### 6. MTU3a (Multi-Function Timer Pulse Unit)
**File:** `lib/rx_hal/inc/rx72n_mtu_regs.h`
**Priority:** HIGH - Encoder phase counting!

#### Questions to Answer:

**Base Addresses:**
- [ ] What is the base address of MTU common registers?
- [ ] What is the base address of MTU0?
- [ ] What is the base address of MTU1?
- [ ] What is the base address of MTU2?
- [ ] What is the base address of MTU3?
- [ ] What is the base address of MTU4?
- [ ] What is the base address of MTU5?
- [ ] What is the base address of MTU6?
- [ ] What is the base address of MTU7?

**MTU Common Registers:**
- [ ] What is the offset of TOERA (Timer Output Master Enable A)?
- [ ] What is the offset of TOERB (Timer Output Master Enable B)?
- [ ] What is the offset of TOCR1A (Timer Output Control 1A)?
- [ ] What is the offset of TOCR1B (Timer Output Control 1B)?
- [ ] What is the offset of TOCR2A (Timer Output Control 2A)?
- [ ] What is the offset of TOCR2B (Timer Output Control 2B)?
- [ ] What is the offset of TCDRA (Timer Cycle Data A)?
- [ ] What is the offset of TCDRB (Timer Cycle Data B)?
- [ ] What is the offset of TCBRA (Timer Cycle Buffer A)?
- [ ] What is the offset of TCBRB (Timer Cycle Buffer B)?
- [ ] What is the offset of TITCR1A (Timer Interrupt Control 1A)?
- [ ] What is the offset of TITCR1B (Timer Interrupt Control 1B)?
- [ ] What is the offset of TITCR2A (Timer Interrupt Control 2A)?
- [ ] What is the offset of TITCR2B (Timer Interrupt Control 2B)?
- [ ] What is the offset of TITCNT1A (Timer Interrupt Count 1A)?
- [ ] What is the offset of TITCNT1B (Timer Interrupt Count 1B)?
- [ ] What is the offset of TITCNT2A (Timer Interrupt Count 2A)?
- [ ] What is the offset of TITCNT2B (Timer Interrupt Count 2B)?
- [ ] What is the offset of TBTER (Timer Buffer Transfer Enable)?
- [ ] What is the offset of TDER (Timer DMA Transfer Enable)?
- [ ] What is the offset of TWCR (Timer Waveform Control)?
- [ ] What is the offset of TSTR (Timer Start)?
- [ ] What is the offset of TSYR (Timer Synchronization)?
- [ ] What is the offset of TRWERA (Timer Read/Write Enable A)?
- [ ] What is the offset of TRWERB (Timer Read/Write Enable B)?

**MTU Channel Registers (for MTU0-4, offsets from channel base):**
- [ ] What is the offset of TCR (Timer Control)?
- [ ] What is the offset of TMDR1 (Timer Mode 1)?
- [ ] What is the offset of TIORH (Timer I/O Control H)?
- [ ] What is the offset of TIORL (Timer I/O Control L)?
- [ ] What is the offset of TIER (Timer Interrupt Enable)?
- [ ] What is the offset of TSR (Timer Status)?
- [ ] What is the offset of TCNT (Timer Counter)?
- [ ] What is the offset of TGRA (Timer General Register A)?
- [ ] What is the offset of TGRB (Timer General Register B)?
- [ ] What is the offset of TGRC (Timer General Register C)?
- [ ] What is the offset of TGRD (Timer General Register D)?

**Memory Layout:**
- [ ] What is the total size of an MTU channel register block?
- [ ] How many bytes between MTU0 base and MTU1 base?
- [ ] Are MTU0-4 layouts identical?
- [ ] Are MTU5-7 layouts different? If so, how?
- [ ] Are there reserved bytes within channel blocks?
- [ ] What register sizes are used (8-bit, 16-bit, 32-bit)?

---

## 🟢 Priority 3: Critical for Communication

### 7. RSPI (Renesas SPI)
**File:** `lib/rx_hal/inc/rx72n_rspi_regs.h`
**Priority:** HIGH - Communication with RPi5!

#### Questions to Answer:

**Base Addresses:**
- [ ] What is the base address of RSPI0?
- [ ] What is the base address of RSPI1?
- [ ] What is the base address of RSPI2?

**RSPI Registers (offsets from channel base):**
- [ ] What is the offset of SPCR (Control)?
- [ ] What is the offset of SSLP (Slave Select Polarity)?
- [ ] What is the offset of SPPCR (Pin Control)?
- [ ] What is the offset of SPSR (Status)?
- [ ] What is the offset of SPDR (Data)?
- [ ] What is the offset of SPSCR (Sequence Control)?
- [ ] What is the offset of SPSSR (Sequence Status)?
- [ ] What is the offset of SPBR (Bit Rate)?
- [ ] What is the offset of SPDCR (Data Control)?
- [ ] What is the offset of SPCKD (Clock Delay)?
- [ ] What is the offset of SSLND (Slave Select Negation Delay)?
- [ ] What is the offset of SPND (Next Access Delay)?
- [ ] What is the offset of SPCR2 (Control 2)?
- [ ] What is the offset of SPCMD0-7 (Command registers)?
- [ ] What is the offset of SPBFCR (Buffer Control)?
- [ ] What is the offset of SPBFDR (Buffer Data Count)?

**Memory Layout:**
- [ ] What is the total size of an RSPI register block?
- [ ] How many bytes between RSPI0 base and RSPI1 base?
- [ ] Are there reserved bytes between registers?
- [ ] What register sizes are used (8-bit, 16-bit, 32-bit)?
- [ ] What size is SPDR (8, 16, or 32-bit)?

---

### 8. SCI (Serial Communication Interface)
**File:** `lib/rx72n-firmware/lib/rx_hal/inc/rx72n_sci_regs.h`
**Priority:** HIGH - UART communication (debug, etc)!

#### Questions to Answer:

**Base Addresses:**
- [ ] What is the base address of SCI0?
- [ ] What is the base address of SCI1?
- [ ] What is the base address of SCI2?
- [ ] What is the base address of SCI5?
- [ ] What is the base address of SCI9?
- [ ] What is the base address of SCI12?

**SCI Registers (offsets from channel base):**
- [ ] What is the offset of SMR (Serial Mode)?
- [ ] What is the offset of BRR (Bit Rate)?
- [ ] What is the offset of SCR (Serial Control)?
- [ ] What is the offset of TDR (Transmit Data)?
- [ ] What is the offset of SSR (Serial Status)?
- [ ] What is the offset of RDR (Receive Data)?
- [ ] What is the offset of SCMR (Smart Card Mode)?
- [ ] What is the offset of SEMR (Serial Extended Mode)?
- [ ] What is the offset of SNFR (Noise Filter)?
- [ ] What is the offset of SIMR1 (I2C Mode 1)?
- [ ] What is the offset of SIMR2 (I2C Mode 2)?
- [ ] What is the offset of SIMR3 (I2C Mode 3)?
- [ ] What is the offset of SISR (I2C Status)?
- [ ] What is the offset of SPMR (SPI Mode)?
- [ ] What is the offset of FTDR (FIFO Transmit Data)?
- [ ] What is the offset of FRDR (FIFO Receive Data)?

**Memory Layout:**
- [ ] What is the total size of a SCI register block?
- [ ] How many bytes between SCI0 base and SCI1 base?
- [ ] Are there reserved bytes between registers?
- [ ] What register sizes are used (8-bit, 16-bit)?

---

### 9. RIIC (I2C Bus Interface)
**File:** `lib/rx_hal/inc/rx72n_riic_regs.h`
**Priority:** MEDIUM - I2C communication (battery monitor, etc)

#### Questions to Answer:

**Base Addresses:**
- [ ] What is the base address of RIIC0?
- [ ] What is the base address of RIIC1?
- [ ] What is the base address of RIIC2?

**RIIC Registers (offsets from channel base):**
- [ ] What is the offset of ICCR1 (Control 1)?
- [ ] What is the offset of ICCR2 (Control 2)?
- [ ] What is the offset of ICMR1 (Mode 1)?
- [ ] What is the offset of ICMR2 (Mode 2)?
- [ ] What is the offset of ICMR3 (Mode 3)?
- [ ] What is the offset of ICFER (Function Enable)?
- [ ] What is the offset of ICSER (Status Enable)?
- [ ] What is the offset of ICIER (Interrupt Enable)?
- [ ] What is the offset of ICSR1 (Status 1)?
- [ ] What is the offset of ICSR2 (Status 2)?
- [ ] What is the offset of SARL0 (Slave Address L0)?
- [ ] What is the offset of SARU0 (Slave Address U0)?
- [ ] What is the offset of SARL1 (Slave Address L1)?
- [ ] What is the offset of SARU1 (Slave Address U1)?
- [ ] What is the offset of SARL2 (Slave Address L2)?
- [ ] What is the offset of SARU2 (Slave Address U2)?
- [ ] What is the offset of ICBRL (Bit Rate L)?
- [ ] What is the offset of ICBRH (Bit Rate H)?
- [ ] What is the offset of ICDRT (Data Transmit)?
- [ ] What is the offset of ICDRR (Data Receive)?

**Memory Layout:**
- [ ] What is the total size of a RIIC register block?
- [ ] How many bytes between RIIC0 base and RIIC1 base?
- [ ] Are there reserved bytes between registers?
- [ ] What register sizes are used (8-bit, 16-bit)?

---

## 🔵 Priority 4: Other Peripherals

### 10. ADC (A/D Converter)
**File:** `lib/rx_hal/inc/rx72n_adc_regs.h`
**Priority:** MEDIUM - Analog readings (current sense, etc)

#### Questions to Answer:

**Base Addresses:**
- [ ] What is the base address of ADC12 (12-bit A/D)?
- [ ] What is the base address of ADC12.S12ADI0?
- [ ] What is the base address of ADC12.S12GBADI?
- [ ] What is the base address of ADC12.S12GCADI?

**ADC Registers (offsets from base):**
- [ ] What is the offset of ADCSR (Control/Status)?
- [ ] What is the offset of ADANSA (Channel Select A)?
- [ ] What is the offset of ADANSB (Channel Select B)?
- [ ] What is the offset of ADANSC (Channel Select C)?
- [ ] What is the offset of ADADS (A/D Disconnect Detection)?
- [ ] What is the offset of ADCER (Control Extended)?
- [ ] What is the offset of ADSTRGR (Start Trigger)?
- [ ] What is the offset of ADEXICR (Extended Input Control)?
- [ ] What is the offset of ADGSPCR (Group Scan Priority Control)?
- [ ] What is the offset of ADDBLDRA (Data Duplexing A)?
- [ ] What is the offset of ADDBLDRB (Data Duplexing B)?
- [ ] What is the offset of ADTSDR (Temperature Sensor Data)?
- [ ] What is the offset of ADOCDR (Internal Reference Voltage Data)?
- [ ] What is the offset of ADDR0-31 (Data registers)?

**Memory Layout:**
- [ ] What is the total size of the ADC register block?
- [ ] Are there reserved bytes between registers?
- [ ] What register sizes are used (8-bit, 16-bit, 32-bit)?
- [ ] What size are ADDR registers (16-bit)?

---

### 11. CRC (CRC Calculator)
**File:** `lib/rx_hal/inc/rx72n_crc_regs.h`
**Priority:** MEDIUM - Hardware CRC for communication

#### Questions to Answer:

**Base Address:**
- [ ] What is the base address of CRC?

**CRC Registers (offsets from base):**
- [ ] What is the offset of CRCCR (Control)?
- [ ] What is the offset of CRCDIR (Data Input)?
- [ ] What is the offset of CRCDOR (Data Output)?
- [ ] What is the offset of CRCDCR (Data Control)?

**Memory Layout:**
- [ ] What is the total size of the CRC register block?
- [ ] Are there reserved bytes between registers?
- [ ] What register sizes are used (8-bit, 16-bit, 32-bit)?
- [ ] What size is CRCDIR (8, 16, or 32-bit)?
- [ ] What size is CRCDOR (16 or 32-bit)?

---

### 12. IWDT (Independent Watchdog Timer)
**File:** `lib/rx_hal/inc/rx72n_iwdt_regs.h`
**Priority:** MEDIUM - Safety watchdog

#### Questions to Answer:

**Base Address:**
- [ ] What is the base address of IWDT?

**IWDT Registers (offsets from base):**
- [ ] What is the offset of IWDTCR (Control)?
- [ ] What is the offset of IWDTSR (Status)?
- [ ] What is the offset of IWDTRCR (Reset Control)?
- [ ] What is the offset of IWDTCSTPR (Count Stop Control)?

**Memory Layout:**
- [ ] What is the total size of the IWDT register block?
- [ ] Are there reserved bytes between registers?
- [ ] What register sizes are used (8-bit, 16-bit)?

---

### 13. USB (USB 2.0 Full-Speed)
**File:** `lib/rx_hal/inc/rx72n_usb_regs.h`
**Priority:** MEDIUM - USB CDC communication

#### Questions to Answer:

**Base Address:**
- [ ] What is the base address of USB0?

**USB Registers (offsets from base):**
- [ ] What is the offset of SYSCFG (System Configuration)?
- [ ] What is the offset of SYSSTS (System Status)?
- [ ] What is the offset of DVSTCTR (Device State Control)?
- [ ] What is the offset of CFIFO (CFIFO Port)?
- [ ] What is the offset of D0FIFO (D0FIFO Port)?
- [ ] What is the offset of D1FIFO (D1FIFO Port)?
- [ ] What is the offset of CFIFOSEL (CFIFO Select)?
- [ ] What is the offset of CFIFOCTR (CFIFO Control)?
- [ ] What is the offset of D0FIFOSEL (D0FIFO Select)?
- [ ] What is the offset of D0FIFOCTR (D0FIFO Control)?
- [ ] What is the offset of D1FIFOSEL (D1FIFO Select)?
- [ ] What is the offset of D1FIFOCTR (D1FIFO Control)?
- [ ] What is the offset of INTENB0 (Interrupt Enable 0)?
- [ ] What is the offset of INTENB1 (Interrupt Enable 1)?
- [ ] What is the offset of BRDYENB (Buffer Ready Interrupt Enable)?
- [ ] What is the offset of NRDYENB (Buffer Not Ready Interrupt Enable)?
- [ ] What is the offset of BEMPENB (Buffer Empty Interrupt Enable)?
- [ ] What is the offset of INTSTS0 (Interrupt Status 0)?
- [ ] What is the offset of INTSTS1 (Interrupt Status 1)?
- [ ] What is the offset of BRDYSTS (Buffer Ready Status)?
- [ ] What is the offset of NRDYSTS (Buffer Not Ready Status)?
- [ ] What is the offset of BEMPSTS (Buffer Empty Status)?
- [ ] What is the offset of FRMNUM (Frame Number)?
- [ ] What is the offset of DCPCFG (DCP Configuration)?
- [ ] What is the offset of DCPMAXP (DCP Max Packet)?
- [ ] What is the offset of DCPCTR (DCP Control)?
- [ ] What is the offset of PIPESEL (Pipe Select)?
- [ ] What is the offset of PIPECFG (Pipe Configuration)?
- [ ] What is the offset of PIPEMAXP (Pipe Max Packet)?
- [ ] What is the offset of PIPECTR (Pipe Control, array for PIPE1-9)?

**Memory Layout:**
- [ ] What is the total size of the USB register block?
- [ ] Are there reserved bytes between registers?
- [ ] What register sizes are used (8-bit, 16-bit, 32-bit)?
- [ ] How many PIPECTR registers exist?

---

## 📋 Verification Template

When verifying each peripheral, create a file like:

```
PERIPHERAL_NAME_verification.txt

Base Address: 0xXXXXXXXX
Source: RX72N Hardware Manual Section X.Y.Z, Page NNN

Register Memory Map:
Offset  | Size | Name     | Description
--------|------|----------|------------------
0x00    | 8b   | REG1     | Description
0x01    | 8b   | REG2     | Description
0x02-03 | ---  | Reserved | Reserved (2 bytes)
0x04    | 16b  | REG3     | Description
...

Total Block Size: XXX bytes

Special Notes:
- Any registers that don't exist on certain pins
- Any reserved byte regions
- Any differences between channels
- Any size variations (8/16/32-bit)
```

---

## 🔧 Adding Static Assertions

After verification, add to each `*_regs.h` file:

```c
#include <stddef.h>

/* Compile-Time Register Layout Verification */
_Static_assert(offsetof(peripheral_regs_t, critical_reg1) == 0xXX,
               "REG1 must be at offset 0xXX");
_Static_assert(offsetof(peripheral_regs_t, critical_reg2) == 0xYY,
               "REG2 must be at offset 0xYY");
_Static_assert(sizeof(peripheral_regs_t) == EXPECTED_SIZE,
               "Peripheral struct size must be XXX bytes");
```

---

## ✅ Success Criteria

For each peripheral, verification is complete when:
- [ ] All base addresses confirmed from hardware manual
- [ ] All register offsets documented
- [ ] All reserved byte regions identified
- [ ] Verification file created (like `verification.txt` for MPC)
- [ ] Static assertions added to header file
- [ ] Build passes with zero warnings
- [ ] All assertions pass

---

## 📚 References

- **RX72N Group Hardware Manual:** R01UH0951EJ0100 (or latest version)
- **Issue #43:** MPC register bug (reference case)
- **PR #143:** MPC register fix (verification example)
