# gpio_test/Makefile -- Build the ThreadX-based GPIO breakout test firmware.
#
# Pulls in:
#   - libs/threadx           : Azure ThreadX (RXv3 GNURX port)
#   - libs/rx_core/inc       : rx_register_protection.h (used by clock.c)
#   - libs/rx_hal/inc        : rx72n_regs.h (used by clock.c)
#
# Outputs:
#   gpio_test.elf  -- ELF binary
#   gpio_test.mot  -- Motorola S-record (Renesas Flash Programmer)
#   gpio_test.hex  -- Intel HEX

CC      := rx-elf-gcc
OBJCOPY := rx-elf-objcopy
SIZE    := rx-elf-size

# GCC for RX only accepts a fixed -mcpu set. RX72N shares the RXv3 core
# with rx72t, so rx72t is the correct flag for this board.
CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data

# ---------------------------------------------------------------------------
# Vendored libraries
# ---------------------------------------------------------------------------
TX_ROOT     := ../libs/threadx
TX_INC      := $(TX_ROOT)/common/inc $(TX_ROOT)/ports/rxv3/gnu/inc $(TX_ROOT)
TX_CSRC     := $(wildcard $(TX_ROOT)/common/src/*.c)
TX_ASRC     := $(wildcard $(TX_ROOT)/ports/rxv3/gnu/src/*.S)

CORE_ROOT   := ../libs/rx_core
HAL_ROOT    := ../libs/rx_hal

INCLUDES := $(addprefix -I, $(TX_INC) $(CORE_ROOT)/inc $(HAL_ROOT)/inc)

CFLAGS  := $(CPU) -std=gnu23 -O0 -g3 -Wall -Wextra $(INCLUDES) \
           -ffunction-sections -fdata-sections \
           -Wno-unused-parameter -Wno-unused-variable
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
           -Wl,--gc-sections

# ---------------------------------------------------------------------------
# Application sources
# ---------------------------------------------------------------------------
APP_CSRC := main.c clock.c cmt0.c
APP_ASRC := startup.S vectors.S

APP_OBJS  := $(APP_CSRC:.c=.o) $(APP_ASRC:.S=.o)
TX_COBJS  := $(notdir $(TX_CSRC:.c=.o))
TX_AOBJS  := $(notdir $(TX_ASRC:.S=.o))

TARGET := gpio_test
RFP_CLI ?= $(HOME)/tools/rfp-cli/bin/rfp-cli

.PHONY: all flash clean

all: $(TARGET).elf $(TARGET).mot $(TARGET).hex
	@echo ""
	@$(SIZE) $(TARGET).elf

$(TARGET).elf: $(APP_OBJS) $(TX_COBJS) $(TX_AOBJS) linker.ld
	$(CC) $(LDFLAGS) $(APP_OBJS) $(TX_COBJS) $(TX_AOBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

# ThreadX common C sources
%.o: $(TX_ROOT)/common/src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ThreadX RXv3 GNU port assembly
%.o: $(TX_ROOT)/ports/rxv3/gnu/src/%.S
	$(CC) $(ASFLAGS) -c $< -o $@

$(TARGET).mot: $(TARGET).elf
	$(OBJCOPY) -O srec $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

flash: $(TARGET).mot
	$(RFP_CLI) -d RX720 -t e2l -if fine -run \
	    -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -a $<

clean:
	rm -f $(TARGET).elf $(TARGET).mot $(TARGET).hex \
	      $(APP_OBJS) $(TX_COBJS) $(TX_AOBJS)
