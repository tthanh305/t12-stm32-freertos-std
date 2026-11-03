/**
 * @file    hal_i2c.c
 * @brief   Driver I2C1 Remap (PB8=SCL, PB9=SDA @ 400kHz) và Callback cho U8g2
 * @note    TODO [Dev 2 - Nhánh feature/hal-i2c-spi]: Cài đặt chi tiết bằng stm32f10x_i2c.c
 */

#include "hal_i2c.h"
#include "bsp_board.h"

void hal_i2c_init(void)
{
    /* TODO [Dev 2]:
     * 1. Bật clock RCC_APB1Periph_I2C1, RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO.
     * 2. Gọi GPIO_PinRemapConfig(GPIO_Remap_I2C1, ENABLE).
     * 3. Cấu hình PB8, PB9 ở GPIO_Mode_AF_OD (50MHz).
     * 4. Khởi tạo I2C1 ở tốc độ 400kHz Fast Mode. */
}

uint8_t u8x8_byte_stm32_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    /* TODO [Dev 2]: Cài đặt truyền byte I2C1 phần cứng cho U8g2 kèm timeout chống treo bus */
    (void)u8x8;
    (void)msg;
    (void)arg_int;
    (void)arg_ptr;
    return 1U;
}

uint8_t u8x8_gpio_and_delay_stm32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    /* TODO [Dev 2]: Dùng vTaskDelay() cho các delay mili-giây khi Scheduler đang chạy */
    (void)u8x8;
    (void)msg;
    (void)arg_int;
    (void)arg_ptr;
    return 1U;
}

