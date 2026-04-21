# uart_test/Makefile -- minimal SCI9 UART smoke test.

CC      := rx-elf-gcc
OBJCOPY := rx-elf-objcopy
SIZE    := rx-elf-size

CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data

HAL_ROOT  := ../libs/rx_hal
CORE_ROOT := ../libs/rx_core

INCLUDES := $(addprefix -I, $(CORE_ROOT)/inc $(HAL_ROOT)/inc)

CFLAGS  := $(CPU) -std=gnu23 -O1 -g3 -Wall -Wextra $(INCLUDES) \
           -ffunction-sections -fdata-sections \
           -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
           -Wl,--gc-sections

APP_CSRC := main.c clock.c sci9.c
APP_ASRC := startup.S vectors.S

OBJS := $(APP_CSRC:.c=.o) $(APP_ASRC:.S=.o)

TARGET := uart_test

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
	rm -f *.o *.elf *.mot *.hex
