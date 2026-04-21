# usb_test/Makefile -- Build the USB CDC bring-up firmware for RX72N.
#
# Pulls in:
#   - libs/threadx           : Azure ThreadX (RXv3 GNU port)
#   - libs/rx_usb            : 3-port CDC-ACM composite stack
#   - libs/rx_hal/inc        : register definitions only (no .c needed)
#   - libs/rx_core/inc       : rx_err, rx_check, rx_log, register protection
#
# log_stubs.c provides empty backends so we don't have to drag in the
# full uart.c + rx_log_usb.c chain just to satisfy linker symbols.
#
# Outputs:
#   usb_test.elf  -- ELF binary
#   usb_test.mot  -- Motorola S-record (Renesas Flash Programmer)
#   usb_test.hex  -- Intel HEX

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

# libs/rx_usb -- used by the cdc_test target (full CDC stack).  The default
# `usb_test` target still bypasses this via usb_min.c.
USB_ROOT    := ../libs/rx_usb
USB_INC     := $(USB_ROOT)/inc $(USB_ROOT)/src
USB_CSRC    :=
CDC_USB_CSRC := $(USB_ROOT)/src/rx_usb.c \
                $(USB_ROOT)/src/rx_usb_hw.c \
                $(USB_ROOT)/src/rx_usb_isr.c \
                $(USB_ROOT)/src/rx_usb_cdc.c

CORE_ROOT   := ../libs/rx_core
CORE_INC    := $(CORE_ROOT)/inc

HAL_ROOT    := ../libs/rx_hal
HAL_INC     := $(HAL_ROOT)/inc

INCLUDES := $(addprefix -I, $(TX_INC) $(USB_INC) $(CORE_INC) $(HAL_INC))

CFLAGS  := $(CPU) -std=gnu23 -O0 -g3 -Wall -Wextra $(INCLUDES) \
           -ffunction-sections -fdata-sections \
           -Wno-unused-parameter -Wno-unused-variable \
           -Wno-unused-but-set-variable
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
           -Wl,--gc-sections

# ----------------------------------------------------------------------------
# Application sources (this directory)
# ----------------------------------------------------------------------------
APP_CSRC := main.c clock.c cmt0.c usb_min.c sci9_debug.c
APP_ASRC := startup.S vectors.S

# ----------------------------------------------------------------------------
# Object lists
# ----------------------------------------------------------------------------
APP_OBJS  := $(APP_CSRC:.c=.o) $(APP_ASRC:.S=.o)
TX_COBJS  := $(notdir $(TX_CSRC:.c=.o))
TX_AOBJS  := $(notdir $(TX_ASRC:.S=.o))
USB_OBJS  := $(notdir $(USB_CSRC:.c=.o))

TARGET := usb_test

# cdc_test target -- drives the full libs/rx_usb CDC stack instead of usb_min.c
CDC_TARGET    := cdc_test
# LOG_LEVEL=0 (k_log_none) compiles out all rx_log_* macros so we don't drag
# in rx_log_uart / uart_debug backends -- we only need USB CDC working.
CDC_CFLAGS    := $(CFLAGS) -DLOG_LEVEL=0
CDC_APP_CSRC  := cdc_test_main.c clock.c cmt0.c sci9_debug.c cdc_stubs.c
CDC_APP_ASRC  := startup.S vectors.S
CDC_APP_COBJS := $(CDC_APP_CSRC:.c=.cdc.o)
CDC_APP_AOBJS := $(CDC_APP_ASRC:.S=.cdc.o)
CDC_TX_COBJS  := $(notdir $(TX_CSRC:.c=.cdc.o))
CDC_TX_AOBJS  := $(notdir $(TX_ASRC:.S=.cdc.o))
CDC_USB_OBJS  := $(notdir $(CDC_USB_CSRC:.c=.cdc.o))

RFP_CLI := /opt/rfp/linux-x64/rfp-cli

.PHONY: all cdc_test flash cdc_flash clean

all: $(TARGET).elf $(TARGET).mot $(TARGET).hex
	@echo ""
	@$(SIZE) $(TARGET).elf

cdc_test: $(CDC_TARGET).elf $(CDC_TARGET).mot $(CDC_TARGET).hex
	@echo ""
	@$(SIZE) $(CDC_TARGET).elf

# ----------------------------------------------------------------------------
# Link
# ----------------------------------------------------------------------------
$(TARGET).elf: $(APP_OBJS) $(TX_COBJS) $(TX_AOBJS) $(USB_OBJS) linker.ld
	$(CC) $(LDFLAGS) $(APP_OBJS) $(TX_COBJS) $(TX_AOBJS) $(USB_OBJS) -o $@

$(CDC_TARGET).elf: $(CDC_APP_COBJS) $(CDC_APP_AOBJS) $(CDC_TX_COBJS) $(CDC_TX_AOBJS) $(CDC_USB_OBJS) linker.ld
	$(CC) $(LDFLAGS) $(CDC_APP_COBJS) $(CDC_APP_AOBJS) $(CDC_TX_COBJS) $(CDC_TX_AOBJS) $(CDC_USB_OBJS) -o $@

# ----------------------------------------------------------------------------
# Compile rules (default usb_test target)
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

# rx_usb sources
%.o: $(USB_ROOT)/src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ----------------------------------------------------------------------------
# Compile rules for cdc_test target (.cdc.o suffix so default target objects
# aren't shared -- cdc_test_main.c and main.c both define tx_application_define)
# ----------------------------------------------------------------------------
%.cdc.o: %.c
	$(CC) $(CDC_CFLAGS) -c $< -o $@

%.cdc.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

%.cdc.o: $(TX_ROOT)/common/src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

%.cdc.o: $(TX_ROOT)/ports/rxv3/gnu/src/%.S
	$(CC) $(ASFLAGS) -c $< -o $@

%.cdc.o: $(USB_ROOT)/src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ----------------------------------------------------------------------------
# Output formats
# ----------------------------------------------------------------------------
$(TARGET).mot: $(TARGET).elf
	$(OBJCOPY) -O srec $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(CDC_TARGET).mot: $(CDC_TARGET).elf
	$(OBJCOPY) -O srec $< $@

$(CDC_TARGET).hex: $(CDC_TARGET).elf
	$(OBJCOPY) -O ihex $< $@

# ----------------------------------------------------------------------------
# Flash via the E2 Lite + rfp-cli
# ----------------------------------------------------------------------------
flash: $(TARGET).mot
	sudo $(RFP_CLI) -d RX72N -t e2l -if fine -run \
	    -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -a $<

cdc_flash: $(CDC_TARGET).mot
	sudo $(RFP_CLI) -d RX72N -t e2l -if fine -run \
	    -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -a $<

clean:
	rm -f $(TARGET).elf $(TARGET).mot $(TARGET).hex \
	      $(CDC_TARGET).elf $(CDC_TARGET).mot $(CDC_TARGET).hex \
	      $(APP_OBJS) $(TX_COBJS) $(TX_AOBJS) $(USB_OBJS) \
	      $(CDC_APP_COBJS) $(CDC_APP_AOBJS) $(CDC_TX_COBJS) $(CDC_TX_AOBJS) $(CDC_USB_OBJS)
