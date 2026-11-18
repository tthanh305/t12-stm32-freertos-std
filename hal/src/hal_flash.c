/**
 * @file    hal_flash.c
 * @brief   Driver Lưu trữ Cấu hình vào Flash Page 63 (0x0800FC00) giả lập EEPROM
 * @author  Dev 2 (UI & Flash) — Nhánh feature/hal-i2c-spi
 * @note    Sử dụng Thư viện chuẩn STM32F10x Standard Peripheral Library (stm32f10x_flash.c)
 *          Tham chiếu nguyên bản: EEPROM.ino (v1.20)
 */

#include "hal_flash.h"
#include <string.h>
#include <stddef.h>

/* =========================================================================
 * 1. HÀM NỘI BỘ (STATIC HELPERS)
 * ========================================================================= */

/**
 * @brief Tính mã kiểm tra toàn vẹn CRC32 (IEEE 802.3) cho cấu trúc SystemConfig_t
 *        (Tính trên toàn bộ các trường đứng trước trường crc32)
 */
static uint32_t prv_calc_config_crc32(const SystemConfig_t *cfg)
{
    const uint8_t *bytes = (const uint8_t *)cfg;
    size_t         len   = offsetof(SystemConfig_t, crc32);
    uint32_t       crc   = 0xFFFFFFFFUL;
    size_t         i;
    uint8_t        bit;

    for (i = 0U; i < len; i++)
    {
        crc ^= (uint32_t)bytes[i];
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1UL) != 0UL)
            {
                crc = (crc >> 1U) ^ 0xEDB88320UL;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return ~crc;
}

/**
 * @brief Nạp bộ thông số cài đặt mặc định chuẩn v1.20 (3.3V STM32) vào cấu trúc cfg
 */
static void prv_load_default_config(SystemConfig_t *cfg)
{
    memset(cfg, 0, sizeof(SystemConfig_t));

    cfg->magic_key    = T12_CONFIG_MAGIC_KEY;
    cfg->p            = 25.0f;
    cfg->i            = 8.7f;
    cfg->d            = 2.6f;
    cfg->sleep_time   = 150U;
    cfg->sleep_temp   = 200U;
    cfg->oled_ld      = 50U;
    cfg->vcc_refer    = 3.30f;
    cfg->vin_refer    = 0.0f;
    cfg->vin_low      = 3.3f;
    cfg->tc1047_refer = 30;
    cfg->t12_kj_jr    = 0U;
    cfg->oled_fx      = 0U;
    cfg->xzbmq_fx     = 0U;
    cfg->sz_temp[0]   = 43U;
    cfg->sz_temp[1]   = 259U;
    cfg->sz_temp[2]   = 382U;
    cfg->sz_temp[3]   = 490U;
    cfg->set_temp     = 320U;
    cfg->boost_temp   = 50U;
    cfg->boost_time   = 30U;
    cfg->buzzer_vol   = 60U;
    cfg->crc32        = prv_calc_config_crc32(cfg);
}

/* =========================================================================
 * 2. CÀI ĐẶT CÁC API CÔNG KHAI (PUBLIC FUNCTIONS)
 * ========================================================================= */

void hal_storage_init_or_load(SystemConfig_t *cfg)
{
    const SystemConfig_t *flash_cfg = (const SystemConfig_t *)T12_FLASH_PAGE63_ADDR;
    uint32_t              expected_crc;

    if (cfg == NULL)
    {
        return;
    }

    /* Kiểm tra cờ khởi tạo magic_key == 0xA55A và mã kiểm tra CRC32 tại Page 63 */
    if (flash_cfg->magic_key == T12_CONFIG_MAGIC_KEY)
    {
        expected_crc = prv_calc_config_crc32(flash_cfg);
        if (flash_cfg->crc32 == expected_crc)
        {
            /* Dữ liệu trong Flash hợp lệ -> Sao chép trực tiếp vào RAM (giống nhánh auto_get == 1) */
            memcpy(cfg, flash_cfg, sizeof(SystemConfig_t));
            return;
        }
    }

    /* Lần đầu nạp code hoặc vừa Reset cài đặt gốc (auto_get == 0) -> Nạp mặc định và ghi xuống Flash */
    prv_load_default_config(cfg);
    (void)hal_storage_save(cfg);
}

bool hal_storage_save(const SystemConfig_t *cfg)
{
    SystemConfig_t  temp_cfg;
    const uint32_t *word_ptr;
    uint32_t        addr;
    size_t          word_count;
    size_t          i;
    FLASH_Status    status;

    if (cfg == NULL)
    {
        return false;
    }

    /* Chuẩn bị bản sao trong RAM với magic_key và CRC32 mới nhất */
    memcpy(&temp_cfg, cfg, sizeof(SystemConfig_t));
    temp_cfg.magic_key     = T12_CONFIG_MAGIC_KEY;
    temp_cfg.reserved_pad  = 0U;
    temp_cfg.reserved_pad2 = 0U;
    temp_cfg.reserved_pad3 = 0U;
    temp_cfg.crc32         = prv_calc_config_crc32(&temp_cfg);

    /* Chống ghi mòn Flash & tránh treo CPU khi dữ liệu không thay đổi */
    if (memcmp((const void *)T12_FLASH_PAGE63_ADDR, &temp_cfg, sizeof(SystemConfig_t)) == 0)
    {
        return true;
    }

    /* Mở khóa Flash và xóa các cờ lỗi tồn đọng */
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    /* Xóa Page 63 (0x0800FC00, kích thước 1KB) */
    status = FLASH_ErasePage(T12_FLASH_PAGE63_ADDR);
    if (status != FLASH_COMPLETE)
    {
        FLASH_Lock();
        return false;
    }

    /* Ghi tuần tự từng từ 32-bit (Word) của cấu trúc SystemConfig_t vào Flash */
    word_ptr   = (const uint32_t *)&temp_cfg;
    addr       = T12_FLASH_PAGE63_ADDR;
    word_count = sizeof(SystemConfig_t) / sizeof(uint32_t);

    for (i = 0U; i < word_count; i++)
    {
        status = FLASH_ProgramWord(addr, word_ptr[i]);
        if (status != FLASH_COMPLETE)
        {
            FLASH_Lock();
            return false;
        }
        addr += sizeof(uint32_t);
    }

    /* Khóa lại bộ điều khiển Flash bảo vệ an toàn dữ liệu */
    FLASH_Lock();

    /* Đối chiếu dữ liệu thực tế trong Flash với bản ghi RAM */
    return (memcmp((const void *)T12_FLASH_PAGE63_ADDR, &temp_cfg, sizeof(SystemConfig_t)) == 0);
}

bool hal_storage_factory_reset(void)
{
    FLASH_Status status;

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    status = FLASH_ErasePage(T12_FLASH_PAGE63_ADDR);

    FLASH_Lock();

    return (status == FLASH_COMPLETE);
}
