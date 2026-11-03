/**
 * @file    task_display.c
 * @brief   Task hiển thị màn hình OLED SSD1306 128x32 (Chu kỳ cố định 30ms ~ 33 FPS)
 * @note    TODO [Dev 2 - Nhánh feature/task-display]: Cài đặt vòng lặp U8g2 Full-Buffer
 */

#include "task_display.h"
#include "app_ipc.h"
#include "hal_i2c.h"

static void prvDisplayTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    /* TODO [Dev 2]: Khởi tạo u8g2_Setup_ssd1306_i2c_128x32_univision_f() và gọi ui_boot_screen() */

    for (;;)
    {
        /* TODO [Dev 2]:
         * - Kiểm tra g_sys_state.hfccsz_state -> ui_draw_factory_reset()
         * - Kiểm tra g_sys_state.xp_state -> ui_draw_screensaver()
         * - Chuyển mạch theo g_sys_state.display_count (ZJM, SZJM, 6 Submenus)
         * - Gọi u8g2_ClearBuffer(&u8g2) -> vẽ -> u8g2_SendBuffer(&u8g2)
         */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK_DISPLAY_UI_PERIOD_MS));
    }
}

BaseType_t task_display_create(void)
{
    return xTaskCreate(prvDisplayTask,
                       "DisplayUI",
                       TASK_DISPLAY_UI_STACK,
                       NULL,
                       TASK_DISPLAY_UI_PRIO,
                       &xTaskDisplayUiHandle);
}

