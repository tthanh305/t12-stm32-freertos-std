/**
 * @file    task_display.h
 * @brief   Giao diện Task hiển thị màn hình OLED SSD1306 128x32 (Thư viện U8g2 Full-Buffer)
 * @note    Phụ trách cài đặt (.c): Dev 2 trên nhánh feature/task-display
 */

#ifndef TASK_DISPLAY_H
#define TASK_DISPLAY_H

#include "FreeRTOS.h"
#include "task.h"
#include "u8g2.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo Task_DisplayUI (Priority: tskIDLE_PRIORITY + 2, Stack: 512 words, Chu kỳ: 30ms)
 * @return pdPASS nếu tạo Task thành công
 */
BaseType_t task_display_create(void);

/* Đối tượng U8g2 dùng chung giữa task_display.c, ui_main.c và ui_menu.c */
extern u8g2_t g_u8g2;

/* Tiện ích vẽ số không dùng sprintf, dùng chung cho các màn hình UI */
void ui_draw_int(u8g2_uint_t x, u8g2_uint_t y, int32_t val);
void ui_draw_float(u8g2_uint_t x, u8g2_uint_t y, float val, uint8_t decimals);
int16_t ui_get_trans_y(void);

/* Các hàm vẽ giao diện con (được cài đặt trong ui_main.c và ui_menu.c bởi Dev 2) */
void ui_boot_screen(void);      /* Màn hình khởi động kj_display() */
void ui_draw_main_screen(void); /* Màn hình chính zjm() */
void ui_draw_screensaver(void); /* Màn hình chờ bầu trời sao xpjm() */
void ui_draw_factory_reset(void); /* Màn hình Resetting... display_hfccsz() */
void ui_draw_menu_carousel(void); /* Menu cuộn ngang 6 mục szjm() */
void ui_draw_submenu_pid(void);
void ui_draw_submenu_sleep(void);
void ui_draw_submenu_oled(void);
void ui_draw_submenu_power(void);
void ui_draw_submenu_calib(void);
void ui_draw_submenu_iron(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_DISPLAY_H */

