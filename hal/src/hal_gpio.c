/**
 * @file    hal_gpio.c
 * @brief   Driver GPIO đọc nút nhấn Encoder (PB5), Cảm biến rung (PA0) và Đế gác (PA1)
 * @note    TODO [Dev 3 - Nhánh feature/hal-gpio-uart]: Cài đặt chi tiết bằng stm32f10x_gpio.c
 */

#include "hal_gpio.h"
#include "bsp_board.h"

void hal_gpio_init(void)
{
    /* TODO [Dev 3]: Cấu hình PA0 (SLEEP), PA1 (VB) và PB5 (EC_SW) ở chế độ GPIO_Mode_IPU */
}

bool hal_gpio_read_encoder_sw(void)
{
    /* TODO [Dev 3]: Trả về (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_5) == Bit_RESET) */
    return false;
}

bool hal_gpio_read_sleep_sensor(void)
{
    /* TODO [Dev 3]: Đọc trạng thái công tắc rung PA0 */
    return false;
}

bool hal_gpio_read_stand_vb(void)
{
    /* TODO [Dev 3]: Trả về (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1) == Bit_RESET) */
    return false;
}

