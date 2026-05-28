#include "bootloader_flash.h"

#include "boot_mode.h"

#include "stm32f4xx_hal.h"

#include <string.h>

namespace {
constexpr uint32_t kBootloaderBase = 0x08000000U;
constexpr uint32_t kBootloaderSize = 0x00008000U;
constexpr uint32_t kAppAddress = kBootloaderBase + kBootloaderSize;
constexpr uint32_t kFlashEnd = 0x08100000U;
constexpr uint32_t kSramStart = 0x20000000U;
constexpr uint32_t kSramEnd = 0x20030000U;

static uint8_t address_writable(uint32_t address, uint32_t length)
{
    if (address < kAppAddress)
    {
        return 0U;
    }
    if (address >= kFlashEnd)
    {
        return 0U;
    }
    if (length > (kFlashEnd - address))
    {
        return 0U;
    }
    return 1U;
}

static uint32_t get_sector(uint32_t address)
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

}

extern "C" uint32_t bootloader_app_base(void)
{
    return kAppAddress;
}

extern "C" uint32_t bootloader_boot_base(void)
{
    return kBootloaderBase;
}

extern "C" uint32_t bootloader_boot_size(void)
{
    return kBootloaderSize;
}

extern "C" uint32_t bootloader_flash_end(void)
{
    return kFlashEnd;
}

extern "C" uint8_t bootloader_application_present(void)
{
    const uint32_t stack_pointer = *reinterpret_cast<volatile const uint32_t *>(kAppAddress);
    const uint32_t reset_handler = *reinterpret_cast<volatile const uint32_t *>(kAppAddress + 4U);

    if ((stack_pointer < kSramStart) || (stack_pointer > kSramEnd))
    {
        return 0U;
    }

    if ((reset_handler < kAppAddress) || (reset_handler >= kFlashEnd))
    {
        return 0U;
    }

    return 1U;
}

extern "C" void bootloader_jump_to_application(void)
{
    using AppEntry = void (*)(void);
    const uint32_t stack_pointer = *reinterpret_cast<volatile const uint32_t *>(kAppAddress);
    const uint32_t reset_handler = *reinterpret_cast<volatile const uint32_t *>(kAppAddress + 4U);
    AppEntry app_entry = reinterpret_cast<AppEntry>(reset_handler);

    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    for (uint32_t index = 0U; index < 8U; ++index)
    {
        NVIC->ICER[index] = 0xFFFFFFFFU;
        NVIC->ICPR[index] = 0xFFFFFFFFU;
    }

    SCB->VTOR = kAppAddress;
    __set_CONTROL(0U);
    __set_PSP(0U);
    __set_MSP(stack_pointer);
    __DSB();
    __ISB();
    __enable_irq();

    app_entry();

    while (1)
    {
    }
}

extern "C" uint8_t bootloader_flash_read(uint32_t address, uint8_t *data, uint32_t length)
{
    if ((data == 0) || (length == 0U) || (address_writable(address, length) == 0U))
    {
        return 1U;
    }

    memcpy(data, reinterpret_cast<const void *>(address), length);
    return 0U;
}

extern "C" uint8_t bootloader_flash_erase_range(uint32_t address, uint32_t length)
{
    if ((length == 0U) || (address_writable(address, length) == 0U))
    {
        return 1U;
    }

    HAL_FLASH_Unlock();

    uint32_t current_address = address;
    const uint32_t end_address = address + length - 1U;
    while (current_address <= end_address)
    {
        FLASH_EraseInitTypeDef erase = {0};
        uint32_t sector_error = 0U;
        erase.TypeErase = FLASH_TYPEERASE_SECTORS;
        erase.Banks = FLASH_BANK_1;
        erase.Sector = get_sector(current_address);
        erase.NbSectors = 1U;
        erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

        if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return 1U;
        }

        switch (erase.Sector)
        {
        case FLASH_SECTOR_2: current_address = 0x0800C000U; break;
        case FLASH_SECTOR_3: current_address = 0x08010000U; break;
        case FLASH_SECTOR_4: current_address = 0x08020000U; break;
        case FLASH_SECTOR_5: current_address = 0x08040000U; break;
        case FLASH_SECTOR_6: current_address = 0x08060000U; break;
        case FLASH_SECTOR_7: current_address = 0x08080000U; break;
        case FLASH_SECTOR_8: current_address = 0x080A0000U; break;
        case FLASH_SECTOR_9: current_address = 0x080C0000U; break;
        case FLASH_SECTOR_10: current_address = 0x080E0000U; break;
        default: current_address = kFlashEnd; break;
        }
    }

    HAL_FLASH_Lock();
    return 0U;
}

extern "C" uint8_t bootloader_flash_write(uint32_t address, const uint8_t *data, uint32_t length)
{
    if ((data == 0) || (length == 0U) || (address_writable(address, length) == 0U))
    {
        return 1U;
    }

    HAL_FLASH_Unlock();

    for (uint32_t offset = 0U; offset < length; offset += 4U)
    {
        uint32_t word = 0xFFFFFFFFU;
        const uint32_t chunk = ((length - offset) >= 4U) ? 4U : (length - offset);
        memcpy(&word, data + offset, chunk);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + offset, word) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return 1U;
        }

        if (memcmp(reinterpret_cast<const void *>(address + offset), data + offset, chunk) != 0)
        {
            HAL_FLASH_Lock();
            return 2U;
        }
    }

    HAL_FLASH_Lock();
    return 0U;
}

extern "C" uint32_t bootloader_crc32(uint32_t address, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFU;

    if ((length == 0U) || (address_writable(address, length) == 0U))
    {
        return 0U;
    }

    for (uint32_t index = 0U; index < length; ++index)
    {
        crc ^= *(reinterpret_cast<volatile const uint8_t *>(address + index));
        for (uint32_t bit = 0U; bit < 8U; ++bit)
        {
            const uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }

    return ~crc;
}