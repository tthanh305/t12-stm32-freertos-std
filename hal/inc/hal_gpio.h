/**
 * @file    hal_gpio.h
 * @brief   Giao diện Driver GPIO đọc nút nhấn Encoder (PB5), Cảm biến rung (PA0) và Đế gác (PA1)
 * @note    Phụ trách cài đặt (.c): Dev 3 trên nhánh feature/hal-gpio-uart
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "stm32f10x.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo các chân ngõ vào số ở chế độ Input Pull-Up (GPIO_Mode_IPU):
 *        - PA0: Cảm biến rung trong tay hàn (SLEEP)
 *        - PA1: Chân kích ngủ tức thời trên đế gác (VB)
 *        - PB5: Nút nhấn Rotary Encoder EC11 (EC_SW)
 */
void hal_gpio_init(void);

/**
 * @brief Đọc trạng thái nút nhấn Encoder EC_SW (PB5)
 * @return true nếu nút đang được nhấn (mức tích cực thấp: Bit_RESET), false nếu nhả
 */
bool hal_gpio_read_encoder_sw(void);

/**
 * @brief Đọc trạng thái logic hiện tại của công tắc rung trong tay hàn SLEEP (PA0)
 * @return Trạng thái mức logic trên chân PA0 (dùng để so sánh phát hiện thay đổi trạng thái)
 */
bool hal_gpio_read_sleep_sensor(void);

/**
 * @brief Đọc trạng thái gác tay hàn lên đế VB (PA1)
 * @return true nếu tay hàn đang đặt trên đế gác (PA1 == Bit_RESET), false nếu đã nhấc ra
 */
bool hal_gpio_read_stand_vb(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_H */

