################################################################################
# Makefile chuẩn cho Dự Án Trạm Hàn T12 (STM32F103C8T6 + STD/SPL v3.5.0 + FreeRTOS)
# Hỗ trợ Arm GNU Toolchain (arm-none-eabi-gcc) trên cả Windows và Linux
################################################################################

TARGET      := t12_station_rtos
BUILD_DIR   := build

PREFIX      := arm-none-eabi-
CC          := $(PREFIX)gcc
AS          := $(PREFIX)gcc -x assembler-with-cpp
CP          := $(PREFIX)objcopy
SZ          := $(PREFIX)size

# 1. Cờ CPU & FPU (Cortex-M3)
CPU         := -mcpu=cortex-m3 -mthumb

# 2. Macro biên dịch bắt buộc cho STM32F10x Standard Peripheral Library
C_DEFS      := \
	-DSTM32F10X_MD \
	-DUSE_STDPERIPH_DRIVER

# 3. Đường dẫn Include (-I)
C_INCLUDES  := \
	-Ibsp \
	-Ihal/inc \
	-Iapp/inc \
	-Ilibraries/CMSIS \
	-Ilibraries/STM32F10x_StdPeriph_Driver/inc \
	-Imiddlewares \
	-Imiddlewares/FreeRTOS/include \
	-Imiddlewares/FreeRTOS/portable/GCC/ARM_CM3 \
	-Imiddlewares/u8g2

# 4. Danh sách file mã nguồn (.c)
C_SOURCES   := \
	main.c \
	bsp/bsp_board.c \
	bsp/stm32f10x_it.c \
	hal/src/hal_adc.c \
	hal/src/hal_timer.c \
	hal/src/hal_i2c.c \
	hal/src/hal_flash.c \
	hal/src/hal_gpio.c \
	hal/src/hal_uart.c \
	app/src/app_ipc.c \
	app/src/curve_fitting.c \
	app/src/task_sensor_pid.c \
	app/src/task_display.c \
	app/src/ui_main.c \
	app/src/ui_menu.c \
	app/src/task_input_comm.c \
	libraries/CMSIS/core_cm3.c \
	libraries/CMSIS/system_stm32f10x.c \
	libraries/CMSIS/startup/startup_stm32f10x_md.c \
	libraries/STM32F10x_StdPeriph_Driver/src/misc.c \
	libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_adc.c \
	libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_dma.c \
	libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_exti.c \
	libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_flash.c \
	libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_gpio.c \
	libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_i2c.c \
	libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_iwdg.c \
	libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_pwr.c \
	libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_rcc.c \
	libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_tim.c \
	libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_usart.c \
	middlewares/FreeRTOS/src/tasks.c \
	middlewares/FreeRTOS/src/queue.c \
	middlewares/FreeRTOS/src/list.c \
	middlewares/FreeRTOS/src/timers.c \
	middlewares/FreeRTOS/src/event_groups.c \
	middlewares/FreeRTOS/portable/GCC/ARM_CM3/port.c \
	middlewares/FreeRTOS/portable/MemMang/heap_4.c \
	$(wildcard middlewares/u8g2/*.c)

# 5. Cờ biên dịch & liên kết (-ffunction-sections -fdata-sections + --gc-sections giúp lọc bỏ font/driver U8g2 không dùng)
CFLAGS      := $(CPU) $(C_DEFS) $(C_INCLUDES) -Os -Wall -Wextra -std=c99 -ffunction-sections -fdata-sections -g
LDSCRIPT    := bsp/stm32f103c8t6.ld
LDFLAGS     := $(CPU) -specs=nano.specs -specs=nosys.specs -T$(LDSCRIPT) -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -lm

OBJECTS     := $(addprefix $(BUILD_DIR)/,$(C_SOURCES:.c=.o))

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(CP) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(CP) -O binary -S $< $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean

