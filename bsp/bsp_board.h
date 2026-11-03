/**
 * @file    bsp_board.h
 * @brief   Định nghĩa Pinout toàn hệ thống (LQFP48) và khởi tạo BSP nền tảng STM32F103C8T6 (STD/SPL)
 * @note    File nền tảng do Leader (Dev 1) thiết lập. Tuyệt đối không tự ý thay đổi chân.
 */

#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "stm32f10x.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * 1. ĐỊNH NGHĨA CHÂN PHẦN CỨNG (PINOUT STM32F103C8T6 LQFP48)
 * ========================================================================= */

/* --- CẠNH 1 (Chân 10..17): Cụm Cảm biến Analog, Công suất & Còi --- */
/* PA0 (Pin 10): Cảm biến rung trong tay hàn (SLEEP) - Input Pull-Up */
#define BSP_SLEEP_GPIO_PORT         GPIOA
#define BSP_SLEEP_GPIO_CLK          RCC_APB2Periph_GPIOA
#define BSP_SLEEP_GPIO_PIN          GPIO_Pin_0

/* PA1 (Pin 11): Chân kích ngủ tức thời khi đặt lên đế gác (VB) - Input Pull-Up */
#define BSP_STAND_VB_GPIO_PORT      GPIOA
#define BSP_STAND_VB_GPIO_CLK       RCC_APB2Periph_GPIOA
#define BSP_STAND_VB_GPIO_PIN       GPIO_Pin_1

/* PA2 (Pin 12): Đọc điện áp nhiệt độ mũi hàn từ Op-Amp GS8332 (ADC1_IN2) */
#define BSP_T12_TEMP_GPIO_PORT      GPIOA
#define BSP_T12_TEMP_GPIO_PIN       GPIO_Pin_2
#define BSP_T12_TEMP_ADC_CHANNEL    ADC_Channel_2

/* PA3 (Pin 13): Đo điện áp nguồn 20V qua cầu phân áp 100K/10K (ADC1_IN3) */
#define BSP_VOL_RE_GPIO_PORT        GPIOA
#define BSP_VOL_RE_GPIO_PIN         GPIO_Pin_3
#define BSP_VOL_RE_ADC_CHANNEL      ADC_Channel_3

/* PA4 (Pin 14): Cảm biến nhiệt độ môi trường bù đầu lạnh TC1047 (ADC1_IN4) */
#define BSP_TC1047_GPIO_PORT        GPIOA
#define BSP_TC1047_GPIO_PIN         GPIO_Pin_4
#define BSP_TC1047_ADC_CHANNEL      ADC_Channel_4

/* PA6 (Pin 16): Xuất xung PWM gia nhiệt kích Q1 C1815 & MOSFET AO4407A (TIM3_CH1) */
#define BSP_HEATER_PWM_GPIO_PORT    GPIOA
#define BSP_HEATER_PWM_GPIO_CLK     RCC_APB2Periph_GPIOA
#define BSP_HEATER_PWM_GPIO_PIN     GPIO_Pin_6

/* PA7 (Pin 17): Xuất xung PWM cho còi thụ động BUZZ1 (TIM3_CH2) */
#define BSP_BUZZER_PWM_GPIO_PORT    GPIOA
#define BSP_BUZZER_PWM_GPIO_CLK     RCC_APB2Periph_GPIOA
#define BSP_BUZZER_PWM_GPIO_PIN     GPIO_Pin_7

/* --- CẠNH 3 (Chân 30..37): Cụm UART Debug & Nạp SWD --- */
/* PA9 (Pin 30) & PA10 (Pin 31): Cổng truyền thông Debug USART1 */
#define BSP_UART_GPIO_PORT          GPIOA
#define BSP_UART_GPIO_CLK           RCC_APB2Periph_GPIOA
#define BSP_UART_TX_PIN             GPIO_Pin_9
#define BSP_UART_RX_PIN             GPIO_Pin_10

/* --- CẠNH 4 (Chân 41..46): Cụm Mặt Trước (Rotary Encoder EC11 & OLED I2C1) --- */
/* PB5 (Pin 41): Nút nhấn của Rotary Encoder EC11 (EC_SW) - Input Pull-Up */
#define BSP_EC_SW_GPIO_PORT         GPIOB
#define BSP_EC_SW_GPIO_CLK          RCC_APB2Periph_GPIOB
#define BSP_EC_SW_GPIO_PIN          GPIO_Pin_5

/* PB6 (Pin 42) & PB7 (Pin 43): Pha A & Pha B của Encoder EC11 (TIM4_CH1 / TIM4_CH2) */
#define BSP_EC_AB_GPIO_PORT         GPIOB
#define BSP_EC_AB_GPIO_CLK          RCC_APB2Periph_GPIOB
#define BSP_EC_CLK_PIN              GPIO_Pin_6
#define BSP_EC_DT_PIN               GPIO_Pin_7

/* PB8 (Pin 45) & PB9 (Pin 46): Màn hình OLED SSD1306 128x32 (I2C1 Remap) */
#define BSP_OLED_I2C_GPIO_PORT      GPIOB
#define BSP_OLED_I2C_GPIO_CLK       RCC_APB2Periph_GPIOB
#define BSP_OLED_SCL_PIN            GPIO_Pin_8
#define BSP_OLED_SDA_PIN            GPIO_Pin_9

/* =========================================================================
 * 2. NGUYÊN MẪU HÀM KHỞI TẠO HỆ THỐNG
 * ========================================================================= */

/**
 * @brief Khởi tạo xung nhịp hệ thống 72MHz, ADCCLK 12MHz, NVIC_PriorityGroup_4,
 *        cấu hình AFIO tắt JTAG (chỉ giữ SWD PA13/PA14) và khởi tạo IWDG.
 */
void bsp_system_init(void);

/**
 * @brief Khởi tạo Independent Watchdog (IWDG) chống treo hệ thống (~2 giây)
 */
void bsp_watchdog_init(void);

/**
 * @brief Làm tươi (Feed) Watchdog định kỳ trong Task điều khiển
 */
void bsp_watchdog_feed(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BOARD_H */

