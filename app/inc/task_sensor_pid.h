/**
 * @file    task_sensor_pid.h
 * @brief   Giao diện Task đọc cảm biến nhiệt độ T12, nội suy đa thức bậc 3 và điều khiển PID
 * @note    Phụ trách cài đặt (.c): Dev 1 (Leader) trên nhánh feature/task-sensor
 */

#ifndef TASK_SENSOR_PID_H
#define TASK_SENSOR_PID_H

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo Task_ControlPID (Priority: tskIDLE_PRIORITY + 4, Stack: 256 words)
 * @return pdPASS nếu tạo Task thành công
 */
BaseType_t task_sensor_pid_create(void);

/**
 * @brief Giải hệ phương trình Khử Gauss & hồi quy đa thức bậc 3 (polyfit) để tính 4 hệ số sz_p[0..3]
 *        từ 4 điểm chuẩn ADC = {10, 350, 650, 950} và mảng nhiệt độ chuẩn sz_temp[4]
 *        (Chuyển đổi từ hàm qxnh_run() / polyfit() trong CurveFitting.ino).
 * @param sz_temp  Mảng 4 điểm nhiệt độ hiệu chuẩn (°C)
 * @param sz_p_out Mảng xuất 4 hệ số đa thức bậc 3 (P0, P1, P2, P3)
 */
void app_curve_recalculate(const uint16_t sz_temp[4], float sz_p_out[4]);

#ifdef __cplusplus
}
#endif

#endif /* TASK_SENSOR_PID_H */

