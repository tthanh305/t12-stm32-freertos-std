/**
 * @file    hal_flash.h
 * @brief   Giao diện Driver Lưu trữ Cấu hình vào Internal Flash Page 63 (0x0800FC00) giả lập EEPROM
 * @note    Phụ trách cài đặt (.c): Dev 2 trên nhánh feature/hal-i2c-spi (Tham chiếu EEPROM.ino)
 */

#ifndef HAL_FLASH_H
#define HAL_FLASH_H

#include "stm32f10x.h"
#include "app_ipc.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Kiểm tra dữ liệu tại Page 63 (0x0800FC00) khi khởi động (tương đương eerpom_auto() gốc):
 *        - Nếu magic_key == 0xA55A và khớp CRC32: Đọc toàn bộ cấu hình từ Flash vào *cfg.
 *        - Nếu chưa khởi tạo hoặc lỗi CRC32: Nạp bộ thông số mặc định chuẩn v1.20 vào *cfg và ghi xuống Flash.
 * @param cfg Con trỏ tới cấu trúc cấu hình hệ thống (g_sys_cfg)
 */
void hal_storage_init_or_load(SystemConfig_t *cfg);

/**
 * @brief Mở khóa Flash, xóa Page 63, tính CRC32 và ghi toàn bộ cấu trúc SystemConfig_t xuống Flash
 * @param cfg Con trỏ tới cấu trúc cấu hình cần lưu
 * @return true nếu ghi và đối chiếu thành công
 */
bool hal_storage_save(const SystemConfig_t *cfg);

/**
 * @brief Xóa Page 63 để khôi phục cài đặt gốc (phục vụ màn hình Resetting... display_hfccsz)
 * @return true nếu xóa thành công
 */
bool hal_storage_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_FLASH_H */

