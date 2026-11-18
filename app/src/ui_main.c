/**
 * @file    ui_main.c
 * @brief   Giao diện Màn hình chính (ZJM) với số lớn font bubble_tn gốc, Màn hình khởi động (Boot Screen), Screensaver (XPJM)
 * @author  Dev 2 (UI & Flash)
 * @note    Giữ nguyên font chữ số nhiệt độ lớn tròn trịa ban đầu (u8g2_font_bubble_tn) kèm hiệu ứng động
 */

#include "task_display.h"
#include "app_ipc.h"
#include "bsp_board.h"
#include "hal_flash.h"
#include "hal_timer.h"
#include "u8g2.h"

/* =========================================================================
 * 1. LIÊN KẾT ĐỐI TƯỢNG U8G2 & HÀM TIỆN ÍCH TỪ task_display.c
 * ========================================================================= */
/* =========================================================================
 * 2. HẰNG SỐ CẤU HÌNH GIAO DIỆN
 * ========================================================================= */
#define BOOT_DURATION_MS        ( 1500U ) /* Thời gian chạy thanh loading khởi động: 1.5s */
#define BOOT_STEP_MS            ( 25U )   /* Chu kỳ cập nhật khung hình khởi động */

/* Biến trượt màn hình chính từ dưới lên khi vừa khởi động */
static int8_t     s_zjm_dh_y         = 32;

/* Bộ nội suy tốc độ hiển thị nhiệt độ theo chu kỳ đọc ADC thực tế */
static int16_t    s_disp_last_target = 0;
static int32_t    s_disp_accum_udeg  = 0;
static int32_t    s_disp_rate_udeg   = 0;
static TickType_t s_disp_last_tick   = 0U;
static uint32_t   s_disp_elapsed_ms  = 0U;
static uint32_t   s_disp_span_ms     = 400U;

/* Biến điều khiển hiệu ứng chuyển trạng thái mượt cho nút Heat / Bst / Slp / Stop */
#define ZJM_BTN_MODE_STOP       ( 0U )
#define ZJM_BTN_MODE_SLP        ( 1U )
#define ZJM_BTN_MODE_HEAT       ( 2U )
#define ZJM_BTN_MODE_BST        ( 3U )

static uint8_t    s_btn_last_mode   = 0xFFU;
static uint8_t    s_btn_prev_mode   = ZJM_BTN_MODE_STOP;
static int16_t    s_btn_fill_x16    = 0;
static int16_t    s_btn_text_dy16   = 0;
static int8_t     s_btn_slide_dir   = 1;

/* Làm mượt thanh PWM Bar, số % PWM và con trỏ thanh trượt SET trên màn hình chính */
static int16_t    s_pwm_fill_x16    = 0;
static int16_t    s_pwm_pct_x16     = 0;
static int16_t    s_bead_x16        = -1;

/* Tọa độ làm mượt chuyển động chữ SLEEP Zzz... và bầu trời sao tĩnh trên màn hình chờ */
#define XP_STAR_COUNT    ( 20U )
typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t phase;
} XpStar_t;

static int8_t     s_xp_x_old     = 50;
static int8_t     s_xp_y_old     = 21;
static XpStar_t   s_xp_stars[XP_STAR_COUNT];
static bool       s_xp_stars_init = false;
static TickType_t s_xp_star_tick  = 0U;

/* Bộ sinh số giả ngẫu nhiên nhẹ (LFSR/LCG) */
static uint32_t   s_prng_state   = 0x12345678UL;

/* Icon mặt trăng khuyết 12x12 (XBM, 24 bytes) cho màn hình chờ Screensaver */
static const uint8_t s_xp_moon_xbm[24] = {
    0xF0, 0x01,
    0xFC, 0x00,
    0x3E, 0x00,
    0x1E, 0x00,
    0x0F, 0x00,
    0x07, 0x00,
    0x07, 0x00,
    0x0F, 0x00,
    0x1E, 0x00,
    0x3E, 0x00,
    0xFC, 0x00,
    0xF0, 0x01
};

/* =========================================================================
 * 3. CÁC HÀM NỘI BỘ (STATIC HELPERS)
 * ========================================================================= */

static int32_t prv_random_range(int32_t min_val, int32_t max_excl)
{
    uint32_t span;

    if (max_excl <= min_val)
    {
        return min_val;
    }

    s_prng_state = (s_prng_state * 1664525UL) + 1013904223UL;
    span         = (uint32_t)(max_excl - min_val);

    return min_val + (int32_t)((s_prng_state >> 16U) % span);
}

static int32_t prv_map_val(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max)
{
    if (in_max == in_min)
    {
        return out_min;
    }
    if (x < in_min)
    {
        x = in_min;
    }
    else if (x > in_max)
    {
        x = in_max;
    }

    return ((x - in_min) * (out_max - out_min)) / (in_max - in_min) + out_min;
}

/* =========================================================================
 * 4. CÁC THÀNH PHẦN MÀN HÌNH CHÍNH (ZJM)
 * ========================================================================= */

/**
 * @brief Cập nhật t12_temp_display mượt theo chu kỳ đọc ADC thực tế, tránh chạy hết sớm rồi khựng nhịp
 */
static void prv_zjm_update_temp_display(void)
{
    TickType_t now_tick;
    uint32_t   dt_ms;
    int16_t    target_temp;
    int32_t    diff_deg;
    int32_t    step;

    now_tick = xTaskGetTickCount();
    if (s_disp_last_tick == 0U)
    {
        dt_ms = TASK_DISPLAY_UI_PERIOD_MS;
    }
    else
    {
        dt_ms = (uint32_t)((now_tick - s_disp_last_tick) * portTICK_PERIOD_MS);
        if (dt_ms == 0U)
        {
            dt_ms = 1U;
        }
        else if (dt_ms > 100U)
        {
            /* Tránh nhảy vọt sau khi thoát menu/screensaver */
            dt_ms = TASK_DISPLAY_UI_PERIOD_MS;
        }
    }
    s_disp_last_tick = now_tick;

    if (g_sys_state.t12_switch &&
        (g_sys_state.this_error_avg <= 4) &&
        (g_sys_state.this_error_avg >= -4))
    {
        target_temp = g_sys_state.t12_temp_avg;
    }
    else
    {
        target_temp = g_sys_state.t12_temp;
    }

    diff_deg = (int32_t)target_temp - (int32_t)g_sys_state.t12_temp_display;
    if (diff_deg == 0)
    {
        s_disp_last_target = target_temp;
        s_disp_accum_udeg  = 0;
        s_disp_rate_udeg   = 0;
        s_disp_elapsed_ms  = 0U;
        return;
    }

    s_disp_elapsed_ms += dt_ms;

    if ((target_temp != s_disp_last_target) ||
        (s_disp_elapsed_ms >= s_disp_span_ms) ||
        (s_disp_rate_udeg == 0))
    {
        uint32_t span_ms = g_sys_state.t12_temp_read_time;

        if (span_ms < 150U)
        {
            span_ms = 150U;
        }
        else if (span_ms > 850U)
        {
            span_ms = 850U;
        }
        if (span_ms >= 400U)
        {
            /* Bù ~1 nhịp Task_SensorPID (50ms) để số trượt đều sát tới lần đọc ADC tiếp theo */
            span_ms += 50U;
        }

        s_disp_last_target = target_temp;
        s_disp_span_ms     = span_ms;
        s_disp_elapsed_ms  = 0U;

        if (((diff_deg > 0) && (s_disp_accum_udeg < 0)) ||
            ((diff_deg < 0) && (s_disp_accum_udeg > 0)))
        {
            s_disp_accum_udeg = 0;
        }

        s_disp_rate_udeg = (diff_deg * 1000000L) / (int32_t)span_ms;
        if (s_disp_rate_udeg == 0)
        {
            s_disp_rate_udeg = (diff_deg > 0) ? 1000L : -1000L;
        }
    }

    s_disp_accum_udeg += s_disp_rate_udeg * (int32_t)dt_ms;
    step               = s_disp_accum_udeg / 1000000L;

    if (step != 0)
    {
        int32_t next_disp = (int32_t)g_sys_state.t12_temp_display + step;

        s_disp_accum_udeg -= step * 1000000L;
        if (((diff_deg > 0) && (next_disp >= (int32_t)target_temp)) ||
            ((diff_deg < 0) && (next_disp <= (int32_t)target_temp)))
        {
            next_disp         = (int32_t)target_temp;
            s_disp_accum_udeg = 0;
        }
        g_sys_state.t12_temp_display = (int16_t)next_disp;
    }
}

/**
 * @brief 1. Vẽ nhiệt độ mũi hàn bằng font lớn bubble_tn nguyên bản (to rõ, không đè ký hiệu °C)
 */
static void prv_zjm_draw_temp_or_error(int16_t y_off)
{
    u8g2_uint_t temp_x;

    if ((!g_sys_state.t12_error) && (!g_sys_state.t12_adc_error))
    {
        /* Làm mượt nhiệt độ hiển thị trước khi tính tọa độ X để tránh lệch khung 2/3 chữ số */
        prv_zjm_update_temp_display();

        /* Font bubble_tn nguyên bản: cực to, tròn đầy, nét đậm rõ ràng */
        u8g2_SetFont(&g_u8g2, u8g2_font_bubble_tn);
        temp_x = (g_sys_state.t12_temp_display >= 100) ? 0U : 10U;

        ui_draw_int(temp_x, (u8g2_uint_t)(y_off + 18), (int32_t)g_sys_state.t12_temp_display);
        /* Không vẽ vòng tròn độ và chữ C để dành trọn không gian, tránh chạm sang cột giữa */
    }
    else
    {
        bool blink_inv = (((xTaskGetTickCount() / pdMS_TO_TICKS(500U)) & 1U) == 0U);

        u8g2_SetFont(&g_u8g2, u8g2_font_ncenB08_tr);
        if (g_sys_state.t12_adc_error)
        {
            if (blink_inv)
            {
                u8g2_DrawRBox(&g_u8g2, 0U, (u8g2_uint_t)(y_off + 0), 58U, 18U, 2U);
                u8g2_SetDrawColor(&g_u8g2, 0U);
            }
            else
            {
                u8g2_DrawRFrame(&g_u8g2, 0U, (u8g2_uint_t)(y_off + 0), 58U, 18U, 2U);
            }
            u8g2_DrawStr(&g_u8g2, 4U, (u8g2_uint_t)(y_off + 9), "ADC ERR");
            u8g2_DrawStr(&g_u8g2, 8U, (u8g2_uint_t)(y_off + 17), "HIGH");
            u8g2_SetDrawColor(&g_u8g2, 1U);
        }
        else if (g_sys_state.t12_error)
        {
            if (blink_inv)
            {
                u8g2_DrawRBox(&g_u8g2, 0U, (u8g2_uint_t)(y_off + 0), 58U, 18U, 2U);
                u8g2_SetDrawColor(&g_u8g2, 0U);
            }
            else
            {
                u8g2_DrawRFrame(&g_u8g2, 0U, (u8g2_uint_t)(y_off + 0), 58U, 18U, 2U);
            }
            u8g2_DrawStr(&g_u8g2, 6U, (u8g2_uint_t)(y_off + 13), "NO TIP");
            u8g2_SetDrawColor(&g_u8g2, 1U);
        }
    }
}

/**
 * @brief 2. Cài đặt nhiệt độ (Set Temp): Hiển thị SET [giá trị] font đậm ncenB08 kèm thước đo mini mượt mà
 */
static void prv_zjm_draw_slider(int16_t y_off)
{
    int16_t     target_bead_x;
    int16_t     target16;
    u8g2_uint_t bead_x;

    /* 1. Nhãn SET/BST và giá trị nhiệt độ cài đặt cố định font đậm ncenB08 tại X: 0..40, Y: 30 */
    u8g2_SetFont(&g_u8g2, u8g2_font_ncenB08_tr);
    u8g2_DrawStr(&g_u8g2, 0U, (u8g2_uint_t)(y_off + 30), g_sys_state.zjm_boost_ts ? "BST" : "SET");
    ui_draw_int(22U, (u8g2_uint_t)(y_off + 30), (int32_t)g_sys_cfg.set_temp);

    /* 2. Thước đo mini tỉ lệ tại X: 44..58, Y: 24..30 */
    u8g2_DrawHLine(&g_u8g2, 44U, (u8g2_uint_t)(y_off + 26), 14U);
    u8g2_DrawVLine(&g_u8g2, 44U, (u8g2_uint_t)(y_off + 24), 5U);
    u8g2_DrawVLine(&g_u8g2, 57U, (u8g2_uint_t)(y_off + 24), 5U);

    /* Vạch chỉ thị vị trí cài đặt trượt mượt trên thước */
    target_bead_x = (int16_t)prv_map_val((int32_t)g_sys_cfg.set_temp,
                                         (int32_t)g_sys_state.set_temp_min,
                                         (int32_t)g_sys_state.set_temp_max,
                                         45,
                                         56);
    target16 = (int16_t)(target_bead_x * 16);
    if (s_bead_x16 < 0)
    {
        s_bead_x16 = target16;
    }
    else if (s_bead_x16 != target16)
    {
        int16_t diff = (int16_t)(target16 - s_bead_x16);
        int16_t step = diff / 2;
        if (step == 0)
        {
            step = (diff > 0) ? 1 : -1;
        }
        s_bead_x16 += step;
    }

    bead_x = (u8g2_uint_t)((s_bead_x16 + 8) / 16);
    u8g2_DrawBox(&g_u8g2, (u8g2_uint_t)(bead_x - 1U), (u8g2_uint_t)(y_off + 23), 3U, 7U);
}

static const char *prv_zjm_btn_label(uint8_t mode)
{
    if (mode == ZJM_BTN_MODE_BST)
    {
        return "Bst";
    }
    if (mode == ZJM_BTN_MODE_HEAT)
    {
        return "Heat";
    }
    if (mode == ZJM_BTN_MODE_SLP)
    {
        return "Slp";
    }
    return "Stop";
}

static bool prv_set_safe_clip(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > 128) x1 = 128;
    if (y1 > 32) y1 = 32;

    if ((x0 >= x1) || (y0 >= y1))
    {
        return false;
    }

    u8g2_SetClipWindow(&g_u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)y0, (u8g2_uint_t)x1, (u8g2_uint_t)y1);
    return true;
}

/**
 * @brief 3. Vẽ nút bấm Heat/Slp/Stop chữ rõ nét kèm hiệu ứng trượt chữ & tô nền mượt 33 FPS
 */
static void prv_zjm_draw_button_and_anim(int16_t y_off)
{
    const u8g2_uint_t btn_x = 64U;
    const int16_t     btn_y_i = y_off + 1;
    const u8g2_uint_t btn_y = (u8g2_uint_t)btn_y_i;
    const u8g2_uint_t btn_w = 30U;
    const u8g2_uint_t btn_h = 16U;
    const u8g2_uint_t btn_r = 2U;
    uint8_t           cur_mode;
    int16_t           target_fill16;
    int16_t           fill_px;

    /* Animation trượt mượt khi vừa khởi động */
    if (s_zjm_dh_y != 0)
    {
        s_zjm_dh_y--;
        if ((s_zjm_dh_y - 11) >= 0)
        {
            uint16_t text_w   = u8g2_GetStrWidth(&g_u8g2, T12_NAMETAG);
            uint16_t screen_w = u8g2_GetDisplayWidth(&g_u8g2);
            uint16_t x_center = (screen_w - text_w) / 2U;
            u8g2_SetFont(&g_u8g2, u8g2_font_ncenB08_tr);
            u8g2_DrawStr(&g_u8g2, (u8g2_uint_t)x_center, (u8g2_uint_t)(s_zjm_dh_y - 11), T12_NAMETAG);
        }
    }

    if (g_sys_state.t12_switch && (!g_sys_state.zjm_sleep_ts))
    {
        cur_mode = g_sys_state.zjm_boost_ts ? ZJM_BTN_MODE_BST : ZJM_BTN_MODE_HEAT;
    }
    else if (g_sys_state.t12_switch && g_sys_state.zjm_sleep_ts)
    {
        cur_mode = ZJM_BTN_MODE_SLP;
    }
    else
    {
        cur_mode = ZJM_BTN_MODE_STOP;
    }

    /* Xóa cờ animation cũ nếu Input Task vừa đặt */
    g_sys_state.zjm_dh_run = false;

    target_fill16 = ((cur_mode == ZJM_BTN_MODE_HEAT) || (cur_mode == ZJM_BTN_MODE_BST))
                  ? (int16_t)(btn_w * 16U)
                  : 0;

    if (s_btn_last_mode == 0xFFU)
    {
        s_btn_last_mode = cur_mode;
        s_btn_prev_mode = cur_mode;
        s_btn_fill_x16  = target_fill16;
        s_btn_text_dy16 = 0;
    }
    else if (cur_mode != s_btn_last_mode)
    {
        s_btn_prev_mode = s_btn_last_mode;
        s_btn_slide_dir = (cur_mode > s_btn_last_mode) ? 1 : -1;
        s_btn_last_mode = cur_mode;
        s_btn_text_dy16 = (int16_t)(s_btn_slide_dir * 14 * 16);
    }

    /* Nội suy độ lấp đầy bo tròn mượt mà mỗi frame 30ms */
    if (s_btn_fill_x16 != target_fill16)
    {
        int16_t diff = (int16_t)(target_fill16 - s_btn_fill_x16);
        int16_t step = diff / 2;
        if (step == 0)
        {
            step = (diff > 0) ? 1 : -1;
        }
        s_btn_fill_x16 += step;
    }

    /* Nội suy trượt chữ mượt mà bên trong nút */
    if (s_btn_text_dy16 != 0)
    {
        int16_t step = s_btn_text_dy16 / 2;
        if (step == 0)
        {
            step = (s_btn_text_dy16 > 0) ? 1 : -1;
        }
        s_btn_text_dy16 -= step;
    }

    /* 1. Vẽ viền nút bấm bo tròn */
    u8g2_DrawRFrame(&g_u8g2, btn_x, btn_y, btn_w, btn_h, btn_r);

    /* 2. Tô nền bo tròn mượt theo tỉ lệ fill_px (luôn giữ góc bo tròn đẹp) */
    fill_px = (s_btn_fill_x16 + 8) / 16;
    if (fill_px > 0)
    {
        if (prv_set_safe_clip((int16_t)btn_x, btn_y_i, (int16_t)btn_x + fill_px, btn_y_i + (int16_t)btn_h))
        {
            u8g2_DrawRBox(&g_u8g2, btn_x, btn_y, btn_w, btn_h, btn_r);
        }
        u8g2_SetMaxClipWindow(&g_u8g2);
    }

    /* 3. Vẽ chữ Heat / Slp / Stop căn giữa bằng font trong suốt (không nhòe nền) */
    if (prv_set_safe_clip((int16_t)btn_x + 2, btn_y_i + 1, (int16_t)btn_x + (int16_t)btn_w - 2, btn_y_i + (int16_t)btn_h - 1))
    {
        const char *cur_str = prv_zjm_btn_label(cur_mode);
        int16_t     cur_dy  = (s_btn_text_dy16 >= 0) ? ((s_btn_text_dy16 + 8) / 16) : ((s_btn_text_dy16 - 8) / 16);
        uint16_t    cur_w;
        uint16_t    cur_x;

        u8g2_SetFont(&g_u8g2, u8g2_font_ncenB08_tr);
        u8g2_SetFontMode(&g_u8g2, 1U);
        u8g2_SetDrawColor(&g_u8g2, 2U);

        if (cur_dy != 0)
        {
            const char *prev_str = prv_zjm_btn_label(s_btn_prev_mode);
            int16_t     prev_dy  = cur_dy - (int16_t)(s_btn_slide_dir * 14);
            uint16_t    prev_w   = u8g2_GetStrWidth(&g_u8g2, prev_str);
            uint16_t    prev_x   = btn_x + ((btn_w - prev_w) / 2U);
            u8g2_DrawStr(&g_u8g2, prev_x, (u8g2_uint_t)(btn_y_i + 12 + prev_dy), prev_str);
        }

        cur_w = u8g2_GetStrWidth(&g_u8g2, cur_str);
        cur_x = btn_x + ((btn_w - cur_w) / 2U);
        u8g2_DrawStr(&g_u8g2, cur_x, (u8g2_uint_t)(btn_y_i + 12 + cur_dy), cur_str);

        u8g2_SetDrawColor(&g_u8g2, 1U);
        u8g2_SetFontMode(&g_u8g2, 0U);
    }
    u8g2_SetMaxClipWindow(&g_u8g2);
}

/**
 * @brief 4. Thanh đo công suất PWM Bar trượt mượt ở dưới nút Heat/Stop
 */
static void prv_zjm_draw_pwm_bar(int16_t y_off)
{
    const u8g2_uint_t bar_x = 64U;
    const u8g2_uint_t bar_y = (u8g2_uint_t)(y_off + 19);
    const u8g2_uint_t bar_w = 30U;
    const u8g2_uint_t bar_h = 11U;
    int16_t           target_w;
    int16_t           target16;
    int16_t           target_pct;
    int16_t           target_pct16;
    int16_t           fill_w;

    /* Khung ngoài */
    u8g2_DrawFrame(&g_u8g2, bar_x, bar_y, bar_w, bar_h);

    target_w = g_sys_state.t12_switch
             ? (int16_t)(((int32_t)g_sys_state.pid_out_bfb * (int32_t)(bar_w - 4U)) / 100)
             : 0;
    target16 = (int16_t)(target_w * 16);

    if (s_pwm_fill_x16 != target16)
    {
        int16_t diff = (int16_t)(target16 - s_pwm_fill_x16);
        int16_t step = diff / 3;
        if (step == 0)
        {
            step = (diff > 0) ? 1 : -1;
        }
        s_pwm_fill_x16 += step;
    }

    target_pct   = g_sys_state.t12_switch ? (int16_t)g_sys_state.pid_out_bfb : 0;
    target_pct16 = (int16_t)(target_pct * 16);
    if (s_pwm_pct_x16 != target_pct16)
    {
        int16_t diff = (int16_t)(target_pct16 - s_pwm_pct_x16);
        int16_t step = diff / 3;
        if (step == 0)
        {
            step = (diff > 0) ? 1 : -1;
        }
        s_pwm_pct_x16 += step;
    }

    fill_w = (s_pwm_fill_x16 + 8) / 16;
    if (fill_w > 0)
    {
        u8g2_DrawBox(&g_u8g2, (u8g2_uint_t)(bar_x + 2U), (u8g2_uint_t)(bar_y + 2U), (u8g2_uint_t)fill_w, (u8g2_uint_t)(bar_h - 4U));
    }
}

/**
 * @brief 5. Cột thông số phụ bên phải: Nhiệt độ phòng, Điện áp Vin, Phần trăm PWM
 * @note  Sử dụng font chữ đậm ncenB08, không hiển thị °C (tự hiểu) giúp rõ nét, dễ nhìn
 */
static void prv_zjm_draw_side_metrics(int16_t y_off)
{
    int16_t disp_pct;

    u8g2_SetFont(&g_u8g2, u8g2_font_ncenB08_tr);

    /* 1. Nhiệt độ phòng TC1047 (Không hiển thị °C, font đậm ncenB08) */
    if (g_sys_state.tc1047_temp >= 10.0f)
    {
        ui_draw_float(102U, (u8g2_uint_t)(y_off + 9), g_sys_state.tc1047_temp, 1U);
    }
    else
    {
        ui_draw_float(107U, (u8g2_uint_t)(y_off + 9), g_sys_state.tc1047_temp, 1U);
    }

    /* 2. Điện áp nguồn DCIN (V, font đậm ncenB08) */
    if (g_sys_state.vin_error)
    {
        bool blink_inv = (((xTaskGetTickCount() / pdMS_TO_TICKS(500U)) & 1U) == 0U);
        if (blink_inv)
        {
            u8g2_DrawRBox(&g_u8g2, 96U, (u8g2_uint_t)(y_off + 11), 31U, 10U, 1U);
            u8g2_SetDrawColor(&g_u8g2, 0U);
        }
        else
        {
            u8g2_DrawRFrame(&g_u8g2, 96U, (u8g2_uint_t)(y_off + 11), 31U, 10U, 1U);
        }
        u8g2_DrawStr(&g_u8g2, 98U, (u8g2_uint_t)(y_off + 19), "LOW V");
        u8g2_SetDrawColor(&g_u8g2, 1U);
    }
    else
    {
        ui_draw_float(98U, (u8g2_uint_t)(y_off + 20), g_sys_state.vin, 1U);
        u8g2_DrawStr(&g_u8g2, 120U, (u8g2_uint_t)(y_off + 20), "V");
    }

    /* 3. Phần trăm công suất PWM đồng bộ mượt theo PWM Bar (%, font đậm ncenB08) */
    disp_pct = (s_pwm_pct_x16 + 8) / 16;
    if (disp_pct < 0)
    {
        disp_pct = 0;
    }
    else if (disp_pct > 100)
    {
        disp_pct = 100;
    }

    if (disp_pct < 10)
    {
        ui_draw_int(112U, (u8g2_uint_t)(y_off + 31), (int32_t)disp_pct);
    }
    else if (disp_pct < 100)
    {
        ui_draw_int(105U, (u8g2_uint_t)(y_off + 31), (int32_t)disp_pct);
    }
    else
    {
        ui_draw_int(98U, (u8g2_uint_t)(y_off + 31), 100);
    }
    u8g2_DrawStr(&g_u8g2, 120U, (u8g2_uint_t)(y_off + 31), "%");
}

/* =========================================================================
 * 6. CÀI ĐẶT CÁC HÀM VẼ GIAO DIỆN CÔNG KHAI (PUBLIC FUNCTIONS)
 * ========================================================================= */

void ui_boot_screen(void)
{
    uint16_t   screen_w   = u8g2_GetDisplayWidth(&g_u8g2);
    uint16_t   screen_h   = u8g2_GetDisplayHeight(&g_u8g2);
    uint16_t   bar_w_max  = 96U;
    uint16_t   bar_x      = (screen_w - bar_w_max) / 2U; /* 16 */
    uint16_t   bar_y      = 22U;
    uint16_t   bar_h      = 6U;
    TickType_t start_tick = xTaskGetTickCount();
    uint32_t   elapsed_ms = 0U;

    while (elapsed_ms <= BOOT_DURATION_MS)
    {
        uint32_t rem   = (elapsed_ms < BOOT_DURATION_MS) ? (BOOT_DURATION_MS - elapsed_ms) : 0U;
        uint16_t cur_w = (uint16_t)(bar_w_max - ((rem * rem * (uint32_t)bar_w_max) / ((uint32_t)BOOT_DURATION_MS * (uint32_t)BOOT_DURATION_MS)));

        u8g2_ClearBuffer(&g_u8g2);

        /* 1. Viền ngoài công nghệ bo nhẹ */
        u8g2_DrawRFrame(&g_u8g2, 0U, 0U, screen_w, screen_h, 3U);

        /* 2. Tiêu đề hệ thống */
        u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tr);
        u8g2_DrawStr(&g_u8g2, 18U, 10U, "T12 STATION PRO");

        /* 3. Tên nhóm & Phiên bản Firmware */
        u8g2_SetFont(&g_u8g2, u8g2_font_5x7_tr);
        u8g2_DrawStr(&g_u8g2, 26U, 18U, "NHOM 2 - RTOS v2.0");

        /* 4. Thanh tiến trình nạp hệ thống bo tròn */
        u8g2_DrawRFrame(&g_u8g2, bar_x, bar_y, bar_w_max, bar_h, 2U);
        if (cur_w > 4U)
        {
            u8g2_DrawRBox(&g_u8g2, (u8g2_uint_t)(bar_x + 1U), (u8g2_uint_t)(bar_y + 1U), (u8g2_uint_t)(cur_w - 2U), (u8g2_uint_t)(bar_h - 2U), 1U);
        }

        u8g2_SendBuffer(&g_u8g2);
        bsp_watchdog_feed();

        vTaskDelay(pdMS_TO_TICKS(BOOT_STEP_MS));
        elapsed_ms = (uint32_t)((xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS);
    }
}

void ui_draw_main_screen(void)
{
    int16_t y_off = (int16_t)s_zjm_dh_y + ui_get_trans_y();

    /* 1. Nhiệt độ mũi hàn lớn font bubble_tn hoặc hộp báo lỗi */
    prv_zjm_draw_temp_or_error(y_off);

    /* 2. Thanh trượt cài đặt nhiệt độ */
    prv_zjm_draw_slider(y_off);

    /* 3. Nút trạng thái Heat/Slp/Stop kèm animation và micro-animation */
    prv_zjm_draw_button_and_anim(y_off);

    /* 4. Thanh đo công suất PWM Bar */
    prv_zjm_draw_pwm_bar(y_off);

    /* 5. Cột thông số phụ bên phải */
    prv_zjm_draw_side_metrics(y_off);
}

void ui_draw_screensaver(void)
{
    uint16_t   screen_w = u8g2_GetDisplayWidth(&g_u8g2);
    uint16_t   screen_h = u8g2_GetDisplayHeight(&g_u8g2);
    TickType_t now_tick = xTaskGetTickCount();
    uint8_t    i;

    /* 1. Khởi tạo bầu trời 20 ngôi sao với pha nhấp nháy khác nhau */
    if (!s_xp_stars_init)
    {
        for (i = 0U; i < XP_STAR_COUNT; i++)
        {
            s_xp_stars[i].x     = (uint8_t)prv_random_range(0, (int32_t)screen_w);
            s_xp_stars[i].y     = (uint8_t)prv_random_range(0, (int32_t)screen_h);
            s_xp_stars[i].phase = (uint8_t)(i % 6U);
        }
        s_xp_stars_init = true;
        s_xp_star_tick  = now_tick;
    }

    /* Cập nhật pha nhấp nháy chậm mỗi 240ms và dịch chuyển 2 ngôi sao mới mỗi chu kỳ */
    if ((now_tick - s_xp_star_tick) >= pdMS_TO_TICKS(240U))
    {
        s_xp_star_tick = now_tick;
        for (i = 0U; i < XP_STAR_COUNT; i++)
        {
            s_xp_stars[i].phase = (uint8_t)((s_xp_stars[i].phase + 1U) % 6U);
            if (s_xp_stars[i].phase == 0U)
            {
                s_xp_stars[i].x = (uint8_t)prv_random_range(0, (int32_t)screen_w);
                s_xp_stars[i].y = (uint8_t)prv_random_range(0, (int32_t)screen_h);
            }
        }
    }

    /* 2. Làm mượt chuyển động của chữ SLEEP Zzz... tới tọa độ mục tiêu (xp_x, xp_y) */
    if ((g_sys_state.xp_x - s_xp_x_old) > 0)
    {
        s_xp_x_old++;
    }
    else if ((g_sys_state.xp_x - s_xp_x_old) < 0)
    {
        s_xp_x_old--;
    }

    if ((g_sys_state.xp_y - s_xp_y_old) > 0)
    {
        s_xp_y_old++;
    }
    else if ((g_sys_state.xp_y - s_xp_y_old) < 0)
    {
        s_xp_y_old--;
    }

    u8g2_SetFont(&g_u8g2, u8g2_font_5x7_tr);

    /* 3. Vẽ bầu trời sao nhấp nháy dịu mắt (ẩn khi phase == 0 hoặc 5) */
    for (i = 0U; i < XP_STAR_COUNT; i++)
    {
        if ((s_xp_stars[i].phase >= 1U) && (s_xp_stars[i].phase <= 4U))
        {
            u8g2_DrawPixel(&g_u8g2, (u8g2_uint_t)s_xp_stars[i].x, (u8g2_uint_t)s_xp_stars[i].y);
        }
    }

    /* 4. Làm sạch vùng nền cục bộ (không vẽ khung hộp) để sao không chớp đè lên mặt trăng & chữ SLEEP */
    {
        int16_t moon_x = (int16_t)s_xp_x_old;
        int16_t moon_y = (int16_t)s_xp_y_old - 10;
        int16_t clr_x  = (moon_x >= 2) ? (moon_x - 2) : 0;
        int16_t clr_y  = (moon_y >= 1) ? (moon_y - 1) : 0;
        uint32_t t_ms  = (uint32_t)(now_tick * portTICK_PERIOD_MS);
        static const char * const s_zzz_chars[3] = { "z", "Z", "z" };

        u8g2_SetDrawColor(&g_u8g2, 0U);
        u8g2_DrawBox(&g_u8g2, (u8g2_uint_t)clr_x, (u8g2_uint_t)clr_y, 44U, 14U);
        u8g2_SetDrawColor(&g_u8g2, 1U);

        /* Vẽ Icon Trăng khuyết 12x12 và chữ "SLEEP" thanh thoát */
        u8g2_SetBitmapMode(&g_u8g2, 1U);
        u8g2_DrawXBM(&g_u8g2, (u8g2_uint_t)moon_x, (u8g2_uint_t)((moon_y >= 0) ? moon_y : 0), 12U, 12U, s_xp_moon_xbm);
        u8g2_SetBitmapMode(&g_u8g2, 0U);

        u8g2_DrawStr(&g_u8g2, (u8g2_uint_t)(moon_x + 15), (u8g2_uint_t)(s_xp_y_old - 1), "SLEEP");

        /* 5. Hiệu ứng 3 chữ z Z z bay nhẹ lên trên và biến mất theo chu kỳ 1.5s */
        for (i = 0U; i < 3U; i++)
        {
            uint32_t phase_ms = (t_ms + ((uint32_t)i * 500U)) % 1500U;

            if (phase_ms < 1250U)
            {
                int16_t rise = (int16_t)((phase_ms * 9U) / 1250U);
                int16_t sway = ((phase_ms / 250U) & 1U) ? 1 : 0;
                int16_t zx   = moon_x + 43 + ((int16_t)i * 6) + sway;
                int16_t zy   = (int16_t)s_xp_y_old - 1 - rise;

                if (zy >= 6)
                {
                    u8g2_SetFont(&g_u8g2, (i == 1U) ? u8g2_font_6x10_tr : u8g2_font_5x7_tr);
                    u8g2_DrawStr(&g_u8g2, (u8g2_uint_t)zx, (u8g2_uint_t)zy, s_zzz_chars[i]);
                }
            }
        }
        u8g2_SetFont(&g_u8g2, u8g2_font_5x7_tr);
    }
}

void ui_draw_factory_reset(void)
{
    const char *title         = "Resetting...";
    uint16_t    screen_w      = u8g2_GetDisplayWidth(&g_u8g2);
    uint16_t    bar_width_max = screen_w - 20U; /* 108px */
    uint16_t    bar_x         = 10U;
    uint16_t    bar_y         = 22U;
    uint16_t    bar_h         = 6U;
    uint16_t    title_x;
    uint16_t    pct;

    /* Ngắt ngay xung PWM gia nhiệt để đảm bảo an toàn tuyệt đối trước khi xóa Flash */
    hal_pwm_set_heater(0U);

    /* Xóa Page 63 trong Flash nội để khôi phục cài đặt gốc ở lần khởi động kế tiếp */
    (void)hal_storage_factory_reset();

    u8g2_SetFont(&g_u8g2, u8g2_font_6x10_tr);
    title_x = (screen_w - u8g2_GetStrWidth(&g_u8g2, title)) / 2U;

    for (pct = 0U; pct <= 100U; pct += 2U)
    {
        uint16_t current_bar_w = (uint16_t)prv_map_val((int32_t)pct, 0, 100, 0, (int32_t)bar_width_max);

        bsp_watchdog_feed();
        u8g2_ClearBuffer(&g_u8g2);

        /* 1. Tiêu đề Resetting... */
        u8g2_DrawStr(&g_u8g2, (u8g2_uint_t)title_x, 10U, title);

        /* 2. Phần trăm tiến trình */
        u8g2_SetFont(&g_u8g2, u8g2_font_5x7_tr);
        ui_draw_int(52U, 19U, (int32_t)pct);
        u8g2_DrawStr(&g_u8g2, 68U, 19U, "%");

        /* 3. Vẽ khung và ruột thanh progress bar */
        u8g2_DrawRFrame(&g_u8g2, (u8g2_uint_t)bar_x, (u8g2_uint_t)bar_y, (u8g2_uint_t)bar_width_max, (u8g2_uint_t)bar_h, 2U);
        if (current_bar_w > 4U)
        {
            u8g2_DrawRBox(&g_u8g2,
                          (u8g2_uint_t)(bar_x + 1U),
                          (u8g2_uint_t)(bar_y + 1U),
                          (u8g2_uint_t)(current_bar_w - 2U),
                          (u8g2_uint_t)(bar_h - 2U),
                          1U);
        }

        u8g2_SendBuffer(&g_u8g2);
        vTaskDelay(pdMS_TO_TICKS(15U));
    }

    /* Khởi động lại vi điều khiển STM32 bằng hàm chuẩn CMSIS */
    NVIC_SystemReset();
}
