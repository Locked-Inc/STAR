from functional import seq
from enum import Enum, auto, unique
import itertools
import json
import os
from datetime import date

MAX_SOLUTIONS_TO_SAVE = 1

# 100-pin LFQFP (PLQP0100KB-B) pin-to-port mapping with full alternate function names
# Source: Renesas Smart Configurator report for R5F572NDDxFP (100-pin)
# Chip: R5F572NNHGFP#30
pin_to_port = {
    1: "AVCC1",
    2: "EMLE",
    3: "AVSS1",
    4: "PJ3/EDACK1/MTIOC3C/CTS6#/RTS6#/SS6#/CTS0#/RTS0#/SS0#/SSITXD0/ET0_EXOUT",
    5: "VCL",
    6: "VBATT",
    7: "MD/FINED",
    8: "XCIN",
    9: "XCOUT",
    10: "RES#",
    11: "P37/XTAL",
    12: "VSS",
    13: "P36/EXTAL",
    14: "VCC",
    15: "P35/UPSEL/NMI",
    16: "P34/TRST#/MTIOC0A/TMCI3/PO12/POE10#/SCK6/SCK0/ET0_LINKSTA/IRQ4",
    17: "P33/EDREQ1/MTIOC0D/TIOCD0/TMRI3/PO11/POE4#/POE11#/RXD6/SMISO6/SSCL6/RXD0/SMISO0/SSCL0/CRX0/IRQ3",
    18: "P32/MTIOC0C/TIOCC0/TMO3/PO10/RTCIC2/RTCOUT/POE0#/POE10#/TXD6/SMOSI6/SSDA6/TXD0/SMOSI0/SSDA0/CTX0/USB0_VBUSEN/IRQ2",
    19: "P31/TMS/MTIOC4D/TMCI2/PO9/RTCIC1/CTS1#/RTS1#/SS1#/SSLB0-A/IRQ1",
    20: "P30/TDI/MTIOC4B/TMRI3/PO8/RTCIC0/POE8#/RXD1/SMISO1/SSCL1/MISOB-A/IRQ0",
    21: "P27/TCK/CS7#/MTIOC2B/TMCI3/PO7/SCK1/RSPCKB-A",
    22: "P26/TDO/CS6#/MTIOC2A/TMO1/PO6/TXD1/SMOSI1/SSDA1/CTS3#/RTS3#/SS3#/MOSIB-A",
    23: "P25/CLKOUT/CS5#/EDACK1/MTIOC4C/MTCLKB/TIOCA4/PO5/RXD3/SMISO3/SSCL3/SSIDATA1/ADTRG0#",
    24: "P24/CS4#/EDREQ1/MTIOC4A/MTCLKA/TIOCB4/TMRI1/PO4/SCK3/USB0_VBUSEN/SSIBCK1",
    25: "P23/EDACK0/MTIOC3D/MTCLKD/TIOCD3/PO3/GTIOC0A/TXD3/SMOSI3/SSDA3/CTS0#/RTS0#/SS0#/CTX1/SSIBCK0",
    26: "P22/EDREQ0/MTIOC3B/MTCLKC/TIOCC3/TMO0/PO2/GTIOC1A/SCK0/USB0_OVRCURB/AUDIO_CLK",
    27: "P21/MTIOC1B/MTIOC4A/TIOCA3/TMCI0/PO1/GTIOC2A/RXD0/SMISO0/SSCL0/USB0_EXICEN/SSILRCK0/SCL1/IRQ9",
    28: "P20/MTIOC1A/TIOCB3/TMRI0/PO0/TXD0/SMOSI0/SSDA0/USB0_ID/SSIRXD0/SDA1/IRQ8",
    29: "P17/MTIOC3A/MTIOC3B/MTIOC4B/TIOCB0/TCLKD/TMO1/PO15/POE8#/GTIOC0B/SCK1/TXD3/SMOSI3/SSDA3/SDA2/SSITXD0/EPLSOUT0/IRQ7/ADTRG1#",
    30: "P16/MTIOC3C/MTIOC3D/TIOCB1/TCLKC/TMO2/PO14/RTCOUT/TXD1/SMOSI1/SSDA1/RXD3/SMISO3/SSCL3/SCL2/USB0_VBUSEN/USB0_VBUS/USB0_OVRCURB/IRQ6/ADTRG0#",
    31: "P15/MTIOC0B/MTCLKB/TIOCB2/TCLKB/TMCI2/PO13/GTETRGA/RXD1/SMISO1/SSCL1/SCK3/CRX1/SSILRCK1/IRQ5",
    32: "P14/MTIOC3A/MTCLKA/TIOCB5/TCLKA/TMRI2/PO15/GTETRGD/CTS1#/RTS1#/SS1#/CTX1/USB0_OVRCURA/IRQ4",
    33: "P13/MTIOC0B/TIOCA5/TMO3/PO13/GTADSM1/TXD2/SMOSI2/SSDA2/SDA0/IRQ3/ADTRG1#",
    34: "P12/TMCI1/GTADSM0/RXD2/SMISO2/SSCL2/SCL0/IRQ2",
    35: "VCC_USB",
    36: "USB0_DM",
    37: "USB0_DP",
    38: "VSS_USB",
    39: "P55/D0/WAIT#/EDREQ0/MTIOC4D/TMO3/CRX1/ET0_EXOUT/IRQ10",
    40: "P54/ALE/D1/EDACK0/MTIOC4B/TMCI1/CTS2#/RTS2#/SS2#/CTX1/ET0_LINKSTA",
    41: "P53/BCLK",
    42: "P52/RD#/RXD2/SMISO2/SSCL2/SSLB3-A",
    43: "P51/WR1#/BC1#/WAIT#/SCK2/SSLB2-A",
    44: "P50/WR0#/WR#/TXD2/SMOSI2/SSDA2/SSLB1-A",
    45: "PC7/UB/A23/CS0#/MTIOC3A/MTCLKB/TMO2/PO31/TOC0/CACREF/GTIOC3A/TXD8/SMOSI8/SSDA8/SMOSI10/SSDA10/TXD10/MISOA-A/ET0_COL/IRQ14",
    46: "PC6/D2/A22/CS1#/MTIOC3C/MTCLKA/TMCI2/PO30/TIC0/GTIOC3B/RXD8/SMISO8/SSCL8/SMISO10/SSCL10/RXD10/MOSIA-A/ET0_ETXD3/IRQ13",
    47: "PC5/D3/A21/CS2#/WAIT#/MTIOC3B/MTCLKD/TMRI2/PO29/GTIOC1A/SCK8/RTS8#/SCK10/RSPCKA-A/ET0_ETXD2",
    48: "PC4/A20/CS3#/MTIOC3D/MTCLKC/TMCI1/PO25/POE0#/GTETRGC/SCK5/CTS8#/SS8#/SS10#/CTS10#/RTS10#/SSLA0-A/ET0_TX_CLK",
    49: "PC3/A19/MTIOC4D/TCLKB/PO24/GTIOC1B/TXD5/SMOSI5/SSDA5/ET0_TX_ER",
    50: "PC2/A18/MTIOC4B/TCLKA/PO21/GTIOC2B/RXD5/SMISO5/SSCL5/SSLA3-A/ET0_RX_DV",
    51: "PC1/A17/MTIOC3A/TCLKD/PO18/SCK5/SSLA2-A/ET0_ERXD2/IRQ12",
    52: "PC0/A16/MTIOC3C/TCLKC/PO17/CTS5#/RTS5#/SS5#/SSLA1-A/ET0_ERXD3/IRQ14",
    53: "PB7/A15/MTIOC3B/TIOCB5/PO31/TXD9/SMOSI9/SSDA9/SMOSI11/SSDA11/TXD11/ET0_CRS/RMII0_CRS_DV",
    54: "PB6/A14/MTIOC3D/TIOCA5/PO30/RXD9/SMISO9/SSCL9/SMISO11/SSCL11/RXD11/ET0_ETXD1/RMII0_TXD1",
    55: "PB5/A13/MTIOC2A/MTIOC1B/TIOCB4/TMRI1/PO29/POE4#/SCK9/RTS9#/SCK11/ET0_ETXD0/RMII0_TXD0/LCD_CLK-B",
    56: "PB4/A12/TIOCA4/PO28/CTS9#/SS9#/SS11#/CTS11#/RTS11#/ET0_TX_EN/RMII0_TXD_EN/LCD_TCON0-B",
    57: "PB3/A11/MTIOC0A/MTIOC4A/TIOCD3/TCLKD/TMO0/PO27/POE11#/SCK6/ET0_RX_ER/RMII0_RX_ER/LCD_TCON1-B",
    58: "PB2/A10/TIOCC3/TCLKC/PO26/CTS6#/RTS6#/SS6#/ET0_RX_CLK/REF50CK0/LCD_TCON2-B",
    59: "PB1/A9/MTIOC0C/MTIOC4C/TIOCB3/TMCI0/PO25/TXD6/SMOSI6/SSDA6/ET0_ERXD0/RMII0_RXD0/LCD_TCON3-B/IRQ4",
    60: "VCC",
    61: "PB0/A8/MTIC5W/TIOCA3/PO24/RXD6/SMISO6/SSCL6/ET0_ERXD1/RMII0_RXD1/LCD_DATA0-B/IRQ12",
    62: "VSS",
    63: "PA7/A7/TIOCB2/PO23/MISOA-B/ET0_WOL/LCD_DATA1-B",
    64: "PA6/A6/MTIC5V/MTCLKB/TIOCA2/TMCI3/PO22/POE10#/GTETRGB/CTS5#/RTS5#/SS5#/MOSIA-B/ET0_EXOUT/LCD_DATA2-B",
    65: "PA5/A5/MTIOC6B/TIOCB1/PO21/GTIOC0A/RSPCKA-B/ET0_LINKSTA/LCD_DATA3-B",
    66: "PA4/A4/MTIC5U/MTCLKA/TIOCA1/TMRI0/PO20/TXD5/SMOSI5/SSDA5/SSLA0-B/ET0_MDC/PMGI0_MDC/LCD_DATA4-B/IRQ5",
    67: "PA3/A3/MTIOC0D/MTCLKD/TIOCD0/TCLKB/PO19/RXD5/SMISO5/SSCL5/ET0_MDIO/PMGI0_MDIO/LCD_DATA5-B/IRQ6",
    68: "PA2/A2/MTIOC7A/PO18/GTIOC1A/RXD5/SMISO5/SSCL5/SSLA3-B/LCD_DATA6-B",
    69: "PA1/A1/MTIOC0B/MTCLKC/MTIOC7B/TIOCB0/PO17/GTIOC2A/SCK5/SSLA2-B/ET0_WOL/LCD_DATA7-B/IRQ11",
    70: "PA0/BC0#/A0/MTIOC4A/MTIOC6D/TIOCA0/PO16/CACREF/GTIOC0B/SSLA1-B/ET0_TX_EN/RMII0_TXD_EN/LCD_DATA8-B",
    71: "PE7/D15/D7/MTIOC6A/TOC1/GTIOC3A/MISOB-B/SDHI_WP/MMC_RES#-B/LCD_DATA9-B/IRQ7/AN105",
    72: "PE6/D14/D6/MTIOC6C/TIC1/GTIOC3B/MOSIB-B/SDHI_CD/MMC_CD-B/LCD_DATA10-B/IRQ6/AN104",
    73: "PE5/D13/D5/MTIOC4C/MTIOC2B/GTIOC0A/RSPCKB-B/ET0_RX_CLK/REF50CK0/LCD_DATA11-B/IRQ5/AN103",
    74: "PE4/D12/D4/MTIOC4D/MTIOC1A/PO28/GTIOC1A/SSLB0-B/ET0_ERXD2/LCD_DATA12-B/AN102",
    75: "PE3/D11/D3/MTIOC4B/PO26/TOC3/POE8#/GTIOC2A/CTS12#/RTS12#/SS12#/ET0_ERXD3/MMC_D7-B/LCD_DATA13-B/AN101",
    76: "PE2/D10/D2/MTIOC4A/PO23/TIC3/GTIOC0B/RXD12/SMISO12/SSCL12/RXDX12/SSLB3-B/MMC_D6-B/LCD_DATA14-B/IRQ7/AN100",
    77: "PE1/D9/D1/MTIOC4C/MTIOC3B/PO18/GTIOC1B/TXD12/SMOSI12/SSDA12/TXDX12/SIOX12/SSLB2-B/MMC_D5-B/LCD_DATA15-B/ANEX1",
    78: "PE0/D8/D0/MTIOC3D/GTIOC2B/SCK12/SSLB1-B/MMC_D4-B/LCD_DATA16-B/ANEX0",
    79: "PD7/D7/MTIC5U/POE0#/SSLC3-A/QMI-B/QIO1-B/SDHI_D1-B/MMC_D1-B/LCD_DATA17-B/IRQ7/AN107",
    80: "PD6/D6/MTIC5V/MTIOC8A/POE4#/SSLC2-A/QMO-B/QIO0-B/SDHI_D0-B/MMC_D0-B/LCD_DATA18-B/IRQ6/AN106",
    81: "PD5/D5/MTIC5W/MTIOC8C/MTCLKA/POE10#/SSLC1-A/QSPCLK-B/SDHI_CLK-B/MMC_CLK-B/LCD_DATA19-B/IRQ5/AN113",
    82: "PD4/D4/MTIOC8B/POE11#/SSLC0-A/QSSL-B/SDHI_CMD-B/MMC_CMD-B/LCD_DATA20-B/IRQ4/AN112",
    83: "PD3/D3/MTIOC8D/TOC2/POE8#/GTIOC0A/RSPCKC-A/QIO3-B/SDHI_D3-B/MMC_D3-B/LCD_DATA21-B/IRQ3/AN111",
    84: "PD2/D2/MTIOC4D/TIC2/GTIOC0B/MISOC-A/CRX0/QIO2-B/SDHI_D2-B/MMC_D2-B/LCD_DATA22-B/IRQ2/AN110",
    85: "PD1/D1/MTIOC4B/POE0#/GTIOC1A/MOSIC-A/CTX0/LCD_DATA23-B/IRQ1/AN109",
    86: "PD0/D0/POE4#/GTIOC1B/LCD_EXTCLK-B/IRQ0/AN108",
    87: "P47/IRQ15/AN007",
    88: "P46/IRQ14/AN006",
    89: "P45/IRQ13/AN005",
    90: "P44/IRQ12/AN004",
    91: "P43/IRQ11/AN003",
    92: "P42/IRQ10/AN002",
    93: "P41/IRQ9/AN001",
    94: "VREFL0",
    95: "P40/IRQ8/AN000",
    96: "VREFH0",
    97: "AVCC0",
    98: "P07/IRQ15/ADTRG0#",
    99: "AVSS0",
    100: "P05/SSILRCK1/IRQ13/DA1",
}


def port_short_name(pin_num):
    """Extract primary port name (first component before '/') for display."""
    return pin_to_port.get(pin_num, "???").split("/")[0]


# MPC pin function names from Chapter 23, Table 23.1
# Maps pin_number -> {function_type: MPC_name}
# Covers all pins in gptw_details and spi_details so any solver solution
# can look up its MPC names.  RSPI0=A suffix, RSPI1=B, RSPI2=C.
pin_mpc_function = {
    # ── GPTW PWM (all alternative pins per channel) ──
    # GPTIOC0 subchannel A
    25: {"GPTW_PWM": "GTIOC0A", "MTU_CLK": "MTCLKD"},
    65: {"GPTW_PWM": "GTIOC0A", "SPI_RSPCK": "RSPCKA"},
    73: {"GPTW_PWM": "GTIOC0A", "SPI_RSPCK": "RSPCKB"},
    83: {"GPTW_PWM": "GTIOC0A", "SPI_RSPCK": "RSPCKC"},
    # GPTIOC0 subchannel B
    29: {"GPTW_PWM": "GTIOC0B", "TPU_CLK": "TCLKD"},
    70: {"GPTW_PWM": "GTIOC0B", "SPI_CS": "SSLA1"},
    76: {"GPTW_PWM": "GTIOC0B", "SPI_CS": "SSLB3", "SIMPLE_SPI_MISO": "SMISO12"},
    84: {"GPTW_PWM": "GTIOC0B", "SPI_CIPO": "MISOC"},
    # GPTIOC1 subchannel A
    26: {"GPTW_PWM": "GTIOC1A", "MTU_CLK": "MTCLKC"},
    47: {"GPTW_PWM": "GTIOC1A", "SPI_RSPCK": "RSPCKA", "MTU_CLK": "MTCLKD"},
    68: {"GPTW_PWM": "GTIOC1A", "SPI_CS": "SSLA3"},
    74: {"GPTW_PWM": "GTIOC1A", "SPI_CS": "SSLB0"},
    85: {"GPTW_PWM": "GTIOC1A", "SPI_COPI": "MOSIC"},
    # GPTIOC1 subchannel B
    49: {"GPTW_PWM": "GTIOC1B", "TPU_CLK": "TCLKB"},
    77: {"GPTW_PWM": "GTIOC1B", "SPI_CS": "SSLB2", "SIMPLE_SPI_MOSI": "SMOSI12"},
    86: {"GPTW_PWM": "GTIOC1B"},
    # GPTIOC2 subchannel A
    27: {"GPTW_PWM": "GTIOC2A", "IIC_SCL": "SCL1"},
    69: {"GPTW_PWM": "GTIOC2A", "SPI_CS": "SSLA2", "MTU_CLK": "MTCLKC"},
    75: {"GPTW_PWM": "GTIOC2A"},
    # GPTIOC2 subchannel B
    50: {"GPTW_PWM": "GTIOC2B", "SPI_CS": "SSLA3", "TPU_CLK": "TCLKA"},
    78: {"GPTW_PWM": "GTIOC2B", "SPI_CS": "SSLB1", "SIMPLE_SPI_SCK": "SCK12"},
    # GPTIOC3 subchannel A
    45: {"GPTW_PWM": "GTIOC3A", "SPI_CIPO": "MISOA", "MTU_CLK": "MTCLKB"},
    71: {"GPTW_PWM": "GTIOC3A", "SPI_CIPO": "MISOB"},
    # GPTIOC3 subchannel B
    46: {"GPTW_PWM": "GTIOC3B", "SPI_COPI": "MOSIA", "MTU_CLK": "MTCLKA"},
    72: {"GPTW_PWM": "GTIOC3B", "SPI_COPI": "MOSIB"},
    # ── SPI-only pins (not shared with GPTW) ──
    # RSPI0_A  (module 0, suffix A)
    48: {"GPTW_EXT_TRIG": "GTETRGC", "SPI_CS": "SSLA0", "MTU_CLK": "MTCLKC"},
    52: {"SPI_CS": "SSLA1", "TPU_CLK": "TCLKC"},
    51: {"SPI_CS": "SSLA2", "TPU_CLK": "TCLKD"},
    # RSPI0_B  (module 0, suffix A — same MPC names, different pins)
    64: {"GPTW_EXT_TRIG": "GTETRGB", "SPI_COPI": "MOSIA", "MTU_CLK": "MTCLKB"},
    63: {"SPI_CIPO": "MISOA"},
    66: {"SPI_CS": "SSLA0"},
    # RSPI1_A  (module 1, suffix B)
    21: {"SPI_RSPCK": "RSPCKB"},
    22: {"SPI_COPI": "MOSIB"},
    20: {"SPI_CIPO": "MISOB"},
    19: {"SPI_CS": "SSLB0"},
    44: {"SPI_CS": "SSLB1"},
    43: {"SPI_CS": "SSLB2"},
    42: {"SPI_CS": "SSLB3"},
    # RSPI2_A  (module 2, suffix C)
    # 83 already has GPTW_PWM entry above
    # 85 already has GPTW_PWM entry above
    # 84 already has GPTW_PWM entry above
    82: {"SPI_CS": "SSLC0"},
    81: {"SPI_CS": "SSLC1", "MTU_CLK": "MTCLKA"},
    80: {"SPI_CS": "SSLC2"},
    79: {"SPI_CS": "SSLC3"},
    # ── External triggers ──
    31: {"GPTW_EXT_TRIG": "GTETRGA", "MTU_CLK": "MTCLKB", "TPU_CLK": "TCLKB"},
    32: {"GPTW_EXT_TRIG": "GTETRGD", "MTU_CLK": "MTCLKA", "TPU_CLK": "TCLKA"},
    # 48: already above
    # 64: already above
    # ── I2C ──
    33: {"IIC_SDA_FMP": "SDA0"},
    34: {"IIC_SCL_FMP": "SCL0"},
    28: {"IIC_SDA": "SDA1"},
    # 27: already above (GTIOC2A + SCL1)
    # ── Debug UART (SCI9) ──
    53: {"DEBUG_UART_TXD": "TXD9"},
    54: {"DEBUG_UART_RXD": "RXD9"},
    # ── MTU clock inputs (pins not already listed above) ──
    23: {"MTU_CLK": "MTCLKB"},
    24: {"MTU_CLK": "MTCLKA"},
    67: {"MTU_CLK": "MTCLKD", "TPU_CLK": "TCLKB"},
    # ── TPU clock inputs (pins not already listed above) ──
    57: {"TPU_CLK": "TCLKD"},
    58: {"TPU_CLK": "TCLKC"},
    # ── ADC (Port 4 dedicated analog) ──
    87: {"ADC": "AN007"},
    88: {"ADC": "AN006"},
    89: {"ADC": "AN005"},
    90: {"ADC": "AN004"},
    91: {"ADC": "AN003"},
    92: {"ADC": "AN002"},
    93: {"ADC": "AN001"},
}


@unique
class PortFunctionality(Enum):
    INTERRUPT = auto()
    UART_TXD = auto()
    UART_RXD = auto()
    TIMER = auto()
    CORE = auto()
    GPTW_PWM = auto()
    GPTW_EXT_TRIG = auto()

    IIC_SCL = auto()
    IIC_SDA = auto()
    IIC_SCL_FMP = auto()
    IIC_SDA_FMP = auto()
    SPI_CIPO = auto()
    SPI_COPI = auto()
    SPI_RSPCK = auto()
    SPI_CS = auto()
    ADC = auto()
    PWM = auto()
    DEBUG_UART_TXD = auto()
    DEBUG_UART_RXD = auto()
    GPIO = auto()
    GOOD_QUADRATURE_TIMER = auto()
    SHITTY_QUADRATURE_TIMER = auto()
    SIMPLE_SPI_SCK = auto()
    SIMPLE_SPI_MOSI = auto()
    SIMPLE_SPI_MISO = auto()


PF = PortFunctionality

# pin capabilities: what each pin can do
pins = {
    1: {PF.CORE},
    2: {PF.CORE},
    3: {PF.CORE},
    4: {PF.TIMER},
    5: {PF.CORE},
    6: {PF.CORE},
    7: {PF.CORE},
    8: {PF.CORE},
    9: {PF.CORE},
    10: {PF.CORE},
    11: {PF.CORE},
    12: {PF.CORE},
    13: {PF.CORE},
    14: {PF.CORE},
    15: {PF.CORE, PF.INTERRUPT},
    16: {PF.CORE, PF.INTERRUPT, PF.TIMER},
    17: {PF.CORE, PF.INTERRUPT, PF.UART_RXD, PF.TIMER},
    18: {PF.INTERRUPT, PF.UART_TXD, PF.TIMER},
    19: {PF.CORE, PF.INTERRUPT, PF.SPI_CS, PF.TIMER},
    20: {PF.CORE, PF.INTERRUPT, PF.UART_RXD, PF.SPI_CIPO, PF.TIMER},
    21: {PF.CORE, PF.SPI_RSPCK, PF.TIMER},
    22: {PF.CORE, PF.UART_TXD, PF.SPI_COPI, PF.TIMER},
    23: {PF.UART_RXD, PF.TIMER, PF.GOOD_QUADRATURE_TIMER},
    24: {PF.TIMER, PF.GOOD_QUADRATURE_TIMER},
    25: {PF.UART_TXD, PF.TIMER, PF.GPTW_PWM, PF.GOOD_QUADRATURE_TIMER},
    26: {PF.TIMER, PF.GPTW_PWM, PF.GOOD_QUADRATURE_TIMER},
    27: {PF.INTERRUPT, PF.UART_RXD, PF.IIC_SCL, PF.TIMER, PF.GPTW_PWM},
    28: {PF.INTERRUPT, PF.UART_TXD, PF.IIC_SDA, PF.TIMER},
    29: {PF.INTERRUPT, PF.UART_TXD, PF.IIC_SDA, PF.TIMER, PF.GPTW_PWM, PF.SHITTY_QUADRATURE_TIMER},
    30: {PF.CORE, PF.INTERRUPT, PF.UART_TXD, PF.UART_RXD, PF.IIC_SCL, PF.TIMER},
    31: {PF.INTERRUPT, PF.UART_RXD, PF.TIMER, PF.GPTW_EXT_TRIG, PF.GOOD_QUADRATURE_TIMER, PF.SHITTY_QUADRATURE_TIMER},
    32: {PF.INTERRUPT, PF.TIMER, PF.GPTW_EXT_TRIG, PF.GOOD_QUADRATURE_TIMER, PF.SHITTY_QUADRATURE_TIMER},
    33: {PF.INTERRUPT, PF.UART_TXD, PF.IIC_SDA_FMP, PF.TIMER},
    34: {PF.INTERRUPT, PF.UART_RXD, PF.IIC_SCL_FMP},
    35: {PF.CORE},
    36: {PF.CORE},
    37: {PF.CORE},
    38: {PF.CORE},
    39: {PF.INTERRUPT, PF.TIMER},
    40: {PF.TIMER},
    41: {PF.CORE},
    42: {PF.UART_RXD, PF.SPI_CS},
    43: {PF.SPI_CS},
    44: {PF.UART_TXD, PF.SPI_CS},
    45: {PF.CORE, PF.INTERRUPT, PF.UART_TXD, PF.SPI_CIPO, PF.TIMER, PF.GPTW_PWM, PF.GOOD_QUADRATURE_TIMER},
    46: {PF.INTERRUPT, PF.UART_RXD, PF.SPI_COPI, PF.TIMER, PF.GPTW_PWM, PF.GOOD_QUADRATURE_TIMER},
    47: {PF.SPI_RSPCK, PF.TIMER, PF.GPTW_PWM, PF.GOOD_QUADRATURE_TIMER},
    48: {PF.SPI_CS, PF.TIMER, PF.GPTW_PWM, PF.GPTW_EXT_TRIG, PF.GOOD_QUADRATURE_TIMER},
    49: {PF.UART_TXD, PF.TIMER, PF.GPTW_PWM, PF.SHITTY_QUADRATURE_TIMER},
    50: {PF.UART_RXD, PF.SPI_CS, PF.TIMER, PF.GPTW_PWM, PF.SHITTY_QUADRATURE_TIMER},
    51: {PF.INTERRUPT, PF.SPI_CS, PF.TIMER, PF.SHITTY_QUADRATURE_TIMER},
    52: {PF.INTERRUPT, PF.SPI_CS, PF.TIMER, PF.SHITTY_QUADRATURE_TIMER},
    53: {PF.DEBUG_UART_TXD, PF.UART_TXD, PF.TIMER},
    54: {PF.DEBUG_UART_RXD, PF.UART_RXD, PF.TIMER},
    55: {PF.TIMER},
    56: {PF.TIMER},
    57: {PF.TIMER, PF.SHITTY_QUADRATURE_TIMER},
    58: {PF.TIMER, PF.SHITTY_QUADRATURE_TIMER},
    59: {PF.INTERRUPT, PF.UART_TXD, PF.TIMER},
    60: {PF.CORE},
    61: {PF.INTERRUPT, PF.UART_RXD, PF.TIMER},
    62: {PF.CORE},
    63: {PF.SPI_CIPO, PF.TIMER},
    64: {PF.SPI_COPI, PF.TIMER, PF.GPTW_PWM, PF.GPTW_EXT_TRIG, PF.GOOD_QUADRATURE_TIMER},
    65: {PF.SPI_RSPCK, PF.TIMER, PF.GPTW_PWM},
    66: {PF.INTERRUPT, PF.UART_TXD, PF.SPI_CS, PF.TIMER},
    67: {PF.INTERRUPT, PF.UART_RXD, PF.TIMER, PF.GOOD_QUADRATURE_TIMER, PF.SHITTY_QUADRATURE_TIMER},
    68: {PF.UART_RXD, PF.SPI_CS, PF.TIMER, PF.GPTW_PWM},
    69: {PF.INTERRUPT, PF.SPI_CS, PF.TIMER, PF.GPTW_PWM, PF.GOOD_QUADRATURE_TIMER},
    70: {PF.SPI_CS, PF.TIMER, PF.GPTW_PWM},
    71: {PF.INTERRUPT, PF.ADC, PF.SPI_CIPO, PF.TIMER, PF.GPTW_PWM},
    72: {PF.INTERRUPT, PF.ADC, PF.SPI_COPI, PF.TIMER, PF.GPTW_PWM},
    73: {PF.INTERRUPT, PF.ADC, PF.SPI_RSPCK, PF.TIMER, PF.GPTW_PWM},
    74: {PF.ADC, PF.SPI_CS, PF.TIMER, PF.GPTW_PWM},
    75: {PF.ADC, PF.GPTW_PWM, PF.TIMER},
    76: {PF.INTERRUPT, PF.ADC, PF.UART_RXD, PF.SPI_CS, PF.TIMER, PF.GPTW_PWM, PF.SIMPLE_SPI_MISO},
    77: {PF.UART_TXD, PF.SPI_CS, PF.TIMER, PF.GPTW_PWM, PF.SIMPLE_SPI_MOSI},
    78: {PF.SPI_CS, PF.TIMER, PF.GPTW_PWM, PF.SIMPLE_SPI_SCK},
    79: {PF.INTERRUPT, PF.ADC, PF.SPI_CS, PF.TIMER},
    80: {PF.INTERRUPT, PF.ADC, PF.SPI_CS, PF.TIMER},
    81: {PF.INTERRUPT, PF.ADC, PF.SPI_CS, PF.TIMER, PF.GOOD_QUADRATURE_TIMER},
    82: {PF.INTERRUPT, PF.ADC, PF.SPI_CS, PF.TIMER},
    83: {PF.INTERRUPT, PF.ADC, PF.SPI_RSPCK, PF.TIMER, PF.GPTW_PWM},
    84: {PF.INTERRUPT, PF.ADC, PF.SPI_CIPO, PF.GPTW_PWM},
    85: {PF.INTERRUPT, PF.ADC, PF.SPI_COPI, PF.GPTW_PWM},
    86: {PF.INTERRUPT, PF.ADC, PF.GPTW_PWM},
    87: {PF.INTERRUPT, PF.ADC},
    88: {PF.INTERRUPT, PF.ADC},
    89: {PF.INTERRUPT, PF.ADC},
    90: {PF.INTERRUPT, PF.ADC},
    91: {PF.INTERRUPT, PF.ADC},
    92: {PF.INTERRUPT, PF.ADC},
    93: {PF.INTERRUPT, PF.ADC},
    94: {PF.CORE},
    95: {PF.INTERRUPT, PF.ADC},
    96: {PF.CORE},
    97: {PF.CORE},
    98: {PF.INTERRUPT},
    99: {PF.CORE},
    100: {PF.INTERRUPT},
}

# 5 available SPI channels with their pin options
spi_details = {
    "RSPI0_A": {
        PF.SPI_RSPCK: [47],
        PF.SPI_COPI: [46],
        PF.SPI_CIPO: [45],
        PF.SPI_CS: [48, 52, 51, 50],
    },
    "RSPI0_B": {
        PF.SPI_RSPCK: [65],
        PF.SPI_COPI: [64],
        PF.SPI_CIPO: [63],
        PF.SPI_CS: [66, 70, 69, 68],
    },
    "RSPI1_A": {
        PF.SPI_RSPCK: [21],
        PF.SPI_COPI: [22],
        PF.SPI_CIPO: [20],
        PF.SPI_CS: [19, 44, 43, 42],
    },
    "RSPI1_B": {
        PF.SPI_RSPCK: [73],
        PF.SPI_COPI: [72],
        PF.SPI_CIPO: [71],
        PF.SPI_CS: [74, 78, 77, 76],
    },
    "RSPI2_A": {
        PF.SPI_RSPCK: [83],
        PF.SPI_COPI: [85],
        PF.SPI_CIPO: [84],
        PF.SPI_CS: [82, 81, 80, 79],
    },
}

# motor PWM channels and fixed trigger/ADC pins
gptw_details = {
    "GPTIOC0": {
        PF.GPTW_PWM: [(25, 65, 73, 83), (29, 70, 76, 84)]
    },
    "GPTIOC1": {
        PF.GPTW_PWM: [(26, 47, 68, 74, 85), (49, 77, 86)]
    },
    "GPTIOC2": {
        PF.GPTW_PWM: [(27, 69, 75), (50, 78)]
    },
    "GPTIOC3": {
        PF.GPTW_PWM: [(45, 71), (46, 72)]
    },
    "GTETRG": {
        PF.GPTW_EXT_TRIG: [31, 64, 48, 32]
    },
}

# MTU encoder channels with ALL alternative pins (MPC PSEL=0x02)
# MTU1 uses MTCLKA+MTCLKB for phase counting
# MTU2 uses MTCLKC+MTCLKD for phase counting (set PHCKSEL=1)
# Source: RX72N Manual Ch23 Table 23.10, Ch24
mtu_details = {
    "MTU1": {
        "MTCLKA": [32, 24, 46, 81],   # P14, P24, PC6, PD5
        "MTCLKB": [31, 23, 45, 64],   # P15, P25, PC7, PA6
    },
    "MTU2": {
        "MTCLKC": [26, 48, 69],       # P22, PC4, PA1
        "MTCLKD": [25, 47, 67],       # P23, PC5, PA3
    },
}

# TPU encoder channels with ALL alternative pins (MPC PSEL settings)
# TPU1+TPU4 share TCLKA+TCLKB for phase counting
# TPU2+TPU5 share TCLKC+TCLKD for phase counting
# Source: RX72N Manual Ch25 (TPU)
tpu_details = {
    "TPU1": {
        "TCLKA": [32, 50],              # P14, PC2
        "TCLKB": [31, 49, 67],          # P15, PC3, PA3
    },
    "TPU2": {
        "TCLKC": [30, 52, 58],          # P16 (CORE), PC0, PB2
        "TCLKD": [29, 51, 57],          # P17, PC1, PB3
    },
}

# I2C channels
# RIIC0 is the only FM+ (1MHz) capable channel on 100-pin
# RIIC1 is standard I2C
riic_details = {
    "RIIC0": {PF.IIC_SCL_FMP: [34], PF.IIC_SDA_FMP: [33]},  # P12, P13
    "RIIC1": {PF.IIC_SCL: [27], PF.IIC_SDA: [28]},           # P21, P20
}

# Debug UART (only SCI9 available on 100-pin LFQFP)
sci_details = {
    "SCI9": {PF.DEBUG_UART_TXD: [53], PF.DEBUG_UART_RXD: [54]},  # PB7, PB6
}

# Simple SPI (SCI clock-synchronous mode) for motor driver config
# SCI12 is the only channel with all 3 data pins free on 100-pin package
# Max speed: PCLKB/4 = 60MHz/4 = 15 Mbps (DRV8243S needs only 10 MHz)
# SS# pin disabled (SSE=0 in SPMR) — GPIO used for 4 chip selects
# Source: RX72N Manual Ch41 (SCI), verified via NotebookLM
simple_spi_details = {
    "SCI12": {
        PF.SIMPLE_SPI_SCK: [78],     # PE0
        PF.SIMPLE_SPI_MOSI: [77],    # PE1
        PF.SIMPLE_SPI_MISO: [76],    # PE2
    },
}

# ══════════════════════════════════════════════════════════════════════════
# REQUIREMENTS — edit this section to adjust what the solver allocates
# ══════════════════════════════════════════════════════════════════════════
REQUIREMENTS = {
    "package": "LQFP144",        # "LQFP100" (R5F572NNHGFP#30) or "LQFP144" (R5F572NNHxFB)
    "gtetrg_count": 4,           # DRV8243S nFAULT triggers (1 per motor)
    "debug_uart": True,          # SCI9 TXD/RXD
    "host_i2c": True,            # RIIC0 FM+ (1 MHz)
    "bms_i2c": True,             # RIIC1 standard I2C
    "motor_driver_cs_count": 4,  # GPIO chip selects for DRV8243S
    "motor_count": 4,            # GPTW channels (GPTIOC0-3)
    "host_spi": True,            # RSPI for RPi5
    "mtu_encoder_count": 2,      # MTU phase counting (good quadrature)
    "tpu_encoder_count": 2,      # TPU phase counting (shitty quadrature)
    "motor_adc_count": 4,        # Current sensing ADC
    "bms_irq": True,             # BMS alert interrupt
    "sonar_count": 4,            # HC-SR04 sonars
    "led_count": 6,              # Status LEDs
    "onewire_count": 1,          # 1-Wire temp sensors
    "motor_spi_bitbang": None,   # None=auto-detect (True for 100-pin, False for 144-pin)
                                 # True=bitbang GPIO, False=SCI12 hardware SPI
}

# ══════════════════════════════════════════════════════════════════════════
# 144-PIN LFQFP PACKAGE DATA (R5F572NNHxFB)
# ══════════════════════════════════════════════════════════════════════════
# On 144-pin, GPTIOC2B gains P86 (pin 41) and P66 (pin 99), breaking the
# deadlock that forced bitbang SPI on 100-pin. Hardware SPI (SCI12) works.

LQFP144_PORT_TO_PIN = {
    "P05": 2, "P03": 4, "P02": 6, "P01": 7, "P00": 8,
    "PF5": 9, "PJ5": 11, "PJ3": 13,
    "P37": 20, "P36": 22, "P35": 24,
    "P34": 25, "P33": 26, "P32": 27, "P31": 28, "P30": 29,
    "P27": 30, "P26": 31, "P25": 32, "P24": 33, "P23": 34,
    "P22": 35, "P21": 36, "P20": 37, "P17": 38,
    "P87": 39, "P16": 40, "P86": 41,
    "P15": 42, "P14": 43, "P13": 44, "P12": 45,
    "P56": 50, "P55": 51, "P54": 52, "P53": 53, "P52": 54, "P51": 55, "P50": 56,
    "P83": 58,
    "PC7": 60, "PC6": 61, "PC5": 62,
    "P82": 63, "P81": 64, "P80": 65,
    "PC4": 66, "PC3": 67,
    "P77": 68, "P76": 69,
    "PC2": 70,
    "P75": 71, "P74": 72,
    "PC1": 73, "PC0": 75,
    "P73": 77,
    "PB7": 78, "PB6": 79, "PB5": 80, "PB4": 81, "PB3": 82, "PB2": 83,
    "PB1": 84,
    "P72": 85, "P71": 86,
    "PB0": 87,
    "PA7": 88, "PA6": 89, "PA5": 90,
    "PA4": 92, "PA3": 94, "PA2": 95,
    "PA1": 96, "PA0": 97,
    "P67": 98, "P66": 99, "P65": 100,
    "PE7": 101, "PE6": 102,
    "P70": 104,
    "PE5": 106, "PE4": 107, "PE3": 108, "PE2": 109, "PE1": 110, "PE0": 111,
    "P64": 112, "P63": 113, "P62": 114, "P61": 115, "P60": 117,
    "PD7": 119, "PD6": 120, "PD5": 121, "PD4": 122, "PD3": 123, "PD2": 124,
    "PD1": 125, "PD0": 126,
    "P93": 127, "P92": 128, "P91": 129, "P90": 131,
    "P47": 133, "P46": 134, "P45": 135, "P44": 136, "P43": 137, "P42": 138,
    "P41": 139, "P40": 141,
    "P07": 144,
}

# Infrastructure pins (power, ground, crystal refs) — always CORE on 144-pin
LQFP144_INFRA_PINS = {
    1: "AVSS0", 3: "AVCC1", 5: "AVSS1", 10: "EMLE",
    12: "VSS", 14: "VCL", 15: "VBATT", 16: "MD/FINED",
    17: "XCIN", 18: "XCOUT", 19: "RES#",
    21: "VSS", 23: "VCC",
    46: "VCC_USB", 47: "USB0_DM", 48: "USB0_DP", 49: "VSS_USB",
    57: "VSS", 59: "VCC", 74: "VCC", 76: "VSS",
    91: "VCC", 93: "VSS", 103: "VCC", 105: "VSS",
    116: "VSS", 118: "VCC", 130: "VSS", 132: "VCC",
    140: "VREFL0", 142: "VREFH0", 143: "AVCC0",
}

# Ports that are CORE on 144-pin (JTAG + crystal + NMI + BCLK)
# NOTE: P33, P16, PC7 are CORE on 100-pin but NOT on 144-pin
LQFP144_CORE_PORTS = {
    "P37", "P36", "P35",                    # Crystal, NMI
    "P34", "P31", "P30", "P27", "P26",      # JTAG
    "P53",                                   # BCLK output
}

# Full function names for 144-pin-only ports
LQFP144_EXTRA_FULL_NAMES = {
    "P87": "P87/MTIOC4C/TIOCA2/GTIOC1B/SMOSI10/SSDA10/TXD10/EPLSOUT1/SDHI_D2-C/PIXD2",
    "P86": "P86/MTIOC4D/TIOCA0/GTIOC2B/SMISO10/SSCL10/RXD10/PIXD1",
    "P83": "P83/TRCLK/EDACK1/MTIOC4C/GTIOC0A/SCK10/SS10#/CTS10#/ET0_CRS/RMII0_CRS_DV",
    "P82": "P82/TRSYNC/EDREQ1/MTIOC4A/PO28/GTIOC2A/SMOSI10/SSDA10/TXD10/ET0_ETXD1/RMII0_TXD1",
    "P81": "P81/TRDATA1/EDACK0/MTIOC3D/PO27/GTIOC0B/SMISO10/SSCL10/RXD10/ET0_ETXD0/RMII0_TXD0",
    "P80": "P80/TRDATA0/EDREQ0/MTIOC3B/PO26/SCK10/RTS10#/ET0_TX_EN/RMII0_TXD_EN",
    "P67": "P67/DQM1/CS7#/MTIOC7C/GTIOC1B/CRX2/EPLSOUT1/IRQ15",
    "P66": "P66/DQM0/CS6#/MTIOC7D/GTIOC2B/CTX2",
    "P65": "P65/CKE/CS5#",
    "P56": "P56/CLKOUT25M/EDACK1/MTIOC3C/TIOCA1/SCK7",
    "P00": "P00/TMRI0/TXD6/SMOSI6/SSDA6/AUDIO_CLK/IRQ8/AN118",
    "P01": "P01/TMCI0/RXD6/SMISO6/SSCL6/SSIBCK0/IRQ9/AN119",
    "P02": "P02/TMCI1/SCK6/SSIBCK1/IRQ10/AN120",
    "P03": "P03/SSIDATA1/IRQ11/DA0",
    "PF5": "PF5/WAIT#/SSILRCK0/IRQ4",
    "PJ5": "PJ5/POE8#/CTS2#/RTS2#/SS2#/SSIRXD0/EPLSOUT0",
    "P70": "P70/SDCLK", "P71": "P71/A18/CS1#", "P72": "P72/A19/CS2#",
    "P73": "P73/TRDATA4/CS3#/PO16", "P74": "P74/TRDATA5/A20/CS4#/PO19",
    "P75": "P75/TRSYNC1/CS5#/PO20/SCK11/RTS11#",
    "P76": "P76/TRDATA6/CS6#/PO22/SMISO11/SSCL11/RXD11",
    "P77": "P77/TRDATA7/CS7#/PO23/SMOSI11/SSDA11/TXD11",
    "P60": "P60/CS0#", "P61": "P61/SDCS#/D0/CS1#",
    "P62": "P62/RAS#/D1/CS2#", "P63": "P63/CAS#/D2/CS3#", "P64": "P64/WE#/D3/CS4#",
    "P90": "P90/A16/TXD7/SMOSI7/SSDA7/AN114",
    "P91": "P91/A17/SCK7/AN115",
    "P92": "P92/A18/POE4#/RXD7/SMISO7/SSCL7/AN116",
    "P93": "P93/A19/POE0#/CTS7#/RTS7#/SS7#/AN117",
}

# Capabilities for 144-pin-only ports
LQFP144_EXTRA_CAPS = {
    "P87": {PF.TIMER, PF.GPTW_PWM}, "P86": {PF.TIMER, PF.GPTW_PWM},
    "P83": {PF.TIMER, PF.GPTW_PWM}, "P82": {PF.TIMER, PF.GPTW_PWM},
    "P81": {PF.TIMER, PF.GPTW_PWM}, "P80": {PF.TIMER},
    "P67": {PF.INTERRUPT, PF.TIMER, PF.GPTW_PWM},
    "P66": {PF.TIMER, PF.GPTW_PWM},
    "P65": set(), "P56": {PF.TIMER},
    "P00": {PF.INTERRUPT, PF.ADC}, "P01": {PF.INTERRUPT, PF.ADC},
    "P02": {PF.INTERRUPT, PF.ADC}, "P03": {PF.INTERRUPT},
    "PF5": {PF.INTERRUPT}, "PJ5": {PF.TIMER},
    "P70": set(), "P71": set(), "P72": set(), "P73": set(), "P74": set(),
    "P75": set(), "P76": set(), "P77": set(),
    "P60": set(), "P61": set(), "P62": set(), "P63": set(), "P64": set(),
    "P90": {PF.ADC}, "P91": {PF.ADC}, "P92": {PF.ADC}, "P93": {PF.ADC},
}

# MPC function names for 144-pin-only ports
LQFP144_EXTRA_MPC = {
    "P86": {"GPTW_PWM": "GTIOC2B"}, "P66": {"GPTW_PWM": "GTIOC2B"},
    "P83": {"GPTW_PWM": "GTIOC0A"}, "P82": {"GPTW_PWM": "GTIOC2A"},
    "P81": {"GPTW_PWM": "GTIOC0B"}, "P87": {"GPTW_PWM": "GTIOC1B"},
    "P67": {"GPTW_PWM": "GTIOC1B"},
    "P00": {"ADC": "AN118"}, "P01": {"ADC": "AN119"}, "P02": {"ADC": "AN120"},
    "P90": {"ADC": "AN114"}, "P91": {"ADC": "AN115"},
    "P92": {"ADC": "AN116"}, "P93": {"ADC": "AN117"},
}

# Extra GPTW options on 144-pin (ports not bonded out on 100-pin)
LQFP144_EXTRA_GPTW = {
    "GPTIOC0": {"A": ["P83"], "B": ["P81"]},
    "GPTIOC1": {"B": ["P87", "P67"]},
    "GPTIOC2": {"A": ["P82"], "B": ["P86", "P66"]},
}


def build_144pin_data():
    """Build all 144-pin data by translating existing 100-pin data.

    Translates pin numbers via port names: pin100 -> port_name -> pin144.
    Adds extra 144-pin-only ports, capabilities, and GPTW options.
    """
    p2p = LQFP144_PORT_TO_PIN

    # Build port -> pin100 mapping from existing pin_to_port
    port_to_pin100 = {}
    for pin100, full_name in pin_to_port.items():
        port = full_name.split("/")[0]
        port_to_pin100[port] = pin100

    def translate_pin(pin100):
        """Translate one 100-pin number to 144-pin via port name."""
        port = pin_to_port.get(pin100, "").split("/")[0]
        return p2p.get(port)

    def translate_list(pin_list):
        """Translate list of pin numbers, filtering unavailable ports."""
        return [p2p[pin_to_port[p].split("/")[0]]
                for p in pin_list
                if pin_to_port.get(p, "").split("/")[0] in p2p]

    def translate_tuple(pin_tuple):
        """Translate tuple of pin numbers."""
        return tuple(p2p[pin_to_port[p].split("/")[0]]
                     for p in pin_tuple
                     if pin_to_port.get(p, "").split("/")[0] in p2p)

    # ── pin_to_port: pin144 → full_name ──
    new_pin_to_port = {}
    for port, pin144 in p2p.items():
        pin100 = port_to_pin100.get(port)
        if pin100 and pin100 in pin_to_port:
            new_pin_to_port[pin144] = pin_to_port[pin100]
        elif port in LQFP144_EXTRA_FULL_NAMES:
            new_pin_to_port[pin144] = LQFP144_EXTRA_FULL_NAMES[port]
        else:
            new_pin_to_port[pin144] = port
    for pin144, name in LQFP144_INFRA_PINS.items():
        new_pin_to_port[pin144] = name

    # ── pins: pin144 → capability set ──
    new_pins = {}
    for pin100, caps in pins.items():
        port = pin_to_port.get(pin100, "").split("/")[0]
        pin144 = p2p.get(port)
        if pin144 is not None:
            new_caps = set(caps)
            new_caps.discard(PF.CORE)
            if port in LQFP144_CORE_PORTS:
                new_caps.add(PF.CORE)
            new_pins[pin144] = new_caps
    for port, caps in LQFP144_EXTRA_CAPS.items():
        pin144 = p2p.get(port)
        if pin144 is not None:
            new_pins[pin144] = set(caps)
    for pin144 in LQFP144_INFRA_PINS:
        new_pins[pin144] = {PF.CORE}

    # ── pin_mpc_function: pin144 → {func: name} ──
    new_pin_mpc = {}
    for pin100, funcs in pin_mpc_function.items():
        pin144 = translate_pin(pin100)
        if pin144 is not None:
            new_pin_mpc[pin144] = funcs
    for port, funcs in LQFP144_EXTRA_MPC.items():
        pin144 = p2p.get(port)
        if pin144 is not None:
            new_pin_mpc[pin144] = funcs

    # ── Peripheral detail dicts ──
    new_spi = {ch: {f: translate_list(pl) for f, pl in fns.items()}
               for ch, fns in spi_details.items()}

    new_gptw = {}
    for ch, funcs in gptw_details.items():
        new_gptw[ch] = {}
        for func, data in funcs.items():
            if func == PF.GPTW_PWM:
                new_subs = []
                for i, sub_tuple in enumerate(data):
                    translated = list(translate_tuple(sub_tuple))
                    sub_key = "A" if i == 0 else "B"
                    if ch in LQFP144_EXTRA_GPTW and sub_key in LQFP144_EXTRA_GPTW[ch]:
                        for extra_port in LQFP144_EXTRA_GPTW[ch][sub_key]:
                            extra_pin = p2p.get(extra_port)
                            if extra_pin is not None:
                                translated.append(extra_pin)
                    new_subs.append(tuple(translated))
                new_gptw[ch][func] = new_subs
            else:
                new_gptw[ch][func] = translate_list(data)

    new_mtu = {ch: {c: translate_list(pl) for c, pl in fns.items()}
               for ch, fns in mtu_details.items()}
    new_tpu = {ch: {c: translate_list(pl) for c, pl in fns.items()}
               for ch, fns in tpu_details.items()}
    new_riic = {ch: {f: translate_list(pl) for f, pl in fns.items()}
                for ch, fns in riic_details.items()}
    new_sci = {ch: {f: translate_list(pl) for f, pl in fns.items()}
               for ch, fns in sci_details.items()}
    new_simple_spi = {ch: {f: translate_list(pl) for f, pl in fns.items()}
                      for ch, fns in simple_spi_details.items()}

    # ── ADC channel map (all ADC-capable ports) ──
    new_adc = {}
    # Map port name → ADC channel for ALL ADC-capable ports
    port_adc_channels = {
        # Port 4 (dedicated analog)
        "P47": "AN007", "P46": "AN006", "P45": "AN005", "P44": "AN004",
        "P43": "AN003", "P42": "AN002", "P41": "AN001", "P40": "AN000",
        # Port E (AN100-AN105, ANEX0-ANEX1)
        "PE7": "AN105", "PE6": "AN104", "PE5": "AN103", "PE4": "AN102",
        "PE3": "AN101", "PE2": "AN100",
        # Port D (AN106-AN113)
        "PD7": "AN107", "PD6": "AN106", "PD5": "AN113", "PD4": "AN112",
        "PD3": "AN111", "PD2": "AN110", "PD1": "AN109", "PD0": "AN108",
        # 100-pin only
        "P95": "AN017",
        # 144-pin only
        "P00": "AN118", "P01": "AN119", "P02": "AN120",
        "P90": "AN114", "P91": "AN115", "P92": "AN116", "P93": "AN117",
    }
    for port, ch_name in port_adc_channels.items():
        pin144 = p2p.get(port)
        if pin144 is not None:
            new_adc[pin144] = ch_name

    return {
        "pin_to_port": new_pin_to_port, "pins": new_pins,
        "pin_mpc_function": new_pin_mpc,
        "spi_details": new_spi, "gptw_details": new_gptw,
        "mtu_details": new_mtu, "tpu_details": new_tpu,
        "riic_details": new_riic, "sci_details": new_sci,
        "simple_spi_details": new_simple_spi, "adc_channel_map": new_adc,
    }


# ══════════════════════════════════════════════════════════════════════════
# PACKAGE SELECTION AND DATA TRANSLATION
# ══════════════════════════════════════════════════════════════════════════
package = REQUIREMENTS.get("package", "LQFP100")

# Auto-detect bitbang: 100-pin needs bitbang, 144-pin doesn't
if REQUIREMENTS["motor_spi_bitbang"] is None:
    REQUIREMENTS["motor_spi_bitbang"] = (package == "LQFP100")

if package == "LQFP144":
    pin_count = 144
    chip_name = "R5F572NNHxFB"
    _data = build_144pin_data()
    pin_to_port = _data["pin_to_port"]
    pins = _data["pins"]
    pin_mpc_function = _data["pin_mpc_function"]
    spi_details = _data["spi_details"]
    gptw_details = _data["gptw_details"]
    mtu_details = _data["mtu_details"]
    tpu_details = _data["tpu_details"]
    riic_details = _data["riic_details"]
    sci_details = _data["sci_details"]
    simple_spi_details = _data["simple_spi_details"]
    adc_channel_map = _data["adc_channel_map"]
else:
    pin_count = 100
    chip_name = "R5F572NNHGFP#30"
    adc_channel_map = {
        # Port 4 (dedicated analog)
        87: "AN007", 88: "AN006", 89: "AN005", 90: "AN004",
        91: "AN003", 92: "AN002", 93: "AN001",
        # Port E
        71: "AN105", 72: "AN104", 73: "AN103", 74: "AN102",
        75: "AN101", 76: "AN100",
        # Port D
        79: "AN107", 80: "AN106", 81: "AN113", 82: "AN112",
        83: "AN111", 84: "AN110", 85: "AN109", 86: "AN108",
        # Misc
        95: "AN017",
    }


# ══════════════════════════════════════════════════════════════════════════
# MULTI-PHASE CONSTRAINT SOLVER
# ══════════════════════════════════════════════════════════════════════════
# Phase 0: CORE filter
# Phase 1: Fixed peripherals (GTETRG, I2C, UART — single-option pins)
# Phase 2: Motor config SPI (DRV8243S — Simple SPI/SCI12 + 4 GPIO CS)
# Phase 3: Motor PWM (GPTW — picks encoder-compatible combo)
# Phase 4: Host comm SPI (RPi5 — RSPI, 1 CS only)
# Phase 5: Encoders (MTU phase counting + TPU phase counting)
# Phase 6: Flexible (ADC, BMS IRQ, sonar, LEDs, 1-Wire)
# ══════════════════════════════════════════════════════════════════════════


# ── Phase 0: PF.CORE filter ─────────────────────────────────────────────
core_pins = {p for p, caps in pins.items() if PF.CORE in caps}

# filter SPI: remove core pins from each pin list per channel
spi_details = {
    ch: {
        func: [p for p in plist if p not in core_pins]
        for func, plist in funcs.items()
    }
    for ch, funcs in spi_details.items()
}
# drop SPI channels that lost an essential signal line
spi_details = {
    ch: funcs for ch, funcs in spi_details.items()
    if all(funcs[f] for f in (PF.SPI_RSPCK, PF.SPI_COPI, PF.SPI_CIPO, PF.SPI_CS))
}

# filter GPTW: remove core pins from subchannel option tuples and fixed lists
gptw_details = {
    ch: {
        func: (
            [tuple(p for p in t if p not in core_pins) for t in data]
            if func == PF.GPTW_PWM
            else [p for p in data if p not in core_pins]
        )
        for func, data in funcs.items()
    }
    for ch, funcs in gptw_details.items()
}

# filter MTU: remove core pins from clock input alternatives
mtu_details = {
    ch: {
        clk: [p for p in plist if p not in core_pins]
        for clk, plist in funcs.items()
    }
    for ch, funcs in mtu_details.items()
}

# filter TPU: remove core pins from clock input alternatives
tpu_details = {
    ch: {
        clk: [p for p in plist if p not in core_pins]
        for clk, plist in funcs.items()
    }
    for ch, funcs in tpu_details.items()
}

print(f"Phase 0: {len(core_pins)} CORE pins excluded: {sorted(core_pins)}")
print(f"  Viable SPI channels: {list(spi_details.keys())}")


# ── Phase 1: Fixed peripherals (no solver — single-option pins) ─────────
phase1_fixed = {}

# GTETRG: DRV8243S nFAULT → GPTW emergency stop triggers
gtetrg_pins_list = gptw_details["GTETRG"][PF.GPTW_EXT_TRIG]
gtetrg_names = ["GTETRGA", "GTETRGB", "GTETRGC", "GTETRGD"]
for name, pin in zip(gtetrg_names[:REQUIREMENTS["gtetrg_count"]],
                     gtetrg_pins_list[:REQUIREMENTS["gtetrg_count"]]):
    phase1_fixed[name] = {"pin": pin, "group": "gtetrg", "function": "GPTW_EXT_TRIG"}

# Debug UART: SCI9 (only UART on 100-pin)
for func, pin_list in sci_details["SCI9"].items():
    name = "DEBUG_TXD9" if func == PF.DEBUG_UART_TXD else "DEBUG_RXD9"
    phase1_fixed[name] = {"pin": pin_list[0], "group": "debug_uart", "function": func.name}

# Host I2C: RIIC0 FM+ (1MHz capable)
for func, pin_list in riic_details["RIIC0"].items():
    name = "HOST_SCL0" if func == PF.IIC_SCL_FMP else "HOST_SDA0"
    phase1_fixed[name] = {"pin": pin_list[0], "group": "host_i2c", "function": func.name}

# BMS I2C: RIIC1 (standard I2C)
for func, pin_list in riic_details["RIIC1"].items():
    name = "BMS_SCL1" if func == PF.IIC_SCL else "BMS_SDA1"
    phase1_fixed[name] = {"pin": pin_list[0], "group": "bms_i2c", "function": func.name}

phase1_pin_set = {v["pin"] for v in phase1_fixed.values()}
reserved_pins = core_pins | phase1_pin_set

print(f"Phase 1: {len(phase1_fixed)} fixed pins reserved: {sorted(phase1_pin_set)}")


# ── Phase 2: Motor config SPI (DRV8243S — SCI12 or bitbang GPIO) ────────
# DRV8243S only needs 10 MHz for config/fault registers.
# SCI12 (Simple SPI) is faster but locks pin 78 (SCK12), which blocks
# GPTIOC2B from using pin 78 — creating a deadlock with TPU1 TCLKA.
# Bitbang GPIO frees pin 78 for GPTW, letting TPU1 use pin 50 for TCLKA.
motor_spi_data_pins = set()
motor_spi_bitbang = REQUIREMENTS.get("motor_spi_bitbang", False)

if not motor_spi_bitbang:
    # Hardware SPI: SCI12 clock-synchronous mode (Simple SPI)
    motor_simple_spi_viable = {}
    for ch, funcs in simple_spi_details.items():
        ch_pins = set()
        for plist in funcs.values():
            ch_pins.update(plist)
        if not ch_pins & reserved_pins:
            motor_simple_spi_viable[ch] = funcs

    if not motor_simple_spi_viable:
        print("ERROR: No viable Simple SPI channel for motor config")
        print(f"  Reserved pins: {sorted(reserved_pins)}")
        raise SystemExit(1)

    motor_spi_name = list(motor_simple_spi_viable.keys())[0]  # SCI12
    motor_spi_def = motor_simple_spi_viable[motor_spi_name]
    for plist in motor_spi_def.values():
        motor_spi_data_pins.update(plist)
    reserved_pins |= motor_spi_data_pins
else:
    # Bitbang GPIO: data pins assigned in Phase 6 (any 3 free GPIO pins).
    # Only CS pins reserved now; SCI12 pins (76/77/78) stay free for GPTW/TPU.
    motor_spi_name = "BITBANG"

# pick GPIO CS pins from available I/O (prefer Port E/D for routing)
motor_cs_pool = sorted(
    p for p in range(1, pin_count + 1)
    if p not in reserved_pins and p in pins and PF.CORE not in pins[p]
)
# prefer nearby Port E pins (72-74) then Port D freed pins (79-81)
motor_cs_pool.sort(key=lambda p: (
    0 if 72 <= p <= 74 else 1 if 79 <= p <= 81 else 2, p
))
motor_cs_pins = motor_cs_pool[:REQUIREMENTS["motor_driver_cs_count"]]
if len(motor_cs_pins) < REQUIREMENTS["motor_driver_cs_count"]:
    print(f"  WARNING: Only found {len(motor_cs_pins)}/{REQUIREMENTS['motor_driver_cs_count']}"
          f" GPIO CS pins for motor SPI")
reserved_pins |= set(motor_cs_pins)

motor_spi_pin_set = motor_spi_data_pins | set(motor_cs_pins)

# format motor SPI mapping (data pins deferred for bitbang — added in Phase 6)
motor_spi_mapping = {}
if not motor_spi_bitbang:
    motor_spi_mapping["DRV_SCK"] = {
        "pin": motor_spi_def[PF.SIMPLE_SPI_SCK][0], "channel": motor_spi_name}
    motor_spi_mapping["DRV_COPI"] = {
        "pin": motor_spi_def[PF.SIMPLE_SPI_MOSI][0], "channel": motor_spi_name}
    motor_spi_mapping["DRV_CIPO"] = {
        "pin": motor_spi_def[PF.SIMPLE_SPI_MISO][0], "channel": motor_spi_name}
for i, cs_pin in enumerate(motor_cs_pins):
    motor_spi_mapping[f"DRV_CS{i}"] = {"pin": cs_pin, "channel": motor_spi_name}

if motor_spi_bitbang:
    print(f"Phase 2: Motor SPI = BITBANG (GPIO), "
          f"CS: {motor_cs_pins}, data pins deferred to Phase 6")
else:
    print(f"Phase 2: Motor SPI = {motor_spi_name} (Simple SPI), "
          f"data: {sorted(motor_spi_data_pins)}, CS: {motor_cs_pins}")


# ── Pre-Phase 3: Compute TPU-critical pins ──────────────────────────────
# If a TPU clock input has only ONE free pin, that pin must be excluded
# from both motor PWM and MTU selection to prevent them from stealing it.
tpu_critical_pins = set()
if REQUIREMENTS["tpu_encoder_count"] > 0:
    for tpu_ch, clk_funcs in tpu_details.items():
        for clk_name, pin_options in clk_funcs.items():
            free = [p for p in pin_options if p not in reserved_pins]
            if len(free) == 1:
                tpu_critical_pins.add(free[0])
                print(f"  TPU-critical: pin {free[0]} reserved for {tpu_ch}/{clk_name} "
                      f"(only option)")


# ── Phase 3: Motor PWM (GPTW — encoder-compatible selection) ────────────
# Filter GPTW pin options against reserved pins AND TPU-critical pins
motor_channels = (
    seq(gptw_details.items())
    .filter(lambda kv: kv[0].startswith("GPTIOC"))
    .sorted(key=lambda kv: kv[0])
    .to_dict()
)

exclude_from_motor = reserved_pins | tpu_critical_pins
motor_channels_filtered = {}
for ch, funcs in motor_channels.items():
    filtered_subs = []
    for sub_tuple in funcs[PF.GPTW_PWM]:
        filtered = tuple(p for p in sub_tuple if p not in exclude_from_motor)
        filtered_subs.append(filtered)
    motor_channels_filtered[ch] = {PF.GPTW_PWM: filtered_subs}

# check that each motor channel has at least one option per subchannel
for ch, funcs in motor_channels_filtered.items():
    for i, sub in enumerate(funcs[PF.GPTW_PWM]):
        if not sub:
            label = "Phase (A)" if i == 0 else "Enable (B)"
            print(f"ERROR: {ch} {label} has no available pins after filtering")
            print(f"  Reserved pins: {sorted(reserved_pins)}")
            raise SystemExit(1)

# generate all AxB combinations per motor
channel_combos = (
    seq(motor_channels_filtered.items())
    .map(
        lambda kv: seq(kv[1][PF.GPTW_PWM][0])
        .flat_map(lambda a: seq(kv[1][PF.GPTW_PWM][1]).map(lambda b: (kv[0], a, b)))
        .to_list()
    )
    .to_list()
)

# cartesian product (materialize to list for reuse)
all_motor_combos = list(itertools.product(*channel_combos))

# find valid motor combos: no pin reuse within motor assignments
valid_motor_combos = [
    mc for mc in all_motor_combos
    if len(seq(mc).flat_map(lambda t: [t[1], t[2]]).to_set()) == len(mc) * 2
]

if not valid_motor_combos:
    print("ERROR: No valid motor PWM pin combinations found")
    raise SystemExit(1)


def check_encoder_viability(motor_combo, already_reserved):
    """Check if ALL MTU encoder clock inputs have at least one free pin.

    Note: TPU encoders are NOT checked here because they use Phase 6 pins
    (sonar, LEDs) that aren't reserved yet at motor combo selection time.
    TPU viability is checked during Phase 5 assignment.
    """
    motor_pin_set = set()
    for t in motor_combo:
        motor_pin_set.add(t[1])
        motor_pin_set.add(t[2])
    all_reserved = already_reserved | motor_pin_set
    # Check MTU encoder viability
    if REQUIREMENTS["mtu_encoder_count"] > 0:
        for mtu_ch, clk_funcs in mtu_details.items():
            for clk_name, pin_options in clk_funcs.items():
                free = [p for p in pin_options if p not in all_reserved]
                if not free:
                    return False
    return True


# prefer motor combos that also allow encoder assignment
encoder_compatible = [
    mc for mc in valid_motor_combos
    if check_encoder_viability(mc, reserved_pins)
]

if encoder_compatible:
    motor_pwm_combo = encoder_compatible[0]
    print(f"  Found {len(encoder_compatible)} encoder-compatible motor combos "
          f"(of {len(valid_motor_combos)} valid)")
else:
    motor_pwm_combo = valid_motor_combos[0]
    print("  WARNING: No motor combo allows both encoders — using best motor combo")

motor_pwm_pin_set = seq(motor_pwm_combo).flat_map(lambda t: [t[1], t[2]]).to_set()
reserved_pins |= motor_pwm_pin_set

# format motor PWM mapping
motor_pwm_mapping = (
    seq(motor_pwm_combo)
    .zip_with_index()
    .flat_map(
        lambda t: [
            (f"MOTOR{t[1]}_PH", {"pin": t[0][1], "channel": t[0][0], "subchannel": "A"}),
            (f"MOTOR{t[1]}_EN", {"pin": t[0][2], "channel": t[0][0], "subchannel": "B"}),
        ]
    )
    .to_dict()
)

print(f"Phase 3: Motor PWM = {len(valid_motor_combos)} valid combos, "
      f"using: {sorted(motor_pwm_pin_set)}")


# ── Phase 4: Host comm SPI (RPi5 — RSPI, needs 1 CS only) ───────────────
# Motor SPI uses SCI12 or bitbang GPIO, so RSPI channels are available.
# RSPI provides 60 Mbps (PCLKA/2) with 128-bit hardware buffers.
host_spi_viable = {}
for ch, funcs in spi_details.items():
    # check RSPCK, COPI, CIPO not reserved
    essential_pins = set(funcs[PF.SPI_RSPCK] + funcs[PF.SPI_COPI] + funcs[PF.SPI_CIPO])
    if essential_pins & reserved_pins:
        continue
    # need at least 1 CS pin not reserved
    free_cs = [p for p in funcs[PF.SPI_CS] if p not in reserved_pins]
    if not free_cs:
        continue
    host_spi_viable[ch] = {
        PF.SPI_RSPCK: funcs[PF.SPI_RSPCK],
        PF.SPI_COPI: funcs[PF.SPI_COPI],
        PF.SPI_CIPO: funcs[PF.SPI_CIPO],
        PF.SPI_CS: free_cs[:1],  # only need 1 CS for RPi5
    }

host_spi_mapping = {}
host_spi_name = None
host_spi_conflict = None

if host_spi_viable:
    # prefer RSPI2_A (dedicated Port D pins, no contention)
    host_spi_priority = {"RSPI2_A": 5, "RSPI0_B": 4, "RSPI0_A": 3, "RSPI1_B": 2, "RSPI1_A": 1}
    host_spi_name = max(host_spi_viable.keys(), key=lambda c: host_spi_priority.get(c, 0))
    host_spi_def = host_spi_viable[host_spi_name]
    host_spi_pin_set = set()
    for plist in host_spi_def.values():
        host_spi_pin_set.update(plist)
    reserved_pins |= host_spi_pin_set

    host_spi_mapping = {
        "HOST_SCLK": {"pin": host_spi_def[PF.SPI_RSPCK][0], "channel": host_spi_name},
        "HOST_COPI": {"pin": host_spi_def[PF.SPI_COPI][0], "channel": host_spi_name},
        "HOST_CIPO": {"pin": host_spi_def[PF.SPI_CIPO][0], "channel": host_spi_name},
        "HOST_CS0": {"pin": host_spi_def[PF.SPI_CS][0], "channel": host_spi_name},
    }
    print(f"Phase 4: Host SPI = {host_spi_name} (RSPI, 60 Mbps), "
          f"pins: {sorted(host_spi_pin_set)}")
else:
    # build conflict report (shouldn't happen now that motor uses Simple SPI)
    conflict_reasons = []
    for ch, funcs in spi_details.items():
        essential = set(funcs[PF.SPI_RSPCK] + funcs[PF.SPI_COPI] + funcs[PF.SPI_CIPO])
        conflicts = essential & reserved_pins
        if conflicts:
            ports = ", ".join(port_short_name(p) for p in sorted(conflicts))
            conflict_reasons.append(
                f"  {ch}: essential pins {sorted(conflicts)} reserved (ports: {ports})")
            continue
        free_cs = [p for p in funcs[PF.SPI_CS] if p not in reserved_pins]
        if not free_cs:
            conflict_reasons.append(f"  {ch}: no free CS pins")
    host_spi_conflict = "\n".join(conflict_reasons)
    print("Phase 4: WARNING — No viable host RSPI channel!")
    print("  Host communication limited to I2C FM+ (RIIC0, 1MHz)")
    for reason in conflict_reasons:
        print(reason)


# ── Phase 5: Encoders (MTU + TPU phase counting) ─────────────────────────
encoder_mapping = {}       # MTU encoders (good quadrature)
encoder_pin_set = set()
tpu_encoder_mapping = {}   # TPU encoders (shitty quadrature)
tpu_encoder_pin_set = set()

# TPU-critical pins were computed before Phase 3 (Pre-Phase 3 block above).
# Recompute with updated reserved_pins to catch any new conflicts after Phases 3-4.
tpu_critical_pins_p5 = set()
if REQUIREMENTS["tpu_encoder_count"] > 0:
    for tpu_ch, clk_funcs in tpu_details.items():
        for clk_name, pin_options in clk_funcs.items():
            free = [p for p in pin_options if p not in reserved_pins]
            if len(free) == 1:
                tpu_critical_pins_p5.add(free[0])
tpu_critical_pins = tpu_critical_pins | tpu_critical_pins_p5

# MTU encoders (good quadrature)
if REQUIREMENTS["mtu_encoder_count"] > 0:
    for mtu_ch, clk_funcs in mtu_details.items():
        # filter against reserved pins AND TPU-critical pins
        filtered_clks = {}
        for clk_name, pin_options in clk_funcs.items():
            free = [p for p in pin_options
                    if p not in reserved_pins and p not in tpu_critical_pins]
            filtered_clks[clk_name] = free

        clk_names = list(filtered_clks.keys())
        if any(not filtered_clks[c] for c in clk_names):
            missing = [c for c in clk_names if not filtered_clks[c]]
            print(f"  WARNING: {mtu_ch} encoder not viable — no pins for: {missing}")
            continue

        # generate all valid combinations (2 clock inputs per MTU channel)
        combos = list(itertools.product(filtered_clks[clk_names[0]], filtered_clks[clk_names[1]]))
        # filter: no pin reuse and no conflict with already-assigned encoder pins
        valid = [
            c for c in combos
            if c[0] != c[1]
            and c[0] not in encoder_pin_set
            and c[1] not in encoder_pin_set
        ]

        if not valid:
            print(f"  WARNING: {mtu_ch} encoder — no valid pin combinations")
            continue

        best = valid[0]
        enc_idx = 0 if mtu_ch == "MTU1" else 1
        encoder_mapping[f"ENC{enc_idx}_{clk_names[0]}"] = {
            "pin": best[0], "channel": mtu_ch, "function": clk_names[0],
        }
        encoder_mapping[f"ENC{enc_idx}_{clk_names[1]}"] = {
            "pin": best[1], "channel": mtu_ch, "function": clk_names[1],
        }
        encoder_pin_set.update(best)

reserved_pins |= encoder_pin_set

# TPU encoders (shitty quadrature)
if REQUIREMENTS["tpu_encoder_count"] > 0:
    tpu_enc_idx = 2  # start numbering after MTU encoders (ENC0, ENC1)
    for tpu_ch, clk_funcs in tpu_details.items():
        # filter each clock input's alternatives against all reserved pins
        filtered_clks = {}
        for clk_name, pin_options in clk_funcs.items():
            free = [p for p in pin_options if p not in reserved_pins]
            filtered_clks[clk_name] = free

        clk_names = list(filtered_clks.keys())
        if any(not filtered_clks[c] for c in clk_names):
            missing = [c for c in clk_names if not filtered_clks[c]]
            print(f"  WARNING: {tpu_ch} encoder not viable — no pins for: {missing}")
            continue

        # generate all valid combinations (2 clock inputs per TPU channel)
        combos = list(itertools.product(filtered_clks[clk_names[0]], filtered_clks[clk_names[1]]))
        # filter: no pin reuse and no conflict with already-assigned encoder/TPU pins
        all_enc_pins = encoder_pin_set | tpu_encoder_pin_set
        valid = [
            c for c in combos
            if c[0] != c[1]
            and c[0] not in all_enc_pins
            and c[1] not in all_enc_pins
        ]

        if not valid:
            print(f"  WARNING: {tpu_ch} encoder — no valid pin combinations")
            continue

        best = valid[0]
        tpu_encoder_mapping[f"ENC{tpu_enc_idx}_{clk_names[0]}"] = {
            "pin": best[0], "channel": tpu_ch, "function": clk_names[0],
        }
        tpu_encoder_mapping[f"ENC{tpu_enc_idx}_{clk_names[1]}"] = {
            "pin": best[1], "channel": tpu_ch, "function": clk_names[1],
        }
        tpu_encoder_pin_set.update(best)
        tpu_enc_idx += 1

reserved_pins |= tpu_encoder_pin_set

mtu_enc_count = len(encoder_mapping)
tpu_enc_count = len(tpu_encoder_mapping)
all_enc_pins = encoder_pin_set | tpu_encoder_pin_set
print(f"Phase 5: {mtu_enc_count} MTU encoder pins: {sorted(encoder_pin_set)}")
print(f"         {tpu_enc_count} TPU encoder pins: {sorted(tpu_encoder_pin_set)}")


# ── Phase 6: Flexible assignment (greedy from remaining I/O) ────────────
available = sorted(
    p for p in range(1, pin_count + 1)
    if p not in reserved_pins and p in pins and PF.CORE not in pins[p]
)


def pick_pins(count, capability, pool, label):
    """Pick `count` pins with `capability` from `pool`, return chosen list."""
    chosen = []
    for p in pool:
        if capability is None or capability in pins.get(p, set()):
            chosen.append(p)
            if len(chosen) == count:
                break
    if len(chosen) < count:
        print(f"  WARNING: Only found {len(chosen)}/{count} pins for {label}")
    return chosen


# Motor ADC: 4 pins with PF.ADC, prefer Port 4 (dedicated analog: AN000-AN007)
port4_adc_pins = {p for p, ch in adc_channel_map.items() if ch.startswith("AN00")}
adc_candidates = sorted(
    [p for p in available if PF.ADC in pins.get(p, set())],
    key=lambda p: (0 if p in port4_adc_pins else 1, p),  # Port 4 first
)
motor_adc_pins = adc_candidates[:REQUIREMENTS["motor_adc_count"]]
motor_adc_mapping = {}
for i, p in enumerate(motor_adc_pins):
    ch_name = adc_channel_map.get(p, f"AN_PIN{p}")
    motor_adc_mapping[f"MOTOR{i}_ISENSE"] = {"pin": p, "channel": ch_name}
    available.remove(p)
reserved_pins |= set(motor_adc_pins)

# BMS interrupt: 1 pin with PF.INTERRUPT
bms_irq_pins = pick_pins(1, PF.INTERRUPT, available, "BMS_IRQ")
bms_mapping = {}
if bms_irq_pins:
    bms_mapping["BMS_ALERT"] = {"pin": bms_irq_pins[0], "function": "IRQ"}
    available.remove(bms_irq_pins[0])
    reserved_pins.add(bms_irq_pins[0])

# Sonar echoes: pins with PF.INTERRUPT (need edge detection for timing)
sonar_echo_pins = pick_pins(REQUIREMENTS["sonar_count"], PF.INTERRUPT, available, "SONAR_ECHO")
sonar_mapping = {}
for i, p in enumerate(sonar_echo_pins):
    sonar_mapping[f"SONAR{i}_ECHO"] = {"pin": p, "function": "IRQ"}
    available.remove(p)
reserved_pins |= set(sonar_echo_pins)

# Sonar triggers: GPIO pins (any non-CORE I/O)
sonar_trig_pins = pick_pins(REQUIREMENTS["sonar_count"], None, available, "SONAR_TRIG")
for i, p in enumerate(sonar_trig_pins):
    sonar_mapping[f"SONAR{i}_TRIG"] = {"pin": p, "function": "GPIO"}
    available.remove(p)
reserved_pins |= set(sonar_trig_pins)

# LEDs: GPIO pins
led_pins = pick_pins(REQUIREMENTS["led_count"], None, available, "LEDS")
led_mapping = {}
for i, p in enumerate(led_pins):
    led_mapping[f"LED{i}"] = {"pin": p, "function": "GPIO"}
    available.remove(p)
reserved_pins |= set(led_pins)

# 1-Wire temperature sensor: GPIO pin(s)
onewire_pins = pick_pins(REQUIREMENTS["onewire_count"], None, available, "ONEWIRE")
onewire_mapping = {}
if onewire_pins:
    onewire_mapping["TEMP_1WIRE"] = {"pin": onewire_pins[0], "function": "GPIO"}
    available.remove(onewire_pins[0])
    reserved_pins.add(onewire_pins[0])

# Bitbang SPI data pins: pick 3 GPIO from remaining pool
# Prefer PE port pins (76, 77) for PCB routing near motor drivers, then PD area
if motor_spi_bitbang:
    bb_pool = list(available)
    # prefer PE port area (76, 77, 75) then PD area (80, 81, 86), then others
    bb_pool.sort(key=lambda p: (
        0 if p in (76, 77) else 1 if 75 <= p <= 79 else 2 if 80 <= p <= 86 else 3, p
    ))
    bb_data_pins = bb_pool[:3]
    if len(bb_data_pins) < 3:
        print(f"  WARNING: Only found {len(bb_data_pins)}/3 GPIO pins for bitbang SPI data")
    # Assign: first=SCK, second=COPI, third=CIPO (arbitrary for bitbang)
    bb_labels = ["DRV_SCK", "DRV_COPI", "DRV_CIPO"]
    for label, p in zip(bb_labels, bb_data_pins):
        motor_spi_mapping[label] = {"pin": p, "channel": "BITBANG"}
        available.remove(p)
        reserved_pins.add(p)
    motor_spi_data_pins = set(bb_data_pins)
    motor_spi_pin_set = motor_spi_data_pins | set(motor_cs_pins)

print(f"Phase 6: ADC={len(motor_adc_pins)} BMS={len(bms_mapping)} "
      f"Sonar={len(sonar_mapping)} LED={len(led_mapping)} "
      f"1Wire={len(onewire_mapping)}"
      + (f" BB_SPI={len(bb_data_pins)}" if motor_spi_bitbang else ""))


# ══════════════════════════════════════════════════════════════════════════
# OUTPUT GENERATION
# ══════════════════════════════════════════════════════════════════════════

all_used_pins = reserved_pins - core_pins


def rspi_fn_key(sig_name):
    """Map RSPI signal name to pin_mpc_function key."""
    if "SCLK" in sig_name:
        return "SPI_RSPCK"
    elif "COPI" in sig_name:
        return "SPI_COPI"
    elif "CIPO" in sig_name:
        return "SPI_CIPO"
    return "SPI_CS"


def simple_spi_fn_key(sig_name):
    """Map Simple SPI (SCI) signal name to pin_mpc_function key."""
    if "SCK" in sig_name:
        return "SIMPLE_SPI_SCK"
    elif "COPI" in sig_name:
        return "SIMPLE_SPI_MOSI"
    elif "CIPO" in sig_name:
        return "SIMPLE_SPI_MISO"
    return None  # GPIO CS — no MPC function


# split phase1_fixed into per-group dicts for output
gtetrg_mapping = {
    name: {"pin": data["pin"], "function": data["function"]}
    for name, data in phase1_fixed.items() if data["group"] == "gtetrg"
}
debug_uart_mapping = {
    name: {"pin": data["pin"], "function": data["function"]}
    for name, data in phase1_fixed.items() if data["group"] == "debug_uart"
}
host_i2c_mapping = {
    name: {"pin": data["pin"], "function": data["function"]}
    for name, data in phase1_fixed.items() if data["group"] == "host_i2c"
}
bms_i2c_mapping = {
    name: {"pin": data["pin"], "function": data["function"]}
    for name, data in phase1_fixed.items() if data["group"] == "bms_i2c"
}

# ── JSON output ──────────────────────────────────────────────────────────
result = {
    "metadata": {
        "package": package,
        "chip": chip_name,
        "pin_count": pin_count,
        "total_valid_motor_combos": len(valid_motor_combos),
        "encoder_compatible_combos": len(encoder_compatible) if encoder_compatible else 0,
        "motor_spi_channel": motor_spi_name,
        "host_spi_channel": host_spi_name,
        "host_spi_conflict": host_spi_conflict,
        "total_pins_used": len(all_used_pins),
    },
    "pin_assignments": {
        "gtetrg": gtetrg_mapping,
        "debug_uart": debug_uart_mapping,
        "host_i2c": host_i2c_mapping,
        "bms_i2c": bms_i2c_mapping,
        "motor_driver_spi": motor_spi_mapping,
        "motor_pwm": motor_pwm_mapping,
        "host_spi": host_spi_mapping,
        "encoders": encoder_mapping,
        "tpu_encoders": tpu_encoder_mapping,
        "motor_adc": motor_adc_mapping,
        "bms_interrupt": bms_mapping,
        "sonar": sonar_mapping,
        "leds": led_mapping,
        "onewire": onewire_mapping,
    },
    "validation": {
        "total_pins_used": len(all_used_pins),
        "unique_pins": sorted(all_used_pins),
        "conflicts_detected": False,
        "conflict_details": [],
    },
}

# validate no conflicts: check every assigned pin is unique
all_assigned = []
for group_name, group_data in result["pin_assignments"].items():
    for sig_name, sig_data in group_data.items():
        all_assigned.append((group_name, sig_name, sig_data["pin"]))

pin_counts = {}
for group, sig, pin in all_assigned:
    pin_counts.setdefault(pin, []).append(f"{group}/{sig}")

conflicts = {p: users for p, users in pin_counts.items() if len(users) > 1}
if conflicts:
    result["validation"]["conflicts_detected"] = True
    result["validation"]["conflict_details"] = [
        {"pin": p, "port": port_short_name(p), "assigned_to": users}
        for p, users in sorted(conflicts.items())
    ]

output_dir = os.path.join(os.path.dirname(__file__), "output")
os.makedirs(output_dir, exist_ok=True)

with open(os.path.join(output_dir, "pin_assignment_solution.json"), "w") as f:
    json.dump(result, f, indent=2)

# ── Pin assignment lookup (for TXT and pinout JSON) ──────────────────────
pin_assignment_lookup = {}

# Phase 1 fixed peripherals
for name, data in phase1_fixed.items():
    p = data["pin"]
    fn = data["function"]
    mpc = pin_mpc_function.get(p, {}).get(fn, "")
    pin_assignment_lookup[p] = {
        "signal": name, "group": data["group"], "function": fn, "mpc_name": mpc,
    }

# Motor SPI (Simple SPI / SCI)
for sig_name, sig_data in motor_spi_mapping.items():
    p = sig_data["pin"]
    fn = simple_spi_fn_key(sig_name)
    mpc = pin_mpc_function.get(p, {}).get(fn, "") if fn else ""
    pin_assignment_lookup[p] = {
        "signal": sig_name, "group": "motor_driver_spi",
        "channel": sig_data["channel"], "mpc_name": mpc,
    }

# Motor PWM
for motor_name, motor_data in motor_pwm_mapping.items():
    p = motor_data["pin"]
    mpc = pin_mpc_function.get(p, {}).get("GPTW_PWM", "")
    pin_assignment_lookup[p] = {
        "signal": motor_name, "group": "motor_pwm",
        "channel": motor_data["channel"], "subchannel": motor_data["subchannel"],
        "mpc_name": mpc,
    }

# Host SPI (RSPI)
for sig_name, sig_data in host_spi_mapping.items():
    p = sig_data["pin"]
    fn = rspi_fn_key(sig_name)
    mpc = pin_mpc_function.get(p, {}).get(fn, "")
    pin_assignment_lookup[p] = {
        "signal": sig_name, "group": "host_spi",
        "channel": sig_data["channel"], "mpc_name": mpc,
    }

# MTU Encoders
for enc_name, enc_data in encoder_mapping.items():
    p = enc_data["pin"]
    mpc = pin_mpc_function.get(p, {}).get("MTU_CLK", "")
    pin_assignment_lookup[p] = {
        "signal": enc_name, "group": "encoders",
        "channel": enc_data["channel"], "function": enc_data["function"],
        "mpc_name": mpc,
    }

# TPU Encoders
for enc_name, enc_data in tpu_encoder_mapping.items():
    p = enc_data["pin"]
    mpc = pin_mpc_function.get(p, {}).get("TPU_CLK", "")
    pin_assignment_lookup[p] = {
        "signal": enc_name, "group": "tpu_encoders",
        "channel": enc_data["channel"], "function": enc_data["function"],
        "mpc_name": mpc,
    }

# Motor ADC
for adc_name, adc_data in motor_adc_mapping.items():
    p = adc_data["pin"]
    mpc = pin_mpc_function.get(p, {}).get("ADC", "")
    pin_assignment_lookup[p] = {
        "signal": adc_name, "group": "motor_adc",
        "channel": adc_data["channel"], "mpc_name": mpc,
    }

# BMS interrupt
for name, data in bms_mapping.items():
    pin_assignment_lookup[data["pin"]] = {
        "signal": name, "group": "bms_interrupt",
        "function": data["function"], "mpc_name": "",
    }

# Sonar
for name, data in sonar_mapping.items():
    pin_assignment_lookup[data["pin"]] = {
        "signal": name, "group": "sonar",
        "function": data["function"], "mpc_name": "",
    }

# LEDs
for name, data in led_mapping.items():
    pin_assignment_lookup[data["pin"]] = {
        "signal": name, "group": "leds",
        "function": data["function"], "mpc_name": "",
    }

# 1-Wire
for name, data in onewire_mapping.items():
    pin_assignment_lookup[data["pin"]] = {
        "signal": name, "group": "onewire",
        "function": data["function"], "mpc_name": "",
    }

# ── Pinout JSON ──────────────────────────────────────────────────────────
pinout_json = {
    "chip": chip_name,
    "package": f"{pin_count}-pin LFQFP",
    "manual_ref": "RX72N User's Manual R01UH0824EJ0120 Rev.1.20, Table 1.10",
    "date": str(date.today()),
    "pins": [],
}
for pin_num in range(1, pin_count + 1):
    port = port_short_name(pin_num)
    full_name = pin_to_port.get(pin_num, "???")
    entry = {"pin": pin_num, "port": port, "full_name": full_name,
             "assigned": pin_num in pin_assignment_lookup}
    if pin_num in pin_assignment_lookup:
        entry["assignment"] = pin_assignment_lookup[pin_num]
    pinout_json["pins"].append(entry)

with open(os.path.join(output_dir, "pin_assignment_pinout.json"), "w") as f:
    json.dump(pinout_json, f, indent=2)

# ── Pinout TXT ───────────────────────────────────────────────────────────
with open(os.path.join(output_dir, "pin_assignment_pinout.txt"), "w") as f:
    f.write("RX72N Pin Assignment Pinout\n")
    f.write(f"Chip: {chip_name} ({pin_count}-pin LFQFP)\n")
    f.write(f"Generated: {date.today()}\n")
    f.write(f"Motor SPI: {motor_spi_name}\n")
    if host_spi_name:
        f.write(f"Host SPI:  {host_spi_name}\n")
    else:
        f.write("Host SPI:  NONE (conflict — see details below)\n")
    unassigned_io = [p for p in range(1, pin_count + 1) if p in pins and p not in all_used_pins
                     and p not in core_pins and PF.CORE not in pins[p]]
    f.write(f"Total Pins Used: {len(all_used_pins)} / {pin_count}\n")
    f.write(f"  Assigned: {len(all_used_pins)}  |  "
            f"Unassigned I/O: {len(unassigned_io)}  |  "
            f"CORE (power/clk/fixed): {len(core_pins)}\n")
    f.write("=" * 90 + "\n\n")

    # ── Assigned pins table ──
    f.write("ASSIGNED PINS\n")
    f.write("-" * 90 + "\n")
    f.write(f"{'Pin':>4}  {'Port':<6}  {'Signal':<16}  {'Group':<18}  "
            f"{'MPC/Function':<12}  {'Details'}\n")
    f.write("-" * 90 + "\n")
    for pin_num in sorted(pin_assignment_lookup.keys()):
        port = port_short_name(pin_num)
        full_name = pin_to_port.get(pin_num, "???")
        info = pin_assignment_lookup[pin_num]
        mpc = info.get("mpc_name", "")
        details = ""
        if info["group"] == "motor_pwm":
            details = f"{info['channel']} subchannel {info['subchannel']}"
        elif info["group"] in ("motor_driver_spi", "host_spi"):
            details = info.get("channel", "")
        elif info["group"] == "gtetrg":
            details = info.get("function", "")
        elif info["group"] in ("encoders", "tpu_encoders"):
            details = f"{info['channel']} {info['function']}"
        elif info["group"] == "motor_adc":
            details = info.get("channel", "")
        f.write(f"{pin_num:>4}  {port:<6}  {info['signal']:<16}  "
                f"{info['group']:<18}  {mpc:<12}  {details}\n")
        if full_name != port:
            f.write(f"        -> {full_name}\n")

    # ── Group summaries ──
    f.write("\n")
    f.write("GTETRG ASSIGNMENTS (DRV8243S nFAULT -> GPTW emergency stop)\n")
    f.write("-" * 90 + "\n")
    for name, data in gtetrg_mapping.items():
        port = port_short_name(data["pin"])
        full_name = pin_to_port.get(data["pin"], "???")
        mpc = pin_mpc_function.get(data["pin"], {}).get("GPTW_EXT_TRIG", "")
        f.write(f"  {name:<10}  Pin {data['pin']:>2} / {port:<5} -> "
                f"{mpc:<12}  ({data['function']})\n")
        if full_name != port:
            f.write(f"              -> {full_name}\n")

    f.write("\n")
    f.write("DEBUG UART (SCI9)\n")
    f.write("-" * 90 + "\n")
    for name, data in debug_uart_mapping.items():
        port = port_short_name(data["pin"])
        full_name = pin_to_port.get(data["pin"], "???")
        mpc = pin_mpc_function.get(data["pin"], {}).get(data["function"], "")
        f.write(f"  {name:<12}  Pin {data['pin']:>2} / {port:<5} -> {mpc}\n")
        if full_name != port:
            f.write(f"                -> {full_name}\n")

    f.write("\n")
    f.write("HOST I2C (RIIC0 FM+ 1MHz)\n")
    f.write("-" * 90 + "\n")
    for name, data in host_i2c_mapping.items():
        port = port_short_name(data["pin"])
        full_name = pin_to_port.get(data["pin"], "???")
        mpc = pin_mpc_function.get(data["pin"], {}).get(data["function"], "")
        f.write(f"  {name:<12}  Pin {data['pin']:>2} / {port:<5} -> {mpc}\n")
        if full_name != port:
            f.write(f"                -> {full_name}\n")

    f.write("\n")
    f.write("BMS I2C (RIIC1)\n")
    f.write("-" * 90 + "\n")
    for name, data in bms_i2c_mapping.items():
        port = port_short_name(data["pin"])
        full_name = pin_to_port.get(data["pin"], "???")
        mpc = pin_mpc_function.get(data["pin"], {}).get(data["function"], "")
        f.write(f"  {name:<12}  Pin {data['pin']:>2} / {port:<5} -> {mpc}\n")
        if full_name != port:
            f.write(f"                -> {full_name}\n")

    f.write("\n")
    spi_mode_label = "Bitbang GPIO" if motor_spi_bitbang else f"{motor_spi_name} Simple SPI"
    f.write(f"MOTOR DRIVER SPI (DRV8243S — {spi_mode_label})\n")
    f.write("-" * 90 + "\n")
    for sig_name, sig_data in motor_spi_mapping.items():
        port = port_short_name(sig_data["pin"])
        full_name = pin_to_port.get(sig_data["pin"], "???")
        if motor_spi_bitbang and "CS" not in sig_name:
            mpc = "GPIO"  # bitbang data pins are plain GPIO
        else:
            fn = simple_spi_fn_key(sig_name)
            mpc = pin_mpc_function.get(sig_data["pin"], {}).get(fn, "") if fn else "GPIO"
        f.write(f"  {sig_name:<12}  Pin {sig_data['pin']:>2} / {port:<5} -> "
                f"{mpc:<10}  ({sig_data['channel']})\n")
        if full_name != port:
            f.write(f"                -> {full_name}\n")

    f.write("\n")
    f.write("MOTOR PWM ASSIGNMENTS\n")
    f.write("-" * 90 + "\n")
    for i in range(4):
        ph = motor_pwm_mapping[f"MOTOR{i}_PH"]
        en = motor_pwm_mapping[f"MOTOR{i}_EN"]
        ph_port = port_short_name(ph["pin"])
        en_port = port_short_name(en["pin"])
        ph_full = pin_to_port.get(ph["pin"], "???")
        en_full = pin_to_port.get(en["pin"], "???")
        ph_mpc = pin_mpc_function.get(ph["pin"], {}).get("GPTW_PWM", "")
        en_mpc = pin_mpc_function.get(en["pin"], {}).get("GPTW_PWM", "")
        f.write(f"  Motor {i} ({ph['channel']}):\n")
        f.write(f"    Phase (A):  Pin {ph['pin']:>2} / {ph_port:<5} -> {ph_mpc}\n")
        if ph_full != ph_port:
            f.write(f"                -> {ph_full}\n")
        f.write(f"    Enable (B): Pin {en['pin']:>2} / {en_port:<5} -> {en_mpc}\n")
        if en_full != en_port:
            f.write(f"                -> {en_full}\n")

    if host_spi_mapping:
        f.write("\n")
        f.write(f"HOST SPI ({host_spi_name} — RSPI, 60 Mbps)\n")
        f.write("-" * 90 + "\n")
        for sig_name, sig_data in host_spi_mapping.items():
            port = port_short_name(sig_data["pin"])
            full_name = pin_to_port.get(sig_data["pin"], "???")
            fn = rspi_fn_key(sig_name)
            mpc = pin_mpc_function.get(sig_data["pin"], {}).get(fn, "")
            f.write(f"  {sig_name:<12}  Pin {sig_data['pin']:>2} / {port:<5} -> "
                    f"{mpc:<10}  ({sig_data['channel']})\n")
            if full_name != port:
                f.write(f"                -> {full_name}\n")
    elif host_spi_conflict:
        f.write("\n")
        f.write("HOST SPI — NO VIABLE CHANNEL\n")
        f.write("-" * 90 + "\n")
        f.write("  All SPI channels conflict with reserved pins.\n")
        f.write("  Host communication limited to I2C FM+ (RIIC0, 1MHz).\n")
        f.write("  Conflict details:\n")
        f.write(host_spi_conflict + "\n")

    if encoder_mapping:
        f.write("\n")
        f.write("MTU ENCODER ASSIGNMENTS (MTU phase counting — good quadrature)\n")
        f.write("-" * 90 + "\n")
        for enc_name, enc_data in encoder_mapping.items():
            port = port_short_name(enc_data["pin"])
            full_name = pin_to_port.get(enc_data["pin"], "???")
            mpc = pin_mpc_function.get(enc_data["pin"], {}).get("MTU_CLK", "")
            f.write(f"  {enc_name:<16}  Pin {enc_data['pin']:>2} / {port:<5} -> "
                    f"{mpc:<10}  ({enc_data['channel']})\n")
            if full_name != port:
                f.write(f"                    -> {full_name}\n")

    if tpu_encoder_mapping:
        f.write("\n")
        f.write("TPU ENCODER ASSIGNMENTS (TPU phase counting — shitty quadrature)\n")
        f.write("-" * 90 + "\n")
        for enc_name, enc_data in tpu_encoder_mapping.items():
            port = port_short_name(enc_data["pin"])
            full_name = pin_to_port.get(enc_data["pin"], "???")
            mpc = pin_mpc_function.get(enc_data["pin"], {}).get("TPU_CLK", "")
            f.write(f"  {enc_name:<16}  Pin {enc_data['pin']:>2} / {port:<5} -> "
                    f"{mpc:<10}  ({enc_data['channel']})\n")
            if full_name != port:
                f.write(f"                    -> {full_name}\n")

    if motor_adc_mapping:
        f.write("\n")
        f.write("MOTOR ADC (current sensing)\n")
        f.write("-" * 90 + "\n")
        for adc_name, adc_data in motor_adc_mapping.items():
            port = port_short_name(adc_data["pin"])
            full_name = pin_to_port.get(adc_data["pin"], "???")
            f.write(f"  {adc_name:<16}  Pin {adc_data['pin']:>2} / {port:<5} -> "
                    f"{adc_data['channel']}\n")
            if full_name != port:
                f.write(f"                    -> {full_name}\n")

    if bms_mapping:
        f.write("\n")
        f.write("BMS INTERRUPT\n")
        f.write("-" * 90 + "\n")
        for name, data in bms_mapping.items():
            port = port_short_name(data["pin"])
            full_name = pin_to_port.get(data["pin"], "???")
            f.write(f"  {name:<16}  Pin {data['pin']:>2} / {port:<5}\n")
            if full_name != port:
                f.write(f"                    -> {full_name}\n")

    if sonar_mapping:
        f.write("\n")
        f.write("SONAR (HC-SR04)\n")
        f.write("-" * 90 + "\n")
        for name, data in sonar_mapping.items():
            port = port_short_name(data["pin"])
            full_name = pin_to_port.get(data["pin"], "???")
            f.write(f"  {name:<16}  Pin {data['pin']:>2} / {port:<5}  "
                    f"({data['function']})\n")
            if full_name != port:
                f.write(f"                    -> {full_name}\n")

    if led_mapping:
        f.write("\n")
        f.write("LED GPIO\n")
        f.write("-" * 90 + "\n")
        for name, data in led_mapping.items():
            port = port_short_name(data["pin"])
            full_name = pin_to_port.get(data["pin"], "???")
            f.write(f"  {name:<16}  Pin {data['pin']:>2} / {port:<5}\n")
            if full_name != port:
                f.write(f"                    -> {full_name}\n")

    if onewire_mapping:
        f.write("\n")
        f.write("1-WIRE TEMPERATURE SENSOR\n")
        f.write("-" * 90 + "\n")
        for name, data in onewire_mapping.items():
            port = port_short_name(data["pin"])
            full_name = pin_to_port.get(data["pin"], "???")
            f.write(f"  {name:<16}  Pin {data['pin']:>2} / {port:<5}\n")
            if full_name != port:
                f.write(f"                    -> {full_name}\n")

    f.write("\n")
    f.write("UNASSIGNED I/O PINS (available for other use)\n")
    f.write("-" * 90 + "\n")
    io_pins = [p for p in range(1, pin_count + 1) if p in pins and p not in pin_assignment_lookup
               and PF.CORE not in pins[p]]
    for pin_num in io_pins:
        port = port_short_name(pin_num)
        full_name = pin_to_port.get(pin_num, "???")
        caps = ", ".join(sorted(pf.name for pf in pins[pin_num]))
        f.write(f"  Pin {pin_num:>2} / {port:<5}  Capabilities: {caps}\n")
        if full_name != port:
            f.write(f"          -> {full_name}\n")

    f.write("\n")
    f.write(f"CORE PINS (power, ground, clock, fixed-function — not assignable) "
            f"[{len(core_pins)} pins]\n")
    f.write("-" * 90 + "\n")
    core_sorted = sorted(core_pins)
    for pin_num in core_sorted:
        full_name = pin_to_port.get(pin_num, "???")
        f.write(f"  Pin {pin_num:>2} / {full_name}\n")

# ── Summary ──────────────────────────────────────────────────────────────
print(f"\nTotal pins assigned: {len(all_used_pins)} / {pin_count}")
print(f"Output written to: {output_dir}/")
if result["validation"]["conflicts_detected"]:
    print("WARNING: Pin conflicts detected!")
    for c in result["validation"]["conflict_details"]:
        print(f"  Pin {c['pin']} ({c['port']}): {c['assigned_to']}")


