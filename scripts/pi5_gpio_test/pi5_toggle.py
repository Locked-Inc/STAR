#!/usr/bin/env python3
"""
pi5_toggle.py -- drive every Pi5 header GPIO in a known sequence.

Runs on the Pi5 itself. Claims GPIO 0..27 one at a time, pulses each
HIGH for PIN_HIGH_MS then LOW for PIN_LOW_MS, then moves to the next
pin. After the full sweep inserts CYCLE_GAP_MS of silence so the Mac-
side capture can anchor on the quiet window.

Pair with ad2_verify.py on the capture host.

Usage (on Pi5):
    sudo python3 pi5_toggle.py           # loops forever, Ctrl-C to stop
    sudo python3 pi5_toggle.py --cycles 3
"""

import argparse
import signal
import sys
import time

import lgpio

NUM_GPIO = 28
GPIO_FIRST = 0
GPIO_LAST = NUM_GPIO - 1

# Pins to leave alone on Pi5 + Hailo AI Hat+ pass-through:
#   0, 1  -> Pi HAT ID EEPROM (ID_SD / ID_SC, per Pi HAT spec)
#   2, 3  -> I2C-1 to Hailo AI Hat+ EEPROM at 0x50 (per Hailo docs)
#   7, 8  -> spi0 CS1 / CS0 (kernel-claimed)
SKIP_PINS = frozenset({0, 1, 2, 3, 7, 8})

PIN_HIGH_MS = 10
PIN_LOW_MS = 10
PIN_PERIOD_MS = PIN_HIGH_MS + PIN_LOW_MS
CYCLE_GAP_MS = 2000


def sleep_ms(ms: float) -> None:
    time.sleep(ms / 1000.0)


def run_sweep(handle: int, cycle_idx: int) -> None:
    pins = [g for g in range(GPIO_FIRST, GPIO_LAST + 1)
            if g not in SKIP_PINS]
    print(f"cycle {cycle_idx}: driving GPIO "
          f"{pins[0]}..{pins[-1]} (skipping {sorted(SKIP_PINS)})",
          flush=True)
    for gpio in pins:
        lgpio.gpio_claim_output(handle, gpio, 0)
        lgpio.gpio_write(handle, gpio, 1)
        sleep_ms(PIN_HIGH_MS)
        lgpio.gpio_write(handle, gpio, 0)
        sleep_ms(PIN_LOW_MS)
        lgpio.gpio_free(handle, gpio)


def main() -> None:
    parser = argparse.ArgumentParser(description="Pi5 GPIO sweep driver")
    parser.add_argument("--cycles", type=int, default=0,
                        help="Number of cycles (0 = forever)")
    parser.add_argument("--chip", type=int, default=4,
                        help="gpiochip index (Pi5 header is gpiochip4)")
    args = parser.parse_args()

    try:
        handle = lgpio.gpiochip_open(args.chip)
    except lgpio.error as e:
        print(f"ERROR: could not open gpiochip{args.chip}: {e}",
              file=sys.stderr)
        print("Pi5 uses gpiochip4 by default. On older Pis try --chip 0.",
              file=sys.stderr)
        sys.exit(1)

    stop = False

    def handle_sigint(_sig, _frame):
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, handle_sigint)

    print(f"Pi5 GPIO sweep: {NUM_GPIO} pins, "
          f"{PIN_HIGH_MS}ms HIGH / {PIN_LOW_MS}ms LOW per pin, "
          f"{CYCLE_GAP_MS}ms gap between cycles")
    print(f"  sweep duration: {NUM_GPIO * PIN_PERIOD_MS} ms")
    print(f"  cycle duration: {NUM_GPIO * PIN_PERIOD_MS + CYCLE_GAP_MS} ms")
    print("Ctrl-C to stop.", flush=True)

    try:
        cycle = 0
        while not stop:
            cycle += 1
            run_sweep(handle, cycle)
            if args.cycles > 0 and cycle >= args.cycles:
                break
            # Quiet anchor window for the capture side.
            for _ in range(CYCLE_GAP_MS // 50):
                if stop:
                    break
                sleep_ms(50)
    finally:
        # Best-effort cleanup: any pin still claimed is returned to input.
        for gpio in range(GPIO_FIRST, GPIO_LAST + 1):
            try:
                lgpio.gpio_free(handle, gpio)
            except lgpio.error:
                pass
        lgpio.gpiochip_close(handle)
        print("\nStopped.")


if __name__ == "__main__":
    main()
