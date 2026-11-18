/**
 * @file    ui_menu.c
 * @brief   Cài đặt Menu cuộn ngang (SZJM) và 6 Menu cài đặt con (PID, Sleep, Scrn, Power, Calib, S.Iron)
 * @author  Dev 2 (UI & Flash) — Nhánh feature/task-display
 * @note    Hỗ trợ chuyển động mượt Ease-Out, Clip Window, Smooth Morphing Cursor và cuộn trang dọc
 */

#include "task_display.h"
#include "app_ipc.h"
#include "u8g2.h"

/* =========================================================================
 * 1. LIÊN KẾT ĐỐI TƯỢNG U8G2 & HÀM TIỆN ÍCH TỪ task_display.c
 * ========================================================================= */
/* =========================================================================
 * 2. HẰNG SỐ TỌA ĐỘ MENU CHÍNH & 6 MENU CON (TỪ DisplaySet.ino)
 * ========================================================================= */

/* Menu chính cuộn ngang (SZJM) */
#define SZJM_Y1                 ( 21U )
#define SZJM_DH_BC              ( 5 )

/* Menu con 1: PID */
#define SZJM_PID_X1             ( 2U )
#define SZJM_PID_X2             ( 14U )
#define SZJM_PID_X3             ( 82U )
#define SZJM_PID_X4             ( 94U )
#define SZJM_PID_Y1             ( 14U )
#define SZJM_PID_Y2             ( 29U )

/* Menu con 2: Sleep */
#define SZJM_SLEEP_X1           ( 2U )
#define SZJM_SLEEP_X2           ( 105U )
#define SZJM_SLEEP_Y1           ( 14U )
#define SZJM_SLEEP_Y2           ( 29U )

/* Menu con 3: OLED (Scrn) */
#define SZJM_OLED_X1            ( 2U )
#define SZJM_OLED_X2            ( 106U )
#define SZJM_OLED_X3            ( 86U )
#define SZJM_OLED_Y1            ( 14U )
#define SZJM_OLED_Y2            ( 29U )

/* Menu con 4: Power */
#define SZJM_POWER_X1           ( 2U )
#define SZJM_POWER_X2           ( 100U )
#define SZJM_POWER_Y1           ( 14U )
#define SZJM_POWER_Y2           ( 29U )

/* Menu con 5: S.Iron (Laotie) */
#define SZJM_LAOTIE_X1          ( 2U )
#define SZJM_LAOTIE_X4          ( 105U )
#define SZJM_LAOTIE_Y1          ( 14U )
#define SZJM_LAOTIE_Y2          ( 29U )

/* Menu con 6: Calib (Jiaozhun) */
#define SZJM_JIAOZHUN_X1        ( 1U )
#define SZJM_JIAOZHUN_X2        ( 34U )
#define SZJM_JIAOZHUN_X3        ( 54U )
#define SZJM_JIAOZHUN_X4        ( 80U )
#define SZJM_JIAOZHUN_X5        ( 106U )
#define SZJM_JIAOZHUN_X6        ( 102U )
#define SZJM_JIAOZHUN_Y1        ( 9U )
#define SZJM_JIAOZHUN_Y2        ( 19U )
#define SZJM_JIAOZHUN_Y3        ( 30U )

/* =========================================================================
 * 3. BIẾN TRẠNG THÁI CHUYỂN ĐỘNG MENU (CAROUSEL, CON TRỎ TRƯỢT, CUỘN TRANG)
 * ========================================================================= */
typedef struct {
    uint8_t last_screen;
    uint8_t last_page;
    bool    active;
    int16_t cur_x16;
    int16_t cur_y16;
    int16_t cur_w16;
    int16_t cur_h16;
} SmoothCursor_t;

typedef struct {
    uint8_t last_screen;
    int16_t cur_y;
} PageScroll_t;

typedef struct {
    uint8_t    last_screen;
    int8_t     last_edit;
    uint32_t   last_toggle_sig;
    TickType_t until_tick;
} SavedToast_t;

static int8_t         s_szjm_last_k    = 0;
static int16_t        s_szjm_anim_dx   = 0;
static int8_t         s_arrow_dir      = 0;
static int16_t        s_arrow_kick_x16 = 0;
static SmoothCursor_t s_cursor         = { 0U, 0U, false, 0, 0, 0, 0 };
static PageScroll_t   s_page_scroll    = { 0U, 0 };
static SavedToast_t   s_saved_toast    = { 0U, 0, 0U, 0U };

/* =========================================================================
 * 4. HÀM NỘI BỘ (STATIC HELPER)
 * ========================================================================= */

/**
 * @brief Đếm số chữ số phần nguyên của một số không dấu (Chuyển từ get_num_digit() trong Buzzer.ino)
 */
static uint8_t prv_get_num_digit(uint32_t num)
{
    uint8_t count = 0U;

    if (num == 0U)
    {
        return 1U;
    }

    while (num != 0U)
    {
        num /= 10U;
        count++;
    }

    return count;
}

/**
 * @brief Nội suy 1 chiều theo fixed-point x16 (giảm 50% sai số mỗi frame ~ 180ms về đích)
 */
static void prv_step_fixed16(int16_t *cur16, int16_t target_px)
{
    int16_t target16 = (int16_t)(target_px * 16);
    int16_t diff     = (int16_t)(target16 - *cur16);

    if ((diff >= -4) && (diff <= 4))
    {
        *cur16 = target16;
    }
    else
    {
        int16_t step = diff / 2;
        if (step == 0)
        {
            step = (diff > 0) ? 1 : -1;
        }
        *cur16 += step;
    }
}

static int16_t prv_fixed16_to_px(int16_t val16)
{
    return (val16 >= 0) ? ((val16 + 8) / 16) : ((val16 - 8) / 16);
}

/**
 * @brief Cập nhật vị trí & kích thước khung con trỏ trượt đàn hồi trong các menu con
 */
static bool prv_update_smooth_cursor(uint8_t  screen_id,
                                     uint8_t  page_id,
                                     bool     has_target,
                                     int16_t  tx,
                                     int16_t  ty,
                                     int16_t  tw,
                                     int16_t  th,
                                     int16_t  page_y_off,
                                     int16_t *out_x,
                                     int16_t *out_y,
                                     int16_t *out_w,
                                     int16_t *out_h)
{
    if (!has_target)
    {
        s_cursor.last_screen = screen_id;
        s_cursor.last_page   = page_id;
        s_cursor.active      = false;
        return false;
    }

    if ((!s_cursor.active) ||
        (s_cursor.last_screen != screen_id) ||
        (s_cursor.last_page != page_id))
    {
        s_cursor.last_screen = screen_id;
        s_cursor.last_page   = page_id;
        s_cursor.active      = true;
        s_cursor.cur_x16     = (int16_t)(tx * 16);
        s_cursor.cur_y16     = (int16_t)(ty * 16);
        s_cursor.cur_w16     = (int16_t)(tw * 16);
        s_cursor.cur_h16     = (int16_t)(th * 16);
    }
    else
    {
        prv_step_fixed16(&s_cursor.cur_x16, tx);
        prv_step_fixed16(&s_cursor.cur_y16, ty);
        prv_step_fixed16(&s_cursor.cur_w16, tw);
        prv_step_fixed16(&s_cursor.cur_h16, th);
    }

    *out_x = prv_fixed16_to_px(s_cursor.cur_x16);
    *out_y = prv_fixed16_to_px(s_cursor.cur_y16) + page_y_off;
    *out_w = prv_fixed16_to_px(s_cursor.cur_w16);
    *out_h = prv_fixed16_to_px(s_cursor.cur_h16);

    return true;
}

/**
 * @brief Cập nhật độ cuộn trang dọc mượt mà (0 cho Trang 1, -32 cho Trang 2, -64 cho Trang 3)
 */
static int16_t prv_update_page_scroll(uint8_t screen_id, uint8_t target_page)
{
    int16_t target_y = -(int16_t)target_page * 32;

    if (s_page_scroll.last_screen != screen_id)
    {
        s_page_scroll.last_screen = screen_id;
        s_page_scroll.cur_y       = target_y;
    }
    else if (s_page_scroll.cur_y != target_y)
    {
        int16_t diff = (int16_t)(target_y - s_page_scroll.cur_y);
        int16_t step = diff / 2;
        if (step == 0)
        {
            step = (diff > 0) ? 1 : -1;
        }
        s_page_scroll.cur_y += step;
    }

    return s_page_scroll.cur_y;
}

/**
 * @brief Vẽ mũi tên chevron thanh thoát (cao 9px) ở mép Carousel
 */
static void prv_draw_chevron(int16_t tip_x, int16_t center_y, int8_t dir, uint8_t thickness)
{
    int8_t dy;

    for (dy = -4; dy <= 4; dy++)
    {
        int16_t ady   = (dy < 0) ? -dy : dy;
        int16_t row_x = (dir < 0) ? (tip_x + ady) : (tip_x - ady - (int16_t)thickness + 1);
        u8g2_DrawHLine(&g_u8g2, (u8g2_uint_t)row_x, (u8g2_uint_t)(center_y + dy), thickness);
    }
}

/**
 * @brief Hiển thị nhanh badge "Saved!" (~450ms) khi vừa thoát chế độ chỉnh sửa hoặc vừa đổi Toggle
 */
static void prv_update_saved_toast(uint8_t screen_id, int8_t cur_edit, uint32_t toggle_sig, bool suppress_save)
{
    TickType_t now_tick = xTaskGetTickCount();

    if (s_saved_toast.last_screen != screen_id)
    {
        s_saved_toast.last_screen     = screen_id;
        s_saved_toast.last_edit       = cur_edit;
        s_saved_toast.last_toggle_sig = toggle_sig;
        s_saved_toast.until_tick      = 0U;
    }
    else
    {
        bool edit_finished  = (s_saved_toast.last_edit > 0) && (cur_edit == 0);
        bool toggle_changed = (toggle_sig != s_saved_toast.last_toggle_sig);

        if ((edit_finished || toggle_changed) && (!suppress_save))
        {
            s_saved_toast.until_tick = now_tick + pdMS_TO_TICKS(450U);
        }
        s_saved_toast.last_edit       = cur_edit;
        s_saved_toast.last_toggle_sig = toggle_sig;
    }

    if ((s_saved_toast.until_tick != 0U) && ((int32_t)(s_saved_toast.until_tick - now_tick) > 0))
    {
        u8g2_SetDrawColor(&g_u8g2, 0U);
        u8g2_DrawRBox(&g_u8g2, 42U, 9U, 44U, 14U, 3U);
        u8g2_SetDrawColor(&g_u8g2, 1U);
        u8g2_DrawRBox(&g_u8g2, 43U, 10U, 42U, 12U, 2U);
        u8g2_SetDrawColor(&g_u8g2, 0U);
        u8g2_SetFont(&g_u8g2, u8g2_font_5x7_tr);
        u8g2_DrawStr(&g_u8g2, 49U, 19U, "Saved!");
        u8g2_SetDrawColor(&g_u8g2, 1U);
    }
    else
    {
        s_saved_toast.until_tick = 0U;
    }
}

/**
 * @brief Vẽ thanh cuộn siêu mảnh 2px ở mép phải cho các menu có nhiều trang (2 hoặc 3 trang)
 */
static void prv_draw_scrollbar_pages(int16_t scroll_y, int16_t trans_y, uint8_t page_count)
{
    int16_t thumb_h   = (page_count >= 3U) ? 10 : 14;
    int16_t max_move  = 30 - thumb_h;
    int16_t max_scrl  = (int16_t)(page_count - 1U) * 32;
    int16_t thumb_y   = 1 + trans_y + ((-scroll_y * max_move) / max_scrl);

    u8g2_DrawVLine(&g_u8g2, 127U, (u8g2_uint_t)(1 + trans_y), 30U);
    u8g2_DrawBox(&g_u8g2, 126U, (u8g2_uint_t)thumb_y, 2U, (u8g2_uint_t)thumb_h);
}

static void prv_draw_scrollbar(int16_t scroll_y, int16_t trans_y)
{
    prv_draw_scrollbar_pages(scroll_y, trans_y, 2U);
}

/* =========================================================================
 * 3b. BỘ ICON PIXEL-ART 16x14 XBM SẮC NÉT CHO 6 MỤC MENU CHÍNH (28B x 6 = 168B)
 * ========================================================================= */
static const uint8_t s_icon_pid_xbm[28] = {
    0x00, 0x00, 0x08, 0x21,
    0x1C, 0x21, 0x1C, 0x71,
    0x1C, 0x71, 0x08, 0x71,
    0x08, 0x21, 0x88, 0x23,
    0x88, 0x23, 0x88, 0x23,
    0x08, 0x21, 0x08, 0x21,
    0x08, 0x21, 0x00, 0x00
};

static const uint8_t s_icon_sleep_xbm[28] = {
    0x00, 0x00, 0xE0, 0x01,
    0xF8, 0x09, 0x7C, 0x1C,
    0x3C, 0x08, 0x1E, 0x00,
    0x1E, 0x10, 0x1E, 0x00,
    0x1E, 0x00, 0x3C, 0x00,
    0xFC, 0x00, 0xF8, 0x03,
    0xE0, 0x03, 0x00, 0x00
};

static const uint8_t s_icon_screen_xbm[28] = {
    0x00, 0x00, 0xFC, 0x3F,
    0x02, 0x40, 0xC2, 0x43,
    0xE2, 0x46, 0xE2, 0x44,
    0xE2, 0x44, 0xE2, 0x46,
    0xC2, 0x43, 0x02, 0x40,
    0xFC, 0x3F, 0x80, 0x01,
    0xF0, 0x0F, 0x00, 0x00
};

static const uint8_t s_icon_power_xbm[28] = {
    0x00, 0x02, 0x00, 0x03,
    0x80, 0x03, 0xC0, 0x03,
    0xE0, 0x01, 0xF0, 0x00,
    0xF8, 0x1F, 0xF8, 0x1F,
    0x80, 0x07, 0xC0, 0x03,
    0xE0, 0x01, 0xE0, 0x00,
    0x60, 0x00, 0x20, 0x00
};

static const uint8_t s_icon_calib_xbm[28] = {
    0x00, 0x00, 0xE0, 0x00,
    0x10, 0x39, 0x50, 0x01,
    0x50, 0x19, 0x50, 0x01,
    0x50, 0x39, 0x50, 0x01,
    0x58, 0x03, 0xE4, 0x04,
    0xE4, 0x04, 0xE4, 0x04,
    0xF8, 0x03, 0xF0, 0x01
};

static const uint8_t s_icon_iron_xbm[28] = {
    0x00, 0x00, 0x00, 0x38,
    0x00, 0x7C, 0x00, 0x3E,
    0x00, 0x1F, 0x80, 0x09,
    0xC0, 0x05, 0xE0, 0x00,
    0x70, 0x00, 0x38, 0x00,
    0x1C, 0x00, 0x06, 0x00,
    0x02, 0x00, 0x00, 0x00
};

/**
 * @brief Vẽ Icon Pixel-Art 16x14 XBM sắc nét cho từng mục trong Carousel
 */
static void prv_draw_menu_icon(uint8_t item_idx, int16_t cx, int16_t top_y)
{
    static const uint8_t * const s_menu_icons[6] = {
        s_icon_pid_xbm,
        s_icon_sleep_xbm,
        s_icon_screen_xbm,
        s_icon_power_xbm,
        s_icon_calib_xbm,
        s_icon_iron_xbm
    };

    if (item_idx < 6U)
    {
        u8g2_SetBitmapMode(&g_u8g2, 1U);
        u8g2_DrawXBM(&g_u8g2, (u8g2_uint_t)(cx - 8), (u8g2_uint_t)top_y, 16U, 14U, s_menu_icons[item_idx]);
        u8g2_SetBitmapMode(&g_u8g2, 0U);
    }
}

/* =========================================================================
 * 5. CÀI ĐẶT CÁC HÀM VẼ MENU CÔNG KHAI (PUBLIC FUNCTIONS)
 * ========================================================================= */

void ui_draw_menu_carousel(void)
{
    static const char * const s_menu_names[6] = {
        "PID", "Sleep", "Screen", "Power", "Calib", "S.Iron"
    };
    int16_t y_off = ui_get_trans_y();
    int8_t  k;
    int8_t  diff;
    uint8_t center_idx;
    uint8_t i;
    int16_t left_arrow_x;
    int16_t right_arrow_x;

    /* 1. Đồng bộ tức thì theo g_sys_state.szjm_kxxk_count (không bao giờ nuốt bước xoay Encoder) */
    k    = (int8_t)(((int16_t)g_sys_state.szjm_kxxk_count % 6 + 6) % 6);
    diff = (int8_t)(k - s_szjm_last_k);
    if (diff > 3)
    {
        diff -= 6;
    }
    else if (diff < -3)
    {
        diff += 6;
    }

    if (diff != 0)
    {
        s_szjm_anim_dx  -= (int16_t)diff * 50;
        s_szjm_last_k    = k;
        s_arrow_dir      = (diff > 0) ? -1 : 1;
        s_arrow_kick_x16 = (int16_t)(3 * 16);
    }

    /* Giảm chấn Ease-Out mượt (~7-8 khung hình = 210-240ms) và trượt từng pixel cuối */
    if (s_szjm_anim_dx != 0)
    {
        int16_t step = (s_szjm_anim_dx * 2) / 5;
        if (step == 0)
        {
            step = (s_szjm_anim_dx > 0) ? 1 : -1;
        }
        s_szjm_anim_dx -= step;
    }

    if (s_arrow_kick_x16 > 0)
    {
        int16_t kstep = s_arrow_kick_x16 / 3;
        if (kstep == 0)
        {
            kstep = 1;
        }
        s_arrow_kick_x16 -= kstep;
        if (s_arrow_kick_x16 == 0)
        {
            s_arrow_dir = 0;
        }
    }

    center_idx = (uint8_t)(((1 - (int16_t)k) % 6 + 6) % 6);

    /* 2. Cắt vùng vẽ an toàn giữa 2 mũi tên điều hướng để Icon/Text trôi khuất mượt mà */
    u8g2_SetClipWindow(&g_u8g2, 11U, 0U, 117U, 32U);

    for (i = 0U; i < 6U; i++)
    {
        int8_t  rel     = (int8_t)(((int16_t)i - (1 - (int16_t)k) + 12) % 6);
        int16_t item_x;
        int16_t item_cx;

        if (rel > 3)
        {
            rel -= 6;
        }

        item_x  = 50 + ((int16_t)rel * 50) + s_szjm_anim_dx;
        item_cx = item_x + 16;

        if ((item_x >= -24) && (item_x <= 128))
        {
            prv_draw_menu_icon(i, item_cx, (int16_t)(3 + y_off));

            u8g2_SetFont(&g_u8g2, u8g2_font_5x7_tr);
            {
                uint16_t text_w = u8g2_GetStrWidth(&g_u8g2, s_menu_names[i]);
                int16_t  text_x = item_cx - (int16_t)(text_w / 2U);

                u8g2_DrawStr(&g_u8g2, (u8g2_uint_t)text_x, (u8g2_uint_t)(26 + y_off), s_menu_names[i]);
            }
        }
    }

    /* Khôi phục vùng vẽ toàn màn hình trước khi vẽ Card, chấm trang và mũi tên */
    u8g2_SetMaxClipWindow(&g_u8g2);

    /* 3. Khung Card bo tròn ở chính giữa làm nổi bật mục đang chọn */
    u8g2_DrawRFrame(&g_u8g2, 37U, (u8g2_uint_t)(1 + y_off), 54U, 28U, 4U);

    /* 4. Thanh chấm chỉ thị vị trí 6 mục ở đáy màn hình */
    for (i = 0U; i < 6U; i++)
    {
        int16_t dot_x = 46 + ((int16_t)i * 7);
        if (i == center_idx)
        {
            u8g2_DrawBox(&g_u8g2, (u8g2_uint_t)(dot_x - 1), (u8g2_uint_t)(30 + y_off), 4U, 2U);
        }
        else
        {
            u8g2_DrawBox(&g_u8g2, (u8g2_uint_t)dot_x, (u8g2_uint_t)(30 + y_off), 2U, 1U);
        }
    }

    /* 5. Mũi tên chevron gọn 5x9px ở 2 bên mép kèm hiệu ứng bật nảy rõ ràng theo chiều xoay */
    {
        int16_t kick_px = (s_arrow_kick_x16 + 8) / 16;
        int16_t l_kick  = (s_arrow_dir < 0) ? kick_px : 0;
        int16_t r_kick  = (s_arrow_dir > 0) ? kick_px : 0;

        left_arrow_x  = 3 - l_kick;
        right_arrow_x = 124 + r_kick;

        prv_draw_chevron(left_arrow_x, (int16_t)(15 + y_off), -1, 2U);
        if (l_kick >= 2)
        {
            prv_draw_chevron((int16_t)(left_arrow_x + 4), (int16_t)(15 + y_off), -1, 1U);
        }

        prv_draw_chevron(right_arrow_x, (int16_t)(15 + y_off), 1, 2U);
        if (r_kick >= 2)
        {
            prv_draw_chevron((int16_t)(right_arrow_x - 4), (int16_t)(15 + y_off), 1, 1U);
        }
    }
}

void ui_draw_submenu_pid(void)
{
    int16_t y_off = ui_get_trans_y();
    int16_t tx = 0;
    int16_t ty = 0;
    int16_t tw = 0;
    int16_t th = 0;
    bool    has_cursor = false;
    int16_t cx, cy, cw, ch;

    u8g2_SetFont(&g_u8g2, u8g2_font_ncenB08_tr);

    /* Hiển thị 3 hệ số P, I, D (1 chữ số thập phân) */
    u8g2_DrawStr(&g_u8g2, SZJM_PID_X1, (u8g2_uint_t)(SZJM_PID_Y1 + y_off), "P");
    ui_draw_float(SZJM_PID_X2, (u8g2_uint_t)(SZJM_PID_Y1 + y_off), g_sys_cfg.p, 1U);

    u8g2_DrawStr(&g_u8g2, SZJM_PID_X3, (u8g2_uint_t)(SZJM_PID_Y1 + y_off), "I");
    ui_draw_float(SZJM_PID_X4, (u8g2_uint_t)(SZJM_PID_Y1 + y_off), g_sys_cfg.i, 1U);

    u8g2_DrawStr(&g_u8g2, SZJM_PID_X1, (u8g2_uint_t)(SZJM_PID_Y2 + y_off), "D");
    ui_draw_float(SZJM_PID_X2, (u8g2_uint_t)(SZJM_PID_Y2 + y_off), g_sys_cfg.d, 1U);

    switch (g_sys_state.szjm_pid_kxxk_count)
    {
        case 1:
            tx         = (int16_t)SZJM_PID_X1 - 2;
            ty         = (int16_t)SZJM_PID_Y1 - 13;
            tw         = 48;
            th         = 16;
            has_cursor = true;
            break;

        case 2:
            tx         = (int16_t)SZJM_PID_X3 - 2;
            ty         = (int16_t)SZJM_PID_Y1 - 13;
            tw         = 48;
            th         = 16;
            has_cursor = true;
            break;

        case 3:
            tx         = (int16_t)SZJM_PID_X1 - 2;
            ty         = (int16_t)SZJM_PID_Y2 - 13;
            tw         = 48;
            th         = 16;
            has_cursor = true;
            break;

        default:
            break;
    }

    if (prv_update_smooth_cursor(UI_SCREEN_SZJM_PID, 0U, has_cursor, tx, ty, tw, th, y_off, &cx, &cy, &cw, &ch))
    {
        if (g_sys_state.szjm_pid_sxxk_count == g_sys_state.szjm_pid_kxxk_count)
        {
            u8g2_SetDrawColor(&g_u8g2, 2U);
            u8g2_DrawBox(&g_u8g2, (u8g2_uint_t)cx, (u8g2_uint_t)cy, (u8g2_uint_t)cw, (u8g2_uint_t)ch);
            if (g_sys_state.dec_state)
            {
                float   cur_val = (g_sys_state.szjm_pid_kxxk_count == 1) ? g_sys_cfg.p
                                : (g_sys_state.szjm_pid_kxxk_count == 2) ? g_sys_cfg.i
                                                                         : g_sys_cfg.d;
                int16_t val_x   = (g_sys_state.szjm_pid_kxxk_count == 2) ? (int16_t)SZJM_PID_X4 : (int16_t)SZJM_PID_X2;
                int16_t line_x  = val_x + 4 + ((int16_t)prv_get_num_digit((uint32_t)cur_val) * 6);
                u8g2_DrawHLine(&g_u8g2, (u8g2_uint_t)line_x, (u8g2_uint_t)(cy + 14), 10U);
            }
            u8g2_SetDrawColor(&g_u8g2, 1U);
        }
        else
        {
            u8g2_DrawRFrame(&g_u8g2, (u8g2_uint_t)cx, (u8g2_uint_t)cy, (u8g2_uint_t)cw, (u8g2_uint_t)ch, 2U);
        }
    }

    prv_update_saved_toast(UI_SCREEN_SZJM_PID, g_sys_state.szjm_pid_sxxk_count, 0U, false);
}

void ui_draw_submenu_sleep(void)
{
    int16_t y_off = ui_get_trans_y();
    int16_t tx = 0;
    int16_t ty = 0;
    bool    has_cursor = false;
    int16_t cx, cy, cw, ch;

    u8g2_SetFont(&g_u8g2, u8g2_font_ncenB08_tr);

    /* Hàng 1: Sleep Time (30..900s hoặc 930 = OFF) */
    u8g2_DrawStr(&g_u8g2, SZJM_SLEEP_X1, (u8g2_uint_t)(SZJM_SLEEP_Y1 + y_off), "Sleep Time");
    if (g_sys_cfg.sleep_time != 930U)
    {
        ui_draw_int(SZJM_SLEEP_X2, (u8g2_uint_t)(SZJM_SLEEP_Y1 + y_off), (int32_t)g_sys_cfg.sleep_time);
    }
    else
    {
        u8g2_DrawStr(&g_u8g2, SZJM_SLEEP_X2, (u8g2_uint_t)(SZJM_SLEEP_Y1 + y_off), "OFF");
    }

    /* Hàng 2: Sleep Temp (°C) */
    u8g2_DrawStr(&g_u8g2, SZJM_SLEEP_X1, (u8g2_uint_t)(SZJM_SLEEP_Y2 + y_off), "Sleep Temp");
    ui_draw_int(SZJM_SLEEP_X2, (u8g2_uint_t)(SZJM_SLEEP_Y2 + y_off), (int32_t)g_sys_cfg.sleep_temp);

    if (g_sys_state.szjm_sleep_kxxk_count == 1)
    {
        tx         = (int16_t)SZJM_SLEEP_X1 - 2;
        ty         = (int16_t)SZJM_SLEEP_Y1 - 13;
        has_cursor = true;
    }
    else if (g_sys_state.szjm_sleep_kxxk_count == 2)
    {
        tx         = (int16_t)SZJM_SLEEP_X1 - 2;
        ty         = (int16_t)SZJM_SLEEP_Y2 - 13;
        has_cursor = true;
    }

    if (prv_update_smooth_cursor(UI_SCREEN_SZJM_SLEEP, 0U, has_cursor, tx, ty, 128, 16, y_off, &cx, &cy, &cw, &ch))
    {
        u8g2_DrawRFrame(&g_u8g2, (u8g2_uint_t)cx, (u8g2_uint_t)cy, (u8g2_uint_t)cw, (u8g2_uint_t)ch, 2U);
        if (g_sys_state.szjm_sleep_sxxk_count == g_sys_state.szjm_sleep_kxxk_count)
        {
            u8g2_SetDrawColor(&g_u8g2, 2U);
            u8g2_DrawBox(&g_u8g2, 101U, (u8g2_uint_t)(cy + 3), 25U, 11U);
            u8g2_SetDrawColor(&g_u8g2, 1U);
        }
    }

    prv_update_saved_toast(UI_SCREEN_SZJM_SLEEP, g_sys_state.szjm_sleep_sxxk_count, 0U, false);
}

void ui_draw_submenu_oled(void)
{
    int16_t trans_y  = ui_get_trans_y();
    uint8_t page_id  = (g_sys_state.szjm_oled_kxxk_count >= 3) ? 1U : 0U;
    int16_t scroll_y = prv_update_page_scroll(UI_SCREEN_SZJM_OLED, page_id);
    int16_t y0       = trans_y + scroll_y;
    int16_t y1       = trans_y + scroll_y + 32;
    int16_t tx = 0;
    int16_t ty = 0;
    bool    has_cursor = false;
    int16_t cx, cy, cw, ch;

    u8g2_SetFont(&g_u8g2, u8g2_font_ncenB08_tr);

    /* Trang 1: Độ sáng (Bright) & Hướng xoay màn hình (Orient 0 / 180) */
    if (scroll_y > -32)
    {
        u8g2_DrawStr(&g_u8g2, SZJM_OLED_X1, (u8g2_uint_t)(SZJM_OLED_Y1 + y0), "Bright");
        if ((g_sys_state.szjm_oled_kxxk_count == 1) && (g_sys_state.szjm_oled_sxxk_count == 1))
        {
            uint16_t fill_w = ((uint16_t)g_sys_cfg.oled_ld * 50U) / 255U;
            u8g2_DrawRFrame(&g_u8g2, 44U, (u8g2_uint_t)(7 + y0), 54U, 7U, 1U);
            if (fill_w > 0U)
            {
                u8g2_DrawBox(&g_u8g2, 46U, (u8g2_uint_t)(9 + y0), (u8g2_uint_t)fill_w, 3U);
            }
        }
        ui_draw_int(SZJM_OLED_X2, (u8g2_uint_t)(SZJM_OLED_Y1 + y0), (int32_t)g_sys_cfg.oled_ld);

        u8g2_DrawStr(&g_u8g2, SZJM_OLED_X1, (u8g2_uint_t)(SZJM_OLED_Y2 + y0), "Orient");
        if (g_sys_cfg.oled_fx == 0U)
        {
            u8g2_DrawStr(&g_u8g2, SZJM_OLED_X2, (u8g2_uint_t)(SZJM_OLED_Y2 + y0), "0");
        }
        else
        {
            u8g2_DrawStr(&g_u8g2, SZJM_OLED_X2, (u8g2_uint_t)(SZJM_OLED_Y2 + y0), "180");
        }
    }

    /* Trang 2: Chiều xoay Encoder (R+L- / R-L+) & Âm lượng còi (Volume 0..100 / OFF) */
    if (scroll_y < 0)
    {
        u8g2_DrawStr(&g_u8g2, SZJM_OLED_X1, (u8g2_uint_t)(SZJM_OLED_Y1 + y1), "Encoder");
        if (g_sys_cfg.xzbmq_fx == 0U)
        {
            u8g2_DrawStr(&g_u8g2, SZJM_OLED_X3, (u8g2_uint_t)(SZJM_OLED_Y1 + y1), "R+L-");
        }
        else
        {
            u8g2_DrawStr(&g_u8g2, SZJM_OLED_X3, (u8g2_uint_t)(SZJM_OLED_Y1 + y1), "R-L+");
        }

        u8g2_DrawStr(&g_u8g2, SZJM_OLED_X1, (u8g2_uint_t)(SZJM_OLED_Y2 + y1), "Volume");
        if ((g_sys_state.szjm_oled_kxxk_count == 4) && (g_sys_state.szjm_oled_sxxk_count == 4))
        {
            uint16_t fill_w = ((uint16_t)g_sys_cfg.buzzer_vol * 50U) / 100U;
            u8g2_DrawRFrame(&g_u8g2, 44U, (u8g2_uint_t)(22 + y1), 54U, 7U, 1U);
            if (fill_w > 0U)
            {
                u8g2_DrawBox(&g_u8g2, 46U, (u8g2_uint_t)(24 + y1), (u8g2_uint_t)fill_w, 3U);
            }
        }
        if (g_sys_cfg.buzzer_vol > 0U)
        {
            ui_draw_int(SZJM_OLED_X2, (u8g2_uint_t)(SZJM_OLED_Y2 + y1), (int32_t)g_sys_cfg.buzzer_vol);
        }
        else
        {
            u8g2_DrawStr(&g_u8g2, SZJM_OLED_X2, (u8g2_uint_t)(SZJM_OLED_Y2 + y1), "OFF");
        }
    }

    switch (g_sys_state.szjm_oled_kxxk_count)
    {
        case 1:
        case 3:
            tx         = (int16_t)SZJM_OLED_X1 - 2;
            ty         = (int16_t)SZJM_OLED_Y1 - 13;
            has_cursor = true;
            break;

        case 2:
        case 4:
            tx         = (int16_t)SZJM_OLED_X1 - 2;
            ty         = (int16_t)SZJM_OLED_Y2 - 13;
            has_cursor = true;
            break;

        default:
            break;
    }

    if (prv_update_smooth_cursor(UI_SCREEN_SZJM_OLED,
                                 page_id,
                                 has_cursor,
                                 tx,
                                 ty,
                                 125,
                                 16,
                                 (page_id == 0U) ? y0 : y1,
                                 &cx,
                                 &cy,
                                 &cw,
                                 &ch))
    {
        u8g2_DrawRFrame(&g_u8g2, (u8g2_uint_t)cx, (u8g2_uint_t)cy, (u8g2_uint_t)cw, (u8g2_uint_t)ch, 2U);
        if (((g_sys_state.szjm_oled_kxxk_count == 1) || (g_sys_state.szjm_oled_kxxk_count == 4)) &&
            (g_sys_state.szjm_oled_sxxk_count == g_sys_state.szjm_oled_kxxk_count))
        {
            u8g2_SetDrawColor(&g_u8g2, 2U);
            u8g2_DrawBox(&g_u8g2, 102U, (u8g2_uint_t)(cy + 3), 22U, 11U);
            u8g2_SetDrawColor(&g_u8g2, 1U);
        }
    }

    prv_draw_scrollbar(scroll_y, trans_y);
    prv_update_saved_toast(UI_SCREEN_SZJM_OLED,
                           g_sys_state.szjm_oled_sxxk_count,
                           ((uint32_t)g_sys_cfg.oled_fx << 1U) | (uint32_t)g_sys_cfg.xzbmq_fx,
                           false);
}

void ui_draw_submenu_power(void)
{
    int16_t trans_y  = ui_get_trans_y();
    uint8_t page_id  = (g_sys_state.szjm_power_kxxk_count == 3) ? 1U : 0U;
    int16_t scroll_y = prv_update_page_scroll(UI_SCREEN_SZJM_POWER, page_id);
    int16_t y0       = trans_y + scroll_y;
    int16_t y1       = trans_y + scroll_y + 32;
    int16_t tx = 0;
    int16_t ty = 0;
    bool    has_cursor = false;
    int16_t cx, cy, cw, ch;

    u8g2_SetFont(&g_u8g2, u8g2_font_ncenB08_tr);

    /* Trang 1: Điện áp tham chiếu (Ref. Voltage) & Bù sai số áp nguồn (Volt. Calib) */
    if (scroll_y > -32)
    {
        u8g2_DrawStr(&g_u8g2, SZJM_POWER_X1, (u8g2_uint_t)(SZJM_POWER_Y1 + y0), "Ref. Voltage");
        ui_draw_float(SZJM_POWER_X2, (u8g2_uint_t)(SZJM_POWER_Y1 + y0), g_sys_cfg.vcc_refer, 2U);

        u8g2_DrawStr(&g_u8g2, SZJM_POWER_X1, (u8g2_uint_t)(SZJM_POWER_Y2 + y0), "Volt. Calib");
        if (g_sys_cfg.vin_refer >= 0.0f)
        {
            u8g2_DrawStr(&g_u8g2, SZJM_POWER_X2 - 6U, (u8g2_uint_t)(SZJM_POWER_Y2 + y0), "+");
            ui_draw_float(SZJM_POWER_X2, (u8g2_uint_t)(SZJM_POWER_Y2 + y0), g_sys_cfg.vin_refer, 2U);
        }
        else
        {
            ui_draw_float(SZJM_POWER_X2 - 6U, (u8g2_uint_t)(SZJM_POWER_Y2 + y0), g_sys_cfg.vin_refer, 2U);
        }
    }

    /* Trang 2: Ngưỡng cảnh báo điện áp thấp (Low V. Alarm) */
    if (scroll_y < 0)
    {
        u8g2_DrawStr(&g_u8g2, SZJM_POWER_X1, (u8g2_uint_t)(SZJM_POWER_Y1 + y1), "Low V. Alarm");
        if (g_sys_cfg.vin_low >= 3.3f)
        {
            ui_draw_float(SZJM_POWER_X2, (u8g2_uint_t)(SZJM_POWER_Y1 + y1), g_sys_cfg.vin_low, 1U);
        }
        else
        {
            u8g2_DrawStr(&g_u8g2, SZJM_POWER_X2, (u8g2_uint_t)(SZJM_POWER_Y1 + y1), "OFF");
        }
    }

    switch (g_sys_state.szjm_power_kxxk_count)
    {
        case 1:
            tx         = (int16_t)SZJM_POWER_X1 - 2;
            ty         = (int16_t)SZJM_POWER_Y1 - 13;
            has_cursor = true;
            break;

        case 2:
            tx         = (int16_t)SZJM_POWER_X1 - 2;
            ty         = (int16_t)SZJM_POWER_Y2 - 13;
            has_cursor = true;
            break;

        case 3:
            tx         = (int16_t)SZJM_POWER_X1 - 2;
            ty         = (int16_t)SZJM_POWER_Y1 - 13;
            has_cursor = true;
            break;

        default:
            break;
    }

    if (prv_update_smooth_cursor(UI_SCREEN_SZJM_POWER,
                                 page_id,
                                 has_cursor,
                                 tx,
                                 ty,
                                 125,
                                 16,
                                 (page_id == 0U) ? y0 : y1,
                                 &cx,
                                 &cy,
                                 &cw,
                                 &ch))
    {
        u8g2_DrawRFrame(&g_u8g2, (u8g2_uint_t)cx, (u8g2_uint_t)cy, (u8g2_uint_t)cw, (u8g2_uint_t)ch, 2U);
        if (g_sys_state.szjm_power_sxxk_count == g_sys_state.szjm_power_kxxk_count)
        {
            u8g2_uint_t box_x = (g_sys_state.szjm_power_kxxk_count == 3) ? 96U : 92U;
            u8g2_uint_t box_w = (g_sys_state.szjm_power_kxxk_count == 3) ? 28U : 32U;
            u8g2_SetDrawColor(&g_u8g2, 2U);
            u8g2_DrawBox(&g_u8g2, box_x, (u8g2_uint_t)(cy + 3), box_w, 11U);
            u8g2_SetDrawColor(&g_u8g2, 1U);
        }
    }

    prv_draw_scrollbar(scroll_y, trans_y);
    prv_update_saved_toast(UI_SCREEN_SZJM_POWER, g_sys_state.szjm_power_sxxk_count, 0U, false);
}

void ui_draw_submenu_calib(void)
{
    int16_t trans_y  = ui_get_trans_y();
    uint8_t page_id  = (g_sys_state.szjm_jiaozhun_kxxk_count == 6) ? 1U : 0U;
    int16_t scroll_y = prv_update_page_scroll(UI_SCREEN_SZJM_JIAOZHUN, page_id);
    int16_t y0       = trans_y + scroll_y;
    int16_t y1       = trans_y + scroll_y + 32;
    int16_t tx = 0;
    int16_t ty = 0;
    int16_t tw = 0;
    int16_t th = 0;
    bool    has_cursor = false;
    int16_t cx, cy, cw, ch;

    u8g2_SetFont(&g_u8g2, u8g2_font_ncenB08_tr);

    /* Trang 1: Bảng 4 mốc chuẩn ADC {10, 350, 650, 950}, nhiệt độ tương ứng, Now ADC và nút Save */
    if (scroll_y > -32)
    {
        u8g2_DrawStr(&g_u8g2, SZJM_JIAOZHUN_X1, (u8g2_uint_t)((int16_t)SZJM_JIAOZHUN_Y1 + y0), "ADC:");
        u8g2_DrawStr(&g_u8g2, SZJM_JIAOZHUN_X2, (u8g2_uint_t)((int16_t)SZJM_JIAOZHUN_Y1 + y0), "10");
        u8g2_DrawStr(&g_u8g2, SZJM_JIAOZHUN_X3, (u8g2_uint_t)((int16_t)SZJM_JIAOZHUN_Y1 + y0), "350");
        u8g2_DrawStr(&g_u8g2, SZJM_JIAOZHUN_X4, (u8g2_uint_t)((int16_t)SZJM_JIAOZHUN_Y1 + y0), "650");
        u8g2_DrawStr(&g_u8g2, SZJM_JIAOZHUN_X5, (u8g2_uint_t)((int16_t)SZJM_JIAOZHUN_Y1 + y0), "950");

        u8g2_DrawStr(&g_u8g2, SZJM_JIAOZHUN_X1 - 1U, (u8g2_uint_t)((int16_t)SZJM_JIAOZHUN_Y2 + y0), "Temp:");
        ui_draw_int(SZJM_JIAOZHUN_X2, (u8g2_uint_t)((int16_t)SZJM_JIAOZHUN_Y2 + y0), (int32_t)g_sys_cfg.sz_temp[0]);
        ui_draw_int(SZJM_JIAOZHUN_X3, (u8g2_uint_t)((int16_t)SZJM_JIAOZHUN_Y2 + y0), (int32_t)g_sys_cfg.sz_temp[1]);
        ui_draw_int(SZJM_JIAOZHUN_X4, (u8g2_uint_t)((int16_t)SZJM_JIAOZHUN_Y2 + y0), (int32_t)g_sys_cfg.sz_temp[2]);
        ui_draw_int(SZJM_JIAOZHUN_X5, (u8g2_uint_t)((int16_t)SZJM_JIAOZHUN_Y2 + y0), (int32_t)g_sys_cfg.sz_temp[3]);

        u8g2_DrawHLine(&g_u8g2, 0U, (u8g2_uint_t)(22 + y0), 128U);

        u8g2_DrawStr(&g_u8g2, SZJM_JIAOZHUN_X1, (u8g2_uint_t)((int16_t)SZJM_JIAOZHUN_Y3 + y0), "Now ADC:");
        ui_draw_float(70U, (u8g2_uint_t)((int16_t)SZJM_JIAOZHUN_Y3 + y0), g_sys_state.t12_ad, 0U);

        u8g2_DrawStr(&g_u8g2, SZJM_JIAOZHUN_X6, (u8g2_uint_t)((int16_t)SZJM_JIAOZHUN_Y3 + y0), "Save");
    }

    /* Trang 2 (Trang ẩn): Hiển thị 4 hệ số đa thức bậc 3 P0..P3 với 10 chữ số thập phân */
    if (scroll_y < 0)
    {
        u8g2_DrawStr(&g_u8g2, 1U, (u8g2_uint_t)(9 + y1), "P0:");
        ui_draw_float(25U, (u8g2_uint_t)(9 + y1), g_sys_state.sz_p[0], 10U);

        u8g2_DrawStr(&g_u8g2, 1U, (u8g2_uint_t)(16 + y1), "P1:");
        ui_draw_float(25U, (u8g2_uint_t)(16 + y1), g_sys_state.sz_p[1], 10U);

        u8g2_DrawStr(&g_u8g2, 1U, (u8g2_uint_t)(24 + y1), "P2:");
        ui_draw_float(25U, (u8g2_uint_t)(24 + y1), g_sys_state.sz_p[2], 10U);

        u8g2_DrawStr(&g_u8g2, 1U, (u8g2_uint_t)(32 + y1), "P3:");
        ui_draw_float(25U, (u8g2_uint_t)(32 + y1), g_sys_state.sz_p[3], 10U);
    }

    switch (g_sys_state.szjm_jiaozhun_kxxk_count)
    {
        case 1:
            tx = (int16_t)SZJM_JIAOZHUN_X2 - 2; ty = (int16_t)SZJM_JIAOZHUN_Y2 - 9; tw = 21; th = 11; has_cursor = true;
            break;
        case 2:
            tx = (int16_t)SZJM_JIAOZHUN_X3 - 2; ty = (int16_t)SZJM_JIAOZHUN_Y2 - 9; tw = 21; th = 11; has_cursor = true;
            break;
        case 3:
            tx = (int16_t)SZJM_JIAOZHUN_X4 - 2; ty = (int16_t)SZJM_JIAOZHUN_Y2 - 9; tw = 21; th = 11; has_cursor = true;
            break;
        case 4:
            tx = (int16_t)SZJM_JIAOZHUN_X5 - 2; ty = (int16_t)SZJM_JIAOZHUN_Y2 - 9; tw = 21; th = 11; has_cursor = true;
            break;
        case 5:
            tx = (int16_t)SZJM_JIAOZHUN_X6 - 2; ty = (int16_t)SZJM_JIAOZHUN_Y3 - 8; tw = 28; th = 9;  has_cursor = true;
            break;
        default:
            break;
    }

    if (prv_update_smooth_cursor(UI_SCREEN_SZJM_JIAOZHUN, page_id, has_cursor, tx, ty, tw, th, y0, &cx, &cy, &cw, &ch))
    {
        if ((g_sys_state.szjm_jiaozhun_kxxk_count == 5) ||
            (g_sys_state.szjm_jiaozhun_sxxk_count == g_sys_state.szjm_jiaozhun_kxxk_count))
        {
            u8g2_SetDrawColor(&g_u8g2, 2U);
            u8g2_DrawBox(&g_u8g2, (u8g2_uint_t)cx, (u8g2_uint_t)cy, (u8g2_uint_t)cw, (u8g2_uint_t)ch);
            u8g2_SetDrawColor(&g_u8g2, 1U);
        }
        else
        {
            u8g2_DrawRFrame(&g_u8g2, (u8g2_uint_t)cx, (u8g2_uint_t)cy, (u8g2_uint_t)cw, (u8g2_uint_t)ch, 2U);
        }
    }

    prv_update_saved_toast(UI_SCREEN_SZJM_JIAOZHUN, g_sys_state.szjm_jiaozhun_sxxk_count, 0U, false);
}

void ui_draw_submenu_iron(void)
{
    int16_t trans_y  = ui_get_trans_y();
    uint8_t page_id  = (g_sys_state.szjm_laotie_kxxk_count >= 5) ? 2U
                     : ((g_sys_state.szjm_laotie_kxxk_count >= 3) ? 1U : 0U);
    int16_t scroll_y = prv_update_page_scroll(UI_SCREEN_SZJM_LAOTIE, page_id);
    int16_t y0       = trans_y + scroll_y;
    int16_t y1       = trans_y + scroll_y + 32;
    int16_t y2       = trans_y + scroll_y + 64;
    int16_t page_y   = (page_id == 0U) ? y0 : ((page_id == 1U) ? y1 : y2);
    int16_t tx = 0;
    int16_t ty = 0;
    bool    has_cursor = false;
    int16_t cx, cy, cw, ch;

    u8g2_SetFont(&g_u8g2, u8g2_font_ncenB08_tr);

    /* Trang 1: Bù nhiệt độ đầu lạnh (Cold End) & Tự nung khi bật nguồn (Auto Heat) */
    if (scroll_y > -32)
    {
        u8g2_DrawStr(&g_u8g2, SZJM_LAOTIE_X1, (u8g2_uint_t)(SZJM_LAOTIE_Y1 + y0), "Cold End");
        ui_draw_int(SZJM_LAOTIE_X4, (u8g2_uint_t)(SZJM_LAOTIE_Y1 + y0), (int32_t)g_sys_cfg.tc1047_refer);

        u8g2_DrawStr(&g_u8g2, SZJM_LAOTIE_X1, (u8g2_uint_t)(SZJM_LAOTIE_Y2 + y0), "Auto Heat");
        if (g_sys_cfg.t12_kj_jr != 0U)
        {
            u8g2_DrawStr(&g_u8g2, SZJM_LAOTIE_X4, (u8g2_uint_t)(SZJM_LAOTIE_Y2 + y0), "Yes");
        }
        else
        {
            u8g2_DrawStr(&g_u8g2, SZJM_LAOTIE_X4, (u8g2_uint_t)(SZJM_LAOTIE_Y2 + y0), "No");
        }
    }

    /* Trang 2: Tăng nhiệt tạm thời (Boost Temp & Boost Time) */
    if ((scroll_y < 0) && (scroll_y > -64))
    {
        u8g2_DrawStr(&g_u8g2, SZJM_LAOTIE_X1, (u8g2_uint_t)(SZJM_LAOTIE_Y1 + y1), "Boost Temp");
        ui_draw_int(SZJM_LAOTIE_X4, (u8g2_uint_t)(SZJM_LAOTIE_Y1 + y1), (int32_t)g_sys_cfg.boost_temp);

        u8g2_DrawStr(&g_u8g2, SZJM_LAOTIE_X1, (u8g2_uint_t)(SZJM_LAOTIE_Y2 + y1), "Boost Time");
        if (g_sys_cfg.boost_time > 0U)
        {
            ui_draw_int(SZJM_LAOTIE_X4, (u8g2_uint_t)(SZJM_LAOTIE_Y2 + y1), (int32_t)g_sys_cfg.boost_time);
        }
        else
        {
            u8g2_DrawStr(&g_u8g2, SZJM_LAOTIE_X4, (u8g2_uint_t)(SZJM_LAOTIE_Y2 + y1), "OFF");
        }
    }

    /* Trang 3: Khôi phục cài đặt gốc (Reset: Yes / No) */
    if (scroll_y < -32)
    {
        u8g2_DrawStr(&g_u8g2, SZJM_LAOTIE_X1, (u8g2_uint_t)(SZJM_LAOTIE_Y1 + y2), "Reset");
        if (g_sys_state.hfccsz_state)
        {
            u8g2_DrawStr(&g_u8g2, SZJM_LAOTIE_X4, (u8g2_uint_t)(SZJM_LAOTIE_Y1 + y2), "Yes");
        }
        else
        {
            u8g2_DrawStr(&g_u8g2, SZJM_LAOTIE_X4, (u8g2_uint_t)(SZJM_LAOTIE_Y1 + y2), "No");
        }
    }

    switch (g_sys_state.szjm_laotie_kxxk_count)
    {
        case 1:
        case 3:
        case 5:
            tx         = (int16_t)SZJM_LAOTIE_X1 - 2;
            ty         = (int16_t)SZJM_LAOTIE_Y1 - 13;
            has_cursor = true;
            break;

        case 2:
        case 4:
            tx         = (int16_t)SZJM_LAOTIE_X1 - 2;
            ty         = (int16_t)SZJM_LAOTIE_Y2 - 13;
            has_cursor = true;
            break;

        default:
            break;
    }

    if (prv_update_smooth_cursor(UI_SCREEN_SZJM_LAOTIE,
                                 page_id,
                                 has_cursor,
                                 tx,
                                 ty,
                                 125,
                                 16,
                                 page_y,
                                 &cx,
                                 &cy,
                                 &cw,
                                 &ch))
    {
        u8g2_DrawRFrame(&g_u8g2, (u8g2_uint_t)cx, (u8g2_uint_t)cy, (u8g2_uint_t)cw, (u8g2_uint_t)ch, 2U);
        if ((g_sys_state.szjm_laotie_kxxk_count != 2) &&
            (g_sys_state.szjm_laotie_sxxk_count == g_sys_state.szjm_laotie_kxxk_count))
        {
            u8g2_SetDrawColor(&g_u8g2, 2U);
            u8g2_DrawBox(&g_u8g2, 101U, (u8g2_uint_t)(cy + 3), 23U, 11U);
            u8g2_SetDrawColor(&g_u8g2, 1U);
        }
    }

    prv_draw_scrollbar_pages(scroll_y, trans_y, 3U);
    prv_update_saved_toast(UI_SCREEN_SZJM_LAOTIE,
                           g_sys_state.szjm_laotie_sxxk_count,
                           (uint32_t)g_sys_cfg.t12_kj_jr,
                           g_sys_state.hfccsz_state);
}
