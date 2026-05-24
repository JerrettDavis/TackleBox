#include "usb_cdc_bridge.h"
#include "boot_mode.h"
#include "bootloader_flash.h"
#include "bootloader_protocol.h"
#include "usbd_cdc_if.h"

#include "stm32f4xx_hal.h"

#include <cstdlib>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static constexpr uint32_t kCommandBufferSize = 320U;
static constexpr uint32_t kMaxReadBytes = 32U;

extern "C" void SysTick_Handler(void)
{
    HAL_IncTick();
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

static void write_line(const char *text)
{
    if (text == 0)
    {
        return;
    }

    usb_cdc_bridge_write(text, (uint16_t)strlen(text));
}

static int hex_value(char ch)
{
    if ((ch >= '0') && (ch <= '9'))
    {
        return ch - '0';
    }
    ch = (char)toupper((unsigned char)ch);
    if ((ch >= 'A') && (ch <= 'F'))
    {
        return 10 + (ch - 'A');
    }
    return -1;
}

static uint8_t decode_hex_bytes(const char *hex_text, uint8_t *output, uint32_t *length)
{
    uint32_t count = 0U;

    if ((hex_text == 0) || (output == 0) || (length == 0))
    {
        return 0U;
    }

    while (hex_text[count * 2U] != 0)
    {
        const char hi = hex_text[count * 2U];
        const char lo = hex_text[(count * 2U) + 1U];
        const int hi_value = hex_value(hi);
        const int lo_value = hex_value(lo);

        if ((lo == 0) || (hi_value < 0) || (lo_value < 0))
        {
            return 0U;
        }

        output[count] = (uint8_t)((hi_value << 4) | lo_value);
        ++count;
    }

    *length = count;
    return 1U;
}

static void replyf(const char *format, uint32_t a, uint32_t b)
{
    char line[96] = {};
    (void)snprintf(line, sizeof(line), format, (unsigned long)a, (unsigned long)b);
    write_line(line);
}

static void write_hex_bytes_line(uint32_t offset, const uint8_t *data, uint32_t length)
{
    char line[192] = {};
    uint32_t used = 0U;

    if ((data == 0) || (length == 0U))
    {
        write_line("ERR empty read\r\n");
        return;
    }

    used = (uint32_t)snprintf(
        line,
        sizeof(line),
        "DATA offset=%lu size=%lu hex=",
        (unsigned long)offset,
        (unsigned long)length);

    for (uint32_t index = 0U; (index < length) && (used + 3U < sizeof(line)); ++index)
    {
        used += (uint32_t)snprintf(line + used, sizeof(line) - used, "%02X", data[index]);
    }

    if (used + 3U < sizeof(line))
    {
        line[used++] = '\r';
        line[used++] = '\n';
        line[used] = 0;
    }

    write_line(line);
}

static void service_command(const char *command)
{
    const BootloaderCommand parsed = bootloader_parse_command(command);

    if (command == 0)
    {
        return;
    }

    if (parsed.type == BootloaderCommandType::None)
    {
        return;
    }

    if (parsed.type == BootloaderCommandType::Ping)
    {
        write_line("PONG\r\n");
        return;
    }

    if (parsed.type == BootloaderCommandType::Info)
    {
        replyf("INFO app_base=0x%08lX present=%lu\r\n", bootloader_app_base(), bootloader_application_present());
        return;
    }

    if (parsed.type == BootloaderCommandType::Help)
    {
        write_line(bootloader_command_help_text());
        return;
    }

    if (parsed.type == BootloaderCommandType::Status)
    {
        char line[128] = {};
        const uint32_t configured = (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED) ? 1U : 0U;
        (void)snprintf(
            line,
            sizeof(line),
            "STATUS mode=BOOTLOADER uptime_ms=%lu usb_configured=%lu app_present=%lu\r\n",
            (unsigned long)HAL_GetTick(),
            (unsigned long)configured,
            (unsigned long)bootloader_application_present());
        write_line(line);
        return;
    }

    if (parsed.type == BootloaderCommandType::Flash)
    {
        char line[160] = {};
        (void)snprintf(
            line,
            sizeof(line),
            "FLASH boot_base=0x%08lX boot_size=%lu app_base=0x%08lX flash_end=0x%08lX\r\n",
            (unsigned long)bootloader_boot_base(),
            (unsigned long)bootloader_boot_size(),
            (unsigned long)bootloader_app_base(),
            (unsigned long)bootloader_flash_end());
        write_line(line);
        return;
    }

    if (parsed.type == BootloaderCommandType::Read)
    {
        uint8_t bytes[kMaxReadBytes] = {};

        if ((parsed.hasOffset == 0U) || (parsed.hasSize == 0U) || (parsed.size == 0U) || (parsed.size > kMaxReadBytes))
        {
            write_line("ERR bad read args\r\n");
            return;
        }

        if (bootloader_flash_read(bootloader_app_base() + parsed.offset, bytes, parsed.size) != 0U)
        {
            write_line("ERR read failed\r\n");
            return;
        }

        write_hex_bytes_line(parsed.offset, bytes, parsed.size);
        return;
    }

    if (parsed.type == BootloaderCommandType::Erase)
    {
        if (parsed.hasSize == 0U)
        {
            write_line("ERR bad erase size\r\n");
            return;
        }

        if (bootloader_flash_erase_range(bootloader_app_base(), parsed.size) != 0U)
        {
            write_line("ERR erase failed\r\n");
            return;
        }

        replyf("OK ERASE size=%lu\r\n", parsed.size, 0U);
        return;
    }

    if (parsed.type == BootloaderCommandType::Write)
    {
        uint8_t data[128] = {};
        uint32_t length = 0U;

        if ((parsed.hasOffset == 0U) || (decode_hex_bytes(parsed.text, data, &length) == 0U))
        {
            write_line("ERR bad write args\r\n");
            return;
        }

        if (bootloader_flash_write(bootloader_app_base() + parsed.offset, data, length) != 0U)
        {
            write_line("ERR write failed\r\n");
            return;
        }

        replyf("OK WRITE offset=%lu bytes=%lu\r\n", parsed.offset, length);
        return;
    }

    if (parsed.type == BootloaderCommandType::Crc)
    {
        if (parsed.hasSize == 0U)
        {
            write_line("ERR bad crc size\r\n");
            return;
        }

        replyf("CRC 0x%08lX size=%lu\r\n", bootloader_crc32(bootloader_app_base(), parsed.size), parsed.size);
        return;
    }

    if (parsed.type == BootloaderCommandType::Boot)
    {
        write_line("BOOTING\r\n");
        HAL_Delay(20U);
        if (bootloader_application_present() != 0U)
        {
            bootloader_request_application_boot();
            NVIC_SystemReset();
        }
        write_line("ERR no app\r\n");
        return;
    }

    if (parsed.type == BootloaderCommandType::Reset)
    {
        write_line("RESETTING\r\n");
        HAL_Delay(20U);
        NVIC_SystemReset();
        return;
    }

    write_line("ERR unknown command\r\n");
}

int main(void)
{
    SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;

    if ((bootloader_consume_application_boot_request() != 0U) &&
        (bootloader_application_present() != 0U))
    {
        bootloader_jump_to_application();
    }

    HAL_Init();
    if (system_clock_config_hse() == 0U)
    {
        system_clock_config_hsi_safe();
    }

    usb_cdc_bridge_init();

    uint8_t announced_ready = 0U;
    char command[kCommandBufferSize] = {};

    while (1)
    {
        if ((announced_ready == 0U) && (usb_cdc_bridge_wait_until_ready(5U) != 0))
        {
            write_line("SKR2 CDC bootloader ready\r\n");
            announced_ready = 1U;
        }

        if (CDC_ReadCommand_FS(command, sizeof(command)) != 0U)
        {
            service_command(command);
        }

        HAL_Delay(2U);
    }
}