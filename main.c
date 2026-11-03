/**
 * @file    main.c
 * @brief   Điểm nhập chính của hệ thống Trạm Hàn T12 (STM32F103C8T6 + Thư viện STD/SPL + FreeRTOS)
 * @note    File nền tảng do Leader (Dev 1) quản lý. Các thành viên không sửa main.c trên nhánh feature.
 */

#include "stm32f10x.h"
#include "bsp_board.h"
#include "hal_adc.h"
#include "hal_timer.h"
#include "hal_i2c.h"
#include "hal_flash.h"
#include "hal_gpio.h"
#include "hal_uart.h"
#include "app_ipc.h"
#include "task_sensor_pid.h"
#include "task_display.h"
#include "task_input_comm.h"

int main(void)
{
    /* 1. Khởi tạo Clock 72MHz, ADCCLK 12MHz, NVIC_PriorityGroup_4, AFIO Remap và tầng Driver STD/SPL */
    bsp_system_init();
    hal_gpio_init();
    hal_uart_init(115200U);
    hal_adc_init();
    hal_timer_init();
    hal_i2c_init();

    /* 2. Đọc cấu hình từ Flash Page 63 (hoặc tự động nạp mặc định nếu lần đầu khởi động) */
    hal_storage_init_or_load(&g_sys_cfg);

    /* 3. Khởi tạo các đối tượng IPC (xBuzzerQueue, xSystemStateMutex) và trạng thái ban đầu */
    app_ipc_init();

    /* 4. Tính toán 4 hệ số đa thức bậc 3 (sz_p[0..3]) và giới hạn nhiệt độ cài đặt */
    app_curve_recalculate(g_sys_cfg.sz_temp, g_sys_state.sz_p);
    app_update_set_temp_limits();

    /* 5. Tạo 5 FreeRTOS Tasks và kiểm tra cấp phát Heap */
    configASSERT(task_sensor_pid_create()  == pdPASS);
    configASSERT(task_input_sleep_create() == pdPASS);
    configASSERT(task_display_create()     == pdPASS);
    configASSERT(task_buzzer_create()      == pdPASS);
    configASSERT(task_debug_uart_create()  == pdPASS);

    /* 6. Khởi tạo Watchdog Timer (IWDG ~2s) và chạy bộ lập lịch FreeRTOS Scheduler */
    bsp_watchdog_init();
    vTaskStartScheduler();

    /* Không bao giờ chạy tới đây nếu đủ Heap */
    for (;;)
    {
    }
}

