#include "sdcard_fatfs.h"

#include "stm32f4xx_hal.h"
#include "ff.h"
#include "diskio.h"

static SD_HandleTypeDef g_sd_handle;
static FATFS g_fatfs;
static volatile DSTATUS g_disk_status = STA_NOINIT;
static uint8_t g_sd_gpio_ready = 0U;
static uint8_t g_sd_initialized = 0U;
static uint8_t g_sd_mounted = 0U;

static void sdcard_prepare_detect_gpio(void)
{
    if (g_sd_gpio_ready != 0U)
    {
        return;
    }

    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef init = {0};
    init.Pin = GPIO_PIN_4;
    init.Mode = GPIO_MODE_INPUT;
    init.Pull = GPIO_PULLUP;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &init);

    g_sd_gpio_ready = 1U;
}

uint8_t sdcard_detected(void)
{
    sdcard_prepare_detect_gpio();
    return (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_4) == GPIO_PIN_RESET) ? 1U : 0U;
}

void HAL_SD_MspInit(SD_HandleTypeDef *hsd)
{
    if ((hsd == 0) || (hsd->Instance != SDIO))
    {
        return;
    }

    __HAL_RCC_SDIO_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitTypeDef init = {0};
    init.Mode = GPIO_MODE_AF_PP;
    init.Pull = GPIO_PULLUP;
    init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    init.Alternate = GPIO_AF12_SDIO;

    init.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
    HAL_GPIO_Init(GPIOC, &init);

    init.Pin = GPIO_PIN_2;
    HAL_GPIO_Init(GPIOD, &init);
}

void HAL_SD_MspDeInit(SD_HandleTypeDef *hsd)
{
    if ((hsd == 0) || (hsd->Instance != SDIO))
    {
        return;
    }

    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12);
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);
    __HAL_RCC_SDIO_CLK_DISABLE();
}

static uint8_t sdcard_init_hal(void)
{
    if (g_sd_initialized != 0U)
    {
        return 1U;
    }

    if (sdcard_detected() == 0U)
    {
        return 0U;
    }

    g_sd_handle.Instance = SDIO;
    g_sd_handle.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
    g_sd_handle.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
    g_sd_handle.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
    g_sd_handle.Init.BusWide = SDIO_BUS_WIDE_1B;
    g_sd_handle.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
    g_sd_handle.Init.ClockDiv = 118U;

    if (HAL_SD_Init(&g_sd_handle) != HAL_OK)
    {
        return 0U;
    }

    if (HAL_SD_ConfigWideBusOperation(&g_sd_handle, SDIO_BUS_WIDE_4B) != HAL_OK)
    {
        return 0U;
    }

    g_sd_initialized = 1U;
    g_disk_status = 0U;
    return 1U;
}

static uint8_t sdcard_mount(void)
{
    if (g_sd_mounted != 0U)
    {
        return 1U;
    }

    if (sdcard_init_hal() == 0U)
    {
        return 0U;
    }

    if (f_mount(&g_fatfs, "0:", 1) != FR_OK)
    {
        return 0U;
    }

    g_sd_mounted = 1U;
    return 1U;
}

uint8_t sdcard_read_text_file(const char *path, char *buffer, uint32_t buffer_len, uint32_t *out_length)
{
    FIL file;
    UINT bytes_read = 0U;

    if ((path == 0) || (buffer == 0) || (buffer_len < 2U) || (out_length == 0))
    {
        return 0U;
    }

    *out_length = 0U;

    if (sdcard_mount() == 0U)
    {
        return 0U;
    }

    if (f_open(&file, path, FA_READ) != FR_OK)
    {
        return 0U;
    }

    if (f_read(&file, buffer, buffer_len - 1U, &bytes_read) != FR_OK)
    {
        f_close(&file);
        return 0U;
    }

    f_close(&file);
    buffer[bytes_read] = 0;
    *out_length = (uint32_t)bytes_read;
    return 1U;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0U)
    {
        return STA_NOINIT;
    }

    return (sdcard_init_hal() != 0U) ? g_disk_status : STA_NOINIT;
}

DSTATUS disk_status(BYTE pdrv)
{
    if ((pdrv != 0U) || (sdcard_detected() == 0U))
    {
        return STA_NOINIT | STA_NODISK;
    }

    return (g_sd_initialized != 0U) ? g_disk_status : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    uint32_t start = HAL_GetTick();

    if ((pdrv != 0U) || (buff == 0) || (count == 0U))
    {
        return RES_PARERR;
    }

    if (sdcard_init_hal() == 0U)
    {
        return RES_NOTRDY;
    }

    if (HAL_SD_ReadBlocks(&g_sd_handle, buff, sector, count, 1000U) != HAL_OK)
    {
        return RES_ERROR;
    }

    while (HAL_SD_GetCardState(&g_sd_handle) != HAL_SD_CARD_TRANSFER)
    {
        if ((HAL_GetTick() - start) > 1000U)
        {
            return RES_ERROR;
        }
    }

    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    HAL_SD_CardInfoTypeDef info;

    if (pdrv != 0U)
    {
        return RES_PARERR;
    }

    if (sdcard_init_hal() == 0U)
    {
        return RES_NOTRDY;
    }

    switch (cmd)
    {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_COUNT:
        HAL_SD_GetCardInfo(&g_sd_handle, &info);
        *(DWORD *)buff = info.LogBlockNbr;
        return RES_OK;
    case GET_SECTOR_SIZE:
        *(WORD *)buff = 512U;
        return RES_OK;
    case GET_BLOCK_SIZE:
        HAL_SD_GetCardInfo(&g_sd_handle, &info);
        *(DWORD *)buff = info.LogBlockSize / 512U;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}