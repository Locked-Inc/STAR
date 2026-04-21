# encoder_handspin/Makefile -- read TCNT live, no motors, hand-spin verify.
CC      := rx-elf-gcc
OBJCOPY := rx-elf-objcopy
SIZE    := rx-elf-size
CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data
HAL_INC := ../libs/rx_hal/inc
CORE_INC:= ../libs/rx_core/inc
INCLUDES := -I$(CORE_INC) -I$(HAL_INC)
CFLAGS  := $(CPU) -std=gnu23 -O1 -g3 -Wall -Wextra $(INCLUDES) -ffunction-sections -fdata-sections
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld -Wl,--gc-sections

OBJS := main.o clock.o sci9.o stubs.o startup.o vectors.o
TARGET := encoder_handspin

.PHONY: all clean
all: $(TARGET).elf $(TARGET).mot
	@$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

$(TARGET).mot: $(TARGET).elf
	$(OBJCOPY) -O srec $< $@

clean:
	rm -f *.o *.elf *.mot
