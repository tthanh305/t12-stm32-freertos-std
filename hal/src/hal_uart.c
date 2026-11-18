/**
 * @file    hal_uart.c
 * @brief   Driver USART1 (PA9/PA10) Thread-Safe dùng xUartTxMutex
 *          sử dụng Thư viện chuẩn STM32F10x Standard Peripheral Library (STD/SPL v3.5.0)
 * @author  Dev 3
 */

#include "hal_uart.h"
#include "bsp_board.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdarg.h>
#include <stdio.h>

#define HAL_UART_TX_TIMEOUT_MAX     ( 100000U )

static SemaphoreHandle_t s_uart_tx_mutex = NULL;

void hal_uart_init(uint32_t baudrate)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    /* 1. Bật xung nhịp cho GPIOA và USART1 */
    RCC_APB2PeriphClockCmd(BSP_UART_GPIO_CLK | RCC_APB2Periph_USART1, ENABLE);

    /* 2. Cấu hình PA9 (USART1_TX) ở chế độ Alternate Function Push-Pull 50MHz */
    GPIO_InitStructure.GPIO_Pin   = BSP_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BSP_UART_GPIO_PORT, &GPIO_InitStructure);

    /* 3. Cấu hình PA10 (USART1_RX) ở chế độ Input Floating */
    GPIO_InitStructure.GPIO_Pin   = BSP_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(BSP_UART_GPIO_PORT, &GPIO_InitStructure);

    /* 4. Cấu hình USART1: 8-N-1, không phần cứng bắt tay */
    USART_DeInit(USART1);
    USART_InitStructure.USART_BaudRate            = baudrate;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);

    USART_Cmd(USART1, ENABLE);

    /* 5. Tạo Mutex bảo vệ luồng truyền UART */
    if (s_uart_tx_mutex == NULL)
    {
        s_uart_tx_mutex = xSemaphoreCreateMutex();
    }
}

void hal_uart_printf(const char *fmt, ...)
{
    char    buf[128];
    va_list args;
    int     len;
    int     i;

    if (s_uart_tx_mutex != NULL)
    {
        if (xSemaphoreTake(s_uart_tx_mutex, pdMS_TO_TICKS(50U)) != pdTRUE)
        {
            return;
        }
    }

    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0)
    {
        if (len >= (int)sizeof(buf))
        {
            len = (int)sizeof(buf) - 1;
        }

        for (i = 0; i < len; i++)
        {
            volatile uint32_t timeout = HAL_UART_TX_TIMEOUT_MAX;

            while ((USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) && (timeout > 0U))
            {
                timeout--;
            }
            if (timeout == 0U)
            {
                break;
            }
            USART_SendData(USART1, (uint16_t)buf[i]);
        }

        {
            volatile uint32_t timeout = HAL_UART_TX_TIMEOUT_MAX;

            while ((USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET) && (timeout > 0U))
            {
                timeout--;
            }
        }
    }

    if (s_uart_tx_mutex != NULL)
    {
        (void)xSemaphoreGive(s_uart_tx_mutex);
    }
}
