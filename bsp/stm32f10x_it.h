/**
 * @file    stm32f10x_it.h
 * @brief   Khai báo các trình phục vụ ngắt ngoại lệ Cortex-M3 và ngoại vi STM32F10x (STD/SPL)
 */

#ifndef __STM32F10x_IT_H
#define __STM32F10x_IT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f10x.h"

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void DebugMon_Handler(void);

/* Lưu ý: SVC_Handler, PendSV_Handler và SysTick_Handler được ánh xạ trực tiếp
 * sang hàm của FreeRTOS Kernel trong FreeRTOSConfig.h */

#ifdef __cplusplus
}
#endif

#endif /* __STM32F10x_IT_H */

