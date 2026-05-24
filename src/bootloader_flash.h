#ifndef BOOTLOADER_FLASH_H
#define BOOTLOADER_FLASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t bootloader_app_base(void);
uint32_t bootloader_boot_base(void);
uint32_t bootloader_boot_size(void);
uint32_t bootloader_flash_end(void);
uint8_t bootloader_application_present(void);
void bootloader_jump_to_application(void);
uint8_t bootloader_flash_read(uint32_t address, uint8_t *data, uint32_t length);
uint8_t bootloader_flash_erase_range(uint32_t address, uint32_t length);
uint8_t bootloader_flash_write(uint32_t address, const uint8_t *data, uint32_t length);
uint32_t bootloader_crc32(uint32_t address, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif