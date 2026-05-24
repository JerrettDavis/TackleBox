#include "usbd_desc.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"

#define USBD_VID                      0x0483U
#ifdef BOOTLOADER_DFU
#define USBD_PID_FS                   0xDF11U
#define USBD_DEVICE_CLASS             0x00U
#define USBD_DEVICE_SUBCLASS          0x00U
#define USBD_DEVICE_PROTOCOL          0x00U
#define USBD_PRODUCT_STRING_FS        "SKR2 DFU Bootloader"
#define USBD_CONFIGURATION_STRING_FS  "DFU Config"
#define USBD_INTERFACE_STRING_FS      "DFU Interface"
#elif defined(BOOTLOADER_CDC)
#define USBD_PID_FS                   0x5741U
#define USBD_DEVICE_CLASS             0x02U
#define USBD_DEVICE_SUBCLASS          0x02U
#define USBD_DEVICE_PROTOCOL          0x00U
#define USBD_PRODUCT_STRING_FS        "SKR2 CDC Bootloader"
#define USBD_CONFIGURATION_STRING_FS  "CDC Boot Config"
#define USBD_INTERFACE_STRING_FS      "CDC Boot Interface"
#else
#define USBD_PID_FS                   0x5740U
#define USBD_DEVICE_CLASS             0x02U
#define USBD_DEVICE_SUBCLASS          0x02U
#define USBD_DEVICE_PROTOCOL          0x00U
#define USBD_PRODUCT_STRING_FS        "SKR2 CDC Telemetry"
#define USBD_CONFIGURATION_STRING_FS  "CDC Config"
#define USBD_INTERFACE_STRING_FS      "CDC Interface"
#endif
#define USBD_LANGID_STRING            0x0409U
#define USBD_MANUFACTURER_STRING      "BTT SKR2"

static uint8_t USBD_FS_DeviceDesc[USB_LEN_DEV_DESC] = {
    0x12,                       /* bLength */
    USB_DESC_TYPE_DEVICE,       /* bDescriptorType */
    0x00,                       /* bcdUSB */
    0x02,
    USBD_DEVICE_CLASS,
    USBD_DEVICE_SUBCLASS,
    USBD_DEVICE_PROTOCOL,
    USB_MAX_EP0_SIZE,           /* bMaxPacketSize */
    LOBYTE(USBD_VID),
    HIBYTE(USBD_VID),
    LOBYTE(USBD_PID_FS),
    HIBYTE(USBD_PID_FS),
    0x00,                       /* bcdDevice rel. 2.00 */
    0x02,
    USBD_IDX_MFC_STR,           /* Index of manufacturer string */
    USBD_IDX_PRODUCT_STR,       /* Index of product string */
    USBD_IDX_SERIAL_STR,        /* Index of serial number string */
    USBD_MAX_NUM_CONFIGURATION  /* bNumConfigurations */
};

static uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] = {
    USB_LEN_LANGID_STR_DESC,
    USB_DESC_TYPE_STRING,
    LOBYTE(USBD_LANGID_STRING),
    HIBYTE(USBD_LANGID_STRING)
};

static uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ];

static uint8_t *USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(USBD_FS_DeviceDesc);
    return USBD_FS_DeviceDesc;
}

static uint8_t *USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    *length = sizeof(USBD_LangIDDesc);
    return USBD_LangIDDesc;
}

static uint8_t *USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_MANUFACTURER_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_PRODUCT_STRING_FS, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)"000000000001", USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_CONFIGURATION_STRING_FS, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    (void)speed;
    USBD_GetString((uint8_t *)USBD_INTERFACE_STRING_FS, USBD_StrDesc, length);
    return USBD_StrDesc;
}

USBD_DescriptorsTypeDef FS_Desc = {
    USBD_FS_DeviceDescriptor,
    USBD_FS_LangIDStrDescriptor,
    USBD_FS_ManufacturerStrDescriptor,
    USBD_FS_ProductStrDescriptor,
    USBD_FS_SerialStrDescriptor,
    USBD_FS_ConfigStrDescriptor,
    USBD_FS_InterfaceStrDescriptor,
};
