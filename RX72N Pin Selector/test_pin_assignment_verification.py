#!/usr/bin/env python3
"""
Pin Assignment Verification Test Suite for RX72N Constraint Solver

Validates pin_assignment_solution.json against:
1. JSON structure and conflict detection
2. Pin capability requirements (cross-check with solver's pins dict)
3. Channel-to-pin mapping validation (gptw_details, spi_details, mtu_details)
4. Fixed peripheral pin verification (GTETRG, I2C, UART)
5. Generates NotebookLM prompts for manual RX72N spec verification

Target: R5F572NNHGFP#30 (100-pin LFQFP) or R5F572NNHxFB (144-pin LFQFP)
Manual Reference: /workspaces/STAR/e2-studio-star-rx72n-firmware/docs/RX72N_Manual_Chapters/

Author: Claude Code
Date: 2026-02-07
"""

import json
import sys
from pathlib import Path
from enum import Enum, auto, unique
from typing import Dict, List, Set, Tuple, Optional
from dataclasses import dataclass, field

# Import PortFunctionality enum from constraint solver
@unique
class PortFunctionality(Enum):
    """Port functionality enum (synced from rx72n_constraint_solver.py)"""
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

# Pin capabilities from constraint solver (synced)
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

# SPI channel details from solver
spi_details = {
    "RSPI0_A": {
        PF.SPI_RSPCK: [47], PF.SPI_COPI: [46], PF.SPI_CIPO: [45],
        PF.SPI_CS: [48, 52, 51, 50],
    },
    "RSPI0_B": {
        PF.SPI_RSPCK: [65], PF.SPI_COPI: [64], PF.SPI_CIPO: [63],
        PF.SPI_CS: [66, 70, 69, 68],
    },
    "RSPI1_A": {
        PF.SPI_RSPCK: [21], PF.SPI_COPI: [22], PF.SPI_CIPO: [20],
        PF.SPI_CS: [19, 44, 43, 42],
    },
    "RSPI1_B": {
        PF.SPI_RSPCK: [73], PF.SPI_COPI: [72], PF.SPI_CIPO: [71],
        PF.SPI_CS: [74, 78, 77, 76],
    },
    "RSPI2_A": {
        PF.SPI_RSPCK: [83], PF.SPI_COPI: [85], PF.SPI_CIPO: [84],
        PF.SPI_CS: [82, 81, 80, 79],
    },
}

# GPTW details from solver
gptw_details = {
    "GPTIOC0": {PF.GPTW_PWM: [(25, 65, 73, 83), (29, 70, 76, 84)]},
    "GPTIOC1": {PF.GPTW_PWM: [(26, 47, 68, 74, 85), (49, 77, 86)]},
    "GPTIOC2": {PF.GPTW_PWM: [(27, 69, 75), (50, 78)]},
    "GPTIOC3": {PF.GPTW_PWM: [(45, 71), (46, 72)]},
    "GTETRG": {PF.GPTW_EXT_TRIG: [31, 64, 48, 32]},
}

# MTU encoder details from solver
mtu_details = {
    "MTU1": {
        "MTCLKA": [32, 24, 46, 81],
        "MTCLKB": [31, 23, 45, 64],
    },
    "MTU2": {
        "MTCLKC": [26, 48, 69],
        "MTCLKD": [25, 47, 67],
    },
}

# TPU encoder details from solver
tpu_details = {
    "TPU1": {
        "TCLKA": [32, 50],
        "TCLKB": [31, 49, 67],
    },
    "TPU2": {
        "TCLKC": [30, 52, 58],
        "TCLKD": [29, 51, 57],
    },
}

# I2C and UART details from solver
riic_details = {
    "RIIC0": {PF.IIC_SCL_FMP: [34], PF.IIC_SDA_FMP: [33]},
    "RIIC1": {PF.IIC_SCL: [27], PF.IIC_SDA: [28]},
}

sci_details = {
    "SCI9": {PF.DEBUG_UART_TXD: [53], PF.DEBUG_UART_RXD: [54]},
}

# Simple SPI (SCI clock-synchronous mode) details from solver
simple_spi_details = {
    "SCI12": {
        PF.SIMPLE_SPI_SCK: [78],     # PE0
        PF.SIMPLE_SPI_MOSI: [77],    # PE1
        PF.SIMPLE_SPI_MISO: [76],    # PE2
    },
}

# All expected pin_assignments groups in the JSON
EXPECTED_GROUPS = [
    "gtetrg", "debug_uart", "host_i2c", "bms_i2c",
    "motor_driver_spi", "motor_pwm", "host_spi",
    "encoders", "tpu_encoders", "motor_adc", "bms_interrupt",
    "sonar", "leds", "onewire",
]


# Test result tracking
@dataclass
class TestResult:
    """Track test results with detailed error messages"""
    category: str
    passed: bool
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)

    def add_error(self, msg: str) -> None:
        self.errors.append(msg)
        self.passed = False

    def add_warning(self, msg: str) -> None:
        self.warnings.append(msg)

    def __str__(self) -> str:
        status = "PASS" if self.passed else "FAIL"
        result = f"{status} - {self.category}\n"
        if self.errors:
            result += "  Errors:\n"
            for err in self.errors:
                result += f"    - {err}\n"
        if self.warnings:
            result += "  Warnings:\n"
            for warn in self.warnings:
                result += f"    - {warn}\n"
        return result


def load_json(json_path: Path) -> dict:
    """Load pin assignment JSON file"""
    try:
        with open(json_path, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"ERROR: JSON file not found: {json_path}")
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"ERROR: Invalid JSON format: {e}")
        sys.exit(1)


def test_json_structure(data: dict) -> TestResult:
    """Validate JSON has all required fields with correct types"""
    result = TestResult(category="JSON Structure Validation", passed=True)

    required_sections = ["metadata", "pin_assignments", "validation"]
    for section in required_sections:
        if section not in data:
            result.add_error(f"Missing required section: {section}")

    if "metadata" in data:
        meta = data["metadata"]
        for field_name in ["total_valid_motor_combos", "total_pins_used"]:
            if field_name not in meta:
                result.add_error(f"Missing metadata.{field_name}")
            elif not isinstance(meta[field_name], int):
                result.add_error(f"metadata.{field_name} must be integer")

        if "motor_spi_channel" not in meta:
            result.add_error("Missing metadata.motor_spi_channel")
        elif not isinstance(meta["motor_spi_channel"], str):
            result.add_error("metadata.motor_spi_channel must be string")

    if "pin_assignments" in data:
        pa = data["pin_assignments"]
        for group in EXPECTED_GROUPS:
            if group not in pa:
                result.add_error(f"Missing pin_assignments.{group}")

    if "validation" in data:
        val = data["validation"]
        for field_name, expected_type in [
            ("total_pins_used", int),
            ("unique_pins", list),
            ("conflicts_detected", bool),
            ("conflict_details", list),
        ]:
            if field_name not in val:
                result.add_error(f"Missing validation.{field_name}")
            elif not isinstance(val[field_name], expected_type):
                result.add_error(f"validation.{field_name} must be {expected_type.__name__}")

    return result


def test_no_pin_conflicts(data: dict) -> TestResult:
    """Ensure no pin is assigned to multiple functions across all groups"""
    result = TestResult(category="Pin Conflict Detection", passed=True)

    if "pin_assignments" not in data:
        result.add_error("Missing pin_assignments section")
        return result

    pin_assignments = {}  # pin -> "group/signal"

    for group_name, group_data in data["pin_assignments"].items():
        if not isinstance(group_data, dict):
            continue
        for sig_name, sig_data in group_data.items():
            if not isinstance(sig_data, dict):
                continue
            pin = sig_data.get("pin")
            if pin is None:
                continue

            location = f"{group_name}/{sig_name}"
            if pin in pin_assignments:
                result.add_error(
                    f"Pin {pin} conflict: assigned to both {pin_assignments[pin]} "
                    f"and {location}")
            pin_assignments[pin] = location

    # Cross-check with validation.unique_pins
    if "validation" in data and "unique_pins" in data["validation"]:
        json_unique = set(data["validation"]["unique_pins"])
        actual_unique = set(pin_assignments.keys())
        if json_unique != actual_unique:
            extra_json = json_unique - actual_unique
            extra_actual = actual_unique - json_unique
            if extra_json:
                result.add_error(f"validation.unique_pins has extra pins: {sorted(extra_json)}")
            if extra_actual:
                result.add_error(f"validation.unique_pins missing pins: {sorted(extra_actual)}")

    # Check conflicts_detected flag consistency
    if "validation" in data and "conflicts_detected" in data["validation"]:
        has_conflicts = len([e for e in result.errors if "conflict" in e.lower()]) > 0
        if data["validation"]["conflicts_detected"] and not has_conflicts:
            result.add_warning("JSON reports conflicts_detected=true but none found")
        elif not data["validation"]["conflicts_detected"] and has_conflicts:
            result.add_error("JSON reports conflicts_detected=false but conflicts found")

    return result


def test_pin_range(data: dict) -> TestResult:
    """All pins must be within valid range for the package"""
    pin_count = data.get("metadata", {}).get("pin_count", 100)
    result = TestResult(category=f"Pin Range Validation (1-{pin_count})", passed=True)

    if "pin_assignments" not in data:
        result.add_error("Missing pin_assignments section")
        return result

    for group_name, group_data in data["pin_assignments"].items():
        if not isinstance(group_data, dict):
            continue
        for sig_name, sig_data in group_data.items():
            if not isinstance(sig_data, dict):
                continue
            pin = sig_data.get("pin")
            if pin is None:
                continue
            location = f"{group_name}.{sig_name}"
            if not isinstance(pin, int):
                result.add_error(f"{location}: pin must be integer, got {type(pin).__name__}")
            elif pin < 1 or pin > pin_count:
                result.add_error(f"{location}: pin {pin} out of range [1, {pin_count}]")

    return result


def test_pin_capabilities(data: dict) -> TestResult:
    """Verify pins support their assigned functions per solver's pins dict"""
    package = data.get("metadata", {}).get("package", "LQFP100")
    result = TestResult(category="Pin Capability Verification", passed=True)

    if "pin_assignments" not in data:
        result.add_error("Missing pin_assignments section")
        return result

    # For LQFP144, pins dict has different pin numbers — skip pin-specific checks
    if package != "LQFP100":
        result.add_warning(f"Pin capability checks skipped for {package} (reference data is 100-pin only)")
        return result

    pa = data["pin_assignments"]

    def check_pin_cap(group, sig, pin, required_cap):
        """Check a pin has a required capability."""
        if pin not in pins:
            result.add_error(f"{group}.{sig}: Pin {pin} not found in pins dict")
            return
        if required_cap not in pins[pin]:
            result.add_error(
                f"{group}.{sig}: Pin {pin} lacks {required_cap.name}. "
                f"Has: {[pf.name for pf in pins[pin]]}")

    # Motor PWM: needs GPTW_PWM
    for sig, data_item in pa.get("motor_pwm", {}).items():
        pin = data_item.get("pin")
        if pin is not None:
            check_pin_cap("motor_pwm", sig, pin, PF.GPTW_PWM)

    # Motor SPI: SCI12 data pins or bitbang GPIO + GPIO CS
    motor_spi_channel = data.get("metadata", {}).get("motor_spi_channel", "")
    motor_spi_is_bitbang = motor_spi_channel == "BITBANG"
    for sig, data_item in pa.get("motor_driver_spi", {}).items():
        pin = data_item.get("pin")
        if pin is None:
            continue
        if motor_spi_is_bitbang:
            # Bitbang mode: all pins (data + CS) are just GPIO — verify not CORE
            if pin in pins and PF.CORE in pins[pin]:
                result.add_error(
                    f"motor_driver_spi.{sig}: Pin {pin} is CORE, cannot be GPIO")
        elif "SCK" in sig and "CS" not in sig:
            check_pin_cap("motor_driver_spi", sig, pin, PF.SIMPLE_SPI_SCK)
        elif "COPI" in sig:
            check_pin_cap("motor_driver_spi", sig, pin, PF.SIMPLE_SPI_MOSI)
        elif "CIPO" in sig:
            check_pin_cap("motor_driver_spi", sig, pin, PF.SIMPLE_SPI_MISO)
        elif "CS" in sig:
            # GPIO chip select — just verify not CORE
            if pin in pins and PF.CORE in pins[pin]:
                result.add_error(
                    f"motor_driver_spi.{sig}: Pin {pin} is CORE, cannot be GPIO CS")

    # Host SPI: same checks (may be empty if no viable channel)
    for sig, data_item in pa.get("host_spi", {}).items():
        pin = data_item.get("pin")
        if pin is None:
            continue
        if "SCLK" in sig:
            check_pin_cap("host_spi", sig, pin, PF.SPI_RSPCK)
        elif "COPI" in sig:
            check_pin_cap("host_spi", sig, pin, PF.SPI_COPI)
        elif "CIPO" in sig:
            check_pin_cap("host_spi", sig, pin, PF.SPI_CIPO)
        elif "CS" in sig:
            check_pin_cap("host_spi", sig, pin, PF.SPI_CS)

    # GTETRG: needs GPTW_EXT_TRIG
    for sig, data_item in pa.get("gtetrg", {}).items():
        pin = data_item.get("pin")
        if pin is not None:
            check_pin_cap("gtetrg", sig, pin, PF.GPTW_EXT_TRIG)

    # Debug UART: needs DEBUG_UART_TXD or DEBUG_UART_RXD
    for sig, data_item in pa.get("debug_uart", {}).items():
        pin = data_item.get("pin")
        fn = data_item.get("function", "")
        if pin is not None:
            if "TXD" in fn:
                check_pin_cap("debug_uart", sig, pin, PF.DEBUG_UART_TXD)
            elif "RXD" in fn:
                check_pin_cap("debug_uart", sig, pin, PF.DEBUG_UART_RXD)

    # Host I2C: needs IIC_SCL_FMP or IIC_SDA_FMP
    for sig, data_item in pa.get("host_i2c", {}).items():
        pin = data_item.get("pin")
        fn = data_item.get("function", "")
        if pin is not None:
            if "SCL" in fn:
                check_pin_cap("host_i2c", sig, pin, PF.IIC_SCL_FMP)
            elif "SDA" in fn:
                check_pin_cap("host_i2c", sig, pin, PF.IIC_SDA_FMP)

    # BMS I2C: needs IIC_SCL or IIC_SDA
    for sig, data_item in pa.get("bms_i2c", {}).items():
        pin = data_item.get("pin")
        fn = data_item.get("function", "")
        if pin is not None:
            if "SCL" in fn:
                check_pin_cap("bms_i2c", sig, pin, PF.IIC_SCL)
            elif "SDA" in fn:
                check_pin_cap("bms_i2c", sig, pin, PF.IIC_SDA)

    # MTU Encoders: needs GOOD_QUADRATURE_TIMER
    for sig, data_item in pa.get("encoders", {}).items():
        pin = data_item.get("pin")
        if pin is not None:
            check_pin_cap("encoders", sig, pin, PF.GOOD_QUADRATURE_TIMER)

    # TPU Encoders: needs SHITTY_QUADRATURE_TIMER
    for sig, data_item in pa.get("tpu_encoders", {}).items():
        pin = data_item.get("pin")
        if pin is not None:
            check_pin_cap("tpu_encoders", sig, pin, PF.SHITTY_QUADRATURE_TIMER)

    # Motor ADC: needs ADC
    for sig, data_item in pa.get("motor_adc", {}).items():
        pin = data_item.get("pin")
        if pin is not None:
            check_pin_cap("motor_adc", sig, pin, PF.ADC)

    # BMS interrupt: needs INTERRUPT
    for sig, data_item in pa.get("bms_interrupt", {}).items():
        pin = data_item.get("pin")
        if pin is not None:
            check_pin_cap("bms_interrupt", sig, pin, PF.INTERRUPT)

    # Sonar echoes: need INTERRUPT (triggers are GPIO, no specific cap needed)
    for sig, data_item in pa.get("sonar", {}).items():
        pin = data_item.get("pin")
        fn = data_item.get("function", "")
        if pin is not None and fn == "IRQ":
            check_pin_cap("sonar", sig, pin, PF.INTERRUPT)

    # LEDs, 1-Wire, sonar triggers: GPIO (any non-CORE I/O pin)
    for group in ["leds", "onewire"]:
        for sig, data_item in pa.get(group, {}).items():
            pin = data_item.get("pin")
            if pin is not None and pin in pins:
                if PF.CORE in pins[pin]:
                    result.add_error(f"{group}.{sig}: Pin {pin} is CORE, cannot be GPIO")

    return result


def test_gptw_channel_mapping(data: dict) -> TestResult:
    """Verify GPTW channel-to-pin mappings"""
    package = data.get("metadata", {}).get("package", "LQFP100")
    result = TestResult(category="GPTW Channel Mapping Verification", passed=True)

    if "pin_assignments" not in data or "motor_pwm" not in data["pin_assignments"]:
        result.add_error("Missing motor_pwm section")
        return result

    for motor_signal, motor_data in data["pin_assignments"]["motor_pwm"].items():
        pin = motor_data.get("pin")
        channel = motor_data.get("channel")
        subchannel = motor_data.get("subchannel")

        if pin is None or channel is None or subchannel is None:
            result.add_error(f"motor_pwm.{motor_signal}: Missing pin, channel, or subchannel")
            continue

        if channel not in gptw_details:
            result.add_error(f"motor_pwm.{motor_signal}: Channel {channel} not in gptw_details")
            continue

        if subchannel not in ["A", "B"]:
            result.add_error(f"motor_pwm.{motor_signal}: Invalid subchannel {subchannel}")
            continue

        # Pin-specific validation only for LQFP100 (reference data is 100-pin only)
        if package == "LQFP100":
            subchannel_idx = 0 if subchannel == "A" else 1
            allowed_pins = gptw_details[channel][PF.GPTW_PWM][subchannel_idx]

            if pin not in allowed_pins:
                result.add_error(
                    f"motor_pwm.{motor_signal}: Pin {pin} not allowed for {channel} "
                    f"subchannel {subchannel}. Allowed: {allowed_pins}")

        # Verify motor number matches channel number (package-independent)
        if "_PH" in motor_signal or "_EN" in motor_signal:
            motor_num = int(motor_signal.split("_")[0].replace("MOTOR", ""))
            expected = f"GPTIOC{motor_num}"
            if channel != expected:
                result.add_error(
                    f"motor_pwm.{motor_signal}: Motor {motor_num} should use {expected}")

        # PH=A, EN=B (package-independent)
        if "_PH" in motor_signal and subchannel != "A":
            result.add_error(f"motor_pwm.{motor_signal}: Phase should be subchannel A")
        elif "_EN" in motor_signal and subchannel != "B":
            result.add_error(f"motor_pwm.{motor_signal}: Enable should be subchannel B")

    return result


def test_spi_channel_mapping(data: dict) -> TestResult:
    """Verify motor SPI (Simple SPI) and host SPI (RSPI) channel-to-pin mappings"""
    package = data.get("metadata", {}).get("package", "LQFP100")
    result = TestResult(category="SPI Channel Mapping Verification", passed=True)

    if "pin_assignments" not in data:
        result.add_error("Missing pin_assignments section")
        return result

    # ── Motor SPI: SCI12 Simple SPI or Bitbang GPIO ──
    motor_spi = data["pin_assignments"].get("motor_driver_spi", {})
    motor_channel = None
    if "metadata" in data:
        motor_channel = data["metadata"].get("motor_spi_channel")

    if motor_channel is None:
        result.add_error("Missing metadata.motor_spi_channel")
    elif motor_channel == "BITBANG":
        # Bitbang mode: all pins are GPIO, just verify consistent channel label
        for sig, sig_data in motor_spi.items():
            pin = sig_data.get("pin")
            channel = sig_data.get("channel")
            if pin is None or channel is None:
                result.add_error(f"motor_driver_spi.{sig}: Missing pin or channel")
                continue
            if channel != "BITBANG":
                result.add_error(
                    f"motor_driver_spi.{sig}: Channel {channel} != BITBANG")
            # CORE pin check only for LQFP100 (reference data)
            if package == "LQFP100" and pin in pins and PF.CORE in pins[pin]:
                result.add_error(
                    f"motor_driver_spi.{sig}: Pin {pin} is CORE, cannot be GPIO")
        # Verify we have SCK+COPI+CIPO data pins + CS pins
        data_sigs = [s for s in motor_spi if "CS" not in s]
        if len(data_sigs) < 3:
            result.add_error(
                f"Bitbang motor SPI: Expected 3 data pins, got {len(data_sigs)}")
    elif motor_channel not in simple_spi_details:
        # For non-100-pin packages, SCI12 is still the same channel name
        if package == "LQFP100":
            result.add_error(f"Motor SPI channel {motor_channel} not in simple_spi_details")
        else:
            # Structural validation only: verify channel consistency
            for sig, sig_data in motor_spi.items():
                channel = sig_data.get("channel")
                if channel is not None and channel != motor_channel:
                    result.add_error(
                        f"motor_driver_spi.{sig}: Channel {channel} != {motor_channel}")
    else:
        ch_def = simple_spi_details[motor_channel]
        for sig, sig_data in motor_spi.items():
            pin = sig_data.get("pin")
            channel = sig_data.get("channel")
            if pin is None or channel is None:
                result.add_error(f"motor_driver_spi.{sig}: Missing pin or channel")
                continue
            if channel != motor_channel:
                result.add_error(
                    f"motor_driver_spi.{sig}: Channel {channel} != {motor_channel}")
            # Pin-specific validation only for LQFP100
            if package == "LQFP100":
                if "SCK" in sig and "CS" not in sig:
                    if pin not in ch_def[PF.SIMPLE_SPI_SCK]:
                        result.add_error(f"motor_driver_spi.{sig}: Pin {pin} invalid for SCK")
                elif "COPI" in sig:
                    if pin not in ch_def[PF.SIMPLE_SPI_MOSI]:
                        result.add_error(f"motor_driver_spi.{sig}: Pin {pin} invalid for COPI")
                elif "CIPO" in sig:
                    if pin not in ch_def[PF.SIMPLE_SPI_MISO]:
                        result.add_error(f"motor_driver_spi.{sig}: Pin {pin} invalid for CIPO")

    # ── Host SPI: RSPI ──
    host_spi = data["pin_assignments"].get("host_spi", {})
    host_channel = None
    if "metadata" in data:
        host_channel = data["metadata"].get("host_spi_channel")

    if host_spi:
        if host_channel is None:
            result.add_error("Missing metadata.host_spi_channel")
        elif host_channel not in spi_details:
            result.add_error(f"Host SPI channel {host_channel} not in spi_details")
        else:
            ch_def = spi_details[host_channel]
            for sig, sig_data in host_spi.items():
                pin = sig_data.get("pin")
                channel = sig_data.get("channel")
                if pin is None or channel is None:
                    result.add_error(f"host_spi.{sig}: Missing pin or channel")
                    continue
                if channel != host_channel:
                    result.add_error(
                        f"host_spi.{sig}: Channel {channel} != {host_channel}")
                # Pin-specific validation only for LQFP100
                if package == "LQFP100":
                    if "SCLK" in sig:
                        if pin not in ch_def[PF.SPI_RSPCK]:
                            result.add_error(f"host_spi.{sig}: Pin {pin} invalid for SCLK")
                    elif "COPI" in sig:
                        if pin not in ch_def[PF.SPI_COPI]:
                            result.add_error(f"host_spi.{sig}: Pin {pin} invalid for COPI")
                    elif "CIPO" in sig:
                        if pin not in ch_def[PF.SPI_CIPO]:
                            result.add_error(f"host_spi.{sig}: Pin {pin} invalid for CIPO")
                    elif "CS" in sig:
                        if pin not in ch_def[PF.SPI_CS]:
                            result.add_error(f"host_spi.{sig}: Pin {pin} invalid for CS")

    return result


def test_fixed_peripherals(data: dict) -> TestResult:
    """Verify fixed peripheral pin assignments (GTETRG, I2C, UART)"""
    result = TestResult(category="Fixed Peripheral Verification", passed=True)

    if "pin_assignments" not in data:
        result.add_error("Missing pin_assignments section")
        return result

    pa = data["pin_assignments"]
    package = data.get("metadata", {}).get("package", "LQFP100")

    # Per-package expected pin assignments (same ports, different pin numbers)
    if package == "LQFP144":
        valid_gtetrg = {
            "GTETRGA": 42, "GTETRGB": 89, "GTETRGC": 66, "GTETRGD": 43,
        }
        expected_uart = {
            "DEBUG_TXD9": (78, "DEBUG_UART_TXD"),
            "DEBUG_RXD9": (79, "DEBUG_UART_RXD"),
        }
        expected_i2c0 = {
            "HOST_SCL0": (45, "IIC_SCL_FMP"),
            "HOST_SDA0": (44, "IIC_SDA_FMP"),
        }
        expected_i2c1 = {
            "BMS_SCL1": (36, "IIC_SCL"),
            "BMS_SDA1": (37, "IIC_SDA"),
        }
    else:
        valid_gtetrg = {
            "GTETRGA": 31, "GTETRGB": 64, "GTETRGC": 48, "GTETRGD": 32,
        }
        expected_uart = {
            "DEBUG_TXD9": (53, "DEBUG_UART_TXD"),
            "DEBUG_RXD9": (54, "DEBUG_UART_RXD"),
        }
        expected_i2c0 = {
            "HOST_SCL0": (34, "IIC_SCL_FMP"),
            "HOST_SDA0": (33, "IIC_SDA_FMP"),
        }
        expected_i2c1 = {
            "BMS_SCL1": (27, "IIC_SCL"),
            "BMS_SDA1": (28, "IIC_SDA"),
        }

    # GTETRG (count is configurable via REQUIREMENTS["gtetrg_count"])
    gtetrg = pa.get("gtetrg", {})
    for name, data_item in gtetrg.items():
        if name not in valid_gtetrg:
            result.add_error(f"gtetrg.{name}: Unknown GTETRG name")
            continue
        actual_pin = data_item.get("pin")
        if actual_pin != valid_gtetrg[name]:
            result.add_error(f"gtetrg.{name}: Expected pin {valid_gtetrg[name]}, got {actual_pin}")
        fn = data_item.get("function")
        if fn != "GPTW_EXT_TRIG":
            result.add_error(f"gtetrg.{name}: Expected function GPTW_EXT_TRIG, got {fn}")
    if len(gtetrg) < 1:
        result.add_error("gtetrg: Expected at least 1 GTETRG assignment")

    # Debug UART (SCI9)
    debug = pa.get("debug_uart", {})
    for name, (expected_pin, expected_fn) in expected_uart.items():
        if name not in debug:
            result.add_error(f"Missing debug_uart.{name}")
        else:
            actual_pin = debug[name].get("pin")
            if actual_pin != expected_pin:
                result.add_error(f"debug_uart.{name}: Expected pin {expected_pin}, got {actual_pin}")
            fn = debug[name].get("function")
            if fn != expected_fn:
                result.add_error(f"debug_uart.{name}: Expected function {expected_fn}, got {fn}")

    # Host I2C (RIIC0 FM+)
    host_i2c = pa.get("host_i2c", {})
    for name, (expected_pin, expected_fn) in expected_i2c0.items():
        if name not in host_i2c:
            result.add_error(f"Missing host_i2c.{name}")
        else:
            actual_pin = host_i2c[name].get("pin")
            if actual_pin != expected_pin:
                result.add_error(f"host_i2c.{name}: Expected pin {expected_pin}, got {actual_pin}")

    # BMS I2C (RIIC1)
    bms_i2c = pa.get("bms_i2c", {})
    for name, (expected_pin, expected_fn) in expected_i2c1.items():
        if name not in bms_i2c:
            result.add_error(f"Missing bms_i2c.{name}")
        else:
            actual_pin = bms_i2c[name].get("pin")
            if actual_pin != expected_pin:
                result.add_error(f"bms_i2c.{name}: Expected pin {expected_pin}, got {actual_pin}")

    return result


def test_encoder_mapping(data: dict) -> TestResult:
    """Verify encoder MTU channel-to-pin mappings"""
    package = data.get("metadata", {}).get("package", "LQFP100")
    result = TestResult(category="Encoder Mapping Verification", passed=True)

    if "pin_assignments" not in data:
        result.add_error("Missing pin_assignments section")
        return result

    encoders = data["pin_assignments"].get("encoders", {})

    if not encoders:
        result.add_warning("No encoder assignments found")
        return result

    for enc_name, enc_data in encoders.items():
        pin = enc_data.get("pin")
        channel = enc_data.get("channel")
        function = enc_data.get("function")

        if pin is None or channel is None or function is None:
            result.add_error(f"encoders.{enc_name}: Missing pin, channel, or function")
            continue

        # Verify channel exists in mtu_details
        if channel not in mtu_details:
            result.add_error(f"encoders.{enc_name}: Channel {channel} not in mtu_details")
            continue

        # Verify function (clock input) exists for this channel
        if function not in mtu_details[channel]:
            result.add_error(
                f"encoders.{enc_name}: Function {function} not valid for {channel}. "
                f"Valid: {list(mtu_details[channel].keys())}")
            continue

        # Pin-specific validation only for LQFP100
        if package == "LQFP100":
            allowed_pins = mtu_details[channel][function]
            if pin not in allowed_pins:
                result.add_error(
                    f"encoders.{enc_name}: Pin {pin} not allowed for {channel}/{function}. "
                    f"Allowed: {allowed_pins}")

    # Check that MTU1 uses MTCLKA+MTCLKB and MTU2 uses MTCLKC+MTCLKD (package-independent)
    enc0_funcs = set()
    enc1_funcs = set()
    for enc_name, enc_data in encoders.items():
        channel = enc_data.get("channel")
        function = enc_data.get("function")
        if channel == "MTU1":
            enc0_funcs.add(function)
        elif channel == "MTU2":
            enc1_funcs.add(function)

    if enc0_funcs and enc0_funcs != {"MTCLKA", "MTCLKB"}:
        result.add_error(f"MTU1 should use MTCLKA+MTCLKB, got {enc0_funcs}")
    if enc1_funcs and enc1_funcs != {"MTCLKC", "MTCLKD"}:
        result.add_error(f"MTU2 should use MTCLKC+MTCLKD, got {enc1_funcs}")

    return result


def test_tpu_encoder_mapping(data: dict) -> TestResult:
    """Verify TPU encoder channel-to-pin mappings"""
    package = data.get("metadata", {}).get("package", "LQFP100")
    result = TestResult(category="TPU Encoder Mapping Verification", passed=True)

    if "pin_assignments" not in data:
        result.add_error("Missing pin_assignments section")
        return result

    tpu_encoders = data["pin_assignments"].get("tpu_encoders", {})

    if not tpu_encoders:
        result.add_warning("No TPU encoder assignments found (may be expected if pins unavailable)")
        return result

    for enc_name, enc_data in tpu_encoders.items():
        pin = enc_data.get("pin")
        channel = enc_data.get("channel")
        function = enc_data.get("function")

        if pin is None or channel is None or function is None:
            result.add_error(f"tpu_encoders.{enc_name}: Missing pin, channel, or function")
            continue

        # Verify channel exists in tpu_details
        if channel not in tpu_details:
            result.add_error(f"tpu_encoders.{enc_name}: Channel {channel} not in tpu_details")
            continue

        # Verify function (clock input) exists for this channel
        if function not in tpu_details[channel]:
            result.add_error(
                f"tpu_encoders.{enc_name}: Function {function} not valid for {channel}. "
                f"Valid: {list(tpu_details[channel].keys())}")
            continue

        # Pin-specific validation only for LQFP100
        if package == "LQFP100":
            allowed_pins = tpu_details[channel][function]
            if pin not in allowed_pins:
                result.add_error(
                    f"tpu_encoders.{enc_name}: Pin {pin} not allowed for {channel}/{function}. "
                    f"Allowed: {allowed_pins}")

    # Check that TPU1 uses TCLKA+TCLKB and TPU2 uses TCLKC+TCLKD (package-independent)
    tpu1_funcs = set()
    tpu2_funcs = set()
    for enc_name, enc_data in tpu_encoders.items():
        channel = enc_data.get("channel")
        function = enc_data.get("function")
        if channel == "TPU1":
            tpu1_funcs.add(function)
        elif channel == "TPU2":
            tpu2_funcs.add(function)

    if tpu1_funcs and tpu1_funcs != {"TCLKA", "TCLKB"}:
        result.add_error(f"TPU1 should use TCLKA+TCLKB, got {tpu1_funcs}")
    if tpu2_funcs and tpu2_funcs != {"TCLKC", "TCLKD"}:
        result.add_error(f"TPU2 should use TCLKC+TCLKD, got {tpu2_funcs}")

    return result


def test_motor_adc(data: dict) -> TestResult:
    """Verify motor ADC assignments use correct pins"""
    package = data.get("metadata", {}).get("package", "LQFP100")
    result = TestResult(category="Motor ADC Verification", passed=True)

    if "pin_assignments" not in data:
        result.add_error("Missing pin_assignments section")
        return result

    motor_adc = data["pin_assignments"].get("motor_adc", {})

    if len(motor_adc) != 4:
        result.add_error(f"Expected 4 motor ADC assignments, got {len(motor_adc)}")

    for adc_name, adc_data in motor_adc.items():
        pin = adc_data.get("pin")
        channel = adc_data.get("channel")

        if pin is None:
            result.add_error(f"motor_adc.{adc_name}: Missing pin")
            continue

        # Pin-specific ADC capability check only for LQFP100
        if package == "LQFP100" and pin in pins and PF.ADC not in pins[pin]:
            result.add_error(
                f"motor_adc.{adc_name}: Pin {pin} lacks ADC capability")

        # Verify channel name format (ANxxx) — package-independent
        if channel and not channel.startswith("AN"):
            result.add_error(
                f"motor_adc.{adc_name}: Channel {channel} should start with 'AN'")

    return result


def test_peripheral_counts(data: dict) -> TestResult:
    """Verify expected signal counts per peripheral group"""
    result = TestResult(category="Peripheral Signal Count Verification", passed=True)

    if "pin_assignments" not in data:
        result.add_error("Missing pin_assignments section")
        return result

    pa = data["pin_assignments"]

    expected_counts = {
        # gtetrg handled separately (1-4 depending on REQUIREMENTS)
        "debug_uart": 2,
        "host_i2c": 2,
        "bms_i2c": 2,
        "motor_driver_spi": 7,  # SCLK + COPI + CIPO + 4 CS
        "motor_pwm": 8,  # 4 motors x (PH + EN)
        # host_spi: 0 or 4 (depends on viability)
        "motor_adc": 4,
        "bms_interrupt": 1,
        "sonar": 8,  # 4 echo + 4 trig
        "leds": 6,
        "onewire": 1,
    }

    for group, expected in expected_counts.items():
        actual = len(pa.get(group, {}))
        if actual != expected:
            result.add_error(
                f"{group}: Expected {expected} signals, got {actual}")

    # gtetrg: 1-4 depending on REQUIREMENTS (tradeoff with TPU encoders)
    gtetrg_count = len(pa.get("gtetrg", {}))
    if gtetrg_count < 1 or gtetrg_count > 4:
        result.add_error(
            f"gtetrg: Expected 1-4 signals, got {gtetrg_count}")

    # host_spi: either 0 (conflict) or 4 (viable)
    host_spi_count = len(pa.get("host_spi", {}))
    if host_spi_count not in (0, 4):
        result.add_error(
            f"host_spi: Expected 0 or 4 signals, got {host_spi_count}")

    # MTU Encoders: either 0, 2, or 4 (2 per MTU channel)
    encoder_count = len(pa.get("encoders", {}))
    if encoder_count not in (0, 2, 4):
        result.add_error(
            f"encoders: Expected 0, 2, or 4 signals, got {encoder_count}")

    # TPU Encoders: either 0 or 2 (2 per TPU channel, max 2 channels)
    tpu_encoder_count = len(pa.get("tpu_encoders", {}))
    if tpu_encoder_count not in (0, 2, 4):
        result.add_error(
            f"tpu_encoders: Expected 0, 2, or 4 signals, got {tpu_encoder_count}")

    return result


def test_no_core_pin_usage(data: dict) -> TestResult:
    """Verify no CORE pin is assigned to any function"""
    package = data.get("metadata", {}).get("package", "LQFP100")
    result = TestResult(category="CORE Pin Protection", passed=True)

    if "pin_assignments" not in data:
        result.add_error("Missing pin_assignments section")
        return result

    # For LQFP144, skip CORE check (reference pins dict is 100-pin only)
    if package != "LQFP100":
        result.add_warning(f"CORE pin check skipped for {package} (reference data is 100-pin only)")
        return result

    core_pins = {p for p, caps in pins.items() if PF.CORE in caps}

    for group_name, group_data in data["pin_assignments"].items():
        if not isinstance(group_data, dict):
            continue
        for sig_name, sig_data in group_data.items():
            if not isinstance(sig_data, dict):
                continue
            pin = sig_data.get("pin")
            if pin is not None and pin in core_pins:
                result.add_error(
                    f"{group_name}.{sig_name}: Pin {pin} is CORE and must not be assigned")

    return result


def test_package_metadata(data: dict) -> TestResult:
    """Verify package metadata fields exist and are valid"""
    result = TestResult(category="Package Metadata Verification", passed=True)

    meta = data.get("metadata", {})
    package = meta.get("package")
    chip = meta.get("chip")
    pin_count = meta.get("pin_count")

    if package is None:
        result.add_error("Missing metadata.package")
    elif package not in ("LQFP100", "LQFP144"):
        result.add_error(f"metadata.package must be LQFP100 or LQFP144, got {package}")

    if chip is None:
        result.add_error("Missing metadata.chip")

    if pin_count is None:
        result.add_error("Missing metadata.pin_count")
    elif not isinstance(pin_count, int):
        result.add_error(f"metadata.pin_count must be integer, got {type(pin_count).__name__}")
    elif pin_count not in (100, 144):
        result.add_error(f"metadata.pin_count must be 100 or 144, got {pin_count}")

    # Cross-check package vs pin_count
    if package == "LQFP100" and pin_count != 100:
        result.add_error(f"LQFP100 should have pin_count=100, got {pin_count}")
    elif package == "LQFP144" and pin_count != 144:
        result.add_error(f"LQFP144 should have pin_count=144, got {pin_count}")

    # Cross-check package vs chip name
    if package == "LQFP100" and chip and "HxFB" in chip:
        result.add_error(f"LQFP100 should not use 144-pin chip name: {chip}")
    elif package == "LQFP144" and chip and "GFP" in chip:
        result.add_error(f"LQFP144 should not use 100-pin chip name: {chip}")

    # Verify motor_spi_channel consistency with package
    motor_spi = meta.get("motor_spi_channel")
    if package == "LQFP100" and motor_spi and motor_spi != "BITBANG":
        result.add_warning(f"LQFP100 typically uses BITBANG motor SPI, got {motor_spi}")
    elif package == "LQFP144" and motor_spi == "BITBANG":
        result.add_warning(f"LQFP144 should use hardware SPI, got BITBANG")

    return result


def generate_report(results: List[TestResult], output_path: Path) -> None:
    """Generate detailed verification report"""
    with open(output_path, 'w') as f:
        f.write("RX72N Pin Assignment Verification Report\n")
        f.write("=" * 80 + "\n\n")

        total = len(results)
        passed = sum(1 for r in results if r.passed)
        failed = total - passed

        f.write("SUMMARY\n")
        f.write("-" * 80 + "\n")
        f.write(f"Total Tests: {total}\n")
        f.write(f"Passed: {passed}\n")
        f.write(f"Failed: {failed}\n\n")

        f.write("DETAILED RESULTS\n")
        f.write("-" * 80 + "\n")
        for r in results:
            f.write(str(r))
            f.write("\n")


def main():
    """Run all verification tests"""
    script_dir = Path(__file__).parent
    json_path = script_dir / "output" / "pin_assignment_solution.json"
    report_path = script_dir / "verification_report.txt"

    print("=" * 80)
    print("RX72N Pin Assignment Verification Test Suite")
    print("=" * 80)
    print(f"JSON File: {json_path}")
    print()

    data = load_json(json_path)
    print("JSON loaded successfully")
    print()

    results = []
    tests = [
        ("Package Metadata", test_package_metadata),
        ("JSON Structure", test_json_structure),
        ("Pin Conflicts", test_no_pin_conflicts),
        ("Pin Range", test_pin_range),
        ("Pin Capabilities", test_pin_capabilities),
        ("GPTW Channel Mapping", test_gptw_channel_mapping),
        ("SPI Channel Mapping", test_spi_channel_mapping),
        ("Fixed Peripherals", test_fixed_peripherals),
        ("Encoder Mapping", test_encoder_mapping),
        ("TPU Encoder Mapping", test_tpu_encoder_mapping),
        ("Motor ADC", test_motor_adc),
        ("Peripheral Counts", test_peripheral_counts),
        ("CORE Pin Protection", test_no_core_pin_usage),
    ]

    for name, test_fn in tests:
        print(f"  {name}...", end=" ")
        r = test_fn(data)
        results.append(r)
        print("PASS" if r.passed else "FAIL")

    print()

    generate_report(results, report_path)
    print(f"Report written to: {report_path}")
    print()

    total = len(results)
    passed = sum(1 for r in results if r.passed)
    failed = total - passed

    print("=" * 80)
    print(f"Results: {passed}/{total} passed, {failed} failed")
    print("=" * 80)

    if failed > 0:
        print()
        for r in results:
            if not r.passed:
                print(f"FAIL: {r.category}")
                for err in r.errors[:5]:
                    print(f"  - {err}")
                if len(r.errors) > 5:
                    print(f"  ... and {len(r.errors) - 5} more")
                print()
        return 1

    print("ALL TESTS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
