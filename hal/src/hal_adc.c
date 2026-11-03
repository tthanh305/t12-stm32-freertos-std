/**
 * @file    hal_adc.c
 * @brief   Driver ADC1 + DMA1_Channel1 (3 kênh: PA2 T12_TEMP, PA3 VOL_RE, PA4 TC1047)
 * @note    TODO [Dev 1 - Nhánh feature/hal-timer-adc]: Cài đặt chi tiết bằng stm32f10x_adc.c & stm32f10x_dma.c
 */

#include "hal_adc.h"
#include "bsp_board.h"

/* Bộ đệm DMA lưu 16 mẫu x 3 kênh ADC1 */
static volatile uint16_t s_adc_dma_buf[16][3];

void hal_adc_init(void)
{
    /* TODO [Dev 1]: Cấu hình GPIO_Mode_AIN cho PA2, PA3, PA4;
     * cấu hình DMA1_Channel1 Circular Mode và ADC1 Scan + Continuous Mode */
    (void)s_adc_dma_buf;
}

float hal_adc_read_t12_scaled(void)
{
    /* TODO [Dev 1]: Tính trung bình 16 mẫu kênh 0 (PA2) và chia 4.0f về thang 0..1023.75f */
    return 0.0f;
}

float hal_adc_read_vin(float vcc_ref, float vin_offset)
{
    /* TODO [Dev 1]: Tính trung bình 16 mẫu kênh 1 (PA3) theo cầu phân áp R8=100K / R10=10K */
    (void)vcc_ref;
    (void)vin_offset;
    return 20.0f;
}

float hal_adc_read_tc1047(float vcc_ref)
{
    /* TODO [Dev 1]: Tính trung bình 16 mẫu kênh 2 (PA4): ((Vout - 0.5f) * 100.0f) */
    (void)vcc_ref;
    return 28.0f;
}

