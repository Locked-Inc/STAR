# imu_test/Makefile -- BNO055 + BMP280 over RIIC1 smoke test, bare-metal.
#
# Mirrors encoder_test layout: direct-register peripheral init, no
# ThreadX, no rx_bus_manager / rx_bno055 lib dependency. Goal: prove
# the I2C wiring and the BNO055 chip respond correctly on the
# production STAR PCB. If chip ID reads back 0xA0 (and BMP280 reads
# back 0x58), the lib version will trivially work too.
#
# Outputs:
#   imu_test.elf / .mot / .hex
#
# Wiring (matches src/inc/hardware_config.h):
#   IMU_SCL  = P21 / SCL1   (RIIC1 clock,  pin 36)
#   IMU_SDA  = P20 / SDA1   (RIIC1 data,   pin 37)
#   IMU_RST  = P83          (active-low BNO055 reset, pin 58)
#   IMU_INT  = P32          (BNO055 interrupt, IRQ12 -- not used in this test)
#   Cypress USB-UART = SCI9 on PB6/PB7 -- /dev/ttyACM0 @ 115200

CC      := rx-elf-gcc
OBJCOPY := rx-elf-objcopy
SIZE    := rx-elf-size

CPU     := -mcpu=rx72t -misa=v3 -mlittle-endian-data

CORE_INC := ../libs/rx_core/inc
HAL_INC  := ../libs/rx_hal/inc
INCLUDES := -I$(CORE_INC) -I$(HAL_INC)

CFLAGS  := $(CPU) -std=gnu23 -O1 -g3 -Wall -Wextra $(INCLUDES) \
           -ffunction-sections -fdata-sections \
           -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function
ASFLAGS := $(CPU)
LDFLAGS := $(CPU) -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld -Wl,--gc-sections

OBJS := main.o clock.o sci9.o stubs.o startup.o vectors.o
TARGET := imu_test

.PHONY: all clean
all: $(TARGET).elf $(TARGET).mot $(TARGET).hex
	@$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

$(TARGET).mot: $(TARGET).elf
	$(OBJCOPY) -O srec $< $@
$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

clean:
	rm -f *.o *.elf *.mot *.hex *.map
