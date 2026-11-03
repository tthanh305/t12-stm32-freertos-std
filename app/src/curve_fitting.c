/**
 * @file    curve_fitting.c
 * @brief   Hồi quy đa thức bậc 3 (polyfit) & Khử Gauss tính 4 hệ số nhiệt độ sz_p[0..3]
 * @note    TODO [Dev 1 - Nhánh feature/task-sensor]: Chuyển đổi từ CurveFitting.ino
 */

#include "task_sensor_pid.h"
#include "app_cfg.h"

void app_curve_recalculate(const uint16_t sz_temp[4], float sz_p_out[4])
{
    /* TODO [Dev 1]: Cài đặt đầy đủ gauss_solve() và polyfit() từ CurveFitting.ino
     * với 4 mốc chuẩn ADC = {10, 350, 650, 950}.
     * Giá trị khởi tạo tạm thời cho Base Project: */
    (void)sz_temp;
    if (sz_p_out != NULL)
    {
        sz_p_out[0] = 30.0f;
        sz_p_out[1] = 0.48f;
        sz_p_out[2] = 0.0f;
        sz_p_out[3] = 0.0f;
    }
}

