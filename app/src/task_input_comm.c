/**
 * @file    task_input_comm.c
 * @brief   Cài đặt 3 Task: Task_InputSleep (20ms), Task_Buzzer (Queue) và Task_DebugUART (100ms)
 * @note    TODO [Dev 3 - Nhánh feature/task-comm]: Chuyển đổi từ InterruptKey.ino, Sleep.ino, Buzzer.ino
 */

#include "task_input_comm.h"
#include "app_ipc.h"
#include "hal_gpio.h"
#include "hal_timer.h"
#include "hal_uart.h"
#include "hal_flash.h"

/* =========================================================================
 * 1. TASK INPUT & SLEEP (CHU KỲ CỐ ĐỊNH 20MS)
 * ========================================================================= */
static void prvInputSleepTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        /* TODO [Dev 3]:
         * 1. Đọc hal_encoder_get_delta() và xử lý xoay ở màn hình chính / 6 Menu con.
         * 2. Quét hal_gpio_read_encoder_sw() bắt 3 sự kiện: Single Click, Double Click, Long Press (800ms).
         * 3. Mỗi 50 chu kỳ (1000ms): Kiểm tra hal_gpio_read_stand_vb() (PA1) và hal_gpio_read_sleep_sensor() (PA0)
         *    để quản lý chế độ Ngủ (hạ nhiệt về sleep_temp, tắt sau 1800s, bật Screensaver sau 180s Stop).
         */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK_INPUT_SLEEP_PERIOD_MS));
    }
}

/* =========================================================================
 * 2. TASK BUZZER (CHỜ SỰ KIỆN TỪ xBuzzerQueue)
 * ========================================================================= */
static void prvBuzzerTask(void *pvParameters)
{
    (void)pvParameters;
    BuzzerMsg_t msg;

    for (;;)
    {
        if (xQueueReceive(xBuzzerQueue, &msg, pdMS_TO_TICKS(1000U)) == pdTRUE)
        {
            uint32_t duration_ms = ((uint32_t)2U * msg.half_period_us * msg.cycle_count) / 1000U;
            if (duration_ms == 0U)
            {
                duration_ms = 1U;
            }
            hal_buzzer_tone_start((uint16_t)(2U * msg.half_period_us));
            vTaskDelay(pdMS_TO_TICKS(duration_ms));
            hal_buzzer_tone_stop();
        }
        else
        {
            /* TODO [Dev 3]: Nếu đang có lỗi vin_error hoặc t12_adc_error thì phát tiếng bíp cảnh báo định kỳ */
        }
    }
}

/* =========================================================================
 * 3. TASK DEBUG UART (CHU KỲ 100MS)
 * ========================================================================= */
static void prvDebugUartTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        /* TODO [Dev 3]: Đọc g_sys_state và in chuỗi Telemetry qua hal_uart_printf() */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK_DEBUG_UART_PERIOD_MS));
    }
}

/* =========================================================================
 * 4. HÀM KHỞI TẠO CÁC TASK CỦA DEV 3
 * ========================================================================= */
BaseType_t task_input_sleep_create(void)
{
    return xTaskCreate(prvInputSleepTask,
                       "InputSleep",
                       TASK_INPUT_SLEEP_STACK,
                       NULL,
                       TASK_INPUT_SLEEP_PRIO,
                       &xTaskInputSleepHandle);
}

BaseType_t task_buzzer_create(void)
{
    return xTaskCreate(prvBuzzerTask,
                       "Buzzer",
                       TASK_BUZZER_STACK,
                       NULL,
                       TASK_BUZZER_PRIO,
                       &xTaskBuzzerHandle);
}

BaseType_t task_debug_uart_create(void)
{
    return xTaskCreate(prvDebugUartTask,
                       "DebugUART",
                       TASK_DEBUG_UART_STACK,
                       NULL,
                       TASK_DEBUG_UART_PRIO,
                       &xTaskDebugUartHandle);
}

