#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_dfu.h"

#include "boot_mode.h"

#include "stm32f4xx_hal.h"

extern "C" {
extern USBD_DFU_MediaTypeDef g_bootloader_dfu_media;
}

static constexpr uint32_t kAppAddress = 0x08008000U;
static constexpr uint32_t kFlashEnd = 0x08100000U;
static constexpr uint32_t kSramStart = 0x20000000U;
static constexpr uint32_t kSramEnd = 0x20030000U;
static constexpr uint32_t kDfuWaitMs = 30000U;

USBD_HandleTypeDef hUsbDeviceFS;

extern "C" void SysTick_Handler(void)
{
    HAL_IncTick();
}

extern "C" void HardFault_Handler(void)
{
    __disable_irq();
    bootloader_request_emergency_stop();
    __DSB();
    __ISB();
    NVIC_SystemReset();
    while (1)
    {
    }
}

extern "C" void OTG_FS_IRQHandler(void)
{
    HAL_PCD_IRQHandler((PCD_HandleTypeDef *)hUsbDeviceFS.pData);
}

static uint8_t system_clock_config_hse(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 8U;
    osc.PLL.PLLN = 336U;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 7U;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        return 0U;
    }

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;

    return (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) == HAL_OK) ? 1U : 0U;
}

static void system_clock_config_hsi_safe(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&osc);

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0);
}

static uint8_t application_present(void)
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

static void jump_to_application(void)
{
    using AppEntry = void (*)(void);

    const uint32_t stack_pointer = *reinterpret_cast<volatile const uint32_t *>(kAppAddress);
    const uint32_t reset_handler = *reinterpret_cast<volatile const uint32_t *>(kAppAddress + 4U);
    AppEntry app_entry = reinterpret_cast<AppEntry>(reset_handler);

    USBD_Stop(&hUsbDeviceFS);
    USBD_DeInit(&hUsbDeviceFS);

    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    for (uint32_t index = 0U; index < 8U; ++index)
    {
        NVIC->ICER[index] = 0xFFFFFFFFU;
        NVIC->ICPR[index] = 0xFFFFFFFFU;
    }

    HAL_RCC_DeInit();
    HAL_DeInit();

    SCB->VTOR = kAppAddress;
    __set_MSP(stack_pointer);
    __DSB();
    __ISB();

    app_entry();

    while (1)
    {
    }
}

int main(void)
{
    SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;

    HAL_Init();
    if (system_clock_config_hse() == 0U)
    {
        system_clock_config_hsi_safe();
    }

    RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
    __DSB();

    USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS);
    USBD_RegisterClass(&hUsbDeviceFS, &USBD_DFU);
    USBD_DFU_RegisterMedia(&hUsbDeviceFS, &g_bootloader_dfu_media);
    USBD_Start(&hUsbDeviceFS);

    const uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < kDfuWaitMs)
    {
        if (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED)
        {
            while (1)
            {
                HAL_Delay(50U);
            }
        }

        HAL_Delay(1U);
    }

    if ((bootloader_emergency_stop_latched() == 0U) && (application_present() != 0U))
    {
        jump_to_application();
    }

    while (1)
    {
        HAL_Delay(50U);
    }
}