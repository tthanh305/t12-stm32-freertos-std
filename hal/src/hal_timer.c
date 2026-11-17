/**
 * @file    hal_timer.c
 * @brief   Driver TIM3 PWM (PA6: MOSFET Heater 0..2047, PA7: Buzzer) và Encoder EC11 (PB6/PB5)
 *          sử dụng Thư viện chuẩn STM32F10x Standard Peripheral Library (STD/SPL v3.5.0)
 * @author  Dev 1 & Dev 3
 * @note    Chân Encoder EC11: PB6 (CLK), PB5 (DT); Nút nhấn SW: PB7.
 *          Kéo lên nội (Internal Pull-Up) bằng code trên tất cả các chân.
 */

#include "hal_timer.h"
#include "bsp_board.h"

/* =========================================================================
 * 1. HẰNG SỐ CẤU HÌNH TIMER PWM
 * ========================================================================= */

/* Độ phân giải PWM gia nhiệt mũi hàn T12 (giống hệt pwm_max = 2047 của bản gốc PID.ino) */
#define HAL_TIM3_PWM_PERIOD         ( 2047U )

/* Prescaler mặc định của TIM3 (dành riêng cho Heater PWM trên PA6):
 * F_TIM3 = 72MHz / ((17 + 1) * (2047 + 1)) = 72MHz / 36864 ≈ 1.953 kHz */
#define HAL_TIM3_DEFAULT_PRESCALER  ( 17U )

/* Prescaler của TIM2 tạo nhịp 1MHz (1us/tick) dành riêng cho Buzzer PA7 */
#define HAL_TIM2_BUZZER_PRESCALER   ( 71U )
#define HAL_BUZZER_MIN_PHASE_US     ( 2U )

/* =========================================================================
 * 2. BIẾN TOÀN CỤC NỘI BỘ GIẢI MÃ ENCODER PB6 (CLK) & PB5 (DT) VÀ BUZZER TIM2
 * ========================================================================= */
static volatile int16_t  s_enc_steps       = 0;
static volatile int8_t   s_enc_accum       = 0;
static volatile uint8_t  s_enc_last        = 0x03U;

static volatile uint16_t s_buzzer_high_arr = 159U;
static volatile uint16_t s_buzzer_low_arr  = 339U;
static volatile uint8_t  s_buzzer_phase    = 0U;

/* Bảng chuyển trạng thái mã Gray chống rung dội tiếp điểm cơ khí */
static const int8_t s_quad_table[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

/**
 * @brief Đọc và giải mã bước xoay Encoder từ cặp chân PB6 (CLK) và PB5 (DT)
 */
static void prv_encoder_update(void)
{
    uint8_t a = (GPIO_ReadInputDataBit(BSP_EC_AB_GPIO_PORT, BSP_EC_CLK_PIN) != Bit_RESET) ? 1U : 0U;
    uint8_t b = (GPIO_ReadInputDataBit(BSP_EC_AB_GPIO_PORT, BSP_EC_DT_PIN)  != Bit_RESET) ? 1U : 0U;
    uint8_t curr = (uint8_t)((a << 1) | b);

    if (curr == s_enc_last)
    {
        return;
    }

    uint8_t idx  = (uint8_t)(((s_enc_last & 0x03U) << 2) | curr);
    s_enc_last   = curr;

    int8_t diff = s_quad_table[idx];
    if (diff != 0)
    {
        s_enc_accum += diff;

        /* Khi núm vặn về vị trí dừng khấc (cả 2 chân kéo lên mức HIGH = 3) */
        if (curr == 0x03U)
        {
            if (s_enc_accum >= 2)
            {
                s_enc_steps++;
            }
            else if (s_enc_accum <= -2)
            {
                s_enc_steps--;
            }
            s_enc_accum = 0;
        }
        else if (s_enc_accum >= 4)
        {
            s_enc_steps++;
            s_enc_accum = 0;
        }
        else if (s_enc_accum <= -4)
        {
            s_enc_steps--;
            s_enc_accum = 0;
        }
    }
}

/* Trình phục vụ ngắt EXTI Line 5..9 (PB5 - DT, PB6 - CLK) */
void EXTI9_5_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line5) != RESET)
    {
        prv_encoder_update();
        EXTI_ClearITPendingBit(EXTI_Line5);
    }
    if (EXTI_GetITStatus(EXTI_Line6) != RESET)
    {
        prv_encoder_update();
        EXTI_ClearITPendingBit(EXTI_Line6);
    }
}

/* Trình phục vụ ngắt TIM2 tạo xung cho còi thụ động BUZZ1 (PA7) độc lập hoàn toàn với TIM3 Heater */
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

        if (s_buzzer_phase != 0U)
        {
            BSP_BUZZER_PWM_GPIO_PORT->BRR = BSP_BUZZER_PWM_GPIO_PIN;
            s_buzzer_phase                = 0U;
            TIM2->ARR                     = s_buzzer_low_arr;
        }
        else
        {
            BSP_BUZZER_PWM_GPIO_PORT->BSRR = BSP_BUZZER_PWM_GPIO_PIN;
            s_buzzer_phase                 = 1U;
            TIM2->ARR                      = s_buzzer_high_arr;
        }
    }
}

/* =========================================================================
 * 3. CÀI ĐẶT CÁC API CÔNG KHAI (PUBLIC FUNCTIONS)
 * ========================================================================= */

void hal_timer_init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef       TIM_OCInitStructure;
    EXTI_InitTypeDef       EXTI_InitStructure;
    NVIC_InitTypeDef       NVIC_InitStructure;

    /* 1. Bật xung nhịp cho TIM2, TIM3 (APB1), GPIOA, GPIOB và AFIO (APB2) */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_TIM3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    /* ---------------------------------------------------------------------
     * A. CẤU HÌNH TIM3 CHỈ DÀNH CHO HEATER PWM (PA6) & TIM2 CHO BUZZER (PA7):
     *    Tách hoàn toàn TIM3->PSC khỏi còi Buzzer để Heater PWM luôn giữ nguyên
     *    tần số ~1.953 kHz, không bị nhảy tần hay reset bộ đếm khi còi kêu.
     * --------------------------------------------------------------------- */

    /* Cấu hình PA6 (TIM3_CH1) ở chế độ Alternate Function Push-Pull */
    GPIO_InitStructure.GPIO_Pin   = BSP_HEATER_PWM_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Cấu hình PA7 (Buzzer) ở chế độ Output Push-Pull, mặc định mức LOW */
    GPIO_ResetBits(BSP_BUZZER_PWM_GPIO_PORT, BSP_BUZZER_PWM_GPIO_PIN);
    GPIO_InitStructure.GPIO_Pin   = BSP_BUZZER_PWM_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BSP_BUZZER_PWM_GPIO_PORT, &GPIO_InitStructure);

    /* Cấu hình TimeBase cố định cho TIM3 (PSC = 17, ARR = 2047) */
    TIM_DeInit(TIM3);
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period        = HAL_TIM3_PWM_PERIOD;
    TIM_TimeBaseStructure.TIM_Prescaler     = HAL_TIM3_DEFAULT_PRESCALER;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    /* Cấu hình TIM3 Channel 1 (PA6 - Heater PWM): Mặc định tắt (Pulse = 0)
     * Tắt Preload trên CH1 để khi gọi hal_pwm_set_heater(0) trước lúc đo ADC,
     * MOSFET ngắt ngay lập tức. */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 0U;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(TIM3, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Disable);

    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);

    /* Cấu hình TIM2 ở 1MHz (1us/tick) cho Buzzer PA7; tắt ARR Preload để đổi nửa chu kỳ trực tiếp trong ISR */
    TIM_DeInit(TIM2);
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period        = 999U;
    TIM_TimeBaseStructure.TIM_Prescaler     = HAL_TIM2_BUZZER_PRESCALER;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_ARRPreloadConfig(TIM2, DISABLE);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, DISABLE);
    TIM_Cmd(TIM2, DISABLE);

    NVIC_InitStructure.NVIC_IRQChannel                   = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 7U;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0U;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* ---------------------------------------------------------------------
     * B. CẤU HÌNH ROTARY ENCODER EC11 (PB6: CLK, PB5: DT) DÙNG EXTI
     *    (Kích hoạt điện trở kéo lên nội - Internal Pull-Up bằng code)
     * --------------------------------------------------------------------- */

    /* Cấu hình PB6 và PB5 ở chế độ Input Pull-Up (IPU) */
    GPIO_InitStructure.GPIO_Pin   = BSP_EC_CLK_PIN | BSP_EC_DT_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BSP_EC_AB_GPIO_PORT, &GPIO_InitStructure);

    /* Thiết lập mức 1 trên thanh ghi ODR để kích hoạt chắc chắn điện trở kéo lên nội */
    GPIO_SetBits(BSP_EC_AB_GPIO_PORT, BSP_EC_CLK_PIN | BSP_EC_DT_PIN);

    /* Ánh xạ chân PB5 và PB6 vào các đường ngắt EXTI Line5 và Line6 */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource5);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource6);

    EXTI_ClearITPendingBit(EXTI_Line5 | EXTI_Line6);
    EXTI_InitStructure.EXTI_Line    = EXTI_Line5 | EXTI_Line6;
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* Cấu hình NVIC ưu tiên ngắt cho EXTI9_5 */
    NVIC_InitStructure.NVIC_IRQChannel                   = EXTI9_5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6U;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0U;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* Khởi tạo trạng thái ban đầu */
    {
        uint8_t a = (GPIO_ReadInputDataBit(BSP_EC_AB_GPIO_PORT, BSP_EC_CLK_PIN) != Bit_RESET) ? 1U : 0U;
        uint8_t b = (GPIO_ReadInputDataBit(BSP_EC_AB_GPIO_PORT, BSP_EC_DT_PIN)  != Bit_RESET) ? 1U : 0U;
        s_enc_last  = (uint8_t)((a << 1) | b);
        s_enc_steps = 0;
        s_enc_accum = 0;
    }
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

/* Mức âm lượng mặc định của còi UI (32% Duty Cycle: âm lượng vừa rõ ràng, không bị chói gắt) */
#define HAL_BUZZER_DEFAULT_DUTY_PCT ( 32U )

void hal_buzzer_tone_start_vol(uint16_t period_us, uint8_t duty_pct)
{
    uint32_t high_us;
    uint32_t low_us;

    if (period_us < 50U)
    {
        period_us = 50U;
    }
    if (duty_pct < 1U)
    {
        duty_pct = 1U;
    }
    else if (duty_pct > 50U)
    {
        duty_pct = 50U;
    }

    high_us = ((uint32_t)period_us * (uint32_t)duty_pct) / 100U;
    if (high_us < HAL_BUZZER_MIN_PHASE_US)
    {
        high_us = HAL_BUZZER_MIN_PHASE_US;
    }
    low_us = (uint32_t)period_us - high_us;
    if (low_us < HAL_BUZZER_MIN_PHASE_US)
    {
        low_us = HAL_BUZZER_MIN_PHASE_US;
    }

    TIM_Cmd(TIM2, DISABLE);
    TIM_ITConfig(TIM2, TIM_IT_Update, DISABLE);

    s_buzzer_high_arr = (uint16_t)(high_us - 1U);
    s_buzzer_low_arr  = (uint16_t)(low_us - 1U);
    s_buzzer_phase    = 1U;

    BSP_BUZZER_PWM_GPIO_PORT->BSRR = BSP_BUZZER_PWM_GPIO_PIN;
    TIM2->CNT                      = 0U;
    TIM2->ARR                      = s_buzzer_high_arr;
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}

void hal_buzzer_tone_start(uint16_t period_us)
{
    hal_buzzer_tone_start_vol(period_us, HAL_BUZZER_DEFAULT_DUTY_PCT);
}

void hal_buzzer_tone_stop(void)
{
    /* Tắt ngắt/bộ đếm TIM2 và kéo PA7 xuống mức thấp */
    TIM_Cmd(TIM2, DISABLE);
    TIM_ITConfig(TIM2, TIM_IT_Update, DISABLE);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    BSP_BUZZER_PWM_GPIO_PORT->BRR = BSP_BUZZER_PWM_GPIO_PIN;
}

int16_t hal_encoder_get_delta(void)
{
    int16_t steps;

    __disable_irq();
    /* Cập nhật và đọc số bước trong vùng Critical để tránh Race Condition với EXTI9_5 */
    prv_encoder_update();
    steps       = s_enc_steps;
    s_enc_steps = 0;
    __enable_irq();

    return steps;
}
