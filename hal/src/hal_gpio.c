/**
 * @file    hal_gpio.c
 * @brief   Driver GPIO đọc nút nhấn Encoder (PB7), Cảm biến rung (PA0) và Đế gác (PA1)
 *          sử dụng Thư viện chuẩn STM32F10x Standard Peripheral Library (STD/SPL v3.5.0)
 * @author  Dev 3
 */

#include "hal_gpio.h"
#include "bsp_board.h"

void hal_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 1. Bật xung nhịp cho GPIOA và GPIOB */
    RCC_APB2PeriphClockCmd(BSP_SLEEP_GPIO_CLK | BSP_STAND_VB_GPIO_CLK | BSP_EC_SW_GPIO_CLK, ENABLE);

    /* 2. Cấu hình PA0 (SLEEP) và PA1 (VB) ở chế độ Input Pull-Up (IPU) */
    GPIO_InitStructure.GPIO_Pin   = BSP_SLEEP_GPIO_PIN | BSP_STAND_VB_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, BSP_SLEEP_GPIO_PIN | BSP_STAND_VB_GPIO_PIN);

    /* 3. Cấu hình PB7 (EC_SW) ở chế độ Input Pull-Up (IPU) - Kéo lên nội bằng code */
    GPIO_InitStructure.GPIO_Pin   = BSP_EC_SW_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BSP_EC_SW_GPIO_PORT, &GPIO_InitStructure);
    GPIO_SetBits(BSP_EC_SW_GPIO_PORT, BSP_EC_SW_GPIO_PIN);
}

bool hal_gpio_read_encoder_sw(void)
{
    /* Trả về true nếu nút đang được nhấn (mức tích cực thấp Bit_RESET) */
    return (GPIO_ReadInputDataBit(BSP_EC_SW_GPIO_PORT, BSP_EC_SW_GPIO_PIN) == Bit_RESET);
}

bool hal_gpio_read_sleep_sensor(void)
{
    /* Đọc trạng thái công tắc rung PA0 (thay đổi mức logic khi có rung lắc tay hàn) */
    return (GPIO_ReadInputDataBit(BSP_SLEEP_GPIO_PORT, BSP_SLEEP_GPIO_PIN) != Bit_RESET);
}

bool hal_gpio_read_stand_vb(void)
{
    /* Trả về true nếu tay hàn đang đặt trên đế gác (mức tích cực thấp Bit_RESET) */
    return (GPIO_ReadInputDataBit(BSP_STAND_VB_GPIO_PORT, BSP_STAND_VB_GPIO_PIN) == Bit_RESET);
}
