# pwm_test_hal/Makefile -- exercise STAR rx_gptw.c HAL directly to produce
# 20 kHz sawtooth PWM on GPTW0 with GTIOC0A -> P23, GTIOC0B -> P17.
#
# Outputs:
#   pwm_test_hal.elf / .mot / .hex
#
# Flash (DO NOT flash while debugger is already attached):
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
CORE_SRC    := $(CORE_ROOT)/src

HAL_ROOT    := ../libs/rx_hal
HAL_INC     := $(HAL_ROOT)/inc
HAL_SRC     := $(HAL_ROOT)/src

INCLUDES := $(addprefix -I, . $(CORE_INC) $(HAL_INC))

# LOG_LEVEL=k_log_none compiles out all rx_log_*() macros so the HAL's
# log calls become ((void)0) and the rx_log_uart ring is never touched.
# __RX__ is predefined by rx-elf-gcc but the HAL gates its entire body
# behind #ifdef __RX__ so we confirm that here too.
DEFINES := -DLOG_LEVEL=k_log_none

CFLAGS  := $(CPU) -std=gnu23 -O0 -g3 -Wall -Wextra $(INCLUDES) $(DEFINES) \
           -ffunction-sections -fdata-sections \
           -Wno-unused-parameter -Wno-unused-variable
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
           -Wl,--gc-sections

# Application sources (local to this directory)
APP_CSRC := main.c clock.c stubs.c
APP_ASRC := startup.S vectors.S

APP_OBJS := $(APP_CSRC:.c=.o) $(APP_ASRC:.S=.o)

# HAL objects -- compiled from libs/rx_hal/src into the build dir.
HAL_OBJS := rx_gptw.o rx_mpc.o

OBJS := $(APP_OBJS) $(HAL_OBJS)

TARGET := pwm_test_hal

RFP_CLI := /opt/rfp/linux-x64/rfp-cli

.PHONY: all flash clean

all: $(TARGET).elf $(TARGET).mot $(TARGET).hex
	@echo ""
	@$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

# Local C sources
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Assembly
%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

# HAL sources -- compile basename.o directly from libs tree.
rx_gptw.o: $(HAL_SRC)/rx_gptw.c
	$(CC) $(CFLAGS) -c $< -o $@

rx_mpc.o: $(HAL_SRC)/rx_mpc.c
	$(CC) $(CFLAGS) -c $< -o $@

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
