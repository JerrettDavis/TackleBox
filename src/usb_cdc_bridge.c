#include "usb_cdc_bridge.h"

#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_desc.h"
#include "usbd_cdc_if.h"

USBD_HandleTypeDef hUsbDeviceFS;

void OTG_FS_IRQHandler(void)
{
    HAL_PCD_IRQHandler((PCD_HandleTypeDef *)hUsbDeviceFS.pData);
}

void usb_cdc_bridge_init(void)
{
    // Ensure USB OTG clock is enabled
    RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
    __DSB();
    
    // Initialize USB stack
    USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS);
    USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC);
    USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS);
    
    // Start USB (non-blocking - enumeration happens asynchronously)
    USBD_Start(&hUsbDeviceFS);
}

void usb_cdc_bridge_poll(void)
{
    // No periodic work required; endpoint handling is interrupt-driven.
}

int usb_cdc_bridge_wait_until_ready(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        if (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED)
        {
            return 1;
        }

        HAL_Delay(1U);
    }

    return 0;
}

int usb_cdc_bridge_write(const char *data, uint16_t len)
{
    uint32_t start;
    uint8_t rc;

    if ((data == 0) || (len == 0U))
    {
        return 0;
    }

    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
    {
        return 0;
    }

    start = HAL_GetTick();
    do
    {
        rc = CDC_Transmit_FS((uint8_t *)data, len);
        if (rc == (uint8_t)USBD_OK)
        {
            return (int)len;
        }

        HAL_Delay(1U);
    }
    while ((rc == (uint8_t)USBD_BUSY) && ((HAL_GetTick() - start) < 50U));

    return 0;
}
