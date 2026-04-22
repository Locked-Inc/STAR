# sonar_test/Makefile -- HC-SR04 bring-up test on the production STAR PCB.
#
# Bare-metal, polled ECHO (no ICU/IRQ). Round-robins all 4 sonars at 5 Hz
# and prints distance in cm over SCI9 (USB-UART -> /dev/ttyACM0).
#
# Mirrors imu_test/ layout: clock.c, sci9.c, stubs.c, startup.S, vectors.S,
# linker.ld are identical copies. Only main.c is sonar-specific.
#
# Wiring (from src/inc/hardware_config.h):
#   S0: TRIG=PF5 (pin 9)   ECHO=P03 (pin 4)
#   S1: TRIG=PJ5 (pin 11)  ECHO=P02 (pin 6)
#   S2: TRIG=PJ3 (pin 13)  ECHO=P01 (pin 7)
#   S3: TRIG=P33 (pin 26)  ECHO=P00 (pin 8)

CC      := rx-elf-gcc
OBJCOPY := rx-elf-objcopy
SIZE    := rx-elf-size

CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data

CORE_INC := ../libs/rx_core/inc
HAL_INC  := ../libs/rx_hal/inc
SRC_INC  := ../src/inc
INCLUDES := -I$(CORE_INC) -I$(HAL_INC) -I$(SRC_INC)

CFLAGS  := $(CPU) -std=gnu23 -O1 -g3 -Wall -Wextra $(INCLUDES) \
           -ffunction-sections -fdata-sections \
           -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld -Wl,--gc-sections

OBJS := main.o clock.o sci9.o stubs.o startup.o vectors.o
TARGET := sonar_test

RFP_CLI := /opt/rfp/linux-x64/rfp-cli

.PHONY: all clean flash
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

flash: $(TARGET).mot
	box64 $(RFP_CLI) -d RX72N -t e2l -if fine -run \
	    -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -a $<

clean:
	rm -f *.o *.elf *.mot *.hex *.map
