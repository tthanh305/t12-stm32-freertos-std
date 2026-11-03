/**
 * @file    app_ipc.c
 * @brief   Cài đặt các đối tượng IPC (xBuzzerQueue, xSystemStateMutex), trạng thái mặc định
 *          và các hàm Hook an toàn của FreeRTOS
 * @note    Thiết lập bởi Leader (Dev 1).
 */

#include "app_ipc.h"
#include "bsp_board.h"
#include "hal_timer.h"
#include <string.h>

/* =========================================================================
 * 1. ĐỊNH NGHĨA BIẾN TOÀN CỤC & ĐỐI TƯỢNG RTOS IPC
 * ========================================================================= */
QueueHandle_t     xBuzzerQueue          = NULL;
SemaphoreHandle_t xSystemStateMutex     = NULL;

SystemConfig_t    g_sys_cfg;
SystemState_t     g_sys_state;

TaskHandle_t      xTaskSensorPidHandle  = NULL;
TaskHandle_t      xTaskInputSleepHandle = NULL;
TaskHandle_t      xTaskDisplayUiHandle  = NULL;
TaskHandle_t      xTaskBuzzerHandle     = NULL;
TaskHandle_t      xTaskDebugUartHandle  = NULL;

/* =========================================================================
 * 2. CÀI ĐẶT CÁC HÀM IPC DÙNG CHUNG
 * ========================================================================= */

void app_ipc_init(void)
{
    /* 1. Tạo hàng đợi chứa tối đa 8 yêu cầu phát tiếng bíp liên tiếp */
    xBuzzerQueue = xQueueCreate(8U, sizeof(BuzzerMsg_t));
    configASSERT(xBuzzerQueue != NULL);

    /* 2. Tạo Mutex bảo vệ cấu trúc trạng thái chung g_sys_state & g_sys_cfg */
    xSystemStateMutex = xSemaphoreCreateMutex();
    configASSERT(xSystemStateMutex != NULL);

    /* 3. Khởi tạo giá trị ban đầu cho cấu trúc trạng thái thời gian thực */
    memset(&g_sys_state, 0, sizeof(SystemState_t));
    g_sys_state.display_count      = UI_SCREEN_ZJM;
    g_sys_state.t12_temp_read_time = 450U;
    g_sys_state.xp_x               = 50;
    g_sys_state.xp_y               = 21;

    /* Tự bật gia nhiệt khi khởi động nếu người dùng cài đặt t12_kj_jr == 1 */
    g_sys_state.t12_switch         = (g_sys_cfg.t12_kj_jr != 0U) ? true : false;
}

void app_buzzer_send(uint16_t half_period_us, uint16_t cycle_count)
{
    if (xBuzzerQueue != NULL)
    {
        BuzzerMsg_t msg;
        msg.half_period_us = half_period_us;
        msg.cycle_count    = cycle_count;
        /* Gửi không chờ (timeout = 0) để không bao giờ làm khựng Task gọi */
        (void)xQueueSend(xBuzzerQueue, &msg, 0U);
    }
}

bool app_state_lock(uint32_t timeout_ms)
{
    if (xSystemStateMutex == NULL)
    {
        return false;
    }
    TickType_t ticks = (timeout_ms == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return (xSemaphoreTake(xSystemStateMutex, ticks) == pdTRUE);
}

void app_state_unlock(void)
{
    if (xSystemStateMutex != NULL)
    {
        (void)xSemaphoreGive(xSystemStateMutex);
    }
}

void app_update_set_temp_limits(void)
{
    /* Chuyển đổi từ hàm auto_get_set_temp() trong EEPROM.ino:
     * Tính nhiệt độ tại x = 10 (min) và x = 950 (max) theo đa thức bậc 3 */
    float x_min = (float)T12_CALIB_ADC0;
    float x_max = (float)T12_CALIB_ADC3;
    float cold_offset = g_sys_state.tc1047_temp - (float)g_sys_cfg.tc1047_refer;

    float t_min = g_sys_state.sz_p[3] * (x_min * x_min * x_min)
                + g_sys_state.sz_p[2] * (x_min * x_min)
                + g_sys_state.sz_p[1] * x_min
                + g_sys_state.sz_p[0] + cold_offset;

    float t_max = g_sys_state.sz_p[3] * (x_max * x_max * x_max)
                + g_sys_state.sz_p[2] * (x_max * x_max)
                + g_sys_state.sz_p[1] * x_max
                + g_sys_state.sz_p[0] + cold_offset;

    /* Làm tròn bội số của 10°C giống bản gốc */
    g_sys_state.set_temp_min = ((int16_t)t_min / 10 + 1) * 10;
    g_sys_state.set_temp_max = ((int16_t)t_max / 10) * 10;

    if (g_sys_cfg.set_temp > (uint16_t)g_sys_state.set_temp_max)
    {
        g_sys_cfg.set_temp = (uint16_t)g_sys_state.set_temp_max;
    }
    if (g_sys_cfg.set_temp < (uint16_t)g_sys_state.set_temp_min)
    {
        g_sys_cfg.set_temp = (uint16_t)g_sys_state.set_temp_min;
    }
}

/* =========================================================================
 * 3. FREERTOS SAFETY HOOKS (BẢO VỆ AN TOÀN CHÁY NỔ KHI LỖI PHẦN MỀM)
 * ========================================================================= */

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    /* Ngắt ngay lập tức xung PWM gia nhiệt MOSFET AO4407A */
    TIM3->CCR1 = 0;
    GPIO_ResetBits(BSP_HEATER_PWM_GPIO_PORT, BSP_HEATER_PWM_GPIO_PIN);

    taskDISABLE_INTERRUPTS();
    for (;;);
}

void vApplicationMallocFailedHook(void)
{
    /* Ngắt ngay lập tức xung PWM gia nhiệt MOSFET AO4407A */
    TIM3->CCR1 = 0;
    GPIO_ResetBits(BSP_HEATER_PWM_GPIO_PORT, BSP_HEATER_PWM_GPIO_PIN);

    taskDISABLE_INTERRUPTS();
    for (;;);
}

