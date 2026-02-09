# Documentation and Testing Implementation Plans

**Document Purpose:** Planning for Items 12 and 14 from todo.md
**Status:** 🔴 NOT STARTED - Documentation and testing infrastructure
**Total Estimated Effort:** 8-12 hours

---

## Table of Contents

1. [Encoder Architecture Documentation (Item 12)](#1-encoder-architecture-documentation-item-12)
2. [Hardware-in-the-Loop (HIL) Testing (Item 14)](#2-hardware-in-the-loop-hil-testing-item-14)

---

# 1. Encoder Architecture Documentation (Item 12)

**Status:** 🔴 NOT STARTED - Design exists but not documented
**Priority:** MEDIUM - Documentation task
**Estimated Effort:** 3-4 hours
**Dependencies:** TPU Encoder implementation (Item 2), existing MTU encoder code

## Problem Statement

The STAR robot uses **two different encoder hardware backends** (MTU and TPU) with a unified abstraction layer. This design is not currently documented, making it difficult for future developers to:
- Understand why two backends are needed
- Know which encoders use which peripheral
- Understand overflow handling differences (32-bit MTU vs 16-bit TPU)
- Troubleshoot encoder issues

**Need:** Comprehensive LaTeX documentation explaining the encoder architecture.

## Documentation Structure

### File to Create: `docs/sections/encoder_architecture.tex`

**Estimated length:** 400-500 lines of LaTeX

**Table of Contents:**
1. Introduction and Overview
2. Hardware Comparison: MTU vs TPU
3. Primary vs Secondary Encoder Concept
4. Encoder Pin Assignments
5. Abstraction Layer Design
6. Overflow Handling
7. Backend Selection and Configuration
8. Performance Analysis
9. Testing Strategy
10. Troubleshooting Guide

### Section 1: Introduction and Overview (~50 lines)

```latex
\section{Encoder Architecture}
\label{sec:encoder-architecture}

The STAR robot uses four quadrature encoders (341 PPR Hall effect) for odometry and closed-loop motor control. These encoders are interfaced using two different RX72N peripherals:

\begin{itemize}
    \item \textbf{MTU (Multi-Function Timer Pulse Unit)}: 32-bit counters for front wheels
    \item \textbf{TPU (16-bit Timer Pulse Unit)}: 16-bit counters for rear wheels
\end{itemize}

This dual-backend architecture provides:
\begin{itemize}
    \item Efficient use of limited MTU channels (reserved for PWM generation)
    \item Adequate performance on TPU for robot's maximum speed (210 RPM)
    \item Unified API regardless of hardware backend (abstraction layer)
\end{itemize}

\subsection{Motivation for Dual Backends}

The RX72N has only 4 MTU channels, and MTU3/MTU4 are required for motor PWM generation. This leaves only MTU1/MTU2 available for encoders. To support 4 encoders, TPU channels must be used for the remaining 2 rear wheel encoders.
```

### Section 2: Hardware Comparison Table (~100 lines)

```latex
\subsection{MTU vs TPU Hardware Comparison}

\begin{table}[h]
\centering
\caption{MTU and TPU Encoder Peripheral Comparison}
\label{tab:mtu-tpu-comparison}
\begin{tabular}{|l|l|l|}
\hline
\textbf{Feature} & \textbf{MTU} & \textbf{TPU} \\
\hline
Counter Width & 32-bit & 16-bit \\
\hline
Maximum Count & 4,294,967,295 & 65,535 \\
\hline
Overflow Period @ 210 RPM & 150 hours & 13.7 seconds \\
\hline
Phase Counting Mode & Yes (Ch18) & Yes (Ch28) \\
\hline
Input Pins per Channel & 2 (MTIOC A/B) & 2 (TIOC A/B) \\
\hline
Available Channels & MTU1, MTU2 (MTU3/4 reserved) & TPU0-5 (6 channels) \\
\hline
Register Base Address & 0x000C\_1290 (MTU1) & 0x0008\_8100 (TPU0) \\
\hline
Manual Reference & Ch18 & Ch28 \\
\hline
\end{tabular}
\end{table}

\subsubsection{Overflow Period Calculation}

For a 341 PPR encoder in 4× quadrature mode:
\begin{equation}
\text{Pulses per revolution} = 341 \times 4 = 1364 \text{ pulses/rev}
\end{equation}

At maximum speed of 210 RPM:
\begin{equation}
\text{Pulses per second} = 1364 \times \frac{210}{60} = 4774 \text{ pulses/sec}
\end{equation}

\textbf{TPU 16-bit overflow period:}
\begin{equation}
\text{Overflow period} = \frac{65535}{4774} = 13.7 \text{ seconds}
\end{equation}

\textbf{MTU 32-bit overflow period:}
\begin{equation}
\text{Overflow period} = \frac{4,294,967,295}{4774} = 899,506 \text{ seconds} \approx 250 \text{ hours}
\end{equation}

\textbf{Conclusion:} TPU overflow period (13.7 sec) is adequate when reading at 100 Hz (10 ms intervals).
```

### Section 3: Primary vs Secondary Encoder Concept (~80 lines)

```latex
\subsection{Primary vs Secondary Encoders}

The STAR encoder architecture uses a \textit{primary/secondary} classification:

\begin{itemize}
    \item \textbf{Primary Encoders (Front Wheels)}: Use MTU for maximum overflow period and higher performance
    \item \textbf{Secondary Encoders (Rear Wheels)}: Use TPU, adequate for robot's speed
\end{itemize}

\begin{table}[h]
\centering
\caption{Encoder Assignments}
\label{tab:encoder-assignments}
\begin{tabular}{|l|l|l|l|l|}
\hline
\textbf{Wheel} & \textbf{Encoder ID} & \textbf{Hardware} & \textbf{Pins} & \textbf{Classification} \\
\hline
Front Left & 0 & MTU1 & P24/P25 & Primary \\
\hline
Front Right & 1 & MTU2 & P22/PA3 & Primary \\
\hline
Rear Left & 2 & TPU1/5 & P14/P15 & Secondary \\
\hline
Rear Right & 3 & TPU2/4 & PC1/PC0 & Secondary \\
\hline
\end{tabular}
\end{table}

\textbf{Design rationale:}
\begin{enumerate}
    \item Front wheels typically experience higher slip (steering, weight distribution)
    \item Longer MTU overflow period provides more margin for missed reads
    \item Rear wheels have more predictable motion (no steering)
    \item TPU adequate for rear wheels at robot's maximum speed
\end{enumerate}
```

### Section 4: Abstraction Layer Diagram (~100 lines)

Include PlantUML or TikZ diagram:

```latex
\subsection{Abstraction Layer Architecture}

\begin{figure}[h]
\centering
\begin{tikzpicture}[
    node distance=1.5cm,
    box/.style={rectangle, draw, minimum width=4cm, minimum height=1cm, align=center},
    arrow/.style={->, >=stealth, thick}
]
    % Application layer
    \node[box] (app) {Motor Control Task};

    % Unified API
    \node[box, below of=app] (api) {Unified Encoder API\\rx\_encoder\_read\_count()};

    % Dispatch layer
    \node[box, below of=api] (dispatch) {Backend Dispatch Layer\\(Function Pointers)};

    % Backend implementations
    \node[box, below left of=dispatch, xshift=-2cm] (mtu) {MTU Backend\\rx\_encoder\_mtu.c};
    \node[box, below right of=dispatch, xshift=2cm] (tpu) {TPU Backend\\rx\_encoder\_tpu.c};

    % HAL layer
    \node[box, below of=mtu] (mtu_hal) {MTU HAL\\rx\_mtu.c};
    \node[box, below of=tpu] (tpu_hal) {TPU HAL\\rx\_tpu.c};

    % Hardware
    \node[box, below of=mtu_hal] (mtu_hw) {MTU1/MTU2\\Hardware};
    \node[box, below of=tpu_hal] (tpu_hw) {TPU1/5, TPU2/4\\Hardware};

    % Arrows
    \draw[arrow] (app) -- (api);
    \draw[arrow] (api) -- (dispatch);
    \draw[arrow] (dispatch) -- (mtu);
    \draw[arrow] (dispatch) -- (tpu);
    \draw[arrow] (mtu) -- (mtu_hal);
    \draw[arrow] (tpu) -- (tpu_hal);
    \draw[arrow] (mtu_hal) -- (mtu_hw);
    \draw[arrow] (tpu_hal) -- (tpu_hw);
\end{tikzpicture}
\caption{Encoder abstraction layer architecture}
\label{fig:encoder-abstraction}
\end{figure}

\textbf{Key features:}
\begin{itemize}
    \item Application code uses unified API (hardware-agnostic)
    \item Backend selection at initialization (configuration-based)
    \item Function pointer dispatch (Dependency Inversion Principle)
    \item Both backends implement identical interface
\end{itemize}
```

### Section 5: Overflow Handling (~100 lines)

Explain software accumulator for 32-bit count with 16-bit hardware:

```latex
\subsection{Overflow Handling for 16-bit TPU}

TPU encoders use a \textbf{software 32-bit accumulator} to extend the 16-bit hardware counter:

\begin{algorithm}[H]
\caption{TPU Encoder Overflow Detection}
\begin{algorithmic}[1]
\State $\text{current\_count} \gets \text{Read TPU TCNT register (16-bit)}$
\State $\text{delta} \gets \text{current\_count} - \text{previous\_count}$ \Comment{Wraps correctly}
\State $\text{accumulator} \gets \text{accumulator} + \text{delta}$ \Comment{32-bit accumulator}
\State $\text{previous\_count} \gets \text{current\_count}$
\State \Return $\text{accumulator}$
\end{algorithmic}
\end{algorithm}

\textbf{Example: Overflow from 65535 to 10}
\begin{align*}
\text{previous\_count} &= 65535 \\
\text{current\_count} &= 10 \\
\text{delta} &= 10 - 65535 = -65525 \text{ (16-bit wraps to 11)} \\
\text{accumulator} &= \text{accumulator} + 11 \text{ (correct increment)}
\end{align*}

\textbf{Requirements:}
\begin{itemize}
    \item Read encoder at $\geq$ 100 Hz (10 ms intervals)
    \item Ensure read interval $<$ 13.7 sec overflow period
    \item Use unsigned arithmetic (wrapping behavior)
\end{itemize}
```

## Implementation Checklist

### Phase 1: Content Creation (2-3 hours)
- [ ] Write Section 1: Introduction and Overview
- [ ] Write Section 2: Hardware Comparison (MTU vs TPU)
- [ ] Write Section 3: Primary vs Secondary Encoder Concept
- [ ] Write Section 4: Encoder Pin Assignments (table)
- [ ] Write Section 5: Abstraction Layer Design (with diagram)
- [ ] Write Section 6: Overflow Handling (algorithm + example)
- [ ] Write Section 7: Backend Selection and Configuration (code examples)
- [ ] Write Section 8: Performance Analysis (timing, CPU usage)
- [ ] Write Section 9: Testing Strategy (unit tests, hardware validation)
- [ ] Write Section 10: Troubleshooting Guide (common issues)

### Phase 2: Integration and Compilation (30 min)
- [ ] Add `\include{sections/encoder_architecture}` to main LaTeX file
- [ ] Compile LaTeX: `pdflatex star_documentation.tex`
- [ ] Verify no compilation errors
- [ ] Check figure/table rendering

### Phase 3: Cross-References (30 min)
- [ ] Update `docs/sections/03_hardware_pinout.tex` to reference encoder architecture
- [ ] Add "Primary (MTU)" note next to Encoder 0, 1 in pinout table
- [ ] Add "Secondary (TPU)" note next to Encoder 2, 3 in pinout table
- [ ] Update `README.md` with link to encoder architecture documentation

### Phase 4: Review (30 min)
- [ ] Peer review (check technical accuracy)
- [ ] Proofread for typos, grammar
- [ ] Verify all equations render correctly
- [ ] Verify all cross-references work

## Success Criteria

- ✅ Complete encoder architecture documented in LaTeX
- ✅ MTU vs TPU comparison clear and accurate
- ✅ Primary vs Secondary concept explained
- ✅ Abstraction layer architecture diagram included
- ✅ Overflow handling algorithm documented
- ✅ Cross-references to encoder architecture in pinout documentation
- ✅ PDF compiles without errors

## References

- **RX72N Manual Ch18:** Multi-Function Timer Pulse Unit (MTU)
- **RX72N Manual Ch28:** 16-bit Timer Pulse Unit (TPU)
- **Existing MTU Encoder Code:** `/workspaces/STAR/star-rx72n-firmware/lib/rx_encoder/src/rx_mtu_encoder.c`
- **TPU Implementation Plan:** `TPU_ENCODER_IMPLEMENTATION_PLAN.md`

---

# 2. Hardware-in-the-Loop (HIL) Testing (Item 14)

**Status:** 🔴 NOT STARTED - No automated hardware testing
**Priority:** MEDIUM - Testing infrastructure
**Estimated Effort:** 5-8 hours (initial setup + scripts)
**Dependencies:** RX72N hardware, E2 emulator or USB debug, test fixture

## Problem Statement

The STAR firmware currently has:
- ✅ Excellent unit test coverage for encoders (45+ tests)
- ✅ Some GPIO HAL tests
- ❌ **Zero automated hardware testing**
- ❌ No regression testing on real hardware
- ❌ No automated validation of peripheral functionality (encoders, ADC, PWM, etc.)

**Need:** Hardware-in-the-Loop (HIL) test infrastructure for automated firmware validation on actual RX72N hardware.

## HIL Testing Goals

### Objectives

1. **Automated Hardware Validation:** Run tests on real RX72N hardware automatically
2. **Regression Detection:** Catch bugs introduced by code changes
3. **Peripheral Verification:** Validate all peripherals work correctly (encoders, PWM, ADC, SPI, I2C)
4. **CI/CD Integration:** Run HIL tests in continuous integration pipeline
5. **Reproducible Results:** Same tests produce same results every run

### Test Coverage

**Phase 1 (Initial):**
- GPIO output (LED blink, measured with oscilloscope or logic analyzer)
- PWM generation (motor driver outputs, duty cycle verification)
- Encoder reading (simulated quadrature signals)
- ADC reading (known input voltages)

**Phase 2 (Advanced):**
- Motor closed-loop control (with actual motors on dynamometer)
- Ultrasonic sensors (with artificial echo generation)
- SPI communication (loopback or with RPi5)
- I2C communication (with BMS or EEPROM)
- Temperature sensor reading (controlled temperature environment)

## HIL Test Fixture Design

### Hardware Requirements

**Test Equipment:**
1. **RX72N Development Board:** PCB with all connections accessible
2. **E2 Emulator or USB Debug:** For firmware download and debugging
3. **Logic Analyzer:** 8+ channels for signal verification (optional but recommended)
4. **Oscilloscope:** 2-4 channels for timing measurements
5. **Quadrature Signal Generator:** For encoder testing (can be microcontroller-based)
6. **Precision Voltage Source:** For ADC testing (3 channels: 0V, 1.65V, 3.3V)
7. **Power Supply:** 5V for motors, 3.3V for logic
8. **PC/Laptop:** Running test automation scripts

**Test Fixture Block Diagram:**
```
┌─────────────────────────────────────────────────────────┐
│                  Test Control PC                         │
│  (Python test scripts, serial communication)            │
└──────────────┬─────────────────────────────┬────────────┘
               │                             │
       ┌───────▼────────┐           ┌────────▼───────┐
       │  E2 Emulator   │           │  USB-Serial    │
       │  (Firmware     │           │  (Test logs,   │
       │   Download)    │           │   results)     │
       └───────┬────────┘           └────────┬───────┘
               │                             │
       ┌───────▼─────────────────────────────▼───────┐
       │         RX72N Development Board              │
       │  - 4 encoder inputs (simulated)              │
       │  - 4 PWM outputs (motor drivers)             │
       │  - 4 ADC inputs (test voltages)              │
       │  - 1 SPI (loopback or RPi5)                  │
       │  - 1 I2C (EEPROM)                            │
       │  - 6 GPIO (LEDs)                             │
       └──────────────┬──────────────────────────────┘
                      │
          ┌───────────▼───────────┐
          │  External Test Devices │
          │  - Quadrature generator │
          │  - Precision voltages   │
          │  - Logic analyzer       │
          └────────────────────────┘
```

### Encoder Test Setup

**Quadrature signal generator** (using Arduino or STM32):
```c
// Arduino code to generate quadrature signals for encoder testing
void setup() {
    pinMode(ENCODER_A, OUTPUT);
    pinMode(ENCODER_B, OUTPUT);
}

void loop() {
    // Generate 1000 pulses at 100 Hz (simulates 6 RPM motor)
    for (int i = 0; i < 1000; i++) {
        // Forward direction: A leads B by 90°
        digitalWrite(ENCODER_A, HIGH);
        delayMicroseconds(2500);  // 2.5ms = 400 Hz pulse
        digitalWrite(ENCODER_B, HIGH);
        delayMicroseconds(2500);
        digitalWrite(ENCODER_A, LOW);
        delayMicroseconds(2500);
        digitalWrite(ENCODER_B, LOW);
        delayMicroseconds(2500);
    }
}
```

## Test Automation Scripts

### Python Test Framework

**Structure:**
```
tests/hil/
  ├── hil_test_runner.py       # Main test runner
  ├── test_gpio.py              # GPIO tests
  ├── test_pwm.py               # PWM tests
  ├── test_encoder.py           # Encoder tests
  ├── test_adc.py               # ADC tests
  ├── requirements.txt          # Python dependencies
  └── README.md                 # Setup instructions
```

**Example: GPIO HIL Test** (`tests/hil/test_gpio.py`)

```python
import serial
import time
import pytest

class TestGPIO:
    @pytest.fixture
    def rx72n_serial(self):
        """Connect to RX72N via serial port"""
        ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
        yield ser
        ser.close()

    def test_led_toggle(self, rx72n_serial):
        """Test LED toggle on PD7 (LED 0)"""
        # Send command to RX72N: "LED 0 ON"
        rx72n_serial.write(b"LED 0 ON\n")
        response = rx72n_serial.readline().decode().strip()

        assert response == "OK", f"LED ON failed: {response}"

        # Manually verify with oscilloscope or logic analyzer
        input("Verify LED 0 is ON (PD7 = HIGH). Press Enter to continue...")

        # Send command: "LED 0 OFF"
        rx72n_serial.write(b"LED 0 OFF\n")
        response = rx72n_serial.readline().decode().strip()

        assert response == "OK", f"LED OFF failed: {response}"

        input("Verify LED 0 is OFF (PD7 = LOW). Press Enter to continue...")

    def test_all_leds(self, rx72n_serial):
        """Test all 6 LEDs in sequence"""
        for led_id in range(6):
            rx72n_serial.write(f"LED {led_id} ON\n".encode())
            response = rx72n_serial.readline().decode().strip()
            assert response == "OK", f"LED {led_id} ON failed"

            time.sleep(0.5)  # 500ms delay

            rx72n_serial.write(f"LED {led_id} OFF\n".encode())
            response = rx72n_serial.readline().decode().strip()
            assert response == "OK", f"LED {led_id} OFF failed"
```

**Example: Encoder HIL Test** (`tests/hil/test_encoder.py`)

```python
class TestEncoder:
    def test_encoder_count(self, rx72n_serial):
        """Test encoder count with quadrature signal generator"""
        # Reset encoder count
        rx72n_serial.write(b"ENCODER 0 RESET\n")
        response = rx72n_serial.readline().decode().strip()
        assert response == "OK"

        # Instruct user to start quadrature generator (1000 pulses)
        input("Start quadrature generator for Encoder 0 (1000 pulses). Press Enter when done...")

        # Read encoder count
        rx72n_serial.write(b"ENCODER 0 READ\n")
        response = rx72n_serial.readline().decode().strip()

        # Parse count
        count = int(response.split()[1])

        # Verify count is 1000 ± 5 (allow 0.5% error)
        assert 995 <= count <= 1005, f"Encoder count out of range: {count} (expected 1000)"

    def test_encoder_direction(self, rx72n_serial):
        """Test encoder direction detection"""
        # Reset
        rx72n_serial.write(b"ENCODER 0 RESET\n")

        # Forward direction (count should increase)
        input("Start quadrature generator FORWARD (500 pulses). Press Enter when done...")
        rx72n_serial.write(b"ENCODER 0 READ\n")
        forward_count = int(rx72n_serial.readline().decode().split()[1])
        assert forward_count > 0, "Forward count should be positive"

        # Reverse direction (count should decrease)
        input("Start quadrature generator REVERSE (500 pulses). Press Enter when done...")
        rx72n_serial.write(b"ENCODER 0 READ\n")
        reverse_count = int(rx72n_serial.readline().decode().split()[1])
        assert reverse_count < forward_count, "Reverse count should decrease"
```

**Example: ADC HIL Test** (`tests/hil/test_adc.py`)

```python
class TestADC:
    def test_adc_known_voltages(self, rx72n_serial):
        """Test ADC with known input voltages"""
        test_voltages = [
            (0.0, 0, 50),        # 0V → ADC count ≈ 0 ± 50
            (1.65, 2048, 50),    # 1.65V (mid-scale) → ADC ≈ 2048 ± 50
            (3.3, 4095, 50),     # 3.3V (full-scale) → ADC ≈ 4095 ± 50
        ]

        for voltage, expected_count, tolerance in test_voltages:
            input(f"Set ADC channel 0 to {voltage}V. Press Enter when ready...")

            # Read ADC
            rx72n_serial.write(b"ADC 0 READ\n")
            response = rx72n_serial.readline().decode().strip()
            adc_count = int(response.split()[1])

            # Verify within tolerance
            assert abs(adc_count - expected_count) <= tolerance, \
                f"ADC reading {adc_count} out of range for {voltage}V " \
                f"(expected {expected_count} ± {tolerance})"
```

### Running HIL Tests

**Command:**
```bash
cd tests/hil
pytest test_gpio.py test_encoder.py test_adc.py -v
```

**Output:**
```
tests/hil/test_gpio.py::TestGPIO::test_led_toggle PASSED
tests/hil/test_gpio.py::TestGPIO::test_all_leds PASSED
tests/hil/test_encoder.py::TestEncoder::test_encoder_count PASSED
tests/hil/test_encoder.py::TestEncoder::test_encoder_direction PASSED
tests/hil/test_adc.py::TestADC::test_adc_known_voltages PASSED

========== 5 passed in 120.45s ==========
```

## Test Command Protocol

**Firmware must implement test command interface** over UART:

```c
/**
 * @brief Test command handler (for HIL testing)
 */
void test_command_handler(const char* command) {
    if (strncmp(command, "LED", 3) == 0) {
        // Parse: "LED <id> <ON|OFF>"
        int led_id;
        char state[4];
        sscanf(command, "LED %d %s", &led_id, state);

        if (strcmp(state, "ON") == 0) {
            gpio_write(led_pins[led_id], true);
            uart_puts("OK\n");
        } else if (strcmp(state, "OFF") == 0) {
            gpio_write(led_pins[led_id], false);
            uart_puts("OK\n");
        } else {
            uart_puts("ERROR: Invalid state\n");
        }
    } else if (strncmp(command, "ENCODER", 7) == 0) {
        // Parse: "ENCODER <id> <RESET|READ>"
        int encoder_id;
        char action[10];
        sscanf(command, "ENCODER %d %s", &encoder_id, action);

        if (strcmp(action, "RESET") == 0) {
            rx_encoder_reset(&encoders[encoder_id]);
            uart_puts("OK\n");
        } else if (strcmp(action, "READ") == 0) {
            int32_t count;
            rx_encoder_read_count(&encoders[encoder_id], &count);
            char buf[32];
            snprintf(buf, sizeof(buf), "COUNT %ld\n", count);
            uart_puts(buf);
        }
    } else if (strncmp(command, "ADC", 3) == 0) {
        // Parse: "ADC <channel> READ"
        int channel;
        sscanf(command, "ADC %d READ", &channel);

        uint16_t adc_value;
        rx_adc_read_channel(&adc_handle, channel, &adc_value);
        char buf[32];
        snprintf(buf, sizeof(buf), "ADC %u\n", adc_value);
        uart_puts(buf);
    } else {
        uart_puts("ERROR: Unknown command\n");
    }
}
```

## Implementation Checklist

### Phase 1: Test Fixture Setup (2-3 hours)
- [ ] Assemble RX72N development board
- [ ] Connect E2 emulator or USB debug
- [ ] Connect USB-Serial adapter for test commands
- [ ] Build quadrature signal generator (Arduino or STM32)
- [ ] Set up precision voltage sources for ADC testing
- [ ] Verify all connections with multimeter

### Phase 2: Firmware Test Interface (1-2 hours)
- [ ] Implement test command handler in firmware
- [ ] Add LED control commands
- [ ] Add encoder control commands (reset, read)
- [ ] Add ADC read commands
- [ ] Test commands manually via serial terminal

### Phase 3: Python Test Scripts (2-3 hours)
- [ ] Create test_gpio.py (LED tests)
- [ ] Create test_encoder.py (encoder count, direction)
- [ ] Create test_adc.py (known voltage tests)
- [ ] Create hil_test_runner.py (main runner)
- [ ] Write README.md with setup instructions

### Phase 4: Validation and Documentation (1 hour)
- [ ] Run all HIL tests, verify passing
- [ ] Document test fixture wiring diagram
- [ ] Document test procedure
- [ ] Add HIL testing to CI/CD pipeline (optional)

## Success Criteria

- ✅ HIL test fixture assembled and operational
- ✅ Firmware test command interface working
- ✅ Python test scripts run automatically
- ✅ GPIO, encoder, and ADC tests passing
- ✅ Test procedure documented
- ✅ Reproducible results across multiple test runs

## Future Enhancements

**Phase 2 (Post-MVP):**
- Fully automated tests (no manual user input)
- Logic analyzer integration (automated signal verification)
- Motor dynamometer for closed-loop control testing
- Thermal chamber for temperature testing
- Automated firmware download before each test run
- CI/CD integration (GitHub Actions or Jenkins)
- Test result trending (track pass/fail rates over time)

## References

- **pytest Documentation:** https://docs.pytest.org/
- **pySerial Documentation:** https://pyserial.readthedocs.io/
- **E2 Emulator User Manual:** Renesas E2 Emulator documentation

---

## Summary

**Item 12 (Encoder Architecture Documentation):**
- Comprehensive LaTeX documentation explaining MTU vs TPU encoder backends
- Primary vs Secondary encoder concept
- Abstraction layer architecture
- Estimated effort: 3-4 hours

**Item 14 (Hardware-in-the-Loop Testing):**
- Automated hardware testing infrastructure
- Test fixture design with quadrature generators and precision voltage sources
- Python test scripts for GPIO, encoders, ADC
- Firmware test command interface
- Estimated effort: 5-8 hours

**Total effort for both tasks:** 8-12 hours

---

**Document Version:** 1.0
**Last Updated:** 2026-02-05
**Author:** STAR Development Team
**Status:** Ready for Implementation
