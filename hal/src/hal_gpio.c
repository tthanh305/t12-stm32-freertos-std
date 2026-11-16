/**
 * @file    hal_gpio.c
 * @brief   Driver GPIO đọc nút nhấn Encoder (PB5), Cảm biến rung (PA0) và Đế gác (PA1)
 * @note    TODO [Dev 3 - Nhánh feature/hal-gpio-uart]: Cài đặt chi tiết bằng stm32f10x_gpio.c
 */

#include "hal_gpio.h"
#include "bsp_board.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

void hal_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // Bat clock cho GPIOA va GPIOB
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB,
        ENABLE
    );

    // Cau hinh PA0 (SLEEP) va PA1 (VB)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // Cau hinh PB5 (EC_SW)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

bool hal_gpio_read_encoder_sw(void)
{
     return (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_5)
            == Bit_RESET);

}

bool hal_gpio_read_sleep_sensor(void)
{
    return (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0)
            == Bit_SET);
}

bool hal_gpio_read_stand_vb(void)
{
    return (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1)
            == Bit_RESET);
}

