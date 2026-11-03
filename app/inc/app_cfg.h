/**
 * @file    app_cfg.h
 * @brief   Bảng thống nhất thông số FreeRTOS Tasks (Priority, Stack Size, Chu kỳ) và Hằng số Hệ thống
 * @note    Định nghĩa bởi Leader (Dev 1) ở Giai đoạn 1 - Bước 2.
 */

#ifndef APP_CFG_H
#define APP_CFG_H

#include "FreeRTOS.h"
#include "task.h"

/* =========================================================================
 * 1. ĐỘ ƯU TIÊN CỦA CÁC TASK (PRIORITY: 1..4)
 * ========================================================================= */
#define TASK_SENSOR_PID_PRIO        ( tskIDLE_PRIORITY + 4 ) /* Cao nhất: Đọc ADC & Điều khiển PID (Dev 1) */
#define TASK_INPUT_SLEEP_PRIO       ( tskIDLE_PRIORITY + 3 ) /* Quét Encoder, nút nhấn, logic ngủ (Dev 3) */
#define TASK_BUZZER_PRIO            ( tskIDLE_PRIORITY + 2 ) /* Phát âm báo PWM qua Queue (Dev 3) */
#define TASK_DISPLAY_UI_PRIO        ( tskIDLE_PRIORITY + 2 ) /* Vẽ màn hình OLED U8g2 33FPS (Dev 2) */
#define TASK_DEBUG_UART_PRIO        ( tskIDLE_PRIORITY + 1 ) /* Thấp nhất: Xuất log UART Debug (Dev 3) */

/* =========================================================================
 * 2. DUNG LƯỢNG STACK CỦA CÁC TASK (ĐƠN VỊ: WORDS, 1 WORD = 4 BYTES)
 * ========================================================================= */
#define TASK_SENSOR_PID_STACK       ( 256U ) /* 1024 Bytes */
#define TASK_INPUT_SLEEP_STACK      ( 256U ) /* 1024 Bytes */
#define TASK_DISPLAY_UI_STACK       ( 512U ) /* 2048 Bytes - Cần lớn cho U8g2 Full-Buffer */
#define TASK_BUZZER_STACK           ( 128U ) /* 512 Bytes */
#define TASK_DEBUG_UART_STACK       ( 256U ) /* 1024 Bytes - Cần cho vsnprintf */

/* =========================================================================
 * 3. CHU KỲ THỰC THI CÁC TASK ĐỊNH KỲ (ĐƠN VỊ: MS)
 * ========================================================================= */
#define TASK_INPUT_SLEEP_PERIOD_MS  ( 20U )  /* Quét phím & Encoder mỗi 20ms (50Hz) */
#define TASK_DISPLAY_UI_PERIOD_MS   ( 30U )  /* Làm tươi màn hình OLED mỗi 30ms (~33 FPS) */
#define TASK_DEBUG_UART_PERIOD_MS   ( 100U ) /* Truyền Telemetry lên PC mỗi 100ms (10Hz) */

/* =========================================================================
 * 4. ĐỊNH DANH CÁC MÀN HÌNH GIAO DIỆN (TƯƠNG ĐƯƠNG BẢN GỐC v1.20)
 * ========================================================================= */
#define UI_SCREEN_ZJM               ( 1U )   /* Màn hình chính (Main Screen) */
#define UI_SCREEN_SZJM              ( 2U )   /* Menu cài đặt chính (Settings Menu) */
#define UI_SCREEN_SZJM_PID          ( 3U )   /* Menu con 1: Chỉnh tham số P, I, D */
#define UI_SCREEN_SZJM_SLEEP        ( 4U )   /* Menu con 2: Thời gian ngủ & Nhiệt độ ngủ */
#define UI_SCREEN_SZJM_OLED         ( 5U )   /* Menu con 3: Độ sáng, Xoay OLED, Chiều Encoder */
#define UI_SCREEN_SZJM_POWER        ( 6U )   /* Menu con 4: Điện áp tham chiếu, Bù áp, Báo áp thấp */
#define UI_SCREEN_SZJM_JIAOZHUN     ( 7U )   /* Menu con 5: Hiệu chuẩn 4 điểm nhiệt độ (Calib) */
#define UI_SCREEN_SZJM_LAOTIE       ( 8U )   /* Menu con 6: Bù đầu lạnh, Tự nung khi bật, Reset */

/* Alias tương thích trực tiếp với tên gốc trong DisplayMain.ino / DisplaySet.ino / InterruptKey.ino */
#define ZJM                         UI_SCREEN_ZJM
#define SZJM                        UI_SCREEN_SZJM
#define SZJM_PID                    UI_SCREEN_SZJM_PID
#define SZJM_SLEEP                  UI_SCREEN_SZJM_SLEEP
#define SZJM_OLED                   UI_SCREEN_SZJM_OLED
#define SZJM_POWER                  UI_SCREEN_SZJM_POWER
#define SZJM_JIAOZHUN               UI_SCREEN_SZJM_JIAOZHUN
#define SZJM_LAOTIE                 UI_SCREEN_SZJM_LAOTIE

/* =========================================================================
 * 5. HẰNG SỐ PHẦN CỨNG & THUẬT TOÁN CHUẨN T12
 * ========================================================================= */
#define T12_NAMETAG                 "NHOM 2 - T12"
#define T12_FLASH_PAGE63_ADDR       ( 0x0800FC00UL ) /* Page cuối của Flash 64KB */
#define T12_CONFIG_MAGIC_KEY        ( 0xA55AU )      /* Cờ nhận biết Flash đã khởi tạo */

/* 4 mốc ADC chuẩn phục vụ hiệu chuẩn đa thức bậc 3 (Thang 10-bit 0..1023) */
#define T12_CALIB_ADC0              ( 10U )
#define T12_CALIB_ADC1              ( 350U )
#define T12_CALIB_ADC2              ( 650U )
#define T12_CALIB_ADC3              ( 950U )

/* Tham số PID cố định khi đang ở màn hình Hiệu chuẩn (Calib) */
#define T12_PID_P_CALIB             ( 12.0f )
#define T12_PID_I_CALIB             ( 2.2f )
#define T12_PID_D_CALIB             ( 1.2f )

/* Giới hạn PWM (TIM3_CH1) */
#define T12_PWM_MAX                 ( 2047 )
#define T12_PWM_MIN                 ( 0 )

#endif /* APP_CFG_H */

