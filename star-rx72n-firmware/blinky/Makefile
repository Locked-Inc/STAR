# blinky/Makefile -- Build minimal RX72N bare-metal LED test.
#
# Requires GNURX toolchain (rx-elf-gcc).
# Install on Arch: yay -S gnurx-toolchain  (or see Renesas GCC for RX releases)
#
# Outputs:
#   blinky.elf  -- ELF binary (use with OpenOCD / J-Link / e2 studio)
#   blinky.mot  -- Motorola S-record (use with Renesas Flash Programmer)
#   blinky.hex  -- Intel HEX (use with some J-Link / OpenOCD flows)

CC      := rx-elf-gcc
OBJCOPY := rx-elf-objcopy
SIZE    := rx-elf-size

# CPU flags must match the RX72N (RXv3 core, little-endian data)
CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data

CFLAGS  := $(CPU) -std=c11 -O0 -g3 -Wall -Wextra
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld

SRCS    := startup.S main.c
TARGET  := blinky

RFP_CLI := rfp-cli

.PHONY: all flash clean

all: $(TARGET).elf $(TARGET).mot $(TARGET).hex
	@echo ""
	@$(SIZE) $(TARGET).elf

$(TARGET).elf: $(SRCS) linker.ld
	$(CC) $(CFLAGS) $(ASFLAGS) $(LDFLAGS) startup.S main.c -o $@

$(TARGET).mot: $(TARGET).elf
	$(OBJCOPY) -O srec $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

flash: $(TARGET).mot
	sudo $(RFP_CLI) -d RX72N -t e2l -if fine -run \
	    -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -a $<

clean:
	rm -f $(TARGET).elf $(TARGET).mot $(TARGET).hex
