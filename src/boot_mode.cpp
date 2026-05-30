#include "boot_mode.h"

#include "stm32f4xx_hal.h"

namespace {
constexpr uint32_t kBootAppRequestMagic = 0xB00720A5U;
constexpr uint32_t kStayInBootloaderMagic = 0xB00710ADU;
constexpr uint32_t kEmergencyStopMagic = 0xE5700001U;

static void enable_backup_register_access(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    __DSB();
    PWR->CR |= PWR_CR_DBP;
    __DSB();
}
}

extern "C" void bootloader_request_application_boot(void)
{
    enable_backup_register_access();
    RTC->BKP0R = kBootAppRequestMagic;
    __DSB();
    __ISB();
}

extern "C" void bootloader_clear_application_boot_request(void)
{
    enable_backup_register_access();
    RTC->BKP0R = 0U;
    __DSB();
    __ISB();
}

extern "C" uint8_t bootloader_consume_application_boot_request(void)
{
    enable_backup_register_access();

    if (RTC->BKP0R != kBootAppRequestMagic)
    {
        return 0U;
    }

    RTC->BKP0R = 0U;
    __DSB();
    __ISB();
    return 1U;
}

extern "C" void bootloader_request_stay_in_bootloader(void)
{
    enable_backup_register_access();
    RTC->BKP0R = kStayInBootloaderMagic;
    __DSB();
    __ISB();
}

extern "C" uint8_t bootloader_consume_stay_in_bootloader_request(void)
{
    enable_backup_register_access();

    if (RTC->BKP0R != kStayInBootloaderMagic)
    {
        return 0U;
    }

    RTC->BKP0R = 0U;
    __DSB();
    __ISB();
    return 1U;
}

extern "C" void bootloader_request_emergency_stop(void)
{
    enable_backup_register_access();
    RTC->BKP1R = kEmergencyStopMagic;
    __DSB();
    __ISB();
}

extern "C" void bootloader_clear_emergency_stop(void)
{
    enable_backup_register_access();
    RTC->BKP1R = 0U;
    __DSB();
    __ISB();
}

extern "C" uint8_t bootloader_emergency_stop_latched(void)
{
    enable_backup_register_access();
    return (RTC->BKP1R == kEmergencyStopMagic) ? 1U : 0U;
}