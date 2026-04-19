# pwm_test/Makefile -- minimal 20 kHz PWM on GPTW0 for AD2 scope verification.
#
# Outputs:
#   pwm_test.elf / .mot / .hex
#
# Flash:
#   make flash
#
# Scope wiring (Tom's PCB header):
#   Ch1+ -> pin 38 (P17 / GTIOC0B)
#   Ch2+ -> pin 34 (P23 / GTIOC0A)
#   GND  -> any GND pad

CC      := rx-elf-gcc
OBJCOPY := rx-elf-objcopy
SIZE    := rx-elf-size

CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data

CORE_ROOT   := ../libs/rx_core
CORE_INC    := $(CORE_ROOT)/inc

HAL_ROOT    := ../libs/rx_hal
HAL_INC     := $(HAL_ROOT)/inc

INCLUDES := $(addprefix -I, $(CORE_INC) $(HAL_INC))

CFLAGS  := $(CPU) -std=gnu23 -O0 -g3 -Wall -Wextra $(INCLUDES) \
           -ffunction-sections -fdata-sections \
           -Wno-unused-parameter -Wno-unused-variable
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
           -Wl,--gc-sections

APP_CSRC := main.c clock.c
APP_ASRC := startup.S vectors.S
APP_OBJS := $(APP_CSRC:.c=.o) $(APP_ASRC:.S=.o)

TARGET := pwm_test

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
