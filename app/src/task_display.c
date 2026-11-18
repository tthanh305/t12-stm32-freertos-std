/**
 * @file    task_display.c
 * @brief   Task hiển thị màn hình OLED SSD1306 128x32 (Chu kỳ cố định 30ms ~ 33 FPS)
 * @author  Dev 2 (UI & Flash) — Nhánh feature/task-display
 * @note    Sử dụng thư viện U8g2 ở chế độ Full-Buffer (512B RAM) và không dùng sprintf tốn Stack
 */

#include "task_display.h"
#include "app_ipc.h"
#include "hal_i2c.h"
#include "u8g2.h"

/* =========================================================================
 * 1. ĐỐI TƯỢNG U8G2 TOÀN CỤC & HIỆU ỨNG CHUYỂN CẢNH (CHIA SẺ VỚI ui_main.c / ui_menu.c)
 * ========================================================================= */
u8g2_t         g_u8g2;
static uint8_t s_last_screen    = UI_SCREEN_ZJM;
static bool    s_last_xp_state  = false;
static int16_t s_screen_trans_y = 0;

/**
 * @brief Lấy độ lệch trục Y hiện tại cho hiệu ứng trượt chuyển màn hình (0 khi đứng yên)
 */
int16_t ui_get_trans_y(void)
{
    return s_screen_trans_y;
}

/* =========================================================================
 * 2. CÁC HÀM TIỆN ÍCH VẼ SỐ NGUYÊN & SỐ THỰC (KHÔNG DÙNG SPRINTF)
 * ========================================================================= */

/**
 * @brief Chuyển số nguyên không dấu 32-bit sang chuỗi ASCII
 */
static uint8_t prv_u32_to_str(uint32_t val, char *buf)
{
    char    rev[10];
    uint8_t idx = 0U;
    uint8_t len = 0U;

    if (val == 0U)
    {
        buf[0] = '0';
        buf[1] = '\0';
        return 1U;
    }

    while ((val > 0U) && (idx < sizeof(rev)))
    {
        rev[idx++] = (char)('0' + (val % 10U));
        val /= 10U;
    }

    while (idx > 0U)
    {
        idx--;
        buf[len++] = rev[idx];
    }
    buf[len] = '\0';

    return len;
}

/**
 * @brief Vẽ số nguyên có dấu tại tọa độ (x, y) mà không dùng sprintf
 */
void ui_draw_int(u8g2_uint_t x, u8g2_uint_t y, int32_t val)
{
    char     buf[16];
    uint8_t  pos = 0U;
    uint32_t uval;

    if (val < 0)
    {
        buf[pos++] = '-';
        uval       = (uint32_t)(-(val + 1)) + 1U;
    }
    else
    {
        uval = (uint32_t)val;
    }

    pos += prv_u32_to_str(uval, &buf[pos]);
    buf[pos] = '\0';

    u8g2_DrawStr(&g_u8g2, x, y, buf);
}

/**
 * @brief Vẽ số thực dấu phẩy động tại tọa độ (x, y) với số chữ số thập phân tùy chọn (0..10)
 *        mà không dùng sprintf/snprintf
 */
void ui_draw_float(u8g2_uint_t x, u8g2_uint_t y, float val, uint8_t decimals)
{
    char     buf[24];
    uint8_t  pos = 0U;
    uint8_t  i;
    uint32_t int_part;

    if (val < 0.0f)
    {
        buf[pos++] = '-';
        val        = -val;
    }

    if (decimals == 0U)
    {
        int_part = (uint32_t)(val + 0.5f);
        pos += prv_u32_to_str(int_part, &buf[pos]);
        buf[pos] = '\0';
        u8g2_DrawStr(&g_u8g2, x, y, buf);
        return;
    }

    if (decimals <= 4U)
    {
        uint32_t scale = 1U;
        uint32_t scaled;
        uint32_t frac_part;
        uint32_t div;

        for (i = 0U; i < decimals; i++)
        {
            scale *= 10U;
        }

        scaled    = (uint32_t)((val * (float)scale) + 0.5f);
        int_part  = scaled / scale;
        frac_part = scaled % scale;

        pos += prv_u32_to_str(int_part, &buf[pos]);
        buf[pos++] = '.';

        div = scale / 10U;
        while (div > 0U)
        {
            buf[pos++] = (char)('0' + ((frac_part / div) % 10U));
            div /= 10U;
        }
        buf[pos] = '\0';
    }
    else
    {
        float frac;

        int_part = (uint32_t)val;
        frac     = val - (float)int_part;

        pos += prv_u32_to_str(int_part, &buf[pos]);
        buf[pos++] = '.';

        for (i = 0U; (i < decimals) && (pos < (sizeof(buf) - 1U)); i++)
        {
            uint8_t digit;

            frac *= 10.0f;
            digit = (uint8_t)frac;
            if (digit > 9U)
            {
                digit = 9U;
            }
            buf[pos++] = (char)('0' + digit);
            frac -= (float)digit;
        }
        buf[pos] = '\0';
    }

    u8g2_DrawStr(&g_u8g2, x, y, buf);
}

/* =========================================================================
 * 3. TIẾN TRÌNH CHÍNH TASK_DISPLAY_UI (CHU KỲ 30MS ~ 33 FPS)
 * ========================================================================= */

static void prvDisplayTask(void *pvParameters)
{
    TickType_t xLastWakeTime;
    uint8_t    last_oled_ld;
    uint8_t    last_oled_fx;
    bool       do_factory_reset;

    (void)pvParameters;

    /* 1. Khởi tạo đối tượng U8g2 ở chế độ Full-Buffer (_f: 512 bytes RAM) */
    last_oled_ld = g_sys_cfg.oled_ld;
    last_oled_fx = g_sys_cfg.oled_fx;

    u8g2_Setup_ssd1306_i2c_128x32_univision_f(&g_u8g2,
                                              (last_oled_fx != 0U) ? U8G2_R2 : U8G2_R0,
                                              u8x8_byte_stm32_hw_i2c,
                                              u8x8_gpio_and_delay_stm32);
    u8g2_InitDisplay(&g_u8g2);
    u8g2_SetPowerSave(&g_u8g2, 0U);
    u8g2_SetContrast(&g_u8g2, last_oled_ld);

    /* 2. Hiển thị màn hình khởi động (kj_display) trong 1.5 giây */
    ui_boot_screen();

    /* Phát chuỗi âm thanh chào mừng G6 - A6 - C7 rõ ràng, êm tai sau màn hình khởi động */
    app_buzzer_play(1568U, 70U);
    app_buzzer_pause(25U);
    app_buzzer_play(1760U, 70U);
    app_buzzer_pause(25U);
    app_buzzer_play(2093U, 130U);

    xLastWakeTime = xTaskGetTickCount();

    /* 3. Vòng lặp làm tươi màn hình cố định mỗi 30ms (~33 FPS) */
    for (;;)
    {
        bool frame_drawn = false;

        do_factory_reset = false;

        /* 3.1. Cập nhật độ sáng/chiều xoay OLED và kiểm tra yêu cầu Reset gốc dưới Mutex */
        if (app_state_lock(5U))
        {
            bool contrast_changed = (g_sys_cfg.oled_ld != last_oled_ld);
            bool rot_changed      = (g_sys_cfg.oled_fx != last_oled_fx);

            g_sys_state.oled_cfg_dirty = false;
            last_oled_ld               = g_sys_cfg.oled_ld;
            last_oled_fx               = g_sys_cfg.oled_fx;

            if (g_sys_state.hfccsz_state && (g_sys_state.szjm_laotie_sxxk_count != 5))
            {
                do_factory_reset = true;
            }

            app_state_unlock();

            if (contrast_changed)
            {
                u8g2_SetContrast(&g_u8g2, last_oled_ld);
            }
            if (rot_changed)
            {
                u8g2_SetDisplayRotation(&g_u8g2, (last_oled_fx != 0U) ? U8G2_R2 : U8G2_R0);
            }
        }

        if (do_factory_reset)
        {
            ui_draw_factory_reset();
        }

        /* 3.3. Chiếm Mutex trước khi xóa/vẽ bộ đệm RAM 512B -> Tránh gửi khung hình trắng nếu Mutex bận */
        if (app_state_lock(10U))
        {
            u8g2_SetMaxClipWindow(&g_u8g2);
            u8g2_ClearBuffer(&g_u8g2);

            if (g_sys_state.xp_state)
            {
                s_last_xp_state  = true;
                s_screen_trans_y = 0;
                ui_draw_screensaver();
            }
            else
            {
                uint8_t cur_screen = g_sys_state.display_count;

                if (s_last_xp_state)
                {
                    s_last_xp_state  = false;
                    s_screen_trans_y = -18;
                    s_last_screen    = cur_screen;
                }
                else if (cur_screen != s_last_screen)
                {
                    /* Vào menu sâu hơn: trượt từ dưới lên (+22px); Thoát về cấp ngoài: trượt từ trên xuống (-22px) */
                    if ((cur_screen == UI_SCREEN_ZJM) ||
                        ((cur_screen == UI_SCREEN_SZJM) && (s_last_screen != UI_SCREEN_ZJM)))
                    {
                        s_screen_trans_y = -22;
                    }
                    else
                    {
                        s_screen_trans_y = 22;
                    }
                    s_last_screen = cur_screen;
                }
                else if (s_screen_trans_y != 0)
                {
                    int16_t step = s_screen_trans_y / 2;
                    if (step == 0)
                    {
                        step = (s_screen_trans_y > 0) ? 1 : -1;
                    }
                    s_screen_trans_y -= step;
                }

                switch (cur_screen)
                {
                    case UI_SCREEN_ZJM:
                        ui_draw_main_screen();
                        break;

                    case UI_SCREEN_SZJM:
                        ui_draw_menu_carousel();
                        break;

                    case UI_SCREEN_SZJM_PID:
                        ui_draw_submenu_pid();
                        break;

                    case UI_SCREEN_SZJM_SLEEP:
                        ui_draw_submenu_sleep();
                        break;

                    case UI_SCREEN_SZJM_OLED:
                        ui_draw_submenu_oled();
                        break;

                    case UI_SCREEN_SZJM_POWER:
                        ui_draw_submenu_power();
                        break;

                    case UI_SCREEN_SZJM_JIAOZHUN:
                        ui_draw_submenu_calib();
                        break;

                    case UI_SCREEN_SZJM_LAOTIE:
                        ui_draw_submenu_iron();
                        break;

                    default:
                        ui_draw_main_screen();
                        break;
                }
            }

            frame_drawn = true;
            app_state_unlock();
        }

        if (frame_drawn)
        {
            u8g2_SendBuffer(&g_u8g2);
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK_DISPLAY_UI_PERIOD_MS));
    }
}

BaseType_t task_display_create(void)
{
    return xTaskCreate(prvDisplayTask,
                       "DisplayUI",
                       TASK_DISPLAY_UI_STACK,
                       NULL,
                       TASK_DISPLAY_UI_PRIO,
                       &xTaskDisplayUiHandle);
}
