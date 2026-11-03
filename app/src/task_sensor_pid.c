/**
 * @file    task_sensor_pid.c
 * @brief   Task đọc cảm biến nhiệt độ mũi hàn T12 (ADC1+DMA1), bảo vệ an toàn 3 cấp
 *          và điều khiển thuật toán PID gia tăng (Incremental PID)
 * @note    Phụ trách: Dev 1 (Leader) trên nhánh feature/task-sensor
 *          Chuyển đổi chuẩn xác 100% từ analogRead.ino và PID.ino (Arduino v1.20).
 */

#include "task_sensor_pid.h"
#include "app_ipc.h"
#include "bsp_board.h"
#include "hal_adc.h"
#include "hal_timer.h"
#include <math.h>

/* =========================================================================
 * 1. HẰNG SỐ THUẬT TOÁN ĐỌC CẢM BIẾN & ĐIỀU KHIỂN PID (TỪ analogRead.ino & PID.ino)
 * ========================================================================= */
#define SENSOR_AVG_CYCLES           ( 10U )     /* Số mẫu trung bình cho Vin và TC1047 (cycs = 10) */
#define T12_ADC_MAX_THRESHOLD       ( 1000.0f ) /* Ngưỡng phát hiện đứt/chưa cắm tay hàn (ADCMax = 1000) */
#define T12_ADC_ERROR_MAX_COUNT     ( 10U )     /* Số chu kỳ liên tiếp bão hòa ADC để ngắt khẩn cấp */
#define T12_OPAMP_SETTLE_MS         ( 4U )      /* Thời gian chờ Op-Amp LMV321 xả điện sau khi ngắt PWM */
#define TASK_BASE_TICK_MS           ( 50U )     /* Chu kỳ nhịp cơ sở kiểm tra trạng thái & làm tươi IWDG */

/* Hằng số bộ điều khiển PID gia tăng (từ v1.20.ino & PID.ino) */
#define PID_DEADBAND                ( 0.0f )    /* Vùng chết sai số (deadband = 0) */
#define PID_ERROR_ABS_MAX           ( 2000.0f ) /* Sai số tối đa cho hệ số tích phân biến thiên */
#define PID_ERROR_ABS_MIN           ( 50.0f )   /* Sai số tối thiểu cho hệ số tích phân biến thiên */
#define PID_ALPHA                   ( 0.2f )    /* Hệ số lọc vi phân không hoàn toàn (alpha = 0.2) */

/* =========================================================================
 * 2. BIẾN TRẠNG THÁI NỘI BỘ CỦA BỘ ĐỌC CẢM BIẾN & PID
 * ========================================================================= */
static float    s_vin_cache               = 0.0f;
static uint8_t  s_vin_cache_count         = 0U;

static float    s_tc1047_cache            = 0.0f;
static uint8_t  s_tc1047_cache_count      = 0U;

static int32_t  s_t12_temp_avg_cache      = 0;
static uint8_t  s_t12_temp_avg_count      = 0U;
static uint8_t  s_t12_adc_error_count     = 0U;

static int16_t  s_last_error              = 0;      /* lasterror: Sai số ở chu kỳ k-1 */
static int16_t  s_pre_error               = 0;      /* preerror:  Sai số ở chu kỳ k-2 */
static float    s_delta_diff              = 0.0f;   /* deltadiff: Thành phần vi phân không hoàn toàn */

static bool     s_buzzer_temp_avg_state   = false;  /* Cờ báo đã kêu bíp khi mũi hàn đạt nhiệt độ mục tiêu */
static uint16_t s_last_set_temp           = 0U;     /* Theo dõi thay đổi set_temp để reset cờ bíp */
static bool     s_last_t12_switch         = false;  /* Theo dõi cạnh lên/xuống của công tắc gia nhiệt */

/* Biến toàn cục hỗ trợ quan sát nhanh trên Keil C (Watch 1) */
volatile uint32_t g_dbg_loop_cnt          = 0U;
volatile float    g_dbg_t12_adc           = 0.0f;
volatile float    g_dbg_vin               = 0.0f;
volatile float    g_dbg_tc1047            = 0.0f;

/* =========================================================================
 * 3. CÁC HÀM NỘI BỘ (STATIC FUNCTIONS)
 * ========================================================================= */

/**
 * @brief Hệ số giảm chấn thành phần Tích phân (Variable Integral) khi sai số lớn
 *        để chống bão hòa tích phân (Integral Windup) và hạn chế vọt lố nhiệt độ.
 *        (Chuyển đổi từ hàm get_factor() trong PID.ino)
 */
static float prv_get_factor(float ek0, float absmax, float absmin)
{
    float err_abs = fabsf(ek0);

    if (err_abs <= absmin)
    {
        return 1.0f;
    }
    if (err_abs >= absmax)
    {
        return 0.0f;
    }
    if (absmax <= absmin)
    {
        return 0.0f;
    }

    return (absmax - err_abs) / (absmax - absmin);
}

/**
 * @brief Đọc điện áp nguồn DCIN (PA3), lấy trung bình 10 mẫu và kiểm tra bảo vệ áp thấp (Volt Low)
 *        (Chuyển đổi từ hàm vin_read() trong analogRead.ino)
 */
static void prv_vin_read(void)
{
    /* Đọc điện áp nguồn thô chưa cộng bù vin_refer, sau khi đủ 10 mẫu mới cộng vin_refer giống bản gốc */
    s_vin_cache += hal_adc_read_vin(g_sys_cfg.vcc_refer, 0.0f);
    s_vin_cache_count++;

    if (s_vin_cache_count >= SENSOR_AVG_CYCLES)
    {
        g_sys_state.vin   = (s_vin_cache / (float)SENSOR_AVG_CYCLES) + g_sys_cfg.vin_refer;
        g_dbg_vin         = g_sys_state.vin;
        s_vin_cache_count = 0U;
        s_vin_cache       = 0.0f;

        /* Kiểm tra lỗi sụt áp nguồn (Volt Low) */
        if ((g_sys_state.vin < g_sys_cfg.vin_low) && (g_sys_cfg.vin_low >= 3.3f))
        {
            g_sys_state.vin_error  = true;
            g_sys_state.t12_switch = false;
        }
        else
        {
            g_sys_state.vin_error  = false;
        }
    }
}

/**
 * @brief Đọc cảm biến nhiệt độ vỏ máy TC1047 (PA4), lấy trung bình 10 mẫu để bù đầu lạnh
 *        (Chuyển đổi từ hàm tc1047_temp_read() trong analogRead.ino)
 */
static void prv_tc1047_temp_read(void)
{
    s_tc1047_cache += hal_adc_read_tc1047(g_sys_cfg.vcc_refer);
    s_tc1047_cache_count++;

    if (s_tc1047_cache_count >= SENSOR_AVG_CYCLES)
    {
        g_sys_state.tc1047_temp = s_tc1047_cache / (float)SENSOR_AVG_CYCLES;
        g_dbg_tc1047            = g_sys_state.tc1047_temp;
        s_tc1047_cache_count    = 0U;
        s_tc1047_cache          = 0.0f;
    }
}

/**
 * @brief Thuật toán PID gia tăng (Incremental PID) với lọc vi phân không hoàn toàn
 *        và chu kỳ lấy mẫu thích nghi theo sai số nhiệt độ (150ms .. 800ms).
 *        (Chuyển đổi từ hàm PIDRegulator() trong PID.ino)
 * @param pv Giá trị quá trình hiện tại:
 *           - Ở màn hình Hiệu chuẩn (SZJM_JIAOZHUN): truyền vào giá trị ADC thô (t12_ad)
 *           - Ở các màn hình khác: truyền vào nhiệt độ thực tế mũi hàn (t12_temp)
 */
static void prv_pid_regulator(float pv)
{
    float factor    = 0.0f;
    float increment = 0.0f;
    float p_error   = 0.0f;
    float i_error   = 0.0f;
    float d_error   = 0.0f;
    float kp, ki, kd;
    float target_min_ref;

    /* 1. Xác định sai số hiện tại (thisError) và bộ hệ số Kp, Ki, Kd theo chế độ */
    if (g_sys_state.display_count == UI_SCREEN_SZJM_JIAOZHUN)
    {
        /* Chế độ Hiệu chuẩn 4 điểm: Điều khiển bám trực tiếp theo mốc ADC chuẩn {10, 350, 650, 950} */
        kp = T12_PID_P_CALIB;
        ki = T12_PID_I_CALIB;
        kd = T12_PID_D_CALIB;

        if (g_sys_state.szjm_jiaozhun_sxxk_count == 1)
        {
            g_sys_state.this_error = (int16_t)((float)T12_CALIB_ADC0 - pv);
        }
        else if (g_sys_state.szjm_jiaozhun_sxxk_count == 2)
        {
            g_sys_state.this_error = (int16_t)((float)T12_CALIB_ADC1 - pv);
        }
        else if (g_sys_state.szjm_jiaozhun_sxxk_count == 3)
        {
            g_sys_state.this_error = (int16_t)((float)T12_CALIB_ADC2 - pv);
        }
        else if (g_sys_state.szjm_jiaozhun_sxxk_count == 4)
        {
            g_sys_state.this_error = (int16_t)((float)T12_CALIB_ADC3 - pv);
        }
        else
        {
            g_sys_state.this_error = 0;
        }
    }
    else
    {
        /* Chế độ vận hành bình thường: Điều khiển bám theo nhiệt độ cài đặt set_temp (°C) */
        kp = g_sys_cfg.p;
        ki = g_sys_cfg.i;
        kd = g_sys_cfg.d;

        g_sys_state.this_error     = (int16_t)((float)g_sys_cfg.set_temp - pv);
        g_sys_state.this_error_avg = (int16_t)g_sys_cfg.set_temp - g_sys_state.t12_temp_avg;
    }

    /* 2. Điều chỉnh chu kỳ đọc cảm biến & tính PID động (Adaptive Sampling Time) */
    if (g_sys_state.this_error < -5)
    {
        g_sys_state.t12_temp_read_time = 150U;
    }
    else if ((g_sys_state.this_error >= -5) && (g_sys_state.this_error <= 10))
    {
        g_sys_state.t12_temp_read_time = 400U;
    }
    else if ((g_sys_state.this_error > 10) && (g_sys_state.this_error <= 30))
    {
        g_sys_state.t12_temp_read_time = 500U;
    }
    else
    {
        g_sys_state.t12_temp_read_time = 800U;
    }

    /* 3. Phân vùng điều khiển:
     *    - Sai số lớn (thisError > 30°C): Gia nhiệt tối đa 100% công suất (Bang-Bang)
     *    - Vọt lố sâu (thisError <= -15°C): Ngắt hoàn toàn PWM (0%)
     *    - Vùng lân cận (-15°C < thisError <= 30°C): Chạy thuật toán PID gia tăng mịn */
    if ((g_sys_state.this_error <= 30) && (g_sys_state.this_error > -15))
    {
        if (fabsf((float)g_sys_state.this_error) > PID_DEADBAND)
        {
            p_error = (float)(g_sys_state.this_error - s_last_error);
            i_error = (float)(g_sys_state.this_error + s_last_error) / 2.0f;
            d_error = (float)(g_sys_state.this_error - (2 * s_last_error) + s_pre_error);

            factor       = prv_get_factor((float)g_sys_state.this_error, PID_ERROR_ABS_MAX, PID_ERROR_ABS_MIN);
            s_delta_diff = kd * (1.0f - PID_ALPHA) * d_error + PID_ALPHA * s_delta_diff;
            increment    = (kp * p_error) + (ki * factor * i_error) + s_delta_diff;
        }
        else
        {
            target_min_ref = (float)g_sys_cfg.set_temp - (float)T12_PWM_MIN;
            if ((fabsf(target_min_ref) < PID_DEADBAND) &&
                (fabsf(pv - (float)T12_PWM_MIN) < PID_DEADBAND))
            {
                g_sys_state.pid_out = T12_PWM_MIN;
            }
            increment = 0.0f;
        }

        g_sys_state.pid_out = (int16_t)((float)g_sys_state.pid_out + increment);

        /* Bão hòa đầu ra PWM trong dải [0 .. 2047] */
        if (g_sys_state.pid_out >= T12_PWM_MAX)
        {
            if (g_sys_state.this_error <= 0)
            {
                g_sys_state.pid_out = T12_PWM_MIN;
            }
            else
            {
                g_sys_state.pid_out = T12_PWM_MAX;
            }
        }
        else if (g_sys_state.pid_out <= T12_PWM_MIN)
        {
            g_sys_state.pid_out = T12_PWM_MIN;
        }
    }
    else if (g_sys_state.this_error > 30)
    {
        g_sys_state.pid_out = T12_PWM_MAX;
    }
    else
    {
        /* this_error <= -15 */
        g_sys_state.pid_out = T12_PWM_MIN;
    }

    /* 4. Lưu vết lịch sử sai số cho chu kỳ tiếp theo */
    s_pre_error  = s_last_error;
    s_last_error = g_sys_state.this_error;
}

/**
 * @brief Điều phối bộ điều khiển PID và xuất xung PWM ra MOSFET (PA6 - TIM3_CH1)
 *        (Chuyển đổi từ hàm pid() trong PID.ino)
 */
static void prv_pid_execute(void)
{
    /* Nếu người dùng vừa đổi nhiệt độ cài đặt hoặc vừa bật lại gia nhiệt -> cho phép bíp báo đủ nhiệt */
    if ((g_sys_cfg.set_temp != s_last_set_temp) || (!s_last_t12_switch && g_sys_state.t12_switch))
    {
        s_buzzer_temp_avg_state = false;
        s_last_set_temp         = g_sys_cfg.set_temp;
    }
    s_last_t12_switch = g_sys_state.t12_switch;

    if (g_sys_state.t12_switch && (!g_sys_state.vin_error) && (!g_sys_state.t12_error))
    {
        int16_t ek_average = (int16_t)g_sys_cfg.set_temp - g_sys_state.t12_temp_avg;

        /* Phát tiếng bíp báo hiệu mũi hàn đã đạt nhiệt độ mục tiêu (sai số trung bình nằm trong [-3..+3°C]) */
        if ((ek_average >= -3) && (ek_average <= 3) && (!s_buzzer_temp_avg_state))
        {
            app_buzzer_send(120U, 300U);
            s_buzzer_temp_avg_state = true;
        }

        if (g_sys_state.display_count == UI_SCREEN_SZJM_JIAOZHUN)
        {
            prv_pid_regulator(g_sys_state.t12_ad);
        }
        else
        {
            prv_pid_regulator((float)g_sys_state.t12_temp);
        }
    }
    else
    {
        /* Khi tắt gia nhiệt (Stop) hoặc có lỗi an toàn -> Xóa toàn bộ tích lũy PID và ngắt PWM */
        g_sys_state.pid_out            = 0;
        s_last_error                   = 0;
        s_pre_error                    = 0;
        s_delta_diff                   = 0.0f;
        g_sys_state.t12_temp_read_time = 450U;
    }

    /* Xuất xung PWM ra chân PA6 (TIM3_CH1) và tính phần trăm công suất (0..100%) */
    hal_pwm_set_heater((uint16_t)g_sys_state.pid_out);
    g_sys_state.pid_out_bfb = (uint8_t)(((float)g_sys_state.pid_out / (float)(T12_PWM_MAX - T12_PWM_MIN)) * 100.0f);
}

/**
 * @brief Chu trình chuẩn đọc cảm biến mũi hàn T12:
 *        Ngắt PWM -> đọc Vin & TC1047 -> chờ 4ms ổn định Op-Amp -> đọc ADC T12 ->
 *        lọc thông thấp -> nội suy đa thức bậc 3 -> tính PID -> kiểm tra 3 cấp bảo vệ.
 *        (Chuyển đổi từ hàm read_t12_temp() và read_timing2() trong analogRead.ino)
 */
static void prv_read_t12_temp_and_control(void)
{
    float t12_ad_new;
    float ad_val;
    float temp_calc;

    /* 1. BẮT BUỘC: Ngắt xung PWM gia nhiệt về 0 trước khi đo cặp nhiệt điện T12 */
    hal_pwm_set_heater(0U);

    /* 2. Chờ 4ms (Non-blocking) để cuộn dây T12 xả hết áp dư, Op-Amp LMV321 ổn định
     *    và DMA1 điền đầy bộ đệm 16 mẫu ADC mới */
    vTaskDelay(pdMS_TO_TICKS(T12_OPAMP_SETTLE_MS));

    /* 3. Chiếm Mutex cập nhật dữ liệu cảm biến và tính toán PID */
    if (app_state_lock(10U))
    {
        /* 3.1. Đọc điện áp nguồn DCIN, nhiệt độ phòng TC1047 và kiểm tra cờ cập nhật độ sáng OLED */
        prv_vin_read();
        prv_tc1047_temp_read();
        if ((g_sys_state.display_count == UI_SCREEN_SZJM_OLED) &&
            (g_sys_state.szjm_oled_sxxk_count == 1))
        {
            g_sys_state.oled_cfg_dirty = true;
        }

        /* 3.2. Đọc giá trị ADC mũi hàn đã quy đổi về thang 10-bit (0.0 .. 1023.75) và lọc thông thấp IIR */
        t12_ad_new          = hal_adc_read_t12_scaled();
        g_dbg_t12_adc       = t12_ad_new;
        g_sys_state.t12_ad += (t12_ad_new - g_sys_state.t12_ad) * 0.7f;

        /* 3.3. Nội suy nhiệt độ thực tế bằng đa thức bậc 3 + bù nhiệt độ đầu lạnh TC1047 */
        ad_val    = g_sys_state.t12_ad;
        temp_calc = (g_sys_state.sz_p[3] * ad_val * ad_val * ad_val)
                  + (g_sys_state.sz_p[2] * ad_val * ad_val)
                  + (g_sys_state.sz_p[1] * ad_val)
                  +  g_sys_state.sz_p[0]
                  + (g_sys_state.tc1047_temp - (float)g_sys_cfg.tc1047_refer);
        g_sys_state.t12_temp = (int16_t)temp_calc;

        /* 3.4. Tính nhiệt độ trung bình mỗi 8 chu kỳ đo (phục vụ làm mượt hiển thị & kiểm tra đạt nhiệt) */
        s_t12_temp_avg_cache += (int32_t)g_sys_state.t12_temp;
        s_t12_temp_avg_count++;
        if (s_t12_temp_avg_count >= 8U)
        {
            g_sys_state.t12_temp_avg = (int16_t)(s_t12_temp_avg_cache >> 3);
            s_t12_temp_avg_cache     = 0;
            s_t12_temp_avg_count     = 0U;
        }

        /* 3.5. Thực thi bộ điều khiển PID và xuất xung PWM mới */
        prv_pid_execute();

        /* 3.6. Kiểm tra an toàn phần cứng tay hàn (NotConnect & T12 ADC HIGH) */
        if ((!g_sys_state.t12_switch) && (g_sys_state.t12_ad >= T12_ADC_MAX_THRESHOLD))
        {
            /* Đang tắt nung mà ADC vọt lên >= 1000 -> Chưa cắm mũi hàn (NotConnect) */
            g_sys_state.t12_error  = true;
            g_sys_state.t12_switch = false;
            hal_pwm_set_heater(0U);
        }
        else if (g_sys_state.t12_switch && (g_sys_state.t12_ad >= T12_ADC_MAX_THRESHOLD))
        {
            /* Đang nung mà ADC bão hòa >= 1000 liên tiếp 10 lần -> Lỗi mạch đo / tuột tay hàn khi đang nung */
            s_t12_adc_error_count++;
            if (s_t12_adc_error_count >= T12_ADC_ERROR_MAX_COUNT)
            {
                s_t12_adc_error_count     = 0U;
                g_sys_state.t12_adc_error = true;
                g_sys_state.t12_switch    = false;
                g_sys_state.pid_out       = 0;
                g_sys_state.pid_out_bfb   = 0U;
                hal_pwm_set_heater(0U);
            }
        }
        else if (g_sys_state.t12_switch && (g_sys_state.t12_ad < T12_ADC_MAX_THRESHOLD))
        {
            g_sys_state.t12_adc_error = false;
            s_t12_adc_error_count     = 0U;
        }
        else
        {
            g_sys_state.t12_error     = false;
        }

        app_state_unlock();
    }
}

/* =========================================================================
 * 4. TIẾN TRÌNH CHÍNH TASK_SENSOR_PID (PRIORITY: tskIDLE_PRIORITY + 4)
 * ========================================================================= */

static void prvSensorPidTask(void *pvParameters)
{
    uint8_t    i;
    TickType_t xLastReadTick;

    (void)pvParameters;

    /* 1. Khởi động: Đọc 10 mẫu đầu tiên giống hệt hàm setup() trong v1.20.ino
     *    để hội tụ ngay giá trị vin, tc1047_temp, t12_temp và giới hạn set_temp_min/max */
    for (i = 0U; i < SENSOR_AVG_CYCLES; i++)
    {
        prv_read_t12_temp_and_control();
        bsp_watchdog_feed();
    }

    if (app_state_lock(20U))
    {
        g_sys_state.t12_temp_display = g_sys_state.t12_temp;
        app_update_set_temp_limits();
        app_state_unlock();
    }

    xLastReadTick = xTaskGetTickCount();

    /* 2. Vòng lặp thời gian thực của Task_Sensor_PID */
    for (;;)
    {
        TickType_t xNowTick     = xTaskGetTickCount();
        uint32_t   wait_time_ms = g_sys_state.t12_temp_read_time;

        g_dbg_loop_cnt++;

        /* Làm tươi Independent Watchdog Timer (IWDG) định kỳ mỗi 50ms */
        bsp_watchdog_feed();

        /* Nếu người dùng vừa tắt nung (t12_switch == 0) mà PWM vẫn còn -> Ngắt PWM ngay lập tức */
        if ((!g_sys_state.t12_switch) && (g_sys_state.pid_out != 0))
        {
            if (app_state_lock(10U))
            {
                prv_pid_execute();
                app_state_unlock();
            }
        }

        /* Kiểm tra nếu đã đến thời điểm đọc cảm biến & tính toán PID theo chu kỳ động (150..800ms) */
        if ((xNowTick - xLastReadTick) >= pdMS_TO_TICKS(wait_time_ms))
        {
            prv_read_t12_temp_and_control();
            xLastReadTick = xTaskGetTickCount();
        }

        /* Ngủ 50ms nhường CPU cho Task_Input_Sleep, Task_Display_UI, Task_Buzzer và Task_Debug_UART */
        vTaskDelay(pdMS_TO_TICKS(TASK_BASE_TICK_MS));
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
