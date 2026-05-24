#ifndef SDCARD_FATFS_H
#define SDCARD_FATFS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t sdcard_detected(void);
uint8_t sdcard_read_text_file(const char *path, char *buffer, uint32_t buffer_len, uint32_t *out_length);

#ifdef __cplusplus
}
#endif

#endif