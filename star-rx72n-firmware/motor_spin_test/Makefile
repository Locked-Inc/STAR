# motor_spin_test/Makefile -- open-loop 4-motor sweep on the real STAR PCB.
#
# Build natively on the Pi5 with the GNURX toolchain at /opt/gnurx/bin
# (or wherever rx-elf-gcc is in $PATH).
#
# Outputs:
#   motor_spin_test.elf / .mot / .hex
#
# Wiring: this firmware uses the production STAR PCB pin map. Power motors
# from the bus voltage input -- USB alone cannot drive them.

CC      := rx-elf-gcc
OBJCOPY := rx-elf-objcopy
SIZE    := rx-elf-size

# RXv3 core, little-endian data; same flags as the other bench tests.
CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data

# Compile-time bitmask selecting which motors spin.
#   0x1=M0, 0x2=M1, 0x4=M2, 0x8=M3, 0xF=all four.
# Override on the command line: make MOTOR_MASK=0x1
MOTOR_MASK ?= 0xF

# Compile-time duty sweep range. Defaults bidirectional. Override for
# forward-only or reverse-only test variants.
MOTOR_DUTY_MIN ?= -100
MOTOR_DUTY_MAX ?= 100

CORE_ROOT  := ../libs/rx_core
HAL_ROOT   := ../libs/rx_hal
MOTOR_ROOT := ../libs/rx_motor

CORE_INC   := $(CORE_ROOT)/inc
HAL_INC    := $(HAL_ROOT)/inc
HAL_SRC    := $(HAL_ROOT)/src
MOTOR_INC  := $(MOTOR_ROOT)/inc
MOTOR_SRC  := $(MOTOR_ROOT)/src

INCLUDES := $(addprefix -I, $(CORE_INC) $(HAL_INC) $(MOTOR_INC))

CFLAGS  := $(CPU) -std=gnu23 -O1 -g3 -Wall -Wextra $(INCLUDES) \
           -ffunction-sections -fdata-sections \
           -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function \
           -DMOTOR_ENABLE_MASK=$(MOTOR_MASK) \
           -DMOTOR_DUTY_MIN=$(MOTOR_DUTY_MIN) \
           -DMOTOR_DUTY_MAX=$(MOTOR_DUTY_MAX)
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
           -Wl,--gc-sections -lm

# Application sources (local to this directory).
APP_CSRC := main.c clock.c sci9.c stubs.c
APP_ASRC := startup.S vectors.S

# Production library sources -- compile from their lib paths into flat .o
# files next to ours so the existing %.o:%.c rule covers them.
LIB_CSRC := \
    $(MOTOR_SRC)/rx_motor.c \
    $(HAL_SRC)/rx_gptw.c    \
    $(HAL_SRC)/rx_mpc.c

APP_OBJS := $(APP_CSRC:.c=.o) $(APP_ASRC:.S=.o)
LIB_OBJS := rx_motor.o rx_gptw.o rx_mpc.o
OBJS     := $(APP_OBJS) $(LIB_OBJS)

TARGET  := motor_spin_test
RFP_CLI ?= $(HOME)/tools/rfp-cli/bin/rfp-cli

.PHONY: all flash clean

all: $(TARGET).elf $(TARGET).mot $(TARGET).hex
	@echo ""
	@$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

# Local rules.
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

# Production library rules.
rx_motor.o: $(MOTOR_SRC)/rx_motor.c
	$(CC) $(CFLAGS) -c $< -o $@
rx_gptw.o: $(HAL_SRC)/rx_gptw.c
	$(CC) $(CFLAGS) -c $< -o $@
rx_mpc.o: $(HAL_SRC)/rx_mpc.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).mot: $(TARGET).elf
	$(OBJCOPY) -O srec $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

flash: $(TARGET).mot
	$(RFP_CLI) -d RX720 -t e2l -if fine -run \
	    -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -a $<

clean:
	rm -f *.o *.elf *.mot *.hex
