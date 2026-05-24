#include "usbd_cdc_if.h"
#include "usbd_core.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *pbuf, uint32_t *len);
static int8_t CDC_TransmitCplt_FS(uint8_t *pbuf, uint32_t *len, uint8_t epnum);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
    CDC_TransmitCplt_FS
};

#ifdef BOOTLOADER_CDC
#define COMMAND_BUFFER_CAPACITY 320U
#else
#define COMMAND_BUFFER_CAPACITY 64U
#endif

static uint8_t UserRxBufferFS[64];
static uint8_t UserTxBufferFS[256];
static volatile uint16_t CommandReadyLen = 0U;
static char CommandReadyBuf[COMMAND_BUFFER_CAPACITY];
static char CommandBuildBuf[COMMAND_BUFFER_CAPACITY];
static uint16_t CommandBuildLen = 0U;

static int8_t CDC_Init_FS(void)
{
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0U);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return (int8_t)USBD_OK;
}

static int8_t CDC_DeInit_FS(void)
{
    return (int8_t)USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    (void)cmd;
    (void)pbuf;
    (void)length;
    return (int8_t)USBD_OK;
}

static int8_t CDC_Receive_FS(uint8_t *pbuf, uint32_t *len)
{
    uint32_t i;

    for (i = 0U; i < *len; ++i)
    {
        char c = (char)pbuf[i];

        if ((c == '\r') || (c == '\n'))
        {
            if ((CommandBuildLen > 0U) && (CommandReadyLen == 0U))
            {
                uint16_t copy_len = CommandBuildLen;
                if (copy_len >= sizeof(CommandReadyBuf))
                {
                    copy_len = (uint16_t)(sizeof(CommandReadyBuf) - 1U);
                }

                for (uint16_t j = 0U; j < copy_len; ++j)
                {
                    CommandReadyBuf[j] = CommandBuildBuf[j];
                }
                CommandReadyBuf[copy_len] = 0;
                CommandReadyLen = copy_len;
            }

            CommandBuildLen = 0U;
        }
        else if ((c >= 32) && (c <= 126))
        {
            if (CommandBuildLen < (sizeof(CommandBuildBuf) - 1U))
            {
                CommandBuildBuf[CommandBuildLen++] = c;
                CommandBuildBuf[CommandBuildLen] = 0;
            }
        }
    }

    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return (int8_t)USBD_OK;
}

static int8_t CDC_TransmitCplt_FS(uint8_t *pbuf, uint32_t *len, uint8_t epnum)
{
    (void)pbuf;
    (void)len;
    (void)epnum;
    return (int8_t)USBD_OK;
}

uint8_t CDC_Transmit_FS(uint8_t *buf, uint16_t len)
{
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    if ((hcdc == NULL) || (hcdc->TxState != 0U))
    {
        return (uint8_t)USBD_BUSY;
    }

    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, buf, len);
    return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}

uint16_t CDC_ReadCommand_FS(char *buf, uint16_t buf_len)
{
    uint16_t copy_len;

    if ((buf == 0) || (buf_len == 0U) || (CommandReadyLen == 0U))
    {
        return 0U;
    }

    copy_len = CommandReadyLen;
    if (copy_len >= buf_len)
    {
        copy_len = (uint16_t)(buf_len - 1U);
    }

    for (uint16_t i = 0U; i < copy_len; ++i)
    {
        buf[i] = CommandReadyBuf[i];
    }
    buf[copy_len] = 0;
    CommandReadyLen = 0U;
    return copy_len;
}
