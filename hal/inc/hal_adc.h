/**
 * @file    hal_adc.h
 * @brief   Giao diện Driver ADC1 + DMA1_Channel1 (3 kênh: T12_TEMP, VOL_RE, TC1047) dùng thư viện STD/SPL
 * @note    Phụ trách cài đặt (.c): Dev 1 (Leader) trên nhánh feature/hal-timer-adc
 */

#ifndef HAL_ADC_H
#define HAL_ADC_H

#include "stm32f10x.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo ADC1 (Scan Mode + Continuous Mode) kết hợp DMA1_Channel1 (Circular Mode)
 *        quét 3 kênh PA2 (ADC_Channel_2), PA3 (ADC_Channel_3), PA4 (ADC_Channel_4)
 *        vào mảng đệm 16 mẫu x 3 kênh.
 */
void hal_adc_init(void);

/**
 * @brief Đọc giá trị trung bình 16 mẫu DMA của kênh mũi hàn T12 (PA2) và chia cho 4.0f
 *        để quy đổi từ ADC 12-bit (0..4095) về thang 10-bit (0.0f..1023.75f) của thuật toán gốc.
 * @return Giá trị ADC mũi hàn đã quy đổi (0.0f .. 1023.75f)
 */
float hal_adc_read_t12_scaled(void);

/**
 * @brief Đọc và tính toán điện áp nguồn đầu vào DCIN 20V (PA3) qua cầu phân áp R8=100K / R10=10K
 * @param vcc_ref    Điện áp tham chiếu VDDA (mặc định 3.30V)
 * @param vin_offset Giá trị bù sai số điện áp nguồn (vin_refer)
 * @return Điện áp nguồn DCIN tính bằng Volt (V)
 */
float hal_adc_read_vin(float vcc_ref, float vin_offset);

/**
 * @brief Đọc và tính toán nhiệt độ môi trường từ cảm biến TC1047 (PA4: 10mV/°C, 0.5V tại 0°C)
 * @param vcc_ref Điện áp tham chiếu VDDA (mặc định 3.30V)
 * @return Nhiệt độ môi trường tính bằng độ C (°C)
 */
float hal_adc_read_tc1047(float vcc_ref);

#ifdef __cplusplus
}
#endif

#endif /* HAL_ADC_H */

