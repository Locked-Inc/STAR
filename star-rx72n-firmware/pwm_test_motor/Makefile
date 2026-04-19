# pwm_test_motor/Makefile -- drive rx_motor on GPTW0 with output on P23/P17.
#
# Outputs:
#   pwm_test_motor.elf / .mot / .hex
#
# Scope wiring (Tom's PCB header):
#   Ch1+ -> pin 38 (P17 / GTIOC0B -> "IN1") -- forward PWM appears here
#   Ch2+ -> pin 34 (P23 / GTIOC0A -> "IN2") -- reverse PWM appears here
#   GND  -> any GND pad
#
# DO NOT flash while the main session owns the E2 Lite.
#
# Build natively on Pi5 with rx-elf-gcc at /opt/gnurx/bin/.

CC      := rx-elf-gcc
OBJCOPY := rx-elf-objcopy
SIZE    := rx-elf-size

# Match pwm_test's target (rx72t, v3, little-endian data).  rx_motor/rx_gptw
# sources compile cleanly for this target since they're guarded by __RX__ only.
CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data

CORE_ROOT  := ../libs/rx_core
CORE_INC   := $(CORE_ROOT)/inc
CORE_SRC   := $(CORE_ROOT)/src

HAL_ROOT   := ../libs/rx_hal
HAL_INC    := $(HAL_ROOT)/inc
HAL_SRC    := $(HAL_ROOT)/src

MOTOR_ROOT := ../libs/rx_motor
MOTOR_INC  := $(MOTOR_ROOT)/inc
MOTOR_SRC  := $(MOTOR_ROOT)/src

INCLUDES := $(addprefix -I, $(CORE_INC) $(HAL_INC) $(MOTOR_INC))

# -Wno-... relax: library headers contain a number of extra-arg warnings we
# don't care about for this throwaway test.
CFLAGS  := $(CPU) -std=gnu23 -O1 -g3 -Wall -Wextra $(INCLUDES) \
           -ffunction-sections -fdata-sections \
           -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
           -Wl,--gc-sections

# Application sources (local to this directory)
APP_CSRC := main.c clock.c
APP_ASRC := startup.S vectors.S

# Library sources pulled in to back rx_motor_init / rx_motor_set_duty.
LIB_CSRC := \
    $(MOTOR_SRC)/rx_motor.c \
    $(HAL_SRC)/rx_gptw.c    \
    $(HAL_SRC)/rx_mpc.c

ALL_CSRC := $(APP_CSRC) $(LIB_CSRC)

APP_OBJS := $(APP_CSRC:.c=.o) $(APP_ASRC:.S=.o)
LIB_OBJS := \
    rx_motor.o \
    rx_gptw.o  \
    rx_mpc.o

OBJS := $(APP_OBJS) $(LIB_OBJS)

TARGET := pwm_test_motor

.PHONY: all clean

all: $(TARGET).elf $(TARGET).mot $(TARGET).hex
	@echo ""
	@$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

# Local .c / .S rules
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

# Library .c rules -- compile from lib path into flat .o next to our sources.
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

clean:
	rm -f *.o *.elf *.mot *.hex
