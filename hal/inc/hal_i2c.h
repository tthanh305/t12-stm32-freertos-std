/**
 * @file    hal_i2c.h
 * @brief   Giao diện Driver I2C1 Remap (PB8: SCL, PB9: SDA @ 400kHz) và Callback cho thư viện U8g2
 * @note    Phụ trách cài đặt (.c): Dev 2 trên nhánh feature/hal-i2c-spi
 */

#ifndef HAL_I2C_H
#define HAL_I2C_H

#include "stm32f10x.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Khai báo trước cấu trúc u8x8_t để không bắt buộc mọi file phải include u8g2.h */
struct u8x8_struct;
typedef struct u8x8_struct u8x8_t;

/**
 * @brief Khởi tạo ngoại vi I2C1 ở chế độ Remap (PB8=SCL, PB9=SDA), tốc độ Fast Mode 400kHz
 *        sử dụng thư viện chuẩn STD/SPL (stm32f10x_i2c.c).
 */
void hal_i2c_init(void);

/**
 * @brief Hàm truyền nhận byte I2C phần cứng theo chuẩn callback của thư viện U8g2
 */
uint8_t u8x8_byte_stm32_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

/**
 * @brief Hàm xử lý GPIO và Delay (dùng vTaskDelay khi RTOS đang chạy) theo chuẩn callback U8g2
 */
uint8_t u8x8_gpio_and_delay_stm32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

#ifdef __cplusplus
}
#endif

#endif /* HAL_I2C_H */

