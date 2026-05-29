#include "app_runtime_config_test_support.h"

#include <array>
#include <cstring>
#include <string>

#include "app_board.h"
#include "boot_mode.h"
#include "sdcard_fatfs.h"
#include "stm32f4xx_hal.h"
#include "usb_cdc_bridge.h"
#include "usbd_cdc_if.h"

namespace {

std::array<uint8_t, 4096> g_config_flash = {};
std::string g_usb_output;
uint32_t g_tick = 0U;

char gpio_port_name(uint8_t port_id)
{
    if ((port_id >= (uint8_t)GpioPortId::A) && (port_id <= (uint8_t)GpioPortId::H))
    {
        return (char)('A' + (port_id - (uint8_t)GpioPortId::A));
    }

    return '?';
}

bool host_flash_address_valid(uintptr_t address)
{
    const uintptr_t begin = reinterpret_cast<uintptr_t>(g_config_flash.data());
    const uintptr_t end = begin + g_config_flash.size();
    return (address >= begin) && ((address + sizeof(uint32_t)) <= end);
}

}  // namespace

USBD_HandleTypeDef hUsbDeviceFS = {};
USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {};

void keyswitch_test_reset_runtime_config_host_state(void)
{
    g_config_flash.fill(0xFFU);
    g_usb_output.clear();
    g_tick = 0U;
}

void keyswitch_test_clear_usb_output(void)
{
    g_usb_output.clear();
}

const char *keyswitch_test_usb_output(void)
{
    return g_usb_output.c_str();
}

uintptr_t keyswitch_test_config_flash_storage_address(void)
{
    return reinterpret_cast<uintptr_t>(g_config_flash.data());
}

extern "C" void NVIC_SystemReset(void)
{
}

uint32_t HAL_GetTick(void)
{
    return g_tick;
}

void HAL_FLASH_Unlock(void)
{
}

void HAL_FLASH_Lock(void)
{
}

HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *erase, uint32_t *sector_error)
{
    (void)erase;
    if (sector_error != 0)
    {
        *sector_error = 0U;
    }
    g_config_flash.fill(0xFFU);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type_program, uintptr_t address, uint64_t data)
{
    (void)type_program;
    if (!host_flash_address_valid(address))
    {
        return 1;
    }

    const uint32_t word = (uint32_t)data;
    std::memcpy(reinterpret_cast<void *>(address), &word, sizeof(word));
    return HAL_OK;
}

GPIO_TypeDef *gpio_port_from_id(uint8_t port_id)
{
    (void)port_id;
    return 0;
}

uint8_t pin_assignment_valid(PinAssignment pin)
{
    return ((pin.portId >= (uint8_t)GpioPortId::A) && (pin.portId <= (uint8_t)GpioPortId::H) && (pin.pin < 16U)) ? 1U : 0U;
}

uint32_t checksum32(const void *data, uint32_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t checksum = 2166136261U;
    for (uint32_t index = 0U; index < length; ++index)
    {
        checksum ^= (uint32_t)bytes[index];
        checksum *= 16777619U;
    }
    return checksum;
}

void format_pin_assignment(PinAssignment pin, char *buffer, uint32_t length)
{
    if ((buffer == 0) || (length == 0U))
    {
        return;
    }

    if (pin_assignment_valid(pin) == 0U)
    {
        std::snprintf(buffer, length, "NONE");
        return;
    }

    std::snprintf(buffer, length, "P%c%u", gpio_port_name(pin.portId), (unsigned)pin.pin);
}

int usb_cdc_bridge_write(const char *data, uint16_t len)
{
    if ((data != 0) && (len > 0U))
    {
        g_usb_output.append(data, data + len);
    }
    return (int)len;
}

void usb_cdc_bridge_init(void)
{
}

void usb_cdc_bridge_poll(void)
{
}

int usb_cdc_bridge_wait_until_ready(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return 1;
}

uint16_t CDC_ReadCommand_FS(char *buf, uint16_t buf_len)
{
    (void)buf;
    (void)buf_len;
    return 0U;
}

uint8_t CDC_Transmit_FS(uint8_t *buf, uint16_t len)
{
    (void)buf;
    (void)len;
    return 0U;
}

uint8_t sdcard_detected(void)
{
    return 0U;
}

uint8_t sdcard_read_text_file(const char *path, char *buffer, uint32_t buffer_len, uint32_t *out_length)
{
    (void)path;
    if ((buffer != 0) && (buffer_len > 0U))
    {
        buffer[0] = 0;
    }
    if (out_length != 0)
    {
        *out_length = 0U;
    }
    return 0U;
}

void bootloader_request_application_boot(void)
{
}

void bootloader_request_stay_in_bootloader(void)
{
}

void bootloader_clear_application_boot_request(void)
{
}

uint8_t bootloader_consume_application_boot_request(void)
{
    return 0U;
}

uint8_t bootloader_consume_stay_in_bootloader_request(void)
{
    return 0U;
}