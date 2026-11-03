/**
 * @file    stm32f10x_conf.h
 * @brief   Cấu hình các module sử dụng của thư viện chuẩn STM32F10x Standard Peripheral Library (STD/SPL v3.5.0)
 * @note    File nền tảng do Leader (Dev 1) quản lý độc quyền.
 */

#ifndef __STM32F10x_CONF_H
#define __STM32F10x_CONF_H

/* Bật các module SPL cần thiết cho dự án Trạm Hàn T12 */
#include "stm32f10x_adc.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_flash.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_iwdg.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_usart.h"
#include "misc.h" /* Cấu hình NVIC và SysTick thuộc CMSIS/SPL */

#ifdef  USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t* file, uint32_t line);
#else
  #define assert_param(expr) ((void)0)
#endif /* USE_FULL_ASSERT */

#endif /* __STM32F10x_CONF_H */

