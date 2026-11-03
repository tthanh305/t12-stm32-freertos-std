/**
 * @file    hal_flash.c
 * @brief   Driver Lưu trữ Cấu hình vào Flash Page 63 (0x0800FC00) giả lập EEPROM
 * @note    TODO [Dev 2 - Nhánh feature/hal-i2c-spi]: Cài đặt chi tiết bằng stm32f10x_flash.c (Tham chiếu EEPROM.ino)
 */

#include "hal_flash.h"
#include <string.h>

void hal_storage_init_or_load(SystemConfig_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    /* TODO [Dev 2]: Đọc từ 0x0800FC00, kiểm tra magic_key == 0xA55A và crc32.
     * Dưới đây là giá trị mặc định ban đầu tương đương eerpom_auto() của bản gốc v1.20: */
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
    cfg->crc32        = 0U;
}

bool hal_storage_save(const SystemConfig_t *cfg)
{
    /* TODO [Dev 2]: FLASH_Unlock() -> FLASH_ErasePage(0x0800FC00) -> FLASH_ProgramWord() -> FLASH_Lock() */
    (void)cfg;
    return true;
}

bool hal_storage_factory_reset(void)
{
    /* TODO [Dev 2]: Xóa Page 63 phục vụ màn hình Resetting... */
    return true;
}

