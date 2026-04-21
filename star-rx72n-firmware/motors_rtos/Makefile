# motors_rtos/Makefile -- motors + encoders under Azure RTOS ThreadX.
#
# Combines:
#   - libs/threadx          (ThreadX 6.1.9 RXv3 GNU port)
#   - libs/rx_motor         (GPTW PWM driver)
#   - libs/rx_hal rx_gptw / rx_mpc / rx_mtu / rx_tpu
#   - libs/rx_encoder       (MTU + TPU quadrature readers)
#   - libs/rx_core          (err codes, register protection)
#
# Outputs:
#   motors_rtos.elf  -- ELF (debugger / OpenOCD)
#   motors_rtos.mot  -- Motorola S-record (rfp-cli)
#   motors_rtos.hex  -- Intel HEX
#
# Flash: `make flash` calls ../../scripts/flash-rx72n.sh which auto-picks
# box64 on aarch64 per memory project_rfp_cli_box64.md.

# Prepend /opt/gnurx/bin so rx-elf-gcc works without a custom shell env.
export PATH := /opt/gnurx/bin:$(PATH)

CC      := rx-elf-gcc
OBJCOPY := rx-elf-objcopy
SIZE    := rx-elf-size

# Project-wide RX72N flags (rx-elf-gcc has no -mcpu=rx72n, rx72t is RXv3 + FPU
# and produces identical codegen -- see pwm_test_fit/Makefile comment).
#
# `-mdfpu` is REQUIRED for ThreadX RXv3 port: its tx_thread_schedule.S at line
# 167 gates FPU context save/restore on `#if (__RX_DFPU_INSNS__ == 1)`, and
# that preprocessor symbol is only defined when -mdfpu is passed. Without it,
# any task that touches FPU (e.g. motor_task calling rx_motor_set_duty(float))
# loses its register state across context switches and silently never runs.
# Matches the production toolchain flags at cmake/toolchain-rx72n.cmake:22.
# Note: `-mcpu=rx72n` is NOT accepted by this gnurx 14.2 (`rx-elf-gcc --help=target`
# lists only rx100/rx13t/rx140/rx200/rx230/rx600/rx610/rx64m/rx66t/rx71m/rx72t).
# `-mcpu=rx72t` is the closest RXv3 target; the crucial FPU behaviour comes
# from `-mdfpu` (enables double-precision FPU instructions) and
# `-m64bit-doubles` (matches production cmake/toolchain-rx72n.cmake:22).
CPU := -mcpu=rx72t -misa=v3 -mlittle-endian-data -mdfpu -m64bit-doubles

# ----------------------------------------------------------------------------
# Vendored library roots
# ----------------------------------------------------------------------------
TX_ROOT    := ../libs/threadx
CORE_ROOT  := ../libs/rx_core
HAL_ROOT   := ../libs/rx_hal
MOTOR_ROOT := ../libs/rx_motor
ENC_ROOT   := ../libs/rx_encoder

TX_INC  := $(TX_ROOT)/common/inc $(TX_ROOT)/ports/rxv3/gnu/inc $(TX_ROOT)
TX_CSRC := $(wildcard $(TX_ROOT)/common/src/*.c)
TX_ASRC := $(wildcard $(TX_ROOT)/ports/rxv3/gnu/src/*.S)

INCLUDES := $(addprefix -I, \
    $(TX_INC) \
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

# ----------------------------------------------------------------------------
# Application sources (this directory)
# ----------------------------------------------------------------------------
APP_CSRC := main.c clock.c sci9.c stubs.c cmt0.c
APP_ASRC := startup.S vectors.S

# ----------------------------------------------------------------------------
# Library sources (compiled into this directory with their bare basenames so
# the default %.o rule picks them up from the VPATH-like explicit rules below).
# ----------------------------------------------------------------------------
LIB_C_FILES := \
    $(MOTOR_ROOT)/src/rx_motor.c \
    $(HAL_ROOT)/src/rx_gptw.c    \
    $(HAL_ROOT)/src/rx_mpc.c     \
    $(HAL_ROOT)/src/rx_mtu.c     \
    $(HAL_ROOT)/src/rx_tpu.c     \
    $(ENC_ROOT)/src/rx_mtu_encoder.c \
    $(ENC_ROOT)/src/rx_encoder_tpu.c

APP_OBJS := $(APP_CSRC:.c=.o) $(APP_ASRC:.S=.o)
LIB_OBJS := rx_motor.o rx_gptw.o rx_mpc.o rx_mtu.o rx_tpu.o \
            rx_mtu_encoder.o rx_encoder_tpu.o
TX_COBJS := $(notdir $(TX_CSRC:.c=.o))
TX_AOBJS := $(notdir $(TX_ASRC:.S=.o))

TARGET := motors_rtos

# ----------------------------------------------------------------------------
# Build rules
# ----------------------------------------------------------------------------
.PHONY: all flash clean
all: $(TARGET).elf $(TARGET).mot $(TARGET).hex
	@echo ""
	@$(SIZE) $(TARGET).elf

$(TARGET).elf: $(APP_OBJS) $(LIB_OBJS) $(TX_COBJS) $(TX_AOBJS) linker.ld
	$(CC) $(LDFLAGS) $(APP_OBJS) $(LIB_OBJS) $(TX_COBJS) $(TX_AOBJS) -o $@

# App compile rules
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

# Library compile rules (one per file -- mirrors encoder_test pattern)
rx_motor.o:       $(MOTOR_ROOT)/src/rx_motor.c
	$(CC) $(CFLAGS) -c $< -o $@
rx_gptw.o:        $(HAL_ROOT)/src/rx_gptw.c
	$(CC) $(CFLAGS) -c $< -o $@
rx_mpc.o:         $(HAL_ROOT)/src/rx_mpc.c
	$(CC) $(CFLAGS) -c $< -o $@
rx_mtu.o:         $(HAL_ROOT)/src/rx_mtu.c
	$(CC) $(CFLAGS) -c $< -o $@
rx_tpu.o:         $(HAL_ROOT)/src/rx_tpu.c
	$(CC) $(CFLAGS) -c $< -o $@
rx_mtu_encoder.o: $(ENC_ROOT)/src/rx_mtu_encoder.c
	$(CC) $(CFLAGS) -c $< -o $@
rx_encoder_tpu.o: $(ENC_ROOT)/src/rx_encoder_tpu.c
	$(CC) $(CFLAGS) -c $< -o $@

# ThreadX common C sources
%.o: $(TX_ROOT)/common/src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ThreadX RXv3 GNU port assembly.
#
# CRITICAL: the upstream .S files (tx_thread_context_save.S etc.) use hybrid
# CC-RX/GNU syntax -- they put C-style `/* ... */` block comments INSIDE
# `;`-prefixed assembler-comment lines, and the `/* */` blocks SPAN REAL
# EXECUTABLE INSTRUCTIONS.  CC-RX treats `;` as a line comment so the `/*`
# and `*/` are inert; cpp (which rx-elf-gcc runs on `.S` by default) does
# NOT know about `;` and cheerfully eats everything from `/*` at line 88 of
# context_save.S to `*/` at line 114 -- deleting the MOV.L/CMP/BEQ/ADD/MOV
# sequence that increments _tx_thread_system_state.  Net effect: every CMT0
# ISR decrements system_state without a matching increment, so after the
# first tick ThreadX thinks it is permanently inside a nested ISR and
# tx_thread_sleep returns TX_CALLER_ERROR forever -- motor_task never runs.
#
# We CAN'T just pass `-x assembler` (no cpp): tx_thread_schedule.S uses real
# `#if (TX_LOW_POWER == 1)` guards that require cpp to skip the BSR to
# tx_low_power_enter / tx_low_power_exit (undefined in our build).
#
# Fix: sed-strip the `/* */` markers (they only appear inside `;` lines, so
# stripping them leaves valid assembly), then let cpp run normally so the
# #if/#endif guards still work.  Verified empirically 2026-04-21: first
# build with raw `-x assembler` produced a broken context_save; second
# build with this sed + cpp produced the correct 11-instruction function
# including the system_state increment.
%.o: $(TX_ROOT)/ports/rxv3/gnu/src/%.S
	sed -e 's|/\*||g' -e 's|\*/||g' $< > $*.sed.S
	$(CC) $(ASFLAGS) -c $*.sed.S -o $@
	@rm -f $*.sed.S

# ----------------------------------------------------------------------------
# Output formats
# ----------------------------------------------------------------------------
$(TARGET).mot: $(TARGET).elf
	$(OBJCOPY) -O srec $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

# ----------------------------------------------------------------------------
# Flash via the E2 Lite (uses box64 auto-selection in flash-rx72n.sh)
# ----------------------------------------------------------------------------
flash: $(TARGET).mot
	bash ../../scripts/flash-rx72n.sh $(TARGET).elf e2lite

clean:
	rm -f $(TARGET).elf $(TARGET).mot $(TARGET).hex \
	      $(APP_OBJS) $(LIB_OBJS) $(TX_COBJS) $(TX_AOBJS)
