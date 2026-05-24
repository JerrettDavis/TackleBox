#ifndef __USBD_CDC_IF_H
#define __USBD_CDC_IF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_cdc.h"

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

uint16_t CDC_ReadCommand_FS(char *buf, uint16_t buf_len);
uint8_t CDC_Transmit_FS(uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
