# encoder_test/Makefile -- drive motors + sample encoders + report via SCI9.

CC      := rx-elf-gcc
OBJCOPY := rx-elf-objcopy
SIZE    := rx-elf-size

CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data

CORE_ROOT  := ../libs/rx_core
HAL_ROOT   := ../libs/rx_hal
MOTOR_ROOT := ../libs/rx_motor
ENC_ROOT   := ../libs/rx_encoder

INCLUDES := $(addprefix -I, \
    $(CORE_ROOT)/inc \
    $(HAL_ROOT)/inc \
    $(MOTOR_ROOT)/inc \
    $(ENC_ROOT)/inc)

CFLAGS  := $(CPU) -std=gnu23 -O1 -g3 -Wall -Wextra $(INCLUDES) \
           -ffunction-sections -fdata-sections \
           -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
           -Wl,--gc-sections -lm

APP_CSRC := main.c clock.c sci9.c stubs.c
APP_ASRC := startup.S vectors.S

LIB_CSRC := \
    $(MOTOR_ROOT)/src/rx_motor.c \
    $(HAL_ROOT)/src/rx_gptw.c    \
    $(HAL_ROOT)/src/rx_mpc.c     \
    $(HAL_ROOT)/src/rx_mtu.c     \
    $(HAL_ROOT)/src/rx_tpu.c     \
    $(ENC_ROOT)/src/rx_mtu_encoder.c \
    $(ENC_ROOT)/src/rx_encoder_tpu.c

APP_OBJS := $(APP_CSRC:.c=.o) $(APP_ASRC:.S=.o)
LIB_OBJS := rx_motor.o rx_gptw.o rx_mpc.o rx_mtu.o rx_tpu.o rx_mtu_encoder.o rx_encoder_tpu.o
OBJS     := $(APP_OBJS) $(LIB_OBJS)

TARGET  := encoder_test
RFP_CLI ?= /opt/rfp/linux-x64/rfp-cli

.PHONY: all flash clean

all: $(TARGET).elf $(TARGET).mot $(TARGET).hex
	@echo ""
	@$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

rx_motor.o: $(MOTOR_ROOT)/src/rx_motor.c
	$(CC) $(CFLAGS) -c $< -o $@
rx_gptw.o: $(HAL_ROOT)/src/rx_gptw.c
	$(CC) $(CFLAGS) -c $< -o $@
rx_mpc.o: $(HAL_ROOT)/src/rx_mpc.c
	$(CC) $(CFLAGS) -c $< -o $@
rx_mtu.o: $(HAL_ROOT)/src/rx_mtu.c
	$(CC) $(CFLAGS) -c $< -o $@
rx_tpu.o: $(HAL_ROOT)/src/rx_tpu.c
	$(CC) $(CFLAGS) -c $< -o $@
rx_mtu_encoder.o: $(ENC_ROOT)/src/rx_mtu_encoder.c
	$(CC) $(CFLAGS) -c $< -o $@
rx_encoder_tpu.o: $(ENC_ROOT)/src/rx_encoder_tpu.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).mot: $(TARGET).elf
	$(OBJCOPY) -O srec $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

flash: $(TARGET).mot
	bash ../../scripts/flash-rx72n.sh $(TARGET).elf e2lite

clean:
	rm -f *.o *.elf *.mot *.hex
