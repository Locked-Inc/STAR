# blinky_rtos/Makefile -- Build ThreadX multi-LED breathing firmware for RX72N.
#
# Requires GNURX toolchain (rx-elf-gcc).
# Install on Arch: yay -S gnurx-toolchain  (or see Renesas GCC for RX releases)
#
# ThreadX sources come from ../../libs/threadx (checked in to the repo).
#
# Outputs:
#   blinky_rtos.elf  -- ELF binary (use with OpenOCD / J-Link / e2 studio)
#   blinky_rtos.mot  -- Motorola S-record (use with Renesas Flash Programmer)
#   blinky_rtos.hex  -- Intel HEX (use with some J-Link / OpenOCD flows)

CC      := rx-elf-gcc
OBJCOPY := rx-elf-objcopy
SIZE    := rx-elf-size

# CPU flags must match the RX72N (RXv3 core, little-endian data)
CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data

# ThreadX library paths (relative to this Makefile)
TX_ROOT  := ../libs/threadx
TX_INC   := $(TX_ROOT)/common/inc $(TX_ROOT)/ports/rxv3/gnu/inc $(TX_ROOT)
TX_CSRC  := $(wildcard $(TX_ROOT)/common/src/*.c)
TX_ASRC  := $(wildcard $(TX_ROOT)/ports/rxv3/gnu/src/*.S)

INCLUDES := $(addprefix -I, $(TX_INC))

CFLAGS  := $(CPU) -std=c11 -O0 -g3 -Wall -Wextra $(INCLUDES)
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld

# Application sources in this directory
APP_CSRC := main.c
APP_ASRC := startup.S vectors.S

# All objects
APP_OBJS := $(APP_CSRC:.c=.o) $(APP_ASRC:.S=.o)
TX_COBJS := $(notdir $(TX_CSRC:.c=.o))
TX_AOBJS := $(notdir $(TX_ASRC:.S=.o))

TARGET := blinky_rtos

RFP_CLI := /opt/rfp/linux-x64/rfp-cli

.PHONY: all flash clean

all: $(TARGET).elf $(TARGET).mot $(TARGET).hex
	@echo ""
	@$(SIZE) $(TARGET).elf

# Link everything together
$(TARGET).elf: $(APP_OBJS) $(TX_COBJS) $(TX_AOBJS) linker.ld
	$(CC) $(LDFLAGS) $(APP_OBJS) $(TX_COBJS) $(TX_AOBJS) -o $@

# Compile application C sources (local directory)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble application .S sources (local directory)
%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

# Compile ThreadX common C sources (output into current directory)
%.o: $(TX_ROOT)/common/src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble ThreadX RXv3 port .S sources (output into current directory)
%.o: $(TX_ROOT)/ports/rxv3/gnu/src/%.S
	$(CC) $(ASFLAGS) -c $< -o $@

$(TARGET).mot: $(TARGET).elf
	$(OBJCOPY) -O srec $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

flash: $(TARGET).mot
	sudo $(RFP_CLI) -d RX72N -t e2l -if fine -run \
	    -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -a $<

clean:
	rm -f $(TARGET).elf $(TARGET).mot $(TARGET).hex \
	      $(APP_OBJS) $(TX_COBJS) $(TX_AOBJS)
