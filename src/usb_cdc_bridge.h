#ifndef USB_CDC_BRIDGE_H
#define USB_CDC_BRIDGE_H

#include <stdint.h>

#include "usbd_core.h"

#ifdef __cplusplus
extern "C" {
#endif

void usb_cdc_bridge_init(void);
void usb_cdc_bridge_poll(void);
int usb_cdc_bridge_wait_until_ready(uint32_t timeout_ms);
int usb_cdc_bridge_write(const char *data, uint16_t len);
extern USBD_HandleTypeDef hUsbDeviceFS;

#ifdef __cplusplus
}
#endif

#endif
