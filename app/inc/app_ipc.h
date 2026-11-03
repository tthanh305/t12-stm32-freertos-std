/**
 * @file    app_ipc.h
 * @brief   Định nghĩa Cấu trúc Dữ liệu Chung, Queue, Mutex và API Liên lạc Liên tiến trình (IPC)
 * @note    Định nghĩa bởi Leader (Dev 1) ở Giai đoạn 1 - Bước 2.
 */

#ifndef APP_IPC_H
#define APP_IPC_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "app_cfg.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * 1. GÓI TIN YÊU CẦU PHÁT ÂM THANH GỬI QUA xBuzzerQueue
 * ========================================================================= */
typedef struct {
    uint16_t half_period_us; /* Nửa chu kỳ xung (us) - quy định tần số âm (tương đương buzzer_time gốc) */
    uint16_t cycle_count;    /* Số chu kỳ xung - quy định độ dài tiếng bíp (tương đương buzzer_count gốc) */
} BuzzerMsg_t;

/* =========================================================================
 * 2. CẤU TRÚC THÔNG SỐ LƯU TRỮ TRONG FLASH (PAGE 63: 0x0800FC00)
 *    Thay thế toàn bộ các biến lưu trong EEPROM của bản Arduino gốc
 * ========================================================================= */
typedef struct {
    uint16_t magic_key;      /* 0xA55A: Đánh dấu đã khởi tạo (thay cho auto_get = 100) */
    uint16_t reserved_pad;   /* Căn chỉnh bộ nhớ 4-byte alignment */
    float    p;              /* Hệ số tỉ lệ PID (mặc định: 25.0f) */
    float    i;              /* Hệ số tích phân PID (mặc định: 8.7f) */
    float    d;              /* Hệ số vi phân PID (mặc định: 2.6f) */
    uint16_t sleep_time;     /* Thời gian chờ vào chế độ ngủ (30..930s, 930 = OFF, mặc định: 150s) */
    uint8_t  sleep_temp;     /* Nhiệt độ ngủ (mặc định: 200°C) */
    uint8_t  oled_ld;        /* Độ sáng màn hình OLED (5..255, mặc định: 50) */
    float    vcc_refer;      /* Điện áp tham chiếu VDDA (mặc định: 3.30f V) */
    float    vin_refer;      /* Bù sai số điện áp nguồn 20V (mặc định: 0.0f V) */
    float    vin_low;        /* Ngưỡng cảnh báo điện áp thấp (mặc định: 3.3f V) */
    int8_t   tc1047_refer;   /* Nhiệt độ bù đầu lạnh TC1047 (mặc định: 30°C) */
    uint8_t  t12_kj_jr;      /* Tự bật gia nhiệt khi khởi động (0: Tắt, 1: Bật) */
    uint8_t  oled_fx;        /* Hướng xoay màn hình OLED (0: 0°, 1: 180°) */
    uint8_t  xzbmq_fx;       /* Chiều xoay Encoder (0: Thuận tăng, 1: Thuận giảm) */
    uint16_t sz_temp[4];     /* 4 điểm nhiệt độ hiệu chuẩn ứng với ADC {10, 350, 650, 950} (mặc định: {43, 259, 382, 490}) */
    uint16_t set_temp;       /* Nhiệt độ cài đặt mục tiêu (mặc định: 320°C) */
    uint16_t reserved_pad2;  /* Căn chỉnh 4-byte */
    uint32_t crc32;          /* Mã kiểm tra toàn vẹn dữ liệu Flash */
} SystemConfig_t;

/* =========================================================================
 * 3. CẤU TRÚC TRẠNG THÁI THỜI GIAN THỰC CỦA TOÀN HỆ THỐNG
 *    Bảo vệ bởi Mutex xSystemStateMutex khi đọc/ghi giữa các Task
 * ========================================================================= */
typedef struct {
    /* --- Trạng thái vận hành & Cờ điều khiển --- */
    bool     t12_switch;          /* 0: Stop (Ngừng nung), 1: Heat (Đang gia nhiệt) */
    bool     zjm_sleep_ts;        /* 0: Bình thường, 1: Đang ở chế độ ngủ hạ nhiệt (Slp) */
    bool     xp_state;            /* 0: Bình thường, 1: Đang ở màn hình chờ bầu trời sao (SLEEP Zzz...) */
    bool     hfccsz_state;        /* 1: Kích hoạt màn hình Khôi phục cài đặt gốc (Resetting...) */
    bool     oled_cfg_dirty;      /* 1: Báo cho Task_DisplayUI cập nhật lại độ sáng / chiều xoay OLED */

    /* --- Thông số đo lường cảm biến & PID (Dev 1 cập nhật) --- */
    float    t12_ad;              /* Giá trị ADC mũi hàn đã lọc thông thấp (thang 0..1023.0f) */
    int16_t  t12_temp;            /* Nhiệt độ thực tế mũi hàn (°C) */
    int16_t  t12_temp_avg;        /* Nhiệt độ trung bình 8 mẫu (°C) */
    int16_t  t12_temp_display;    /* Nhiệt độ hiển thị đã làm mượt trên màn hình chính */
    int16_t  this_error;          /* Sai số nhiệt độ hiện tại (thisError) */
    int16_t  this_error_avg;      /* Sai số nhiệt độ trung bình (thisError_average) */
    float    vin;                 /* Điện áp nguồn đầu vào DCIN (V) */
    float    tc1047_temp;         /* Nhiệt độ phòng đo từ IC TC1047 (°C) */
    int16_t  pid_out;             /* Độ rộng xung PWM xuất ra MOSFET (0..2047) */
    uint8_t  pid_out_bfb;         /* Phần trăm công suất PWM (0..100%) */
    int16_t  set_temp_min;        /* Giới hạn dưới nhiệt độ cài đặt (tính tự động theo đường cong) */
    int16_t  set_temp_max;        /* Giới hạn trên nhiệt độ cài đặt (tính tự động theo đường cong) */
    float    sz_p[4];             /* 4 hệ số đa thức bậc 3 (P0..P3) đang áp dụng */
    uint32_t t12_temp_read_time;  /* Chu kỳ đọc ADC & tính PID động (150..800ms) */

    /* --- Cờ báo lỗi bảo vệ an toàn --- */
    bool     t12_error;           /* Lỗi chưa cắm tay hàn (NotConnect: tắt nung mà ADC >= 1000) */
    bool     t12_adc_error;       /* Lỗi bão hòa ADC khi đang nung (T12 ADC HIGH) */
    bool     vin_error;           /* Lỗi điện áp nguồn đầu vào thấp (Volt Low) */

    /* --- Bộ đếm thời gian Ngủ & Màn hình chờ (Dev 3 quản lý) --- */
    uint16_t sleep_count;         /* Bộ đếm số giây không chuyển động tay hàn */
    uint16_t sleep_temp_cache;    /* Lưu nhiệt độ set_temp trước khi hạ nhiệt ngủ */
    uint16_t xp_time_count;       /* Bộ đếm số giây ở trạng thái Stop để bật Screensaver */
    int8_t   xp_x;                /* Tọa độ X chữ SLEEP Zzz... trên màn hình chờ */
    int8_t   xp_y;                /* Tọa độ Y chữ SLEEP Zzz... trên màn hình chờ */

    /* --- Biến trạng thái Điều hướng Giao diện & Menu (Dev 2 vẽ, Dev 3 điều khiển) --- */
    uint8_t  display_count;       /* Màn hình hiện tại (UI_SCREEN_ZJM = 1 .. UI_SCREEN_SZJM_LAOTIE = 8) */
    bool     zjm_dh_run;          /* Cờ kích hoạt animation nút bấm Heat/Stop khi nhấn đúp */
    bool     dec_state;           /* Trạng thái chỉnh phần thập phân trong menu (0: Nguyên, 1: Thập phân) */

    /* Menu cuộn ngang SZJM */
    bool     szjm_kxxk_L;         /* Cờ trượt menu sang trái */
    bool     szjm_kxxk_R;         /* Cờ trượt menu sang phải */
    int8_t   szjm_kxxk_count;     /* Chỉ mục mục đang chọn ở Menu chính (0..5) */

    /* Con trỏ khung rỗng (kxxk) và khung đặc (sxxk) của 6 Menu con */
    int8_t   szjm_pid_kxxk_count;
    int8_t   szjm_pid_sxxk_count;

    int8_t   szjm_sleep_kxxk_count;
    int8_t   szjm_sleep_sxxk_count;

    int8_t   szjm_oled_kxxk_count;
    int8_t   szjm_oled_sxxk_count;

    int8_t   szjm_power_kxxk_count;
    int8_t   szjm_power_sxxk_count;

    int8_t   szjm_laotie_kxxk_count;
    int8_t   szjm_laotie_sxxk_count;

    int8_t   szjm_jiaozhun_kxxk_count;
    int8_t   szjm_jiaozhun_sxxk_count;
} SystemState_t;

/* =========================================================================
 * 4. BIẾN TOÀN CỤC & ĐỐI TƯỢNG RTOS IPC
 * ========================================================================= */
extern QueueHandle_t     xBuzzerQueue;
extern SemaphoreHandle_t xSystemStateMutex;

extern SystemConfig_t    g_sys_cfg;
extern SystemState_t     g_sys_state;

/* Task Handles phục vụ đo kiểm Stack High Water Mark ở Giai đoạn 3 */
extern TaskHandle_t      xTaskSensorPidHandle;
extern TaskHandle_t      xTaskInputSleepHandle;
extern TaskHandle_t      xTaskDisplayUiHandle;
extern TaskHandle_t      xTaskBuzzerHandle;
extern TaskHandle_t      xTaskDebugUartHandle;

/* =========================================================================
 * 5. API KHỞI TẠO & TIỆN ÍCH IPC DÙNG CHUNG
 * ========================================================================= */

/**
 * @brief Khởi tạo xBuzzerQueue, xSystemStateMutex và đặt giá trị mặc định cho g_sys_state
 */
void app_ipc_init(void);

/**
 * @brief Gửi yêu cầu phát tiếng bíp vào xBuzzerQueue (Non-blocking, an toàn từ mọi Task)
 * @param half_period_us Nửa chu kỳ xung tính bằng micro-giây (tương đương tham số t1 của hàm buzzer cũ)
 * @param cycle_count    Số chu kỳ lặp lại (tương đương tham số t2 của hàm buzzer cũ)
 */
void app_buzzer_send(uint16_t half_period_us, uint16_t cycle_count);

/**
 * @brief Khóa quyền truy cập cấu trúc trạng thái chung g_sys_state / g_sys_cfg
 * @param timeout_ms Thời gian chờ tối đa (ms), truyền portMAX_DELAY nếu chờ vô hạn
 * @return true nếu chiếm được Mutex thành công
 */
bool app_state_lock(uint32_t timeout_ms);

/**
 * @brief Nhả quyền truy cập cấu trúc trạng thái chung
 */
void app_state_unlock(void);

/**
 * @brief Tự động tính toán giới hạn nhiệt độ cài đặt (set_temp_min, set_temp_max)
 *        dựa trên các hệ số đường cong sz_p[4] và nhiệt độ đầu lạnh (chuyển từ auto_get_set_temp)
 */
void app_update_set_temp_limits(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_IPC_H */

