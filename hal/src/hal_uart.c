/**
 * @file    hal_uart.c
 * @brief   Driver USART1 (PA9/PA10) Thread-Safe dùng xUartTxMutex
 * @note    TODO [Dev 3 - Nhánh feature/hal-gpio-uart]: Cài đặt chi tiết bằng stm32f10x_usart.c
 */

#include "hal_uart.h"
#include "bsp_board.h"
#include <stdarg.h>

void hal_uart_init(uint32_t baudrate)
{
    /* TODO [Dev 3]: Cấu hình PA9 (AF_PP), PA10 (IN_FLOATING), USART1 (115200, 8-N-1) và tạo xUartTxMutex */
    (void)baudrate;
}

void hal_uart_printf(const char *fmt, ...)
{
    /* TODO [Dev 3]: Khóa xUartTxMutex (timeout 50ms), dùng vsnprintf và truyền qua USART_SendData(USART1, ...) */
    (void)fmt;
}

