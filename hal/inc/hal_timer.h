/**
 * @file    hal_timer.h
 * @brief   Giao diện Driver TIM3 PWM (MOSFET Heater PA6 & Buzzer PA7) và Rotary Encoder EC11 (PB6/PB5)
 * @note    Hỗ trợ thay đổi sơ đồ chân: PB6 (CLK), PB5 (DT), PB7 (SW)
 */

#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include "stm32f10x.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo TIM3 (CH1 PA6 cho Heater PWM 0..2047, CH2 PA7 cho Buzzer PWM)
 *        và GPIO / EXTI cho Encoder EC11 (Pha A PB6, Pha B PB5).
 */
void hal_timer_init(void);

/**
 * @brief Đặt độ rộng xung PWM gia nhiệt mũi hàn T12 trên chân PA6 (TIM3_OC1)
 *        (Tương đương hàm pwm_1b(num) trên bản Arduino gốc)
 * @param duty_0_to_2047 Giá trị độ rộng xung từ 0 (tắt hoàn toàn) đến 2047 (100% công suất)
 */
void hal_pwm_set_heater(uint16_t duty_0_to_2047);

/**
 * @brief Bắt đầu phát xung PWM cho còi thụ động BUZZ1 trên chân PA7 (TIM3_OC2)
 * @param period_us Chu kỳ sóng âm tính bằng micro-giây (ví dụ: 2 * half_period_us)
 */
void hal_buzzer_tone_start(uint16_t period_us);

/**
 * @brief Bắt đầu phát xung PWM cho còi thụ động BUZZ1 với mức âm lượng (Duty Cycle %) tùy chỉnh
 * @param period_us Chu kỳ sóng âm tính bằng micro-giây
 * @param duty_pct  Phần trăm Duty Cycle (1..50%, giá trị nhỏ giúp âm dịu, không chói tai)
 */
void hal_buzzer_tone_start_vol(uint16_t period_us, uint8_t duty_pct);

/**
 * @brief Dừng phát xung PWM còi báo trên chân PA7 (TIM3_OC2 = 0)
 */
void hal_buzzer_tone_stop(void);

/**
 * @brief Đọc độ lệch bước xoay (Delta) của Encoder EC11 (PB6/PB5) kể từ lần gọi trước
 * @return Số bước xoay có dấu (> 0: xoay thuận, < 0: xoay ngược, 0: đứng yên)
 */
int16_t hal_encoder_get_delta(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_TIMER_H */

