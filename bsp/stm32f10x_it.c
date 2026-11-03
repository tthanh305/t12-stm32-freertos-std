/**
 * @file    stm32f10x_it.c
 * @brief   Trình phục vụ ngắt hệ thống (Cortex-M3 System Exceptions)
 * @note    Khi xảy ra lỗi phần cứng nghiêm trọng (HardFault), lập tức ngắt PWM gia nhiệt (PA6)
 *          để bảo vệ an toàn cho mũi hàn T12 và MOSFET AO4407A.
 */

#include "stm32f10x_it.h"
#include "bsp_board.h"

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    /* Ngắt ngay lập tức xung PWM mũi hàn (TIM3_CH1) để đảm bảo an toàn cháy nổ */
    TIM3->CCR1 = 0;
    GPIO_ResetBits(BSP_HEATER_PWM_GPIO_PORT, BSP_HEATER_PWM_GPIO_PIN);

    while (1)
    {
    }
}

void MemManage_Handler(void)
{
    TIM3->CCR1 = 0;
    while (1)
    {
    }
}

void BusFault_Handler(void)
{
    TIM3->CCR1 = 0;
    while (1)
    {
    }
}

void UsageFault_Handler(void)
{
    TIM3->CCR1 = 0;
    while (1)
    {
    }
}

void DebugMon_Handler(void)
{
}

