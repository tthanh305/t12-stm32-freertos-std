/**
 * @file    ui_main.c
 * @brief   Cài đặt giao diện Màn hình khởi động, Màn hình chính (ZJM), Screensaver (XPJM) và Reset
 * @note    TODO [Dev 2 - Nhánh feature/task-display]: Chuyển đổi từ Display.ino & DisplayMain.ino
 */

#include "task_display.h"
#include "app_ipc.h"

void ui_boot_screen(void)
{
    /* TODO [Dev 2]: Chuyển đổi hàm kj_display() */
}

void ui_draw_main_screen(void)
{
    /* TODO [Dev 2]: Chuyển đổi hàm zjm() và các hàm con vẽ nhiệt độ, thanh trượt, nút Heat/Stop, cột PWM */
}

void ui_draw_screensaver(void)
{
    /* TODO [Dev 2]: Chuyển đổi hàm xpjm() (bầu trời 25 ngôi sao và chữ SLEEP Zzz...) */
}

void ui_draw_factory_reset(void)
{
    /* TODO [Dev 2]: Chuyển đổi hàm display_hfccsz() và gọi NVIC_SystemReset() */
}

