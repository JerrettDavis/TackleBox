#include "usbd_dfu.h"

#include "stm32f4xx_hal.h"

#include <string.h>

#define BOOTLOADER_APP_ADDRESS 0x08008000U
#define BOOTLOADER_FLASH_END   0x08100000U
#define FLASH_DESC_STR         "@Internal Flash /0x08008000/02*016Kg,01*064Kg,07*128Kg"
#define FLASH_ERASE_TIME       ((uint16_t)50U)
#define FLASH_PROGRAM_TIME     ((uint16_t)50U)

static uint32_t bootloader_get_sector(uint32_t address);
static uint8_t bootloader_address_writable(uint32_t address, uint32_t length);

static uint16_t BootloaderFlash_Init(void)
{
    HAL_FLASH_Unlock();
    return 0U;
}

static uint16_t BootloaderFlash_DeInit(void)
{
    HAL_FLASH_Lock();
    return 0U;
}

static uint16_t BootloaderFlash_Erase(uint32_t address)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_error = 0U;

    if (bootloader_address_writable(address, 1U) == 0U)
    {
        return 1U;
    }

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks = FLASH_BANK_1;
    erase.Sector = bootloader_get_sector(address);
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    return (HAL_FLASHEx_Erase(&erase, &sector_error) == HAL_OK) ? 0U : 1U;
}

static uint16_t BootloaderFlash_Write(uint8_t *src, uint8_t *dest, uint32_t length)
{
    uint32_t offset = 0U;
    const uint32_t destination_address = (uint32_t)dest;

    if ((src == 0) || (dest == 0) || (length == 0U))
    {
        return 1U;
    }

    if (bootloader_address_writable(destination_address, length) == 0U)
    {
        return 1U;
    }

    while (offset < length)
    {
        uint32_t word = 0xFFFFFFFFU;
        const uint32_t chunk = ((length - offset) >= 4U) ? 4U : (length - offset);
        memcpy(&word, src + offset, chunk);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, destination_address + offset, word) != HAL_OK)
        {
            return 1U;
        }

        if (memcmp((const void *)(destination_address + offset), src + offset, chunk) != 0)
        {
            return 2U;
        }

        offset += 4U;
    }

    return 0U;
}

static uint8_t *BootloaderFlash_Read(uint8_t *src, uint8_t *dest, uint32_t length)
{
    if ((src == 0) || (dest == 0) || (length == 0U))
    {
        return 0;
    }

    memcpy(dest, src, length);
    return dest;
}

static uint16_t BootloaderFlash_GetStatus(uint32_t address, uint8_t cmd, uint8_t *buffer)
{
    (void)address;

    if (buffer == 0)
    {
        return 1U;
    }

    switch (cmd)
    {
    case DFU_MEDIA_PROGRAM:
        buffer[1] = (uint8_t)FLASH_PROGRAM_TIME;
        buffer[2] = (uint8_t)(FLASH_PROGRAM_TIME >> 8);
        buffer[3] = 0U;
        break;
    case DFU_MEDIA_ERASE:
    default:
        buffer[1] = (uint8_t)FLASH_ERASE_TIME;
        buffer[2] = (uint8_t)(FLASH_ERASE_TIME >> 8);
        buffer[3] = 0U;
        break;
    }

    return 0U;
}

USBD_DFU_MediaTypeDef g_bootloader_dfu_media = {
    (uint8_t *)FLASH_DESC_STR,
    BootloaderFlash_Init,
    BootloaderFlash_DeInit,
    BootloaderFlash_Erase,
    BootloaderFlash_Write,
    BootloaderFlash_Read,
    BootloaderFlash_GetStatus,
};

static uint8_t bootloader_address_writable(uint32_t address, uint32_t length)
{
    if (address < BOOTLOADER_APP_ADDRESS)
    {
        return 0U;
    }

    if (address >= BOOTLOADER_FLASH_END)
    {
        return 0U;
    }

    if (length > (BOOTLOADER_FLASH_END - address))
    {
        return 0U;
    }

    return 1U;
}

static uint32_t bootloader_get_sector(uint32_t address)
{
    if (address < 0x0800C000U)
    {
        return FLASH_SECTOR_2;
    }
    if (address < 0x08010000U)
    {
        return FLASH_SECTOR_3;
    }
    if (address < 0x08020000U)
    {
        return FLASH_SECTOR_4;
    }
    if (address < 0x08040000U)
    {
        return FLASH_SECTOR_5;
    }
    if (address < 0x08060000U)
    {
        return FLASH_SECTOR_6;
    }
    if (address < 0x08080000U)
    {
        return FLASH_SECTOR_7;
    }
    if (address < 0x080A0000U)
    {
        return FLASH_SECTOR_8;
    }
    if (address < 0x080C0000U)
    {
        return FLASH_SECTOR_9;
    }
    if (address < 0x080E0000U)
    {
        return FLASH_SECTOR_10;
    }
    return FLASH_SECTOR_11;
}