#ifndef BOOT_MODE_H
#define BOOT_MODE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void bootloader_request_application_boot(void);
void bootloader_request_stay_in_bootloader(void);
void bootloader_clear_application_boot_request(void);
uint8_t bootloader_consume_application_boot_request(void);
uint8_t bootloader_consume_stay_in_bootloader_request(void);

#ifdef __cplusplus
}
#endif

#endif