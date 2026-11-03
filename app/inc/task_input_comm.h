/**
 * @file    task_input_comm.h
 * @brief   Giao diện các Task xử lý Input/Sleep (20ms), Buzzer (Queue) và Debug UART (100ms)
 * @note    Phụ trách cài đặt (.c): Dev 3 trên nhánh feature/task-comm
 */

#ifndef TASK_INPUT_COMM_H
#define TASK_INPUT_COMM_H

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo Task_InputSleep (Priority: tskIDLE_PRIORITY + 3, Stack: 256 words, Chu kỳ: 20ms)
 *        Quét xung xoay Encoder, bắt 3 sự kiện nút nhấn (Single/Double/Long) và quản lý bộ đếm Ngủ.
 * @return pdPASS nếu tạo Task thành công
 */
BaseType_t task_input_sleep_create(void);

/**
 * @brief Khởi tạo Task_Buzzer (Priority: tskIDLE_PRIORITY + 2, Stack: 128 words)
 *        Chờ nhận gói tin từ xBuzzerQueue và phát xung PWM còi báo không chặn luồng.
 * @return pdPASS nếu tạo Task thành công
 */
BaseType_t task_buzzer_create(void);

/**
 * @brief Khởi tạo Task_DebugUART (Priority: tskIDLE_PRIORITY + 1, Stack: 256 words, Chu kỳ: 100ms)
 *        Truyền chuỗi log thông số hoạt động và đồ thị PID qua cổng USART1.
 * @return pdPASS nếu tạo Task thành công
 */
BaseType_t task_debug_uart_create(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_INPUT_COMM_H */

