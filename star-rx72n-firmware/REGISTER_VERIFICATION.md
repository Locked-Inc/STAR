# RX72N Register Structure Verification Checklist

**Purpose:** Verify all hardware register struct definitions match the RX72N hardware manual exactly.

**Background:** This systematic verification prevents critical bugs where register offsets don't match hardware, causing writes to wrong addresses and hardware malfunction.

**How to Use:**
1. For each peripheral, consult the RX72N Hardware Manual
2. Fill in the **Answer:** fields from the manual
3. Compare with our struct definitions in `lib/rx_hal/inc/rx72n_*_regs.h`
4. Fix any mismatches and add compile-time static assertions

---

## 🔴 Priority 1: Critical for System Operation

### 1. MPC (Multi-Function Pin Controller)
**File:** `lib/rx_hal/inc/rx72n_mpc_regs.h`
**Priority:** CRITICAL - Pin function selection!

#### Base Address
- [ ] What is the base address of MPC?
  - **Answer:**

#### Bus Control Registers
- [ ] What is the offset of PFCSE (CS Output Enable)?
  - **Answer:**
- [ ] What is the offset of PFCSS0 (CS Output Pin Select 0)?
  - **Answer:**
- [ ] What is the offset of PFCSS1 (CS Output Pin Select 1)?
  - **Answer:**
- [ ] What is the offset of PFAOE0 (Address Output Enable 0)?
  - **Answer:**
- [ ] What is the offset of PFAOE1 (Address Output Enable 1)?
  - **Answer:**
- [ ] What is the offset of PFBCR0 (External Bus Control 0)?
  - **Answer:**
- [ ] What is the offset of PFBCR1 (External Bus Control 1)?
  - **Answer:**
- [ ] What is the offset of PFBCR2 (External Bus Control 2)?
  - **Answer:**
- [ ] What is the offset of PFBCR3 (External Bus Control 3)?
  - **Answer:**
- [ ] What is the offset of PFENET (Ethernet Control)?
  - **Answer:**

#### Write Protection
- [ ] What is the offset of PWPR (Write Protect Register)?
  - **Answer:**

#### Drive Capacity Control
- [ ] What is the offset of DSCR2 registers (first register)?
  - **Answer:**
- [ ] How many DSCR2 registers are there?
  - **Answer:**

#### Pin Function Select Registers
- [ ] What is the offset of P00PFS (Port 0 Pin 0)?
  - **Answer:**
- [ ] What is the offset of P50PFS (Port 5 Pin 0)?
  - **Answer:**
- [ ] What is the offset of PA0PFS (Port A Pin 0)?
  - **Answer:**
- [ ] What is the offset of PE0PFS (Port E Pin 0)?
  - **Answer:**
- [ ] What is the offset of PJ0PFS (Port J Pin 0)?
  - **Answer:**

#### Memory Layout
- [ ] How many bytes of reserved space after PFCSE?
  - **Answer:**
- [ ] How many bytes of reserved space after PFBCR3?
  - **Answer:**
- [ ] How many bytes of reserved space after PFENET?
  - **Answer:**
- [ ] How many bytes of reserved space after PWPR?
  - **Answer:**
- [ ] What is the total size of the MPC register block?
  - **Answer:**

#### Port-Specific Questions
- [ ] Which ports have fewer than 8 PFS registers? List them with pin counts.
  - **Answer:**
- [ ] Does Port I exist in RX72N?
  - **Answer:**

---

### 2. PORT (GPIO Registers)
**File:** `lib/rx_hal/inc/rx72n_port_regs.h`
**Priority:** CRITICAL - GPIO control!

#### Base Addresses
- [ ] What is the base address of PORT0?
  - **Answer:**
- [ ] What is the base address of PORT1?
  - **Answer:**
- [ ] What is the base address of PORT2?
  - **Answer:**
- [ ] What is the base address of PORT3?
  - **Answer:**
- [ ] What is the base address of PORT4?
  - **Answer:**
- [ ] What is the base address of PORT5?
  - **Answer:**
- [ ] What is the base address of PORTA?
  - **Answer:**
- [ ] What is the base address of PORTB?
  - **Answer:**
- [ ] What is the base address of PORTC?
  - **Answer:**
- [ ] What is the base address of PORTD?
  - **Answer:**
- [ ] What is the base address of PORTE?
  - **Answer:**
- [ ] What is the base address of PORTJ?
  - **Answer:**

#### Register Offsets (from each port's base)
- [ ] What is the offset of PDR (Port Direction Register)?
  - **Answer:**
- [ ] What is the offset of PODR (Port Output Data Register)?
  - **Answer:**
- [ ] What is the offset of PIDR (Port Input Data Register)?
  - **Answer:**
- [ ] What is the offset of PMR (Port Mode Register)?
  - **Answer:**
- [ ] What is the offset of ODR0 (Open Drain Control 0)?
  - **Answer:**
- [ ] What is the offset of ODR1 (Open Drain Control 1)?
  - **Answer:**
- [ ] What is the offset of PCR (Pull-up Control Register)?
  - **Answer:**
- [ ] What is the offset of DSCR (Drive Capacity Control)?
  - **Answer:**

#### Memory Layout
- [ ] How many bytes are in a complete PORT register block?
  - **Answer:**
- [ ] Are there reserved bytes between registers within a port?
  - **Answer:**
- [ ] Are there reserved bytes between different ports?
  - **Answer:**
- [ ] Do all ports (0-9, A-J) have identical register layouts?
  - **Answer:**
- [ ] Which ports exist on 100-pin LFQFP package?
  - **Answer:**

#### Specific Register Sizes
- [ ] Are PDR, PODR, PIDR, PMR all 8-bit registers?
  - **Answer:**
- [ ] What size are ODR0/ODR1 registers?
  - **Answer:**
- [ ] What size is PCR register?
  - **Answer:**
- [ ] What size is DSCR register?
  - **Answer:**

---

### 3. CMT (Compare Match Timer)
**File:** `lib/rx_hal/inc/rx72n_cmt_regs.h`
**Priority:** CRITICAL - Used for ThreadX system tick!

#### Base Addresses
- [ ] What is the base address of CMT unit (CMSTR)?
  - **Answer:**
- [ ] What is the base address of CMT0?
  - **Answer:**
- [ ] What is the base address of CMT1?
  - **Answer:**
- [ ] What is the base address of CMT2?
  - **Answer:**
- [ ] What is the base address of CMT3?
  - **Answer:**

#### CMSTR Register (Start Register)
- [ ] What is the absolute address of CMSTR0 (CMT0/1 start)?
  - **Answer:**
- [ ] What is the absolute address of CMSTR1 (CMT2/3 start)?
  - **Answer:**
- [ ] What is the offset between CMSTR0 and CMSTR1?
  - **Answer:**

#### CMT Channel Registers (offsets from channel base)
- [ ] What is the offset of CMCR (Control Register)?
  - **Answer:**
- [ ] What is the offset of CMCNT (Counter)?
  - **Answer:**
- [ ] What is the offset of CMCOR (Compare Match Register)?
  - **Answer:**
- [ ] What size is CMCR (8-bit or 16-bit)?
  - **Answer:**
- [ ] What size is CMCNT (16-bit or 32-bit)?
  - **Answer:**
- [ ] What size is CMCOR (16-bit or 32-bit)?
  - **Answer:**

#### Memory Layout
- [ ] Are there reserved bytes between CMCR and CMCNT?
  - **Answer:**
- [ ] Are there reserved bytes between CMCNT and CMCOR?
  - **Answer:**
- [ ] What is the total size of a CMT channel register block?
  - **Answer:**
- [ ] How many bytes between CMT0 base and CMT1 base?
  - **Answer:**

---

### 4. ICU (Interrupt Controller)
**File:** `lib/rx_hal/inc/rx72n_icu_regs.h`
**Priority:** CRITICAL - All interrupts depend on this!

#### Base Address
- [ ] What is the base address of ICU?
  - **Answer:**

#### Register Arrays
- [ ] What is the offset of IR (Interrupt Request flags array)?
  - **Answer:**
- [ ] What is the size of the IR array? (How many interrupt vectors?)
  - **Answer:**
- [ ] What is the offset of DTCER (DTC Enable Register array)?
  - **Answer:**
- [ ] What is the size of the DTCER array?
  - **Answer:**
- [ ] What is the offset of IER (Interrupt Enable Register array)?
  - **Answer:**
- [ ] What is the size of the IER array?
  - **Answer:**
- [ ] What is the offset of IPR (Interrupt Priority Register array)?
  - **Answer:**
- [ ] What is the size of the IPR array?
  - **Answer:**

#### Other ICU Registers
- [ ] What is the offset of DMRSR (DMA Route Select)?
  - **Answer:**
- [ ] What is the offset of IRQCR (IRQ Control)?
  - **Answer:**
- [ ] What is the offset of IRQFLTE (IRQ Filter Enable)?
  - **Answer:**
- [ ] What is the offset of IRQFLTC (IRQ Filter Clock)?
  - **Answer:**
- [ ] What is the offset of NMISR (NMI Status)?
  - **Answer:**
- [ ] What is the offset of NMIER (NMI Enable)?
  - **Answer:**
- [ ] What is the offset of NMICLR (NMI Clear)?
  - **Answer:**
- [ ] What is the offset of NMICR (NMI Control)?
  - **Answer:**

#### Memory Layout
- [ ] Are there reserved bytes between these register arrays?
  - **Answer:**
- [ ] What is the total size of the ICU register block?
  - **Answer:**
- [ ] Are all IR registers 8-bit?
  - **Answer:**
- [ ] Are all IPR registers 8-bit?
  - **Answer:**

---

### 5. SYSTEM (Clock and Module Control)
**File:** `lib/rx_hal/inc/rx72n_system_regs.h`
**Priority:** CRITICAL - Clock configuration and module power!

#### Base Address
- [ ] What is the base address of SYSTEM registers?
  - **Answer:**

#### Clock Control Registers
- [ ] What is the offset of SCKCR (System Clock Control)?
  - **Answer:**
- [ ] What is the offset of SCKCR2 (System Clock Control 2)?
  - **Answer:**
- [ ] What is the offset of SCKCR3 (System Clock Control 3)?
  - **Answer:**
- [ ] What is the offset of PLLCR (PLL Control)?
  - **Answer:**
- [ ] What is the offset of PLLCR2 (PLL Control 2)?
  - **Answer:**
- [ ] What is the offset of BCKCR (External Bus Clock Control)?
  - **Answer:**
- [ ] What is the offset of MOSCCR (Main Clock Oscillator Control)?
  - **Answer:**
- [ ] What is the offset of SOSCCR (Sub-Clock Oscillator Control)?
  - **Answer:**
- [ ] What is the offset of LOCOCR (Low-Speed On-Chip Oscillator Control)?
  - **Answer:**
- [ ] What is the offset of ILOCOCR (Iwdt-Dedicated On-Chip Oscillator Control)?
  - **Answer:**
- [ ] What is the offset of HOCOCR (High-Speed On-Chip Oscillator Control)?
  - **Answer:**
- [ ] What is the offset of HOCOCR2 (High-Speed On-Chip Oscillator Control 2)?
  - **Answer:**
- [ ] What is the offset of OSCOVFSR (Oscillation Stabilization Flag)?
  - **Answer:**

#### Module Stop Control
- [ ] What is the offset of MSTPCRA (Module Stop Control A)?
  - **Answer:**
- [ ] What is the offset of MSTPCRB (Module Stop Control B)?
  - **Answer:**
- [ ] What is the offset of MSTPCRC (Module Stop Control C)?
  - **Answer:**
- [ ] What is the offset of MSTPCRD (Module Stop Control D)?
  - **Answer:**

#### Protection Register
- [ ] What is the offset of PRCR (Protect Register)?
  - **Answer:**

#### Memory Layout
- [ ] Are there reserved bytes between these registers?
  - **Answer:**
- [ ] What register sizes are used (8-bit, 16-bit, 32-bit)?
  - **Answer:**
- [ ] What is the total size of the SYSTEM register block?
  - **Answer:**

---

## 🟡 Priority 2: Critical for Motor Control

### 6. GPTW (General PWM Timer)
**File:** `lib/rx_hal/inc/rx72n_gptw_regs.h`
**Priority:** HIGH - Motor PWM generation!

#### Base Addresses
- [ ] What is the base address of GPTW common registers?
  - **Answer:**
- [ ] What is the base address of GPTW0?
  - **Answer:**
- [ ] What is the base address of GPTW1?
  - **Answer:**
- [ ] What is the base address of GPTW2?
  - **Answer:**
- [ ] What is the base address of GPTW3?
  - **Answer:**

#### GPTW Common Registers
- [ ] What is the offset of GTSTR (Start Register)?
  - **Answer:**
- [ ] What is the offset of GTSTP (Stop Register)?
  - **Answer:**
- [ ] What is the offset of GTCLR (Clear Register)?
  - **Answer:**
- [ ] What is the offset of GTSSR (Start Source Select)?
  - **Answer:**
- [ ] What is the offset of GTPSR (Stop Source Select)?
  - **Answer:**
- [ ] What is the offset of GTCSR (Clear Source Select)?
  - **Answer:**

#### GPTW Channel Registers (offsets from channel base)
- [ ] What is the offset of GTWP (Write Protect)?
  - **Answer:**
- [ ] What is the offset of GTSTR (Start)?
  - **Answer:**
- [ ] What is the offset of GTSTP (Stop)?
  - **Answer:**
- [ ] What is the offset of GTCLR (Clear)?
  - **Answer:**
- [ ] What is the offset of GTSSR (Start Source)?
  - **Answer:**
- [ ] What is the offset of GTPSR (Stop Source)?
  - **Answer:**
- [ ] What is the offset of GTCSR (Clear Source)?
  - **Answer:**
- [ ] What is the offset of GTUPSR (Up Count Source)?
  - **Answer:**
- [ ] What is the offset of GTDNSR (Down Count Source)?
  - **Answer:**
- [ ] What is the offset of GTICASR (Input Capture A Source)?
  - **Answer:**
- [ ] What is the offset of GTICBSR (Input Capture B Source)?
  - **Answer:**
- [ ] What is the offset of GTCR (Control)?
  - **Answer:**
- [ ] What is the offset of GTUDDTYC (Up/Down Count Control)?
  - **Answer:**
- [ ] What is the offset of GTIOR (I/O Control)?
  - **Answer:**
- [ ] What is the offset of GTINTAD (Interrupt ADC Start)?
  - **Answer:**
- [ ] What is the offset of GTST (Status)?
  - **Answer:**
- [ ] What is the offset of GTBER (Buffer Enable)?
  - **Answer:**
- [ ] What is the offset of GTITC (Interrupt Control)?
  - **Answer:**
- [ ] What is the offset of GTCNT (Counter)?
  - **Answer:**
- [ ] What is the offset of GTCCRA (Compare Capture A)?
  - **Answer:**
- [ ] What is the offset of GTCCRB (Compare Capture B)?
  - **Answer:**
- [ ] What is the offset of GTCCRC (Compare Capture C)?
  - **Answer:**
- [ ] What is the offset of GTCCRE (Compare Capture E)?
  - **Answer:**
- [ ] What is the offset of GTCCRD (Compare Capture D)?
  - **Answer:**
- [ ] What is the offset of GTCCRF (Compare Capture F)?
  - **Answer:**
- [ ] What is the offset of GTPR (Period)?
  - **Answer:**
- [ ] What is the offset of GTPBR (Period Buffer)?
  - **Answer:**
- [ ] What is the offset of GTPDBR (Period Double Buffer)?
  - **Answer:**
- [ ] What is the offset of GTADTRA (ADC Trigger A)?
  - **Answer:**
- [ ] What is the offset of GTADTBRA (ADC Trigger A Buffer)?
  - **Answer:**
- [ ] What is the offset of GTADTDBRA (ADC Trigger A Double Buffer)?
  - **Answer:**
- [ ] What is the offset of GTADTRB (ADC Trigger B)?
  - **Answer:**
- [ ] What is the offset of GTADTBRB (ADC Trigger B Buffer)?
  - **Answer:**
- [ ] What is the offset of GTADTDBRB (ADC Trigger B Double Buffer)?
  - **Answer:**

#### Memory Layout
- [ ] What is the total size of a GPTW channel register block?
  - **Answer:**
- [ ] How many bytes between GPTW0 base and GPTW1 base?
  - **Answer:**
- [ ] Are there reserved bytes within the channel block?
  - **Answer:**
- [ ] What register sizes are used (8-bit, 16-bit, 32-bit)?
  - **Answer:**

---

### 7. MTU3a (Multi-Function Timer Pulse Unit)
**File:** `lib/rx_hal/inc/rx72n_mtu_regs.h`
**Priority:** HIGH - Encoder phase counting!

#### Base Addresses
- [ ] What is the base address of MTU common registers?
  - **Answer:**
- [ ] What is the base address of MTU0?
  - **Answer:**
- [ ] What is the base address of MTU1?
  - **Answer:**
- [ ] What is the base address of MTU2?
  - **Answer:**
- [ ] What is the base address of MTU3?
  - **Answer:**
- [ ] What is the base address of MTU4?
  - **Answer:**
- [ ] What is the base address of MTU5?
  - **Answer:**
- [ ] What is the base address of MTU6?
  - **Answer:**
- [ ] What is the base address of MTU7?
  - **Answer:**

#### MTU Common Registers
- [ ] What is the offset of TOERA (Timer Output Master Enable A)?
  - **Answer:**
- [ ] What is the offset of TOERB (Timer Output Master Enable B)?
  - **Answer:**
- [ ] What is the offset of TOCR1A (Timer Output Control 1A)?
  - **Answer:**
- [ ] What is the offset of TOCR1B (Timer Output Control 1B)?
  - **Answer:**
- [ ] What is the offset of TOCR2A (Timer Output Control 2A)?
  - **Answer:**
- [ ] What is the offset of TOCR2B (Timer Output Control 2B)?
  - **Answer:**
- [ ] What is the offset of TCDRA (Timer Cycle Data A)?
  - **Answer:**
- [ ] What is the offset of TCDRB (Timer Cycle Data B)?
  - **Answer:**
- [ ] What is the offset of TCBRA (Timer Cycle Buffer A)?
  - **Answer:**
- [ ] What is the offset of TCBRB (Timer Cycle Buffer B)?
  - **Answer:**
- [ ] What is the offset of TITCR1A (Timer Interrupt Control 1A)?
  - **Answer:**
- [ ] What is the offset of TITCR1B (Timer Interrupt Control 1B)?
  - **Answer:**
- [ ] What is the offset of TITCR2A (Timer Interrupt Control 2A)?
  - **Answer:**
- [ ] What is the offset of TITCR2B (Timer Interrupt Control 2B)?
  - **Answer:**
- [ ] What is the offset of TITCNT1A (Timer Interrupt Count 1A)?
  - **Answer:**
- [ ] What is the offset of TITCNT1B (Timer Interrupt Count 1B)?
  - **Answer:**
- [ ] What is the offset of TITCNT2A (Timer Interrupt Count 2A)?
  - **Answer:**
- [ ] What is the offset of TITCNT2B (Timer Interrupt Count 2B)?
  - **Answer:**
- [ ] What is the offset of TBTER (Timer Buffer Transfer Enable)?
  - **Answer:**
- [ ] What is the offset of TDER (Timer DMA Transfer Enable)?
  - **Answer:**
- [ ] What is the offset of TWCR (Timer Waveform Control)?
  - **Answer:**
- [ ] What is the offset of TSTR (Timer Start)?
  - **Answer:**
- [ ] What is the offset of TSYR (Timer Synchronization)?
  - **Answer:**
- [ ] What is the offset of TRWERA (Timer Read/Write Enable A)?
  - **Answer:**
- [ ] What is the offset of TRWERB (Timer Read/Write Enable B)?
  - **Answer:**

#### MTU Channel Registers (for MTU0-4, offsets from channel base)
- [ ] What is the offset of TCR (Timer Control)?
  - **Answer:**
- [ ] What is the offset of TMDR1 (Timer Mode 1)?
  - **Answer:**
- [ ] What is the offset of TIORH (Timer I/O Control H)?
  - **Answer:**
- [ ] What is the offset of TIORL (Timer I/O Control L)?
  - **Answer:**
- [ ] What is the offset of TIER (Timer Interrupt Enable)?
  - **Answer:**
- [ ] What is the offset of TSR (Timer Status)?
  - **Answer:**
- [ ] What is the offset of TCNT (Timer Counter)?
  - **Answer:**
- [ ] What is the offset of TGRA (Timer General Register A)?
  - **Answer:**
- [ ] What is the offset of TGRB (Timer General Register B)?
  - **Answer:**
- [ ] What is the offset of TGRC (Timer General Register C)?
  - **Answer:**
- [ ] What is the offset of TGRD (Timer General Register D)?
  - **Answer:**

#### Memory Layout
- [ ] What is the total size of an MTU channel register block?
  - **Answer:**
- [ ] How many bytes between MTU0 base and MTU1 base?
  - **Answer:**
- [ ] Are MTU0-4 layouts identical?
  - **Answer:**
- [ ] Are MTU5-7 layouts different? If so, how?
  - **Answer:**
- [ ] Are there reserved bytes within channel blocks?
  - **Answer:**
- [ ] What register sizes are used (8-bit, 16-bit, 32-bit)?
  - **Answer:**

---

## 🟢 Priority 3: Critical for Communication

### 8. RSPI (Renesas SPI)
**File:** `lib/rx_hal/inc/rx72n_rspi_regs.h`
**Priority:** HIGH - Communication with RPi5!

#### Base Addresses
- [ ] What is the base address of RSPI0?
  - **Answer:**
- [ ] What is the base address of RSPI1?
  - **Answer:**
- [ ] What is the base address of RSPI2?
  - **Answer:**

#### RSPI Registers (offsets from channel base)
- [ ] What is the offset of SPCR (Control)?
  - **Answer:**
- [ ] What is the offset of SSLP (Slave Select Polarity)?
  - **Answer:**
- [ ] What is the offset of SPPCR (Pin Control)?
  - **Answer:**
- [ ] What is the offset of SPSR (Status)?
  - **Answer:**
- [ ] What is the offset of SPDR (Data)?
  - **Answer:**
- [ ] What is the offset of SPSCR (Sequence Control)?
  - **Answer:**
- [ ] What is the offset of SPSSR (Sequence Status)?
  - **Answer:**
- [ ] What is the offset of SPBR (Bit Rate)?
  - **Answer:**
- [ ] What is the offset of SPDCR (Data Control)?
  - **Answer:**
- [ ] What is the offset of SPCKD (Clock Delay)?
  - **Answer:**
- [ ] What is the offset of SSLND (Slave Select Negation Delay)?
  - **Answer:**
- [ ] What is the offset of SPND (Next Access Delay)?
  - **Answer:**
- [ ] What is the offset of SPCR2 (Control 2)?
  - **Answer:**
- [ ] What is the offset of SPCMD0 (Command 0)?
  - **Answer:**
- [ ] How many SPCMD registers are there (SPCMD0-7)?
  - **Answer:**
- [ ] What is the offset of SPBFCR (Buffer Control)?
  - **Answer:**
- [ ] What is the offset of SPBFDR (Buffer Data Count)?
  - **Answer:**

#### Memory Layout
- [ ] What is the total size of an RSPI register block?
  - **Answer:**
- [ ] How many bytes between RSPI0 base and RSPI1 base?
  - **Answer:**
- [ ] Are there reserved bytes between registers?
  - **Answer:**
- [ ] What register sizes are used (8-bit, 16-bit, 32-bit)?
  - **Answer:**
- [ ] What size is SPDR (8, 16, or 32-bit)?
  - **Answer:**

---

### 9. SCI (Serial Communication Interface)
**File:** `lib/rx_hal/inc/rx72n_sci_regs.h`
**Priority:** HIGH - UART communication (debug, etc)!

#### Base Addresses
- [ ] What is the base address of SCI0?
  - **Answer:**
- [ ] What is the base address of SCI1?
  - **Answer:**
- [ ] What is the base address of SCI2?
  - **Answer:**
- [ ] What is the base address of SCI5?
  - **Answer:**
- [ ] What is the base address of SCI9?
  - **Answer:**
- [ ] What is the base address of SCI12?
  - **Answer:**

#### SCI Registers (offsets from channel base)
- [ ] What is the offset of SMR (Serial Mode)?
  - **Answer:**
- [ ] What is the offset of BRR (Bit Rate)?
  - **Answer:**
- [ ] What is the offset of SCR (Serial Control)?
  - **Answer:**
- [ ] What is the offset of TDR (Transmit Data)?
  - **Answer:**
- [ ] What is the offset of SSR (Serial Status)?
  - **Answer:**
- [ ] What is the offset of RDR (Receive Data)?
  - **Answer:**
- [ ] What is the offset of SCMR (Smart Card Mode)?
  - **Answer:**
- [ ] What is the offset of SEMR (Serial Extended Mode)?
  - **Answer:**
- [ ] What is the offset of SNFR (Noise Filter)?
  - **Answer:**
- [ ] What is the offset of SIMR1 (I2C Mode 1)?
  - **Answer:**
- [ ] What is the offset of SIMR2 (I2C Mode 2)?
  - **Answer:**
- [ ] What is the offset of SIMR3 (I2C Mode 3)?
  - **Answer:**
- [ ] What is the offset of SISR (I2C Status)?
  - **Answer:**
- [ ] What is the offset of SPMR (SPI Mode)?
  - **Answer:**
- [ ] What is the offset of FTDR (FIFO Transmit Data)?
  - **Answer:**
- [ ] What is the offset of FRDR (FIFO Receive Data)?
  - **Answer:**

#### Memory Layout
- [ ] What is the total size of a SCI register block?
  - **Answer:**
- [ ] How many bytes between SCI0 base and SCI1 base?
  - **Answer:**
- [ ] Are there reserved bytes between registers?
  - **Answer:**
- [ ] What register sizes are used (8-bit, 16-bit)?
  - **Answer:**

---

### 10. RIIC (I2C Bus Interface)
**File:** `lib/rx_hal/inc/rx72n_riic_regs.h`
**Priority:** MEDIUM - I2C communication (battery monitor, etc)

#### Base Addresses
- [ ] What is the base address of RIIC0?
  - **Answer:**
- [ ] What is the base address of RIIC1?
  - **Answer:**
- [ ] What is the base address of RIIC2?
  - **Answer:**

#### RIIC Registers (offsets from channel base)
- [ ] What is the offset of ICCR1 (Control 1)?
  - **Answer:**
- [ ] What is the offset of ICCR2 (Control 2)?
  - **Answer:**
- [ ] What is the offset of ICMR1 (Mode 1)?
  - **Answer:**
- [ ] What is the offset of ICMR2 (Mode 2)?
  - **Answer:**
- [ ] What is the offset of ICMR3 (Mode 3)?
  - **Answer:**
- [ ] What is the offset of ICFER (Function Enable)?
  - **Answer:**
- [ ] What is the offset of ICSER (Status Enable)?
  - **Answer:**
- [ ] What is the offset of ICIER (Interrupt Enable)?
  - **Answer:**
- [ ] What is the offset of ICSR1 (Status 1)?
  - **Answer:**
- [ ] What is the offset of ICSR2 (Status 2)?
  - **Answer:**
- [ ] What is the offset of SARL0 (Slave Address L0)?
  - **Answer:**
- [ ] What is the offset of SARU0 (Slave Address U0)?
  - **Answer:**
- [ ] What is the offset of SARL1 (Slave Address L1)?
  - **Answer:**
- [ ] What is the offset of SARU1 (Slave Address U1)?
  - **Answer:**
- [ ] What is the offset of SARL2 (Slave Address L2)?
  - **Answer:**
- [ ] What is the offset of SARU2 (Slave Address U2)?
  - **Answer:**
- [ ] What is the offset of ICBRL (Bit Rate L)?
  - **Answer:**
- [ ] What is the offset of ICBRH (Bit Rate H)?
  - **Answer:**
- [ ] What is the offset of ICDRT (Data Transmit)?
  - **Answer:**
- [ ] What is the offset of ICDRR (Data Receive)?
  - **Answer:**

#### Memory Layout
- [ ] What is the total size of a RIIC register block?
  - **Answer:**
- [ ] How many bytes between RIIC0 base and RIIC1 base?
  - **Answer:**
- [ ] Are there reserved bytes between registers?
  - **Answer:**
- [ ] What register sizes are used (8-bit, 16-bit)?
  - **Answer:**

---

## 🔵 Priority 4: Other Peripherals

### 11. ADC (A/D Converter)
**File:** `lib/rx_hal/inc/rx72n_adc_regs.h`
**Priority:** MEDIUM - Analog readings (current sense, etc)

#### Base Addresses
- [ ] What is the base address of ADC12 (12-bit A/D)?
  - **Answer:**
- [ ] What is the base address of ADC12.S12ADI0?
  - **Answer:**
- [ ] What is the base address of ADC12.S12GBADI?
  - **Answer:**
- [ ] What is the base address of ADC12.S12GCADI?
  - **Answer:**

#### ADC Registers (offsets from base)
- [ ] What is the offset of ADCSR (Control/Status)?
  - **Answer:**
- [ ] What is the offset of ADANSA (Channel Select A)?
  - **Answer:**
- [ ] What is the offset of ADANSB (Channel Select B)?
  - **Answer:**
- [ ] What is the offset of ADANSC (Channel Select C)?
  - **Answer:**
- [ ] What is the offset of ADADS (A/D Disconnect Detection)?
  - **Answer:**
- [ ] What is the offset of ADCER (Control Extended)?
  - **Answer:**
- [ ] What is the offset of ADSTRGR (Start Trigger)?
  - **Answer:**
- [ ] What is the offset of ADEXICR (Extended Input Control)?
  - **Answer:**
- [ ] What is the offset of ADGSPCR (Group Scan Priority Control)?
  - **Answer:**
- [ ] What is the offset of ADDBLDRA (Data Duplexing A)?
  - **Answer:**
- [ ] What is the offset of ADDBLDRB (Data Duplexing B)?
  - **Answer:**
- [ ] What is the offset of ADTSDR (Temperature Sensor Data)?
  - **Answer:**
- [ ] What is the offset of ADOCDR (Internal Reference Voltage Data)?
  - **Answer:**
- [ ] What is the offset of ADDR0 (first data register)?
  - **Answer:**
- [ ] How many ADDR registers are there (ADDR0-31)?
  - **Answer:**

#### Memory Layout
- [ ] What is the total size of the ADC register block?
  - **Answer:**
- [ ] Are there reserved bytes between registers?
  - **Answer:**
- [ ] What register sizes are used (8-bit, 16-bit, 32-bit)?
  - **Answer:**
- [ ] What size are ADDR registers (16-bit)?
  - **Answer:**

---

### 12. CRC (CRC Calculator)
**File:** `lib/rx_hal/inc/rx72n_crc_regs.h`
**Priority:** MEDIUM - Hardware CRC for communication

#### Base Address
- [ ] What is the base address of CRC?
  - **Answer:**

#### CRC Registers (offsets from base)
- [ ] What is the offset of CRCCR (Control)?
  - **Answer:**
- [ ] What is the offset of CRCDIR (Data Input)?
  - **Answer:**
- [ ] What is the offset of CRCDOR (Data Output)?
  - **Answer:**
- [ ] What is the offset of CRCDCR (Data Control)?
  - **Answer:**

#### Memory Layout
- [ ] What is the total size of the CRC register block?
  - **Answer:**
- [ ] Are there reserved bytes between registers?
  - **Answer:**
- [ ] What register sizes are used (8-bit, 16-bit, 32-bit)?
  - **Answer:**
- [ ] What size is CRCDIR (8, 16, or 32-bit)?
  - **Answer:**
- [ ] What size is CRCDOR (16 or 32-bit)?
  - **Answer:**

---

### 13. IWDT (Independent Watchdog Timer)
**File:** `lib/rx_hal/inc/rx72n_iwdt_regs.h`
**Priority:** MEDIUM - Safety watchdog

#### Base Address
- [ ] What is the base address of IWDT?
  - **Answer:**

#### IWDT Registers (offsets from base)
- [ ] What is the offset of IWDTCR (Control)?
  - **Answer:**
- [ ] What is the offset of IWDTSR (Status)?
  - **Answer:**
- [ ] What is the offset of IWDTRCR (Reset Control)?
  - **Answer:**
- [ ] What is the offset of IWDTCSTPR (Count Stop Control)?
  - **Answer:**

#### Memory Layout
- [ ] What is the total size of the IWDT register block?
  - **Answer:**
- [ ] Are there reserved bytes between registers?
  - **Answer:**
- [ ] What register sizes are used (8-bit, 16-bit)?
  - **Answer:**

---

### 14. USB (USB 2.0 Full-Speed)
**File:** `lib/rx_hal/inc/rx72n_usb_regs.h`
**Priority:** MEDIUM - USB CDC communication

#### Base Address
- [ ] What is the base address of USB0?
  - **Answer:**

#### USB Registers (offsets from base)
- [ ] What is the offset of SYSCFG (System Configuration)?
  - **Answer:**
- [ ] What is the offset of SYSSTS (System Status)?
  - **Answer:**
- [ ] What is the offset of DVSTCTR (Device State Control)?
  - **Answer:**
- [ ] What is the offset of CFIFO (CFIFO Port)?
  - **Answer:**
- [ ] What is the offset of D0FIFO (D0FIFO Port)?
  - **Answer:**
- [ ] What is the offset of D1FIFO (D1FIFO Port)?
  - **Answer:**
- [ ] What is the offset of CFIFOSEL (CFIFO Select)?
  - **Answer:**
- [ ] What is the offset of CFIFOCTR (CFIFO Control)?
  - **Answer:**
- [ ] What is the offset of D0FIFOSEL (D0FIFO Select)?
  - **Answer:**
- [ ] What is the offset of D0FIFOCTR (D0FIFO Control)?
  - **Answer:**
- [ ] What is the offset of D1FIFOSEL (D1FIFO Select)?
  - **Answer:**
- [ ] What is the offset of D1FIFOCTR (D1FIFO Control)?
  - **Answer:**
- [ ] What is the offset of INTENB0 (Interrupt Enable 0)?
  - **Answer:**
- [ ] What is the offset of INTENB1 (Interrupt Enable 1)?
  - **Answer:**
- [ ] What is the offset of BRDYENB (Buffer Ready Interrupt Enable)?
  - **Answer:**
- [ ] What is the offset of NRDYENB (Buffer Not Ready Interrupt Enable)?
  - **Answer:**
- [ ] What is the offset of BEMPENB (Buffer Empty Interrupt Enable)?
  - **Answer:**
- [ ] What is the offset of INTSTS0 (Interrupt Status 0)?
  - **Answer:**
- [ ] What is the offset of INTSTS1 (Interrupt Status 1)?
  - **Answer:**
- [ ] What is the offset of BRDYSTS (Buffer Ready Status)?
  - **Answer:**
- [ ] What is the offset of NRDYSTS (Buffer Not Ready Status)?
  - **Answer:**
- [ ] What is the offset of BEMPSTS (Buffer Empty Status)?
  - **Answer:**
- [ ] What is the offset of FRMNUM (Frame Number)?
  - **Answer:**
- [ ] What is the offset of DCPCFG (DCP Configuration)?
  - **Answer:**
- [ ] What is the offset of DCPMAXP (DCP Max Packet)?
  - **Answer:**
- [ ] What is the offset of DCPCTR (DCP Control)?
  - **Answer:**
- [ ] What is the offset of PIPESEL (Pipe Select)?
  - **Answer:**
- [ ] What is the offset of PIPECFG (Pipe Configuration)?
  - **Answer:**
- [ ] What is the offset of PIPEMAXP (Pipe Max Packet)?
  - **Answer:**
- [ ] What is the offset of PIPECTR (first Pipe Control register)?
  - **Answer:**
- [ ] How many PIPECTR registers exist (PIPE1-9)?
  - **Answer:**

#### Memory Layout
- [ ] What is the total size of the USB register block?
  - **Answer:**
- [ ] Are there reserved bytes between registers?
  - **Answer:**
- [ ] What register sizes are used (8-bit, 16-bit, 32-bit)?
  - **Answer:**

---

## 📝 After Completing Verification

For each peripheral where you found answers:

1. **Compare with current implementation** in `lib/rx_hal/inc/rx72n_*_regs.h`
2. **Fix any mismatches** in struct definitions
3. **Add static assertions** to prevent future regressions:

```c
#include <stddef.h>

/* Compile-Time Register Layout Verification */
_Static_assert(offsetof(peripheral_regs_t, critical_reg1) == 0xXX,
               "REG1 must be at offset 0xXX (address 0xBASE+XX)");
_Static_assert(offsetof(peripheral_regs_t, critical_reg2) == 0xYY,
               "REG2 must be at offset 0xYY (address 0xBASE+YY)");
_Static_assert(sizeof(peripheral_regs_t) == EXPECTED_SIZE,
               "Peripheral struct size must be XXX bytes");
```

4. **Test build** - Verify all assertions pass
5. **Update this checklist** - Mark peripheral as verified

---

## 📚 References

- **RX72N Group Hardware Manual:** R01UH0951EJ0100 (or latest version)
- **MPC Bug (Issue #43):** Example of critical offset mismatch
- **PR #143:** MPC register fix with static assertions
