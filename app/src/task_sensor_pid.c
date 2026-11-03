/**
 * @file    task_sensor_pid.c
 * @brief   Task điều khiển nhiệt độ mũi hàn T12, đọc ADC1+DMA1 và thuật toán PID gia lượng
 * @note    TODO [Dev 1 - Nhánh feature/task-sensor]: Chuyển đổi từ analogRead.ino & PID.ino
 */

#include "task_sensor_pid.h"
#include "app_ipc.h"
#include "bsp_board.h"
#include "hal_adc.h"
#include "hal_timer.h"

static void prvSensorPidTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        /* 1. Làm tươi Watchdog Timer (IWDG) */
        bsp_watchdog_feed();

        /* TODO [Dev 1]:
         * - Ngắt PWM: hal_pwm_set_heater(0);
         * - Chờ ổn định cặp nhiệt điện: vTaskDelay(pdMS_TO_TICKS(2));
         * - Đọc hal_adc_read_t12_scaled(), hal_adc_read_vin(), hal_adc_read_tc1047();
         * - Tính nhiệt độ thực tế theo đa thức bậc 3 sz_p[0..3] + bù đầu lạnh;
         * - Kiểm tra lỗi bảo vệ (NotConnect, T12 ADC HIGH, Volt Low);
         * - Chạy thuật toán PIDRegulator() và xuất hal_pwm_set_heater(pid_out);
         */

        vTaskDelay(pdMS_TO_TICKS(g_sys_state.t12_temp_read_time));
    }
}

BaseType_t task_sensor_pid_create(void)
{
    return xTaskCreate(prvSensorPidTask,
                       "SensorPID",
                       TASK_SENSOR_PID_STACK,
                       NULL,
                       TASK_SENSOR_PID_PRIO,
                       &xTaskSensorPidHandle);
}

