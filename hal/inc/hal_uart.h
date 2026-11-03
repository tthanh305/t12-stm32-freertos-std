/**
 * @file    hal_uart.h
 * @brief   Giao diện Driver USART1 (PA9: TX, PA10: RX) Thread-Safe phục vụ xuất Log Debug & Đồ thị PID
 * @note    Phụ trách cài đặt (.c): Dev 3 trên nhánh feature/hal-gpio-uart
 */

#ifndef HAL_UART_H
#define HAL_UART_H

#include "stm32f10x.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo ngoại vi USART1 (PA9/PA10, 8-N-1) bằng thư viện chuẩn STD/SPL
 *        và tạo Mutex nội bộ xUartTxMutex bảo vệ truyền chuỗi đa luồng.
 * @param baudrate Tốc độ Baud (mặc định: 115200)
 */
void hal_uart_init(uint32_t baudrate);

/**
 * @brief Gửi một chuỗi ký tự định dạng (printf-style) qua USART1 an toàn giữa các Task
 *        (Sử dụng xUartTxMutex với timeout 50ms).
 * @param fmt Chuỗi định dạng chuẩn C
 */
void hal_uart_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_H */

