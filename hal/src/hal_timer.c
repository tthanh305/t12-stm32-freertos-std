/**
 * @file    hal_timer.c
 * @brief   Driver TIM3 PWM (PA6: MOSFET Heater 0..2047, PA7: Buzzer) và TIM4 Hardware Encoder (PB6/PB7)
 *          sử dụng Thư viện chuẩn STM32F10x Standard Peripheral Library (STD/SPL v3.5.0)
 * @author  Dev 1 (Leader) — Nhánh feature/hal-timer-adc
 * @note    Tham chiếu nguyên bản: PID.ino, Buzzer.ino, InterruptKey.ino (v1.20)
 */

#include "hal_timer.h"
#include "bsp_board.h"

/* =========================================================================
 * 1. HẰNG SỐ CẤU HÌNH TIMER PWM & ENCODER
 * ========================================================================= */

/* Độ phân giải PWM gia nhiệt mũi hàn T12 (giống hệt pwm_max = 2047 của bản gốc PID.ino) */
#define HAL_TIM3_PWM_PERIOD         ( 2047U )

/* Prescaler mặc định của TIM3 khi còi Buzzer không kêu:
 * F_TIM3 = 72MHz / ((17 + 1) * (2047 + 1)) = 72MHz / 36864 ≈ 1.953 kHz */
#define HAL_TIM3_DEFAULT_PRESCALER  ( 17U )

/* Số xung đếm phần cứng TIM4 (TI12 mode đếm cả 2 pha A & B) trên mỗi khấc xoay của EC11:
 * Encoder EC11 tiêu chuẩn tạo 4 cạnh chuyển mức (4 counts) cho mỗi 1 khấc (detent). */
#define HAL_ENC_COUNTS_PER_STEP     ( 4 )

/* Biến lưu giá trị bộ đếm TIM4->CNT ở lần đọc trước và phần dư tích lũy */
static uint16_t s_last_enc_cnt = 0U;
static int16_t  s_enc_rem      = 0;

/* =========================================================================
 * 2. CÀI ĐẶT CÁC API CÔNG KHAI (PUBLIC FUNCTIONS)
 * ========================================================================= */

void hal_timer_init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef       TIM_OCInitStructure;
    TIM_ICInitTypeDef       TIM_ICInitStructure;

    /* 1. Bật xung nhịp cho TIM3, TIM4 (APB1) và GPIOA, GPIOB (APB2) */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3 | RCC_APB1Periph_TIM4, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    /* ---------------------------------------------------------------------
     * A. CẤU HÌNH TIM3:
     *    - Channel 1 (PA6): PWM điều khiển MOSFET gia nhiệt (0..2047)
     *    - Channel 2 (PA7): PWM điều khiển còi thụ động BUZZ1
     * --------------------------------------------------------------------- */

    /* Cấu hình PA6 (TIM3_CH1) và PA7 (TIM3_CH2) ở chế độ Alternate Function Push-Pull */
    GPIO_InitStructure.GPIO_Pin   = BSP_HEATER_PWM_GPIO_PIN | BSP_BUZZER_PWM_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Cấu hình TimeBase cho TIM3 với Period (ARR) cố định = 2047 */
    TIM_DeInit(TIM3);
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period        = HAL_TIM3_PWM_PERIOD;
    TIM_TimeBaseStructure.TIM_Prescaler     = HAL_TIM3_DEFAULT_PRESCALER;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    /* Cấu hình TIM3 Channel 1 (PA6 - Heater PWM): Mặc định tắt (Pulse = 0) */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 0U;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(TIM3, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);

    /* Cấu hình TIM3 Channel 2 (PA7 - Buzzer PWM): Mặc định tắt (Pulse = 0) */
    TIM_OCInitStructure.TIM_Pulse       = 0U;
    TIM_OC2Init(TIM3, &TIM_OCInitStructure);
    TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);

    /* ---------------------------------------------------------------------
     * B. CẤU HÌNH TIM4:
     *    - Channel 1 (PB6: EC_CLK) & Channel 2 (PB7: EC_DT)
     *    - Chế độ Hardware Encoder Interface (TIM_EncoderMode_TI12)
     * --------------------------------------------------------------------- */

    /* Cấu hình PB6 và PB7 ở chế độ Input Pull-Up (IPU) */
    GPIO_InitStructure.GPIO_Pin   = BSP_EC_CLK_PIN | BSP_EC_DT_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* Cấu hình TimeBase cho TIM4 chạy tự do 16-bit (0..0xFFFF) */
    TIM_DeInit(TIM4);
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period        = 0xFFFFU;
    TIM_TimeBaseStructure.TIM_Prescaler     = 0U;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    /* Kích hoạt giải mã phần cứng 2 pha A/B bằng hàm chuẩn SPL */
    TIM_EncoderInterfaceConfig(TIM4,
                               TIM_EncoderMode_TI12,
                               TIM_ICPolarity_Rising,
                               TIM_ICPolarity_Rising);

    /* Bật bộ lọc số (Digital Input Filter = 0x06) trên cả 2 kênh TI1 & TI2
     * để triệt tiêu hoàn toàn hiện tượng dội tiếp điểm cơ khí của núm vặn EC11 */
    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_ICFilter = 0x06U;
    TIM_ICInitStructure.TIM_Channel  = TIM_Channel_1;
    TIM_ICInit(TIM4, &TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_Channel  = TIM_Channel_2;
    TIM_ICInit(TIM4, &TIM_ICInitStructure);

    /* Xóa bộ đếm về 0 và khởi chạy TIM4 */
    TIM_SetCounter(TIM4, 0U);
    s_last_enc_cnt = 0U;
    s_enc_rem      = 0;
    TIM_Cmd(TIM4, ENABLE);
}

void hal_pwm_set_heater(uint16_t duty_0_to_2047)
{
    if (duty_0_to_2047 >= HAL_TIM3_PWM_PERIOD)
    {
        duty_0_to_2047 = HAL_TIM3_PWM_PERIOD;
    }

    /* Cập nhật thanh ghi CCR1 của TIM3 (Tương đương OCR1B trong hàm pwm_1b() gốc) */
    TIM_SetCompare1(TIM3, duty_0_to_2047);
}

void hal_buzzer_tone_start(uint16_t period_us)
{
    uint32_t psc;

    if (period_us < 50U)
    {
        period_us = 50U;
    }

    /* Giữ nguyên TIM3->ARR = 2047 để tỷ lệ phần trăm Duty Cycle của Heater PWM (TIM3_CH1)
     * không bao giờ bị ảnh hưởng khi còi kêu.
     * Tính Prescaler mới cho TIM3 để tạo đúng tần số âm thanh theo period_us:
     * T_tim3 = ((PSC + 1) * 2048) / 72MHz = period_us (us)
     * => PSC = (72 * period_us) / 2048 - 1 */
    psc = ((uint32_t)72U * (uint32_t)period_us) / (HAL_TIM3_PWM_PERIOD + 1U);
    if (psc > 0U)
    {
        psc -= 1U;
    }
    if (psc > 0xFFFFU)
    {
        psc = 0xFFFFU;
    }

    TIM_PrescalerConfig(TIM3, (uint16_t)psc, TIM_PSCReloadMode_Immediate);

    /* Xuất xung vuông 50% Duty Cycle trên chân PA7 (TIM3_OC2) */
    TIM_SetCompare2(TIM3, (HAL_TIM3_PWM_PERIOD + 1U) / 2U);
}

void hal_buzzer_tone_stop(void)
{
    /* Ngắt xung PWM còi báo trên chân PA7 (TIM3_OC2 = 0) */
    TIM_SetCompare2(TIM3, 0U);

    /* Khôi phục Prescaler mặc định của TIM3 */
    TIM_PrescalerConfig(TIM3, HAL_TIM3_DEFAULT_PRESCALER, TIM_PSCReloadMode_Immediate);
}

int16_t hal_encoder_get_delta(void)
{
    uint16_t now_cnt  = TIM_GetCounter(TIM4);
    int16_t  raw_diff = (int16_t)(now_cnt - s_last_enc_cnt);
    int16_t  steps;

    s_last_enc_cnt = now_cnt;

    if (raw_diff == 0)
    {
        return 0;
    }

    /* Cộng dồn vào phần dư và quy đổi sang số khấc xoay vật lý (1 step = 4 xung TI12) */
    s_enc_rem += raw_diff;
    steps      = s_enc_rem / HAL_ENC_COUNTS_PER_STEP;
    s_enc_rem  = s_enc_rem % HAL_ENC_COUNTS_PER_STEP;

    return steps;
}
