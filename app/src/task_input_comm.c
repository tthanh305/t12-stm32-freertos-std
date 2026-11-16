/**
 * @file    task_input_comm.c
 * @brief   Cài đặt 3 Task: Task_InputSleep (20ms), Task_Buzzer (Queue) và Task_DebugUART (100ms)
 * @note    TODO [Dev 3 - Nhánh feature/task-comm]: Chuyển đổi từ InterruptKey.ino, Sleep.ino, Buzzer.ino
 */

#include "task_input_comm.h"
#include "app_ipc.h"
#include "hal_gpio.h"
#include "hal_timer.h"
#include "hal_uart.h"
#include "hal_flash.h"

/* =========================================================================
 * 1. TASK INPUT & SLEEP (CHU KỲ CỐ ĐỊNH 20MS)
 * ========================================================================= */

static void prvInputSleepTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    int16_t delta, menu;
    int32_t temp;
    bool sw, sw_old = false, pressed = false;
    bool sensor, sensor_old = hal_gpio_read_sleep_sensor();
    bool motion = false, stand;
    bool long_done = false;
    uint8_t db = 0, clicks = 0, wait = 0, sec = 0;
    uint8_t pos = 0;
    uint16_t hold = 0, sleep_sec = 0;

    const uint8_t menu_list[6] = {
        UI_SCREEN_SZJM_PID, UI_SCREEN_SZJM_SLEEP,
        UI_SCREEN_SZJM_OLED, UI_SCREEN_SZJM_POWER,
        UI_SCREEN_SZJM_JIAOZHUN, UI_SCREEN_SZJM_LAOTIE
    };

    for (;;)
    {
        /* 1. XU LY XOAY ENCODER */
        delta = hal_encoder_get_delta();

        if (delta != 0 && app_state_lock(10))
        {
            // Dao chieu xoay
            if (g_sys_cfg.xzbmq_fx) delta = -delta;

            if (g_sys_state.display_count == UI_SCREEN_ZJM)
            {
                // Danh thuc khi dang Sleep
                if (g_sys_state.zjm_sleep_ts)
                {
                    g_sys_cfg.set_temp = g_sys_state.sleep_temp_cache;
                    g_sys_state.zjm_sleep_ts = false;
                    sleep_sec = 0;
                }

                // Tang/giam nhiet do 10C
                temp = (int32_t)g_sys_cfg.set_temp + delta * 10;

                if (temp > g_sys_state.set_temp_max)
                    temp = g_sys_state.set_temp_min;
                if (temp < g_sys_state.set_temp_min)
                    temp = g_sys_state.set_temp_max;

                g_sys_cfg.set_temp = (uint16_t)temp;
                g_sys_state.sleep_count = 0;
                g_sys_state.xp_state = false;
                g_sys_state.xp_time_count = 0;
            }
            else if (g_sys_state.display_count == UI_SCREEN_SZJM)
            {
                // Chon 1 trong 6 menu
                menu = g_sys_state.szjm_kxxk_count + delta;
                g_sys_state.szjm_kxxk_count = (menu % 6 + 6) % 6;
            }

            // TODO: Xu ly xoay trong 6 menu con

            app_state_unlock();
            app_buzzer_send(100, 70);
        }

        /* 2. XU LY NUT NHAN ENCODER */
        sw = hal_gpio_read_encoder_sw();

        // Chong doi nut nhan
        db = (sw == sw_old) ? (db < 3 ? db + 1 : 3) : 0;
        sw_old = sw;

        if (db >= 3 && sw != pressed)
        {
            pressed = sw;

            if (!pressed && !long_done)
            {
                if (clicks < 2) clicks++;
                wait = 0;
            }
        }

        // Nhan giu 800ms
        if (pressed)
        {
            if (hold < 40) hold++;

            if (hold >= 40 && !long_done)
            {
                long_done = true;
                clicks = 0;
                app_buzzer_send(150, 720);

                if (app_state_lock(10))
                {
                    if (g_sys_state.display_count == UI_SCREEN_ZJM)
                        g_sys_state.display_count = UI_SCREEN_SZJM;
                    else if (g_sys_state.display_count != UI_SCREEN_SZJM_JIAOZHUN)
                        g_sys_state.display_count = UI_SCREEN_ZJM;

                    app_state_unlock();
                }
            }
        }
        else
        {
            hold = 0;
            long_done = false;
        }

        // Nhan don va nhan dup
        if (clicks && !pressed && ++wait >= 15)
        {
            if (app_state_lock(10))
            {
                if (clicks == 2 &&
                    g_sys_state.display_count == UI_SCREEN_ZJM &&
                    !g_sys_state.vin_error &&
                    !g_sys_state.t12_error &&
                    !g_sys_state.t12_adc_error)
                {
                    // Bat/tat gia nhiet
                    g_sys_state.t12_switch = !g_sys_state.t12_switch;
                    g_sys_state.zjm_dh_run = true;

                    if (g_sys_state.zjm_sleep_ts)
                    {
                        g_sys_cfg.set_temp = g_sys_state.sleep_temp_cache;
                        g_sys_state.zjm_sleep_ts = false;
                    }

                    g_sys_state.sleep_count = 0;
                    sleep_sec = 0;
                }
                else if (clicks == 1 &&
                         g_sys_state.display_count == UI_SCREEN_SZJM)
                {
                    // Nhan don de vao menu con
                    menu = g_sys_state.szjm_kxxk_count;

                    if (menu >= 0 && menu < 6)
                        g_sys_state.display_count = menu_list[menu];
                }

                // TODO: Xu ly nhan don trong 6 menu con

                app_state_unlock();
            }

            clicks = 0;
            wait = 0;
        }

        /* 3. QUAN LY SLEEP VA SCREENSAVER */

        // Doc cam bien rung moi 20ms
        sensor = hal_gpio_read_sleep_sensor();
        if (sensor != sensor_old) motion = true;
        sensor_old = sensor;

        // Xu ly moi 50 chu ky = 1 giay
        if (++sec >= 50)
        {
            sec = 0;
            stand = hal_gpio_read_stand_vb();

            if (app_state_lock(10))
            {
                if (g_sys_state.t12_switch)
                {
                    // Reset thoi gian khi co chuyen dong
                    if (motion)
                    {
                        g_sys_state.sleep_count = 0;

                        if (g_sys_state.zjm_sleep_ts && !stand)
                        {
                            g_sys_cfg.set_temp = g_sys_state.sleep_temp_cache;
                            g_sys_state.zjm_sleep_ts = false;
                            sleep_sec = 0;
                            app_buzzer_send(150, 300);
                        }
                    }
                    else if (g_sys_state.sleep_count < 1801)
                        g_sys_state.sleep_count++;

                    // Xu ly che do Sleep OFF
                    if (g_sys_cfg.sleep_time == 930 &&
                        g_sys_state.zjm_sleep_ts)
                    {
                        g_sys_cfg.set_temp = g_sys_state.sleep_temp_cache;
                        g_sys_state.zjm_sleep_ts = false;
                        sleep_sec = 0;
                    }

                    // Tu dong vao Sleep
                    if (g_sys_cfg.sleep_time != 930 &&
                        !g_sys_state.zjm_sleep_ts &&
                        (stand || g_sys_state.sleep_count >= g_sys_cfg.sleep_time))
                    {
                        g_sys_state.sleep_temp_cache = g_sys_cfg.set_temp;
                        g_sys_cfg.set_temp = g_sys_cfg.sleep_temp;
                        g_sys_state.zjm_sleep_ts = true;
                        sleep_sec = 0;
                        app_buzzer_send(150, 300);
                    }

                    // Tu tat gia nhiet sau 1800s ngu
                    if (g_sys_state.zjm_sleep_ts && ++sleep_sec >= 1800)
                    {
                        g_sys_state.t12_switch = false;
                        g_sys_cfg.set_temp = g_sys_state.sleep_temp_cache;
                        g_sys_state.zjm_sleep_ts = false;
                        g_sys_state.sleep_count = 0;
                        sleep_sec = 0;
                        app_buzzer_send(150, 1000);
                    }

                    g_sys_state.xp_time_count = 0;
                    g_sys_state.xp_state = false;
                    pos = 0;
                }
                else
                {
                    // Dung gia nhiet: dem 180s
                    if (g_sys_state.display_count != UI_SCREEN_SZJM_JIAOZHUN)
                    {
                        if (g_sys_state.xp_time_count < 180)
                            g_sys_state.xp_time_count++;

                        if (g_sys_state.xp_time_count >= 180)
                            g_sys_state.xp_state = true;
                    }

                    // Doi vi tri Screensaver moi 3 giay
                    if (g_sys_state.xp_state && ++pos >= 3)
                    {
                        pos = 0;
                        g_sys_state.xp_x = (g_sys_state.xp_x + 17) % 100;
                        g_sys_state.xp_y = (g_sys_state.xp_y + 7) % 24;
                    }
                }

                app_state_unlock();
            }

            motion = false;
        }

        // Duy tri chu ky 20ms
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK_INPUT_SLEEP_PERIOD_MS));
    }
}

/* =========================================================================
 * 2. TASK BUZZER (CHỜ SỰ KIỆN TỪ xBuzzerQueue)
 * ========================================================================= */

static void prvBuzzerTask(void *pvParameters)
{
    (void)pvParameters;
    BuzzerMsg_t msg;
    uint32_t duration_ms;
    TickType_t last_warn = xTaskGetTickCount();
    TickType_t now, wait_time;
    bool error;

    for (;;)
    {
        // Kiem tra trang thai loi
        error = false;
        if (app_state_lock(10))
        {
            error = g_sys_state.vin_error ||
                    g_sys_state.t12_adc_error;
            app_state_unlock();
        }

        // Tinh thoi gian cho toi lan canh bao tiep theo
        now = xTaskGetTickCount();
        wait_time = pdMS_TO_TICKS(100);

        if (error)
        {
            if ((now - last_warn) >= pdMS_TO_TICKS(1000))
            {
                // Phat coi canh bao trong 200ms
                hal_buzzer_tone_start(300);
                vTaskDelay(pdMS_TO_TICKS(200));
                hal_buzzer_tone_stop();

                last_warn = now;
                continue;
            }

            if ((now - last_warn) + wait_time >
                pdMS_TO_TICKS(1000))
            {
                wait_time = pdMS_TO_TICKS(1000) -
                            (now - last_warn);
            }
        }
        else
        {
            // Khi het loi, bat dau lai chu ky canh bao
            last_warn = now;
        }

        // Nhan yeu cau phat bip tu Queue
        if (xQueueReceive(xBuzzerQueue, &msg, wait_time) == pdTRUE)
        {
            // Tinh thoi gian phat am
            duration_ms = (2UL * msg.half_period_us *
                           msg.cycle_count) / 1000UL;

            if (duration_ms == 0U)
                duration_ms = 1U;

            // Phat am thanh theo yeu cau
            hal_buzzer_tone_start(
                (uint16_t)(2U * msg.half_period_us));

            vTaskDelay(pdMS_TO_TICKS(duration_ms));
            hal_buzzer_tone_stop();
        }
    }
}


/* =========================================================================
 * 3. TASK DEBUG UART (CHU KỲ 100MS)
 * ========================================================================= */
static void prvDebugUartTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
	  SystemState_t state;
    uint16_t set_temp;

    for (;;)
    {
        /* Doc trang thai he thong */
        if (app_state_lock(10))
        {
            state = g_sys_state;
            set_temp = g_sys_cfg.set_temp;
            app_state_unlock();

            /* Gui thong so qua UART */
            hal_uart_printf(
                "[T12] Mode:%s | Set:%uC | Temp:%dC "
                "| ADC:%.1f | PWM:%u%% | Vin:%.1fV "
                "| Amb:%.1fC | Err:%u%u%u\r\n",
                state.zjm_sleep_ts ? "Slp" :
                (state.t12_switch ? "Heat" : "Stop"),
                (unsigned int)set_temp,
                (int)state.t12_temp,
                (double)state.t12_ad,
                (unsigned int)state.pid_out_bfb,
                (double)state.vin,
                (double)state.tc1047_temp,
                (unsigned int)state.vin_error,
                (unsigned int)state.t12_error,
                (unsigned int)state.t12_adc_error
            );
        }

        /* Lap lai moi 100ms */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK_DEBUG_UART_PERIOD_MS));
    }
}

/* =========================================================================
 * 4. HÀM KHỞI TẠO CÁC TASK CỦA DEV 3
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

