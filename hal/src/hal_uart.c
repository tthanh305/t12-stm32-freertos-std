/**
 * @file    hal_uart.c
 * @brief   Driver USART1 (PA9/PA10) Thread-Safe dùng xUartTxMutex
 * @note    TODO [Dev 3 - Nhánh feature/hal-gpio-uart]: Cài đặt chi tiết bằng stm32f10x_usart.c
 */

#include "hal_uart.h"
#include "bsp_board.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
static SemaphoreHandle_t xUartTxMutex = NULL;

void hal_uart_init(uint32_t baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // Bat clock cho GPIOA va USART1
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1,
        ENABLE
    );

    // Cau hinh PA9 - TX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // Cau hinh PA10 - RX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // Cau hinh USART1
    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl =
        USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);

    // Tao Mutex bao ve UART
    if (xUartTxMutex == NULL)
    {
        xUartTxMutex = xSemaphoreCreateMutex();
    }
    configASSERT(xUartTxMutex != NULL);
}

void hal_uart_printf(const char *fmt, ...)
{
    // Khai bao bo dem va cac bien can thiet
    char buffer[192];
    va_list args;
    int len, i;
    uint32_t timeout;

    // Kiem tra tham so, Mutex va trang thai FreeRTOS
    if (!fmt || !xUartTxMutex ||
        xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
        return;

    // Khoa Mutex, cho toi da 50ms
    if (xSemaphoreTake(xUartTxMutex, pdMS_TO_TICKS(50)) != pdTRUE)
        return;
		
    // Dinh dang du lieu thanh chuoi ky tu
    va_start(args, fmt);
    len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // Gioi han so ky tu theo kich thuoc bo dem
    if (len > (int)sizeof(buffer) - 1)
        len = sizeof(buffer) - 1;

    // Gui lan luot tung ky tu qua USART1
    for (i = 0; i < len; i++)
    {
        // Khoi tao gioi han so vong lap cho
        timeout = 100000UL;

        // Cho thanh ghi truyen TXE san sang
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE)
               == RESET)
        {
            if (--timeout == 0U)
                break;
        }
        // Dung truyen neu het timeout
        if (timeout == 0U)
            break;
				
        // Gui mot ky tu qua USART1
        USART_SendData(USART1, (uint8_t)buffer[i]);
    }
    // Giai phong Mutex cho cac Task khac
    xSemaphoreGive(xUartTxMutex);
}

