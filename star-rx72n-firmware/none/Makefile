# none/Makefile -- "do nothing" firmware: park the chip in software standby.
#
# Outputs:
#   none.elf / none.mot / none.hex
#
# Wake: external reset (button) or re-flash via E2 Lite.
#
# See main.c for the full rationale; same flags as blinky/Makefile.

CC      := rx-elf-gcc
OBJCOPY := rx-elf-objcopy
SIZE    := rx-elf-size

CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data

CFLAGS  := $(CPU) -std=gnu23 -O0 -g3 -Wall -Wextra -ffunction-sections -fdata-sections
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld -Wl,--gc-sections

OBJS   := main.o startup.o
TARGET := none

.PHONY: all clean
all: $(TARGET).elf $(TARGET).mot $(TARGET).hex
	@$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

$(TARGET).mot: $(TARGET).elf
	$(OBJCOPY) -O srec $< $@
$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

clean:
	rm -f *.o *.elf *.mot *.hex *.map
