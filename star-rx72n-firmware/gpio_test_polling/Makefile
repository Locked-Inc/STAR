# gpio_test/Makefile -- Build the GPIO breakout board test firmware.
#
# Polling-based, no RTOS. Depends on:
#   - libs/rx_core/inc : rx_register_protection.h used by clock.c
#   - libs/rx_hal/inc  : rx72n_regs.h         used by clock.c
#
# Outputs:
#   gpio_test.elf / .mot / .hex

CC      := rx-elf-gcc
OBJCOPY := rx-elf-objcopy
SIZE    := rx-elf-size

# GCC for RX only accepts a fixed -mcpu set (rx100 rx13t rx140 rx200 rx230
# rx600 rx610 rx64m rx66t rx71m rx72t). RX72N shares the RXv3 core/ISA with
# rx72t, so rx72t is the correct flag for this board.
CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data

CORE_ROOT := ../libs/rx_core
HAL_ROOT  := ../libs/rx_hal

INCLUDES := -I$(CORE_ROOT)/inc -I$(HAL_ROOT)/inc

CFLAGS  := $(CPU) -std=gnu23 -O0 -g3 -Wall -Wextra $(INCLUDES) \
           -ffunction-sections -fdata-sections \
           -Wno-unused-parameter -Wno-unused-variable
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
           -Wl,--gc-sections

APP_CSRC := main.c clock.c
APP_ASRC := startup.S vectors.S
APP_OBJS := $(APP_CSRC:.c=.o) $(APP_ASRC:.S=.o)

TARGET  := gpio_test
RFP_CLI ?= $(HOME)/tools/rfp-cli/bin/rfp-cli

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
	$(RFP_CLI) -d RX720 -t e2l -if fine -run \
	    -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -a $<

clean:
	rm -f $(TARGET).elf $(TARGET).mot $(TARGET).hex $(APP_OBJS)
