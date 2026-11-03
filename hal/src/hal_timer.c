/**
 * @file    hal_timer.c
 * @brief   Driver TIM3 PWM (PA6 Heater, PA7 Buzzer) và TIM4 Hardware Encoder (PB6/PB7)
 * @note    TODO [Dev 1 - Nhánh feature/hal-timer-adc]: Cài đặt chi tiết bằng stm32f10x_tim.c
 */

#include "hal_timer.h"
#include "bsp_board.h"

void hal_timer_init(void)
{
    /* TODO [Dev 1]:
     * 1. Cấu hình TIM3_OC1 (PA6) PWM1 Period = 2047 cho MOSFET Heater.
     * 2. Cấu hình TIM3_OC2 (PA7) PWM1 cho Buzzer.
     * 3. Cấu hình TIM4 (PB6, PB7) ở chế độ TIM_EncoderMode_TI12. */
}

void hal_pwm_set_heater(uint16_t duty_0_to_2047)
{
    /* TODO [Dev 1]: Giới hạn 0..2047 và gọi TIM_SetCompare1(TIM3, duty) */
    (void)duty_0_to_2047;
}

void hal_buzzer_tone_start(uint16_t period_us)
{
    /* TODO [Dev 1]: Cấu hình tần số / chu kỳ cho Buzzer PWM và bật 50% duty cycle */
    (void)period_us;
}

void hal_buzzer_tone_stop(void)
{
    /* TODO [Dev 1]: Tắt xung PWM trên chân PA7 (TIM_SetCompare2(TIM3, 0)) */
}

int16_t hal_encoder_get_delta(void)
{
    /* TODO [Dev 1]: Đọc độ lệch bước xoay từ TIM_GetCounter(TIM4) */
    return 0;
}

