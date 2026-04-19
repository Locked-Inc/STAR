# temp_sensor_test/Makefile -- DS18B20 1-Wire temperature test on P51 (pin 55).
#
# Outputs:
#   temp_sensor_test.elf / .mot / .hex
#
# Flash (externally -- NOT run here while E2 Lite in use):
#   make flash
#
# Scope wiring (Tom's PCB header):
#   Ch1+ -> pin 38 (P17) -- temperature bit stream (see main.c encoding)
#   Ch2+ -> pin 88 (PA7) -- heartbeat LED (one toggle per conversion cycle)
#   GND  -> any GND pad
#   DS18B20 DQ -> pin 55 (P51) [external 4.7k pullup to 3V3 on Tom's PCB]

CC      := /opt/gnurx/bin/rx-elf-gcc
OBJCOPY := /opt/gnurx/bin/rx-elf-objcopy
SIZE    := /opt/gnurx/bin/rx-elf-size

CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data

CFLAGS  := $(CPU) -std=gnu23 -O0 -g3 -Wall -Wextra \
           -ffunction-sections -fdata-sections \
           -Wno-unused-parameter -Wno-unused-variable
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
           -Wl,--gc-sections

APP_CSRC := main.c
APP_ASRC := startup.S vectors.S
APP_OBJS := $(APP_CSRC:.c=.o) $(APP_ASRC:.S=.o)

TARGET := temp_sensor_test

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
