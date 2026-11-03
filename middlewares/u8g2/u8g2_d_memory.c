/* u8g2_d_memory.c — Cấp phát bộ đệm tĩnh cho màn hình OLED 128x32 (16x4 tiles) trên STM32F103 */
#include "u8g2.h"

uint8_t *u8g2_m_16_4_1(uint8_t *page_cnt)
{
    static uint8_t buf[128];
    *page_cnt = 1;
    return buf;
}

uint8_t *u8g2_m_16_4_2(uint8_t *page_cnt)
{
    static uint8_t buf[256];
    *page_cnt = 2;
    return buf;
}

uint8_t *u8g2_m_16_4_f(uint8_t *page_cnt)
{
    static uint8_t buf[512];
    *page_cnt = 4;
    return buf;
}
