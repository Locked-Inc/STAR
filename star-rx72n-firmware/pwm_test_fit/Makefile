# pwm_test_fit/Makefile -- 20 kHz PWM on GPTW0 driven via Renesas iodefine.h
# register structs (the authoritative FIT-style register definitions from the
# public rx-driver-package r_bsp v7.70).
#
# The public rx-driver-package has NO dedicated GPTW FIT driver. The only
# code in the whole package that writes GPTW registers is r_aeropoint_rx,
# and it does so via the same iodefine.h we include here, using statements
# like `MSTP(GPTW) = 0;` and `GPTW1.GTCR.LONG = ...;`. That is the FIT-style
# reference pattern this firmware reproduces.
#
# Outputs:
#   pwm_test_fit.elf / .mot / .hex
#
# Scope wiring (Tom's PCB header):
#   Ch1+ -> pin 38 (P17 / GTIOC0B)
#   Ch2+ -> pin 34 (P23 / GTIOC0A)
#   GND  -> any GND pad

CC      := /opt/gnurx/bin/rx-elf-gcc
OBJCOPY := /opt/gnurx/bin/rx-elf-objcopy
SIZE    := /opt/gnurx/bin/rx-elf-size

# -mcpu=rx72t because the rx-elf-gcc shipped in /opt/gnurx does not ship a
# dedicated -mcpu=rx72n and -mcpu=rx72t is RXv3 with the same codegen profile
# that pwm_test / pwm_test_hal use successfully on Tom's PCB.
CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data

INCLUDES := -I.

# iodefine.h conditions every bitfield union on __RX_LITTLE_ENDIAN__ -- define
# it so the little-endian layout is selected. BSP_MCU_RX72N is harmless if
# defined and makes intent clear to anyone reading the build.
DEFINES := -D__RX_LITTLE_ENDIAN__=1 -DBSP_MCU_RX72N=1

CFLAGS  := $(CPU) -std=gnu23 -O0 -g3 -Wall -Wextra $(INCLUDES) $(DEFINES) \
           -ffunction-sections -fdata-sections \
           -Wno-unused-parameter -Wno-unused-variable
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
           -Wl,--gc-sections

APP_CSRC := main.c clock.c
APP_ASRC := startup.S vectors.S
APP_OBJS := $(APP_CSRC:.c=.o) $(APP_ASRC:.S=.o)

TARGET := pwm_test_fit

RFP_CLI := /opt/rfp/linux-x64/rfp-cli

.PHONY: all flash clean

all: $(TARGET).elf $(TARGET).mot $(TARGET).hex
	@echo ""
	@$(SIZE) $(TARGET).elf

$(TARGET).elf: $(APP_OBJS) linker.ld
	$(CC) $(LDFLAGS) $(APP_OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

$(TARGET).mot: $(TARGET).elf
	$(OBJCOPY) -O srec $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

flash: $(TARGET).mot
	DOTNET_EnableDiagnostics=0 sudo box64 $(RFP_CLI) \
	    -d RX72N -t e2l -if fine \
	    -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \
	    -run -p $(TARGET).mot

clean:
	rm -f *.o *.elf *.mot *.hex
