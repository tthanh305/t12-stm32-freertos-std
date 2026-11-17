/**
 * @file    task_input_comm.c
 * @brief   Cài đặt 3 Task: Task_InputSleep (20ms), Task_Buzzer (Queue) và Task_DebugUART (100ms)
 *          sử dụng Thư viện chuẩn STM32F10x Standard Peripheral Library (STD/SPL v3.5.0) & FreeRTOS
 * @author  Dev 3
 * @note    Chuyển đổi hoàn thiện từ InterruptKey.ino, Sleep.ino, Buzzer.ino (v1.20)
 */

#include "task_input_comm.h"
#include "app_ipc.h"
#include "hal_gpio.h"
#include "hal_timer.h"
#include "hal_uart.h"
#include "hal_flash.h"
#include "task_sensor_pid.h"
#include <string.h>

/* =========================================================================
 * 1. HÀM NỘI BỘ HỖ TRỢ ĐỒNG BỘ MUTEX, NGỦ & SCREENSAVER
 * ========================================================================= */

static uint32_t s_xp_prng_state = 0x5A5A1234UL;

/**
 * @brief Sinh số ngẫu nhiên nhẹ (LCG) trong đoạn [min_val, max_incl] cho tọa độ Screensaver
 */
static int8_t prv_prng_range(int8_t min_val, int8_t max_incl)
{
    uint32_t span;

    if (max_incl <= min_val)
    {
        return min_val;
    }

    s_xp_prng_state = (s_xp_prng_state * 1664525UL) + 1013904223UL;
    span            = (uint32_t)(max_incl - min_val + 1);

    return (int8_t)(min_val + (int8_t)((s_xp_prng_state >> 16U) % span));
}

/**
 * @brief Thoát chế độ Boost và khôi phục nhiệt độ cài đặt gốc (yêu cầu đã khóa Mutex)
 */
static void prv_exit_boost_locked(void)
{
    if (g_sys_state.zjm_boost_ts)
    {
        g_sys_state.zjm_boost_ts = false;
        g_sys_state.boost_count  = 0U;
        if (g_sys_state.boost_temp_cache > 0U)
        {
            g_sys_cfg.set_temp           = g_sys_state.boost_temp_cache;
            g_sys_state.boost_temp_cache = 0U;
        }
    }
}

/**
 * @brief Kích hoạt chế độ Boost gia nhiệt tăng cường tạm thời (yêu cầu đã khóa Mutex)
 */
static void prv_enter_boost_locked(void)
{
    int16_t target_boost;

    if ((g_sys_cfg.boost_time == 0U) || (!g_sys_state.t12_switch) || g_sys_state.zjm_sleep_ts)
    {
        return;
    }

    if (!g_sys_state.zjm_boost_ts)
    {
        g_sys_state.boost_temp_cache = g_sys_cfg.set_temp;
    }

    target_boost = (int16_t)g_sys_state.boost_temp_cache + (int16_t)g_sys_cfg.boost_temp;
    if (target_boost > g_sys_state.set_temp_max)
    {
        target_boost = g_sys_state.set_temp_max;
    }

    g_sys_cfg.set_temp       = (uint16_t)target_boost;
    g_sys_state.zjm_boost_ts = true;
    g_sys_state.boost_count  = (uint16_t)g_sys_cfg.boost_time;

    /* Âm báo kích hoạt Boost 3 nốt đi lên: C7 -> E7 -> G7 */
    app_buzzer_play(2093U, 45U);
    app_buzzer_pause(15U);
    app_buzzer_play(2637U, 45U);
    app_buzzer_pause(15U);
    app_buzzer_play(3136U, 80U);
}

/**
 * @brief Đưa hệ thống thoát chế độ ngủ và khôi phục nhiệt độ cài đặt trước khi ngủ (yêu cầu đã khóa Mutex)
 */
static void prv_wake_from_sleep_locked(void)
{
    if (g_sys_state.zjm_sleep_ts)
    {
        g_sys_state.zjm_sleep_ts = false;
        if (g_sys_state.sleep_temp_cache > 0U)
        {
            g_sys_cfg.set_temp           = g_sys_state.sleep_temp_cache;
            g_sys_state.sleep_temp_cache = 0U;
        }
    }
}

/**
 * @brief Đưa hệ thống vào chế độ ngủ (yêu cầu đã khóa Mutex)
 *        Chỉ hạ nhiệt khi set_temp > sleep_temp để tránh vô tình làm nóng mũi hàn khi ngủ.
 */
static void prv_enter_sleep_locked(void)
{
    prv_exit_boost_locked();
    g_sys_state.zjm_sleep_ts     = true;
    g_sys_state.sleep_temp_cache = g_sys_cfg.set_temp;
    if (g_sys_cfg.set_temp > (uint16_t)g_sys_cfg.sleep_temp)
    {
        g_sys_cfg.set_temp = (uint16_t)g_sys_cfg.sleep_temp;
    }
    /* Âm đi xuống báo vào chế độ Ngủ (B6 -> F#6) */
    app_buzzer_play(1976U, 55U);
    app_buzzer_pause(25U);
    app_buzzer_play(1480U, 85U);
}

/**
 * @brief Tạo bản sao cấu hình an toàn để lưu Flash (không lưu đè nhiệt độ Ngủ hoặc Boost tạm thời)
 */
static void prv_snapshot_cfg_locked(SystemConfig_t *out_cfg)
{
    memcpy(out_cfg, &g_sys_cfg, sizeof(SystemConfig_t));
    if (g_sys_state.zjm_sleep_ts && (g_sys_state.sleep_temp_cache > 0U))
    {
        out_cfg->set_temp = g_sys_state.sleep_temp_cache;
    }
    else if (g_sys_state.zjm_boost_ts && (g_sys_state.boost_temp_cache > 0U))
    {
        out_cfg->set_temp = g_sys_state.boost_temp_cache;
    }
}

/**
 * @brief Cộng delta vào chỉ mục menu và xoay vòng chính xác trong [0 .. count - 1]
 */
static int8_t prv_wrap_menu_index(int8_t curr, int16_t delta, int8_t count)
{
    int16_t next = ((int16_t)curr + delta) % (int16_t)count;

    if (next < 0)
    {
        next += (int16_t)count;
    }

    return (int8_t)next;
}

/**
 * @brief Xử lý sự kiện xoay Rotary Encoder (yêu cầu đã khóa Mutex)
 */
static void prv_handle_encoder_rotate_locked(int16_t delta)
{
    /* Đảo chiều xoay nếu người dùng đã cài đặt xzbmq_fx == 1 */
    if (g_sys_cfg.xzbmq_fx != 0U)
    {
        delta = (int16_t)(-delta);
    }

    /* Đánh thức khỏi màn hình chờ, thoát ngủ và reset các bộ đếm */
    g_sys_state.sleep_count   = 0U;
    prv_wake_from_sleep_locked();
    g_sys_state.xp_state      = false;
    g_sys_state.xp_time_count = 0U;
    g_sys_state.t12_adc_error = false;

    /* Phát tiếng tick gọn, rõ khi xoay núm vặn (tự lọc nếu xoay quá nhanh) */
    app_buzzer_tick(1800U, 20U);

    switch (g_sys_state.display_count)
    {
        case UI_SCREEN_ZJM: /* Màn hình chính */
        {
            int16_t new_temp;

            prv_exit_boost_locked();
            new_temp = (int16_t)g_sys_cfg.set_temp + (delta * 10);
            if (new_temp > g_sys_state.set_temp_max)
            {
                new_temp = g_sys_state.set_temp_min;
            }
            else if (new_temp < g_sys_state.set_temp_min)
            {
                new_temp = g_sys_state.set_temp_max;
            }
            g_sys_cfg.set_temp = (uint16_t)new_temp;
            break;
        }

        case UI_SCREEN_SZJM: /* Menu Cài đặt Carousel: phản hồi tức thì mọi nấc xoay, không khóa animation */
        {
            g_sys_state.szjm_kxxk_count = prv_wrap_menu_index(g_sys_state.szjm_kxxk_count, -delta, 6);
            break;
        }

        case UI_SCREEN_SZJM_PID: /* Menu chỉnh hệ số PID */
        {
            if (g_sys_state.szjm_pid_sxxk_count == 0)
            {
                g_sys_state.szjm_pid_kxxk_count = prv_wrap_menu_index(g_sys_state.szjm_pid_kxxk_count, delta, 4);
            }
            else
            {
                float step = g_sys_state.dec_state ? 0.1f : 1.0f;
                if (g_sys_state.szjm_pid_sxxk_count == 1)
                {
                    g_sys_cfg.p += ((float)delta * step);
                    if (g_sys_cfg.p < 0.0f) g_sys_cfg.p = 0.0f;
                    else if (g_sys_cfg.p > 99.9f) g_sys_cfg.p = 99.9f;
                }
                else if (g_sys_state.szjm_pid_sxxk_count == 2)
                {
                    g_sys_cfg.i += ((float)delta * step);
                    if (g_sys_cfg.i < 0.0f) g_sys_cfg.i = 0.0f;
                    else if (g_sys_cfg.i > 99.9f) g_sys_cfg.i = 99.9f;
                }
                else if (g_sys_state.szjm_pid_sxxk_count == 3)
                {
                    g_sys_cfg.d += ((float)delta * step);
                    if (g_sys_cfg.d < 0.0f) g_sys_cfg.d = 0.0f;
                    else if (g_sys_cfg.d > 99.9f) g_sys_cfg.d = 99.9f;
                }
            }
            break;
        }

        case UI_SCREEN_SZJM_SLEEP: /* Menu cài đặt chế độ ngủ */
        {
            if (g_sys_state.szjm_sleep_sxxk_count == 0)
            {
                g_sys_state.szjm_sleep_kxxk_count = prv_wrap_menu_index(g_sys_state.szjm_sleep_kxxk_count, delta, 3);
            }
            else if (g_sys_state.szjm_sleep_sxxk_count == 1) /* Chỉnh Sleep Time */
            {
                int32_t val = (int32_t)g_sys_cfg.sleep_time + ((int32_t)delta * 30);
                if (val > 930) val = 30;
                else if (val < 30) val = 930;
                g_sys_cfg.sleep_time = (uint16_t)val;
            }
            else if (g_sys_state.szjm_sleep_sxxk_count == 2) /* Chỉnh Sleep Temp (50..250°C) */
            {
                int32_t val = (int32_t)g_sys_cfg.sleep_temp + ((int32_t)delta * 10);
                if (val > 250) val = 250;
                else if (val < 50) val = 50;
                g_sys_cfg.sleep_temp = (uint8_t)val;
            }
            break;
        }

        case UI_SCREEN_SZJM_OLED: /* Menu cài đặt OLED, Encoder & Âm lượng còi */
        {
            if (g_sys_state.szjm_oled_sxxk_count == 0)
            {
                g_sys_state.szjm_oled_kxxk_count = prv_wrap_menu_index(g_sys_state.szjm_oled_kxxk_count, delta, 5);
            }
            else if (g_sys_state.szjm_oled_sxxk_count == 1) /* Chỉnh độ sáng OLED */
            {
                int32_t val = (int32_t)g_sys_cfg.oled_ld + ((int32_t)delta * 5);
                if (val > 255) val = 255;
                else if (val < 5) val = 5;
                g_sys_cfg.oled_ld          = (uint8_t)val;
                g_sys_state.oled_cfg_dirty = true;
            }
            else if (g_sys_state.szjm_oled_sxxk_count == 2) /* Hướng xoay OLED */
            {
                if ((delta & 1) != 0)
                {
                    g_sys_cfg.oled_fx          = (g_sys_cfg.oled_fx != 0U) ? 0U : 1U;
                    g_sys_state.oled_cfg_dirty = true;
                }
            }
            else if (g_sys_state.szjm_oled_sxxk_count == 3) /* Đảo chiều Encoder */
            {
                if ((delta & 1) != 0)
                {
                    g_sys_cfg.xzbmq_fx = (g_sys_cfg.xzbmq_fx != 0U) ? 0U : 1U;
                }
            }
            else if (g_sys_state.szjm_oled_sxxk_count == 4) /* Chỉnh âm lượng còi (0..100%) */
            {
                int32_t val = (int32_t)g_sys_cfg.buzzer_vol + ((int32_t)delta * 10);
                if (val > 100) val = 100;
                else if (val < 0) val = 0;
                g_sys_cfg.buzzer_vol = (uint8_t)val;
            }
            break;
        }

        case UI_SCREEN_SZJM_POWER: /* Menu Nguồn điện & Cảnh báo */
        {
            if (g_sys_state.szjm_power_sxxk_count == 0)
            {
                g_sys_state.szjm_power_kxxk_count = prv_wrap_menu_index(g_sys_state.szjm_power_kxxk_count, delta, 4);
            }
            else if (g_sys_state.szjm_power_sxxk_count == 1) /* Ref. Voltage (giới hạn an toàn 3.00V .. 3.60V) */
            {
                float val = g_sys_cfg.vcc_refer + ((float)delta * 0.01f);
                if (val > 3.60f) val = 3.60f;
                else if (val < 3.00f) val = 3.00f;
                g_sys_cfg.vcc_refer = val;
            }
            else if (g_sys_state.szjm_power_sxxk_count == 2) /* Volt Calib (giới hạn bù -5.0V .. +5.0V) */
            {
                float val = g_sys_cfg.vin_refer + ((float)delta * 0.1f);
                if (val > 5.0f) val = 5.0f;
                else if (val < -5.0f) val = -5.0f;
                g_sys_cfg.vin_refer = val;
            }
            else if (g_sys_state.szjm_power_sxxk_count == 3) /* Low V. Alarm */
            {
                float val = g_sys_cfg.vin_low + ((float)delta * 0.1f);
                if (val > 30.0f) val = 3.2f;
                else if (val < 3.2f) val = 30.0f;
                g_sys_cfg.vin_low = val;
            }
            break;
        }

        case UI_SCREEN_SZJM_LAOTIE: /* Menu Mũi hàn (Cold End, Auto Heat, Boost, Reset) */
        {
            if (g_sys_state.szjm_laotie_sxxk_count == 0)
            {
                g_sys_state.szjm_laotie_kxxk_count = prv_wrap_menu_index(g_sys_state.szjm_laotie_kxxk_count, delta, 6);
            }
            else if (g_sys_state.szjm_laotie_sxxk_count == 1) /* Cold End bù nhiệt phòng */
            {
                int32_t val = (int32_t)g_sys_cfg.tc1047_refer + (int32_t)delta;
                if (val > 99) val = -99;
                else if (val < -99) val = 99;
                g_sys_cfg.tc1047_refer = (int8_t)val;
            }
            else if (g_sys_state.szjm_laotie_sxxk_count == 3) /* Chỉnh Boost Temp (+10..+100°C) */
            {
                int32_t val = (int32_t)g_sys_cfg.boost_temp + ((int32_t)delta * 10);
                if (val > 100) val = 100;
                else if (val < 10) val = 10;
                g_sys_cfg.boost_temp = (uint8_t)val;
            }
            else if (g_sys_state.szjm_laotie_sxxk_count == 4) /* Chỉnh Boost Time (10..120s, 0 = OFF) */
            {
                int32_t val = (int32_t)g_sys_cfg.boost_time + ((int32_t)delta * 10);
                if (val > 120) val = 0;
                else if (val < 0) val = 120;
                g_sys_cfg.boost_time = (uint8_t)val;
            }
            else if (g_sys_state.szjm_laotie_sxxk_count == 5) /* Reset cài đặt gốc */
            {
                if ((delta & 1) != 0)
                {
                    g_sys_state.hfccsz_state = !g_sys_state.hfccsz_state;
                }
            }
            break;
        }

        case UI_SCREEN_SZJM_JIAOZHUN: /* Menu Hiệu chuẩn Calib */
        {
            if (g_sys_state.szjm_jiaozhun_sxxk_count == 0)
            {
                g_sys_state.szjm_jiaozhun_kxxk_count = prv_wrap_menu_index(g_sys_state.szjm_jiaozhun_kxxk_count, delta, 7);
            }
            else if ((g_sys_state.szjm_jiaozhun_sxxk_count >= 1) &&
                     (g_sys_state.szjm_jiaozhun_sxxk_count <= 4))
            {
                int32_t val = (int32_t)g_sys_cfg.sz_temp[g_sys_state.szjm_jiaozhun_sxxk_count - 1] + (int32_t)delta;
                if (val < 0) val = 0;
                if (val > 600) val = 600;
                g_sys_cfg.sz_temp[g_sys_state.szjm_jiaozhun_sxxk_count - 1] = (uint16_t)val;
            }
            break;
        }

        default:
            break;
    }
}

/**
 * @brief Xử lý sự kiện nhấn giữ >= 800ms (yêu cầu đã khóa Mutex)
 * @return true nếu cần lưu cấu hình ra Flash sau khi nhả Mutex
 */
static bool prv_handle_long_press_locked(SystemConfig_t *cfg_out)
{
    bool need_save = false;

    g_sys_state.xp_state      = false;
    g_sys_state.xp_time_count = 0U;

    if (g_sys_state.display_count == UI_SCREEN_ZJM)
    {
        /* Từ Màn hình chính vào Menu cài đặt: Hủy Boost tạm thời và phát âm đôi đi lên G6 -> C7 */
        prv_exit_boost_locked();
        g_sys_state.t12_adc_error = false;
        g_sys_state.display_count = UI_SCREEN_SZJM;
        app_buzzer_play(1568U, 65U);
        app_buzzer_pause(20U);
        app_buzzer_play(2093U, 110U);
    }
    else
    {
        /* Nếu đang hiệu chuẩn Calib mà nhấn giữ thoát -> bắt buộc tắt gia nhiệt và tính lại đường cong */
        if (g_sys_state.display_count == UI_SCREEN_SZJM_JIAOZHUN)
        {
            if (g_sys_state.szjm_jiaozhun_sxxk_count != 0)
            {
                g_sys_state.szjm_jiaozhun_sxxk_count = 0;
                g_sys_state.t12_switch               = false;
            }
            g_sys_state.szjm_jiaozhun_kxxk_count = 0;
            app_curve_recalculate(g_sys_cfg.sz_temp, g_sys_state.sz_p);
            app_update_set_temp_limits();
        }

        prv_snapshot_cfg_locked(cfg_out);
        need_save                 = true;
        g_sys_state.display_count = UI_SCREEN_ZJM;
        /* Thoát Menu về màn hình chính: Âm đôi đi xuống C7 -> G6 */
        app_buzzer_play(2093U, 65U);
        app_buzzer_pause(20U);
        app_buzzer_play(1568U, 110U);
    }

    return need_save;
}

/**
 * @brief Xử lý sự kiện nhấn đúp (yêu cầu đã khóa Mutex)
 * @return true nếu cần lưu cấu hình ra Flash sau khi nhả Mutex
 */
static bool prv_handle_double_click_locked(SystemConfig_t *cfg_out)
{
    bool need_save = false;

    if (g_sys_state.display_count == UI_SCREEN_ZJM)
    {
        if (!g_sys_state.vin_error && !g_sys_state.t12_error)
        {
            prv_exit_boost_locked();
            if (g_sys_state.t12_switch && (!g_sys_state.zjm_sleep_ts))
            {
                prv_snapshot_cfg_locked(cfg_out);
                need_save = true;
            }
            prv_wake_from_sleep_locked();
            g_sys_state.t12_switch    = !g_sys_state.t12_switch;
            g_sys_state.zjm_dh_run    = true;
            g_sys_state.sleep_count   = 0U;
            g_sys_state.xp_state      = false;
            g_sys_state.xp_time_count = 0U;
            if (g_sys_state.t12_switch)
            {
                /* Bật gia nhiệt (Heat): A6 -> D7 */
                app_buzzer_play(1760U, 50U);
                app_buzzer_pause(20U);
                app_buzzer_play(2349U, 80U);
            }
            else
            {
                /* Tắt gia nhiệt (Stop): D7 -> G6 */
                app_buzzer_play(2349U, 50U);
                app_buzzer_pause(20U);
                app_buzzer_play(1568U, 80U);
            }
        }
    }

    return need_save;
}

/**
 * @brief Xử lý sự kiện nhấn đơn (yêu cầu đã khóa Mutex)
 * @return true nếu cần lưu cấu hình ra Flash sau khi nhả Mutex
 */
static bool prv_handle_single_click_locked(SystemConfig_t *cfg_out)
{
    bool need_save   = false;
    bool custom_tone = false;

    switch (g_sys_state.display_count)
    {
        case UI_SCREEN_ZJM:
            g_sys_state.t12_adc_error = false;
            g_sys_state.xp_state      = false;
            g_sys_state.xp_time_count = 0U;
            if (g_sys_state.t12_switch && (!g_sys_state.zjm_sleep_ts) &&
                (!g_sys_state.vin_error) && (!g_sys_state.t12_error))
            {
                if (g_sys_state.zjm_boost_ts)
                {
                    prv_exit_boost_locked();
                    /* Âm hủy Boost về Heat: E7 -> C7 */
                    app_buzzer_play(2637U, 45U);
                    app_buzzer_pause(20U);
                    app_buzzer_play(2093U, 70U);
                    custom_tone = true;
                }
                else if (g_sys_cfg.boost_time > 0U)
                {
                    prv_enter_boost_locked();
                    custom_tone = true;
                }
            }
            break;

        case UI_SCREEN_SZJM:
        {
            int8_t c = g_sys_state.szjm_kxxk_count;
            if (c == 0) g_sys_state.display_count = UI_SCREEN_SZJM_SLEEP;
            else if ((c == -5) || (c == 1)) g_sys_state.display_count = UI_SCREEN_SZJM_PID;
            else if ((c == -1) || (c == 5)) g_sys_state.display_count = UI_SCREEN_SZJM_OLED;
            else if ((c == -2) || (c == 4)) g_sys_state.display_count = UI_SCREEN_SZJM_POWER;
            else if ((c == -4) || (c == 2)) g_sys_state.display_count = UI_SCREEN_SZJM_LAOTIE;
            else if ((c == -3) || (c == 3))
            {
                g_sys_state.display_count = UI_SCREEN_SZJM_JIAOZHUN;
            }
            break;
        }

        case UI_SCREEN_SZJM_PID:
            switch (g_sys_state.szjm_pid_kxxk_count)
            {
                case 0:
                    g_sys_state.display_count = UI_SCREEN_SZJM;
                    break;
                case 1:
                case 2:
                case 3:
                    if (g_sys_state.szjm_pid_sxxk_count == 0)
                    {
                        g_sys_state.szjm_pid_sxxk_count = g_sys_state.szjm_pid_kxxk_count;
                    }
                    else if (!g_sys_state.dec_state)
                    {
                        g_sys_state.dec_state = true;
                    }
                    else
                    {
                        g_sys_state.dec_state           = false;
                        g_sys_state.szjm_pid_sxxk_count = 0;
                        prv_snapshot_cfg_locked(cfg_out);
                        need_save = true;
                    }
                    break;
                default:
                    break;
            }
            break;

        case UI_SCREEN_SZJM_SLEEP:
            switch (g_sys_state.szjm_sleep_kxxk_count)
            {
                case 0:
                    g_sys_state.display_count = UI_SCREEN_SZJM;
                    break;
                case 1:
                case 2:
                    if (g_sys_state.szjm_sleep_sxxk_count == 0)
                    {
                        g_sys_state.szjm_sleep_sxxk_count = g_sys_state.szjm_sleep_kxxk_count;
                    }
                    else
                    {
                        g_sys_state.szjm_sleep_sxxk_count = 0;
                        prv_snapshot_cfg_locked(cfg_out);
                        need_save = true;
                    }
                    break;
                default:
                    break;
            }
            break;

        case UI_SCREEN_SZJM_OLED:
            switch (g_sys_state.szjm_oled_kxxk_count)
            {
                case 0:
                    g_sys_state.display_count = UI_SCREEN_SZJM;
                    break;
                case 1:
                case 4:
                    if (g_sys_state.szjm_oled_sxxk_count == 0)
                    {
                        g_sys_state.szjm_oled_sxxk_count = g_sys_state.szjm_oled_kxxk_count;
                    }
                    else
                    {
                        g_sys_state.szjm_oled_sxxk_count = 0;
                        prv_snapshot_cfg_locked(cfg_out);
                        need_save = true;
                    }
                    break;
                case 2:
                    g_sys_cfg.oled_fx          = (g_sys_cfg.oled_fx != 0U) ? 0U : 1U;
                    g_sys_state.oled_cfg_dirty = true;
                    prv_snapshot_cfg_locked(cfg_out);
                    need_save = true;
                    break;
                case 3:
                    g_sys_cfg.xzbmq_fx = (g_sys_cfg.xzbmq_fx != 0U) ? 0U : 1U;
                    prv_snapshot_cfg_locked(cfg_out);
                    need_save = true;
                    break;
                default:
                    break;
            }
            break;

        case UI_SCREEN_SZJM_POWER:
            switch (g_sys_state.szjm_power_kxxk_count)
            {
                case 0:
                    g_sys_state.display_count = UI_SCREEN_SZJM;
                    break;
                case 1:
                case 2:
                case 3:
                    if (g_sys_state.szjm_power_sxxk_count == 0)
                    {
                        g_sys_state.szjm_power_sxxk_count = g_sys_state.szjm_power_kxxk_count;
                    }
                    else
                    {
                        g_sys_state.szjm_power_sxxk_count = 0;
                        prv_snapshot_cfg_locked(cfg_out);
                        need_save = true;
                    }
                    break;
                default:
                    break;
            }
            break;

        case UI_SCREEN_SZJM_LAOTIE:
            switch (g_sys_state.szjm_laotie_kxxk_count)
            {
                case 0:
                    g_sys_state.display_count = UI_SCREEN_SZJM;
                    break;
                case 1:
                case 3:
                case 4:
                    if (g_sys_state.szjm_laotie_sxxk_count == 0)
                    {
                        g_sys_state.szjm_laotie_sxxk_count = g_sys_state.szjm_laotie_kxxk_count;
                    }
                    else
                    {
                        g_sys_state.szjm_laotie_sxxk_count = 0;
                        prv_snapshot_cfg_locked(cfg_out);
                        need_save = true;
                    }
                    break;
                case 2:
                    g_sys_cfg.t12_kj_jr = (g_sys_cfg.t12_kj_jr != 0U) ? 0U : 1U;
                    prv_snapshot_cfg_locked(cfg_out);
                    need_save = true;
                    break;
                case 5:
                    if (g_sys_state.szjm_laotie_sxxk_count == 0)
                    {
                        g_sys_state.szjm_laotie_sxxk_count = 5;
                    }
                    else
                    {
                        g_sys_state.szjm_laotie_sxxk_count = 0;
                        /* Nếu hfccsz_state == true, Task_DisplayUI sẽ xử lý Reset */
                    }
                    break;
                default:
                    break;
            }
            break;

        case UI_SCREEN_SZJM_JIAOZHUN:
            switch (g_sys_state.szjm_jiaozhun_kxxk_count)
            {
                case 0:
                    g_sys_state.display_count            = UI_SCREEN_SZJM;
                    g_sys_state.szjm_jiaozhun_kxxk_count = 0;
                    break;
                case 1:
                case 2:
                case 3:
                case 4:
                    if (g_sys_state.szjm_jiaozhun_sxxk_count == 0)
                    {
                        g_sys_state.szjm_jiaozhun_sxxk_count = g_sys_state.szjm_jiaozhun_kxxk_count;
                        g_sys_state.t12_switch               = true;
                    }
                    else
                    {
                        g_sys_state.szjm_jiaozhun_sxxk_count = 0;
                        g_sys_state.t12_switch               = false;
                        prv_snapshot_cfg_locked(cfg_out);
                        need_save = true;
                    }
                    break;
                case 5:
                    app_curve_recalculate(g_sys_cfg.sz_temp, g_sys_state.sz_p);
                    app_update_set_temp_limits();
                    prv_snapshot_cfg_locked(cfg_out);
                    need_save                            = true;
                    g_sys_state.szjm_jiaozhun_kxxk_count = 6;
                    break;
                case 6:
                    g_sys_state.display_count            = UI_SCREEN_SZJM;
                    g_sys_state.szjm_jiaozhun_kxxk_count = 0;
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }

    if ((g_sys_state.display_count >= UI_SCREEN_ZJM) && (!custom_tone))
    {
        /* Âm xác nhận nhấn đơn rõ ràng B6 (1976 Hz, 35ms) */
        app_buzzer_play(1976U, 35U);
    }

    return need_save;
}

/**
 * @brief Xử lý tức thì khi có chuyển động tay hàn (PA0) hoặc đặt/nhấc khỏi đế gác (PA1) mỗi 20ms
 */
static void prv_handle_motion_and_stand_locked(bool sensor_moved, bool stand_active, bool stand_changed)
{
    if (sensor_moved)
    {
        g_sys_state.sleep_count = 0U;

        /* Đánh thức tức thì nếu đang ngủ và tay hàn không nằm trên đế gác */
        if (g_sys_state.zjm_sleep_ts && !stand_active)
        {
            prv_wake_from_sleep_locked();
            app_buzzer_play(1568U, 55U);
            app_buzzer_pause(25U);
            app_buzzer_play(2093U, 80U);
        }
    }

    if (stand_changed)
    {
        g_sys_state.sleep_count = 0U;

        if (stand_active)
        {
            /* Gác lên đế gác -> Kích ngủ ngay lập tức (chỉ khi sleep_time != 930 OFF) */
            if ((g_sys_cfg.sleep_time < 930U) && g_sys_state.t12_switch && !g_sys_state.zjm_sleep_ts)
            {
                prv_enter_sleep_locked();
            }
        }
        else if (g_sys_state.zjm_sleep_ts)
        {
            /* Nhấc tay hàn ra khỏi đế gác -> Đánh thức ngay lập tức */
            prv_wake_from_sleep_locked();
            app_buzzer_play(1568U, 55U);
            app_buzzer_pause(25U);
            app_buzzer_play(2093U, 80U);
        }
    }
}

/**
 * @brief Quản lý bộ đếm Ngủ, Auto-Shutoff và Screensaver mỗi 1 giây (yêu cầu đã khóa Mutex)
 */
static void prv_handle_sleep_1s_locked(bool stand_active)
{
    if (stand_active)
    {
        /* Dự phòng nếu mới bật lại sleep_time trong Menu khi tay hàn đang gác sẵn trên đế */
        if ((g_sys_cfg.sleep_time < 930U) && g_sys_state.t12_switch && !g_sys_state.zjm_sleep_ts)
        {
            prv_enter_sleep_locked();
        }
    }

    if (g_sys_state.t12_switch)
    {
        if (g_sys_state.zjm_boost_ts)
        {
            if (g_sys_state.boost_count > 0U)
            {
                g_sys_state.boost_count--;
            }
            if (g_sys_state.boost_count == 0U)
            {
                prv_exit_boost_locked();
                /* Âm báo kết thúc thời gian Boost về nhiệt độ thường: E7 -> C7 */
                app_buzzer_play(2637U, 50U);
                app_buzzer_pause(20U);
                app_buzzer_play(2093U, 80U);
            }
        }

        if (g_sys_cfg.sleep_time < 930U) /* 930 = Không ngủ */
        {
            g_sys_state.sleep_count++;

            if ((g_sys_state.sleep_count >= g_sys_cfg.sleep_time) && !g_sys_state.zjm_sleep_ts)
            {
                prv_enter_sleep_locked();
            }

            /* Ngủ quá 30 phút (1800s) -> Tắt nung an toàn */
            if (g_sys_state.zjm_sleep_ts && (g_sys_state.sleep_count >= 1800U))
            {
                g_sys_state.t12_switch  = false;
                g_sys_state.sleep_count = 0U;
                prv_wake_from_sleep_locked();
                /* Âm tắt hẳn an toàn: A6 -> E6 */
                app_buzzer_play(1760U, 120U);
                app_buzzer_pause(50U);
                app_buzzer_play(1319U, 180U);
            }
        }
    }
    else
    {
        /* Đang ở chế độ Stop: Sau 180s bật Screensaver và đổi vị trí mỗi 3s chống burn-in OLED */
        g_sys_state.xp_time_count++;
        if (g_sys_state.xp_time_count >= 180U)
        {
            if (!g_sys_state.xp_state)
            {
                g_sys_state.display_count = UI_SCREEN_ZJM;
                g_sys_state.xp_state      = true;
            }
            if ((g_sys_state.xp_time_count % 3U) == 0U)
            {
                g_sys_state.xp_x = prv_prng_range(4, 62);
                g_sys_state.xp_y = prv_prng_range(10, 27);
            }
            if (g_sys_state.xp_time_count >= 65530U)
            {
                g_sys_state.xp_time_count = 180U;
            }
        }
    }
}

/* =========================================================================
 * 2. TASK INPUT & SLEEP (CHU KỲ CỐ ĐỊNH 20MS)
 * ========================================================================= */
static void prvInputSleepTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    /* Biến theo dõi nút nhấn */
    uint16_t s_sw_press_ticks    = 0U;
    uint16_t s_sw_release_ticks  = 0U;
    uint8_t  s_sw_release_glitch = 0U;
    uint8_t  s_click_count       = 0U;
    bool     s_sw_long_pressed   = false;

    /* Biến theo dõi cảm biến rung (20ms), đế gác (debounce 40ms), bộ đếm 1s & tự lưu set_temp */
    uint8_t  s_sec_tick_count      = 0U;
    bool     s_last_sleep_sensor   = hal_gpio_read_sleep_sensor();
    bool     s_stand_stable        = hal_gpio_read_stand_vb();
    uint8_t  s_stand_debounce      = 0U;
    uint16_t s_set_temp_save_ticks = 0U;

    for (;;)
    {
        SystemConfig_t cfg_snapshot;
        bool           need_save         = false;
        bool           evt_long_press    = false;
        bool           evt_double_click  = false;
        bool           evt_single_click  = false;
        bool           sec_elapsed       = false;
        bool           auto_save_elapsed = false;
        bool           raw_stand;
        bool           sleep_sensor;
        bool           sensor_moved;
        bool           stand_changed     = false;

        /* -----------------------------------------------------------------
         * 2.1. ĐỌC ENCODER (PB6: CLK, PB5: DT) & NÚT NHẤN (PB7: EC_SW)
         * ----------------------------------------------------------------- */
        int16_t delta      = hal_encoder_get_delta();
        bool    sw_pressed = hal_gpio_read_encoder_sw();

        if (sw_pressed)
        {
            s_sw_release_glitch = 0U;
            s_sw_press_ticks++;
            s_sw_release_ticks  = 0U;

            /* Phát tiếng "tít" nhịp phản hồi khi đang giữ nút giống bản gốc (mỗi 180ms trước ngưỡng 600ms) */
            if (!s_sw_long_pressed &&
                ((s_sw_press_ticks == 9U) || (s_sw_press_ticks == 18U) || (s_sw_press_ticks == 27U)))
            {
                app_buzzer_tick(1661U, 25U);
            }

            /* Nhấn giữ >= 600ms (30 chu kỳ 20ms) -> LONG PRESS */
            if ((s_sw_press_ticks >= 30U) && (!s_sw_long_pressed))
            {
                s_sw_long_pressed = true;
                s_click_count     = 0U;
                evt_long_press    = true;
            }
        }
        else if (s_sw_press_ticks > 0U)
        {
            /* Lọc nhiễu nảy tiếp điểm khi đang giữ (cần nhả >= 40ms mới tính là nhả thật) */
            s_sw_release_glitch++;
            if (s_sw_release_glitch >= 2U)
            {
                if (!s_sw_long_pressed)
                {
                    s_click_count++;
                }
                s_sw_press_ticks    = 0U;
                s_sw_release_glitch = 0U;
                s_sw_long_pressed   = false;
            }
        }
        else if (s_click_count > 0U)
        {
            s_sw_release_ticks++;
            if (s_click_count >= 2U)
            {
                s_click_count      = 0U;
                s_sw_release_ticks = 0U;
                evt_double_click   = true;
            }
            else if (s_sw_release_ticks >= 12U) /* Chờ 240ms -> SINGLE CLICK */
            {
                s_click_count      = 0U;
                s_sw_release_ticks = 0U;
                evt_single_click   = true;
            }
        }

        /* -----------------------------------------------------------------
         * 2.2. QUÉT CẢM BIẾN RUNG (PA0) & ĐẾ GÁC (PA1) MỖI 20MS + ĐẾM 1 GIÂY
         * ----------------------------------------------------------------- */
        sleep_sensor = hal_gpio_read_sleep_sensor();
        sensor_moved = (sleep_sensor != s_last_sleep_sensor);
        if (sensor_moved)
        {
            s_last_sleep_sensor = sleep_sensor;
        }

        raw_stand = hal_gpio_read_stand_vb();
        if (raw_stand != s_stand_stable)
        {
            s_stand_debounce++;
            if (s_stand_debounce >= 2U)
            {
                s_stand_stable   = raw_stand;
                s_stand_debounce = 0U;
                stand_changed    = true;
            }
        }
        else
        {
            s_stand_debounce = 0U;
        }

        if (s_set_temp_save_ticks > 0U)
        {
            s_set_temp_save_ticks--;
            if (s_set_temp_save_ticks == 0U)
            {
                auto_save_elapsed = true;
            }
        }

        s_sec_tick_count++;
        if (s_sec_tick_count >= 50U)
        {
            s_sec_tick_count = 0U;
            sec_elapsed      = true;
        }

        /* -----------------------------------------------------------------
         * 2.3. CẬP NHẬT TRẠNG THÁI CHUNG DƯỚI BẢO VỆ MUTEX
         * ----------------------------------------------------------------- */
        if ((delta != 0) || evt_long_press || evt_double_click || evt_single_click ||
            sensor_moved || stand_changed || sec_elapsed || auto_save_elapsed)
        {
            if (app_state_lock(50U))
            {
                if (delta != 0)
                {
                    prv_handle_encoder_rotate_locked(delta);
                    if (g_sys_state.display_count == UI_SCREEN_ZJM)
                    {
                        /* Tự lưu set_temp sau 3.0 giây ngừng xoay trên màn hình chính */
                        s_set_temp_save_ticks = 150U;
                    }
                }
                if (evt_long_press)
                {
                    need_save |= prv_handle_long_press_locked(&cfg_snapshot);
                }
                if (evt_double_click)
                {
                    need_save |= prv_handle_double_click_locked(&cfg_snapshot);
                }
                if (evt_single_click)
                {
                    need_save |= prv_handle_single_click_locked(&cfg_snapshot);
                }
                if (sensor_moved || stand_changed)
                {
                    prv_handle_motion_and_stand_locked(sensor_moved, s_stand_stable, stand_changed);
                }
                if (sec_elapsed)
                {
                    prv_handle_sleep_1s_locked(s_stand_stable);
                }

                if (need_save)
                {
                    s_set_temp_save_ticks = 0U;
                }
                else if (auto_save_elapsed)
                {
                    prv_snapshot_cfg_locked(&cfg_snapshot);
                    need_save = true;
                }

                app_state_unlock();
            }
        }

        /* Ghi Flash ngoài vùng Mutex để tránh giữ khóa khi xóa/ghi Page 63 */
        if (need_save)
        {
            (void)hal_storage_save(&cfg_snapshot);
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK_INPUT_SLEEP_PERIOD_MS));
    }
}

/* =========================================================================
 * 3. TASK BUZZER (CHỜ SỰ KIỆN TỪ xBuzzerQueue)
 * ========================================================================= */

/**
 * @brief Quy đổi âm lượng cài đặt (0..100%) sang Duty Cycle PWM (0..50%) theo đường cong bậc 2
 */
static uint8_t prv_buzzer_vol_to_duty(uint8_t vol_pct)
{
    uint32_t duty;

    if (vol_pct == 0U)
    {
        return 0U;
    }
    if (vol_pct >= 100U)
    {
        return 50U;
    }

    duty = (((uint32_t)vol_pct * (uint32_t)vol_pct * 50U) + 5000U) / 10000U;
    if (duty < 1U)
    {
        duty = 1U;
    }
    return (uint8_t)duty;
}

static void prvBuzzerTask(void *pvParameters)
{
    (void)pvParameters;
    BuzzerMsg_t msg;

    for (;;)
    {
        if (xQueueReceive(xBuzzerQueue, &msg, pdMS_TO_TICKS(1000U)) == pdTRUE)
        {
            if (msg.half_period_us == 0U)
            {
                /* Khoảng nghỉ giữa các nốt (ms) */
                hal_buzzer_tone_stop();
                if (msg.cycle_count > 0U)
                {
                    vTaskDelay(pdMS_TO_TICKS(msg.cycle_count));
                }
            }
            else
            {
                uint8_t  duty        = prv_buzzer_vol_to_duty(g_sys_cfg.buzzer_vol);
                uint32_t duration_ms = (((uint32_t)2U * msg.half_period_us * msg.cycle_count) + 500U) / 1000U;
                if (duration_ms == 0U)
                {
                    duration_ms = 1U;
                }

                if (duty > 0U)
                {
                    hal_buzzer_tone_start_vol((uint16_t)(2U * msg.half_period_us), duty);
                    vTaskDelay(pdMS_TO_TICKS(duration_ms));
                    hal_buzzer_tone_stop();
                }
                else
                {
                    hal_buzzer_tone_stop();
                    vTaskDelay(pdMS_TO_TICKS(duration_ms));
                }
            }
        }
        else
        {
            bool has_alarm = false;

            if (app_state_lock(5U))
            {
                has_alarm = (g_sys_state.vin_error || g_sys_state.t12_adc_error);
                app_state_unlock();
            }

            /* Nếu đang có lỗi vin_error hoặc t12_adc_error -> phát bíp đôi cảnh báo 2200Hz */
            if (has_alarm)
            {
                uint8_t alarm_duty = prv_buzzer_vol_to_duty(g_sys_cfg.buzzer_vol);
                if (alarm_duty > 0U)
                {
                    hal_buzzer_tone_start_vol(455U, alarm_duty);
                    vTaskDelay(pdMS_TO_TICKS(65U));
                    hal_buzzer_tone_stop();
                    vTaskDelay(pdMS_TO_TICKS(60U));
                    hal_buzzer_tone_start_vol(455U, alarm_duty);
                    vTaskDelay(pdMS_TO_TICKS(65U));
                    hal_buzzer_tone_stop();
                }
            }
        }
    }
}

/* =========================================================================
 * 4. TASK DEBUG UART (CHU KỲ 100MS)
 * ========================================================================= */
static void prvDebugUartTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        const char *state_str = "STOP";
        uint16_t    set_temp  = 0U;
        int16_t     disp_temp = 0;
        float       t12_ad    = 0.0f;
        uint8_t     pwm_pct   = 0U;
        float       vin       = 0.0f;
        float       amb_temp  = 0.0f;

        if (app_state_lock(10U))
        {
            state_str = g_sys_state.t12_switch
                      ? (g_sys_state.zjm_sleep_ts ? "SLEEP" : (g_sys_state.zjm_boost_ts ? "BOOST" : "HEAT"))
                      : "STOP";
            set_temp  = g_sys_cfg.set_temp;
            disp_temp = g_sys_state.t12_temp_display;
            t12_ad    = g_sys_state.t12_ad;
            pwm_pct   = g_sys_state.pid_out_bfb;
            vin       = g_sys_state.vin;
            amb_temp  = g_sys_state.tc1047_temp;
            app_state_unlock();

            hal_uart_printf("[T12] State:%s | Set:%dC | Temp:%dC | ADC:%.1f | PWM:%d%% | Vin:%.1fV | Amb:%.1fC\r\n",
                            state_str,
                            (int)set_temp,
                            (int)disp_temp,
                            t12_ad,
                            (int)pwm_pct,
                            vin,
                            amb_temp);
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK_DEBUG_UART_PERIOD_MS));
    }
}

/* =========================================================================
 * 5. HÀM KHỞI TẠO CÁC TASK CỦA DEV 3
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
