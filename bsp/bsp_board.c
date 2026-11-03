/**
 * @file    bsp_board.c
 * @brief   Cài đặt khởi tạo Clock hệ thống, NVIC, AFIO Remap và Watchdog bằng thư viện chuẩn STD/SPL
 * @note    File nền tảng do Leader (Dev 1) thiết lập.
 */

#include "bsp_board.h"

void bsp_system_init(void)
{
    /* 1. Cập nhật biến SystemCoreClock (72MHz từ HSE 8MHz qua PLL x9) */
    SystemCoreClockUpdate();

    /* 2. Cấu hình xung nhịp ADC1: PCLK2 (72MHz) / 6 = 12MHz (Chuẩn <= 14MHz của STM32F103) */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* 3. Cấu hình phân nhóm ưu tiên ngắt Cortex-M3:
     *    4 bits cho Preemption Priority (0..15), 0 bit cho SubPriority.
     *    BẮT BUỘC cho FreeRTOS trên kiến trúc ARM Cortex-M3! */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    /* 4. Bật xung nhịp cho ngoại vi AFIO và các Port GPIO cơ bản */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO  |
                           RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC, ENABLE);

    /* 5. Vô hiệu hóa JTAG-DP, chỉ giữ lại SW-DP (PA13: SWDIO, PA14: SWCLK)
     *    giúp giải phóng PA15, PB3, PB4 làm GPIO thông thường */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
}

void bsp_watchdog_init(void)
{
    /* Cho phép ghi vào thanh ghi IWDG_PR và IWDG_RLR */
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

    /* Xung nhịp LSI nội ~40kHz, chia 64 -> 625Hz (1.6ms / tick) */
    IWDG_SetPrescaler(IWDG_Prescaler_64);

    /* Đặt giá trị nạp lại 1250 -> Thời gian chờ ~ 2.0 giây (Thay thế WDT.ino gốc) */
    IWDG_SetReload(1250);

    /* Nạp bộ đếm và kích hoạt Watchdog */
    IWDG_ReloadCounter();
    IWDG_Enable();
}

void bsp_watchdog_feed(void)
{
    IWDG_ReloadCounter();
}

