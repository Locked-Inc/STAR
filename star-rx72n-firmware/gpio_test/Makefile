# gpio_test/Makefile -- Build the GPIO breakout board test firmware for RX72N.
#
# Pulls in:
#   - libs/threadx           : Azure ThreadX (RXv3 GNU port)
#   - libs/rx_hal/inc        : register definitions (rx72n_regs.h for clock.c)
#   - libs/rx_core/inc       : rx_register_protection.h for clock.c
#
# No USB, no SCI9, no logging -- just GPIO toggles with ThreadX timing.
#
# Outputs:
#   gpio_test.elf  -- ELF binary
#   gpio_test.mot  -- Motorola S-record (Renesas Flash Programmer)
#   gpio_test.hex  -- Intel HEX

CC      := rx-elf-gcc
OBJCOPY := rx-elf-objcopy
SIZE    := rx-elf-size

CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data

# ----------------------------------------------------------------------------
# Vendored libraries
# ----------------------------------------------------------------------------
TX_ROOT     := ../libs/threadx
TX_INC      := $(TX_ROOT)/common/inc $(TX_ROOT)/ports/rxv3/gnu/inc $(TX_ROOT)
TX_CSRC     := $(wildcard $(TX_ROOT)/common/src/*.c)
TX_ASRC     := $(wildcard $(TX_ROOT)/ports/rxv3/gnu/src/*.S)

CORE_ROOT   := ../libs/rx_core
CORE_INC    := $(CORE_ROOT)/inc

HAL_ROOT    := ../libs/rx_hal
HAL_INC     := $(HAL_ROOT)/inc

INCLUDES := $(addprefix -I, $(TX_INC) $(CORE_INC) $(HAL_INC))

CFLAGS  := $(CPU) -std=gnu23 -O0 -g3 -Wall -Wextra $(INCLUDES) \
           -ffunction-sections -fdata-sections \
           -Wno-unused-parameter -Wno-unused-variable
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
           -Wl,--gc-sections

# ----------------------------------------------------------------------------
# Application sources (this directory)
# ----------------------------------------------------------------------------
APP_CSRC := main.c clock.c cmt0.c
APP_ASRC := startup.S vectors.S

# ----------------------------------------------------------------------------
# Object lists
# ----------------------------------------------------------------------------
APP_OBJS  := $(APP_CSRC:.c=.o) $(APP_ASRC:.S=.o)
TX_COBJS  := $(notdir $(TX_CSRC:.c=.o))
TX_AOBJS  := $(notdir $(TX_ASRC:.S=.o))

TARGET := gpio_test

RFP_CLI := /opt/rfp/linux-x64/rfp-cli

.PHONY: all flash clean

all: $(TARGET).elf $(TARGET).mot $(TARGET).hex
	@echo ""
	@$(SIZE) $(TARGET).elf

# ----------------------------------------------------------------------------
# Link
# ----------------------------------------------------------------------------
$(TARGET).elf: $(APP_OBJS) $(TX_COBJS) $(TX_AOBJS) linker.ld
	$(CC) $(LDFLAGS) $(APP_OBJS) $(TX_COBJS) $(TX_AOBJS) -o $@

# ----------------------------------------------------------------------------
# Compile rules
# ----------------------------------------------------------------------------
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

# ----------------------------------------------------------------------------
# Output formats
# ----------------------------------------------------------------------------
$(TARGET).mot: $(TARGET).elf
	$(OBJCOPY) -O srec $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

# ----------------------------------------------------------------------------
# Flash via the E2 Lite + rfp-cli
# ----------------------------------------------------------------------------
flash: $(TARGET).mot
	sudo $(RFP_CLI) -d RX72N -t e2l -if fine -run \
	    -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -a $<

clean:
	rm -f $(TARGET).elf $(TARGET).mot $(TARGET).hex \
	      $(APP_OBJS) $(TX_COBJS) $(TX_AOBJS)
