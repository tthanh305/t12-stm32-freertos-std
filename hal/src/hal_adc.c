/**
 * @file    hal_adc.c
 * @brief   Driver ADC1 + DMA1_Channel1 quét 3 kênh (PA2: T12_TEMP, PA3: VOL_RE, PA4: TC1047)
 *          sử dụng Thư viện chuẩn STM32F10x Standard Peripheral Library (STD/SPL v3.5.0)
 * @author  Dev 1 (Leader) — Nhánh feature/hal-timer-adc
 * @note    Tham chiếu nguyên bản: analogRead.ino (v1.20)
 */

#include "hal_adc.h"
#include "bsp_board.h"

/* =========================================================================
 * 1. HẰNG SỐ CẤU HÌNH ĐO LƯỜNG ADC & DMA
 * ========================================================================= */

/* Số mẫu lấy trung bình cho mỗi kênh (giống hệt vòng lặp 16 mẫu trong denoiseAnalog() gốc) */
#define HAL_ADC_SAMPLE_COUNT        ( 16U )
#define HAL_ADC_CHANNEL_COUNT       ( 3U )
#define HAL_ADC_DMA_BUF_LEN         ( HAL_ADC_SAMPLE_COUNT * HAL_ADC_CHANNEL_COUNT )

/* Chỉ mục kênh trong mảng DMA thứ tự Rank 1..3 */
#define HAL_ADC_IDX_T12_TEMP        ( 0U ) /* Rank 1: PA2 (ADC_Channel_2) */
#define HAL_ADC_IDX_VOL_RE          ( 1U ) /* Rank 2: PA3 (ADC_Channel_3) */
#define HAL_ADC_IDX_TC1047          ( 2U ) /* Rank 3: PA4 (ADC_Channel_4) */

/* Tỉ lệ cầu phân áp đo nguồn 20V (VOL_RE):
 * - Thiết kế chuẩn 3.3V (R8 = 100K, R10 = 10K): 10 / (100 + 10) = 1 / 11 ≈ 0.090909f
 * - Nếu giữ nguyên trở cũ trên mạch 5V (R8 = 68K, R10 = 10K): 10 / (68 + 10) ≈ 0.128205f */
#define HAL_ADC_VIN_DIVIDER_RATIO   ( 10.0f / (100.0f + 10.0f) )

/* Hệ số quy đổi thang đo 10-bit (0..1023.0f) dùng chung với công thức gốc */
#define HAL_ADC_10BIT_MAX           ( 1023.0f )

/* Bộ đệm DMA tĩnh lưu 16 mẫu x 3 kênh, tự động cập nhật liên tục ở chế độ Circular */
static volatile uint16_t s_adc_dma_buf[HAL_ADC_SAMPLE_COUNT][HAL_ADC_CHANNEL_COUNT];

/* =========================================================================
 * 2. HÀM NỘI BỘ (STATIC HELPER)
 * ========================================================================= */

/**
 * @brief Tính giá trị trung bình 16 mẫu DMA của 1 kênh và chia cho 4.0f
 *        để quy đổi từ ADC 12-bit (0..4095) về thang 10-bit (0.0f..1023.75f)
 *        giúp giữ được độ phân giải mịn 0.25 bước của 12-bit đồng thời tương thích
 *        100% với bộ 4 mốc chuẩn ADC = {10, 350, 650, 950} của bản gốc.
 */
static float prv_adc_get_channel_scaled_10bit(uint8_t ch_idx)
{
    uint32_t sum_12bit = 0U;
    uint8_t  i;

    for (i = 0U; i < HAL_ADC_SAMPLE_COUNT; i++)
    {
        sum_12bit += (uint32_t)s_adc_dma_buf[i][ch_idx];
    }

    /* Trung bình 16 mẫu = sum_12bit / 16.0f;
     * Quy đổi từ 12-bit (4096) về 10-bit (1024) = chia thêm cho 4.0f
     * -> Tổng cộng chia cho (16.0f * 4.0f) = 64.0f */
    return ((float)sum_12bit / 64.0f);
}

/* =========================================================================
 * 3. CÀI ĐẶT CÁC API CÔNG KHAI (PUBLIC FUNCTIONS)
 * ========================================================================= */

void hal_adc_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    DMA_InitTypeDef  DMA_InitStructure;
    ADC_InitTypeDef  ADC_InitStructure;

    /* 1. Bật xung nhịp cho DMA1 (AHB), ADC1 và GPIOA (APB2) */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOA, ENABLE);

    /* Đảm bảo ADCCLK = PCLK2 / 6 = 72MHz / 6 = 12MHz (<= 14MHz) */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* 2. Cấu hình các chân PA2 (T12_TEMP), PA3 (VOL_RE), PA4 (TC1047) làm ngõ vào Analog (AIN) */
    GPIO_InitStructure.GPIO_Pin  = BSP_T12_TEMP_GPIO_PIN |
                                   BSP_VOL_RE_GPIO_PIN   |
                                   BSP_TC1047_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 3. Cấu hình DMA1_Channel1 truyền dữ liệu tự động từ ADC1->DR về s_adc_dma_buf */
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(ADC1->DR);
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)s_adc_dma_buf;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize         = HAL_ADC_DMA_BUF_LEN;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);

    /* Kích hoạt DMA1_Channel1 */
    DMA_Cmd(DMA1_Channel1, ENABLE);

    /* 4. Cấu hình ADC1 ở chế độ quét đa kênh liên tục (Scan + Continuous Mode) */
    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = HAL_ADC_CHANNEL_COUNT;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* 5. Thiết lập thứ tự quét (Rank 1..3) và thời gian lấy mẫu dài nhất (239.5 chu kỳ)
     *    để tụ lấy mẫu nội nạp đầy và triệt tiêu nhiễu cao tần trên đường cặp nhiệt điện:
     *    Thời gian chuyển đổi 1 kênh = (239.5 + 12.5) / 12MHz = 21 us
     *    -> Quét 3 kênh x 16 mẫu chỉ mất ~1.0 ms! */
    ADC_RegularChannelConfig(ADC1, BSP_T12_TEMP_ADC_CHANNEL, 1U, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, BSP_VOL_RE_ADC_CHANNEL,   2U, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, BSP_TC1047_ADC_CHANNEL,   3U, ADC_SampleTime_239Cycles5);

    /* 6. Bật kết nối DMA cho ADC1 và bật nguồn ADC1 */
    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    /* 7. Thực hiện quy trình tự hiệu chuẩn (Self-Calibration) bắt buộc của STM32F10x
     *    (Có kèm bộ đếm timeout để không bị kẹt khi chạy chế độ mô phỏng Keil Simulator) */
    {
        volatile uint32_t calib_timeout = 100000U;
        ADC_ResetCalibration(ADC1);
        while ((ADC_GetResetCalibrationStatus(ADC1) != RESET) && (calib_timeout > 0U))
        {
            calib_timeout--;
        }

        calib_timeout = 100000U;
        ADC_StartCalibration(ADC1);
        while ((ADC_GetCalibrationStatus(ADC1) != RESET) && (calib_timeout > 0U))
        {
            calib_timeout--;
        }
    }

    /* 8. Bắt đầu chuyển đổi ADC liên tục bằng phần mềm */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

float hal_adc_read_t12_scaled(void)
{
    /* Trả về giá trị trung bình 16 mẫu DMA của kênh PA2 đã quy đổi về thang 0..1023.75f */
    return prv_adc_get_channel_scaled_10bit(HAL_ADC_IDX_T12_TEMP);
}

float hal_adc_read_vin(float vcc_ref, float vin_offset)
{
    /* Đọc giá trị trung bình thang 10-bit của kênh đo áp nguồn PA3 */
    float adc_10bit = prv_adc_get_channel_scaled_10bit(HAL_ADC_IDX_VOL_RE);

    /* Tính điện áp tại chân PA3: V_pin = (adc_10bit / 1023.0f) * vcc_ref
     * Suy ra điện áp nguồn 20V: Vin = (V_pin / HAL_ADC_VIN_DIVIDER_RATIO) + vin_offset
     * (Tương đương công thức gốc trong analogRead.ino: (denoiseAnalog(vin_pin) / ratio) * vcc_refer_fl + vin_refer) */
    float v_pin = (adc_10bit * vcc_ref) / HAL_ADC_10BIT_MAX;
    return (v_pin / HAL_ADC_VIN_DIVIDER_RATIO) + vin_offset;
}

float hal_adc_read_tc1047(float vcc_ref)
{
    /* Đọc giá trị trung bình thang 10-bit của kênh cảm biến nhiệt độ phòng TC1047 (PA4) */
    float adc_10bit = prv_adc_get_channel_scaled_10bit(HAL_ADC_IDX_TC1047);

    /* Tính điện áp ngõ ra TC1047 (V): V_out = (adc_10bit / 1023.0f) * vcc_ref
     * Đặc tuyến TC1047: 10mV/°C, tại 0°C xuất 0.5V -> Temp (°C) = (V_out - 0.5f) * 100.0f */
    float v_out = (adc_10bit * vcc_ref) / HAL_ADC_10BIT_MAX;
    return (v_out - 0.5f) * 100.0f;
}
