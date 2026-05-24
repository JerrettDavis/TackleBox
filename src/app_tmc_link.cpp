#include "app_tmc_link.h"

#include "app_board.h"
#include "usb_cdc_bridge.h"

#include <stdio.h>

namespace {

DriverRuntimeConfig g_driver_runtime = {
    {5U, 0U, 4U, 4U, 0U},
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
};

uint32_t cycles_from_us_local(uint32_t microseconds)
{
    const uint64_t cycles = ((uint64_t)SystemCoreClock * (uint64_t)microseconds) / 1000000ULL;
    return (cycles == 0ULL) ? 1U : (uint32_t)cycles;
}

uint32_t cycle_counter_now_local(void)
{
    return DWT->CYCCNT;
}

uint8_t current_tmc_uart_slave_address(const PersistedFirmwareConfig &config)
{
    return active_motion_channel(config).transportAddress;
}

void x_tmc_uart_idle(const PersistedFirmwareConfig &config)
{
    pin_write_high(active_motion_channel(config).pins.uart);
}

void x_tmc_uart_set_output_mode(const PersistedFirmwareConfig &config)
{
    pin_set_output(active_motion_channel(config).pins.uart);
    x_tmc_uart_idle(config);
}

void x_tmc_uart_set_input_mode(const PersistedFirmwareConfig &config)
{
    pin_set_input_pull(active_motion_channel(config).pins.uart, 1U);
}

void x_tmc_uart_write_byte(const PersistedFirmwareConfig &config, uint8_t value)
{
    const uint32_t bit_cycles = cycles_from_us_local(config.tmcUartBitUs);

    pin_write_low(active_motion_channel(config).pins.uart);
    delay_cycles(bit_cycles);

    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
        if ((value & 0x01U) != 0U)
        {
            pin_write_high(active_motion_channel(config).pins.uart);
        }
        else
        {
            pin_write_low(active_motion_channel(config).pins.uart);
        }

        delay_cycles(bit_cycles);
        value >>= 1;
    }

    pin_write_high(active_motion_channel(config).pins.uart);
    delay_cycles(bit_cycles);
}

void x_tmc_uart_write_frame(const PersistedFirmwareConfig &config, const uint8_t *frame, uint32_t length)
{
    if (frame == 0)
    {
        return;
    }

    __disable_irq();
    x_tmc_uart_idle(config);
    delay_cycles(cycles_from_us_local(config.tmcUartBitUs));

    for (uint32_t index = 0U; index < length; ++index)
    {
        x_tmc_uart_write_byte(config, frame[index]);
    }

    x_tmc_uart_idle(config);
    __enable_irq();
}

uint8_t x_tmc_uart_read_byte(const PersistedFirmwareConfig &config, uint32_t timeout_cycles, uint32_t bit_cycles, uint8_t *value)
{
    const uint32_t start_cycles = cycle_counter_now_local();

    while (pin_read(active_motion_channel(config).pins.uart) != 0U)
    {
        if ((uint32_t)(cycle_counter_now_local() - start_cycles) >= timeout_cycles)
        {
            return 0U;
        }
    }

    delay_cycles(bit_cycles / 2U);
    if (pin_read(active_motion_channel(config).pins.uart) != 0U)
    {
        return 0U;
    }

    delay_cycles(bit_cycles);

    uint8_t data = 0U;
    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
        if (pin_read(active_motion_channel(config).pins.uart) != 0U)
        {
            data |= (uint8_t)(1U << bit);
        }
        delay_cycles(bit_cycles);
    }

    if (pin_read(active_motion_channel(config).pins.uart) == 0U)
    {
        return 0U;
    }

    delay_cycles(bit_cycles);
    *value = data;
    return 1U;
}

uint8_t x_tmc2209_read_register(const PersistedFirmwareConfig &config, keyswitch::Tmc2209Register reg, uint32_t *value)
{
    uint8_t request[4] = {0};
    uint8_t reply[8] = {0};
    const uint32_t request_length = keyswitch::encodeTmc2209ReadFrame(
        current_tmc_uart_slave_address(config),
        reg,
        request,
        sizeof(request));
    const uint32_t bit_cycles = cycles_from_us_local(config.tmcUartBitUs);
    const uint32_t byte_timeout_cycles = bit_cycles * 24U;

    if ((request_length == 0U) || (value == 0))
    {
        return 0U;
    }

    __disable_irq();
    x_tmc_uart_set_output_mode(config);
    delay_cycles(bit_cycles);
    for (uint32_t index = 0U; index < request_length; ++index)
    {
        x_tmc_uart_write_byte(config, request[index]);
    }

    x_tmc_uart_set_input_mode(config);
    delay_cycles(bit_cycles * 2U);

    uint8_t sync_bytes = 0U;
    while (sync_bytes < 3U)
    {
        uint8_t byte = 0U;
        if (x_tmc_uart_read_byte(config, byte_timeout_cycles, bit_cycles, &byte) == 0U)
        {
            x_tmc_uart_set_output_mode(config);
            __enable_irq();
            return 0U;
        }

        if ((sync_bytes == 0U) && (byte != 0x05U))
        {
            continue;
        }
        if ((sync_bytes == 1U) && (byte != 0xFFU))
        {
            sync_bytes = 0U;
            continue;
        }
        if ((sync_bytes == 2U) && (byte != ((uint8_t)reg & 0x7FU)))
        {
            sync_bytes = 0U;
            continue;
        }

        reply[sync_bytes++] = byte;
    }

    for (uint32_t index = 3U; index < sizeof(reply); ++index)
    {
        if (x_tmc_uart_read_byte(config, byte_timeout_cycles, bit_cycles, &reply[index]) == 0U)
        {
            x_tmc_uart_set_output_mode(config);
            __enable_irq();
            return 0U;
        }
    }

    x_tmc_uart_set_output_mode(config);
    __enable_irq();
    return keyswitch::decodeTmc2209ReadReply(reg, reply, sizeof(reply), value);
}

void x_tmc2209_write_register(const PersistedFirmwareConfig &config, keyswitch::Tmc2209Register reg, uint32_t value)
{
    uint8_t frame[8] = {0};
    const uint32_t length = keyswitch::encodeTmc2209WriteFrame(
        current_tmc_uart_slave_address(config),
        reg,
        value,
        frame,
        sizeof(frame));

    if (length != 0U)
    {
        x_tmc_uart_write_frame(config, frame, length);
        delay_cycles(cycles_from_us_local(250U));
    }
}

}  // namespace

DriverRuntimeConfig &tmc_driver_runtime(void)
{
    return g_driver_runtime;
}

void x_driver_enable(const PersistedFirmwareConfig &config)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    if (channel.enableActiveLow != 0U)
    {
        pin_write_low(channel.pins.enable);
    }
    else
    {
        pin_write_high(channel.pins.enable);
    }
}

void x_driver_disable(const PersistedFirmwareConfig &config)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    if (channel.enableActiveLow != 0U)
    {
        pin_write_high(channel.pins.enable);
    }
    else
    {
        pin_write_low(channel.pins.enable);
    }
}

uint8_t x_tmc2209_refresh_verification(const PersistedFirmwareConfig &config)
{
    uint32_t gconf = 0U;
    uint32_t ihold_irun = 0U;
    uint32_t tpowerdown = 0U;
    uint32_t sgthrs = 0U;
    uint32_t ifcnt = 0U;
    const uint8_t read_ok =
        (x_tmc2209_read_register(config, keyswitch::Tmc2209Register::Gconf, &gconf) != 0U) &&
        (x_tmc2209_read_register(config, keyswitch::Tmc2209Register::IholdIrun, &ihold_irun) != 0U) &&
        (x_tmc2209_read_register(config, keyswitch::Tmc2209Register::Tpowerdown, &tpowerdown) != 0U) &&
        (x_tmc2209_read_register(config, keyswitch::Tmc2209Register::Sgthrs, &sgthrs) != 0U);

    g_driver_runtime.ifcntValid = x_tmc2209_read_register(config, keyswitch::Tmc2209Register::Ifcnt, &ifcnt);
    g_driver_runtime.ifcnt = (uint8_t)(ifcnt & 0xFFU);

    if (read_ok == 0U)
    {
        g_driver_runtime.uartConfigured = 0U;
        return 0U;
    }

    g_driver_runtime.verifiedGconf = gconf;
    g_driver_runtime.verifiedIholdIrun = ihold_irun;
    g_driver_runtime.verifiedTpowerdown = tpowerdown;
    g_driver_runtime.verifiedSgthrs = sgthrs;
    g_driver_runtime.uartConfigured =
        (gconf == keyswitch::makeTmc2209GconfValue()) &&
        (ihold_irun == keyswitch::makeTmc2209IholdIrunValue(g_driver_runtime.tmc2209)) &&
        (tpowerdown == keyswitch::makeTmc2209TpowerdownValue(g_driver_runtime.tmc2209)) &&
        (sgthrs == keyswitch::makeTmc2209SgthrsValue(g_driver_runtime.tmc2209));
    return g_driver_runtime.uartConfigured;
}

uint8_t x_tmc2209_apply_config(const PersistedFirmwareConfig &config)
{
    x_tmc2209_write_register(config, keyswitch::Tmc2209Register::Gconf, keyswitch::makeTmc2209GconfValue());
    x_tmc2209_write_register(config, keyswitch::Tmc2209Register::IholdIrun, keyswitch::makeTmc2209IholdIrunValue(g_driver_runtime.tmc2209));
    x_tmc2209_write_register(config, keyswitch::Tmc2209Register::Tpowerdown, keyswitch::makeTmc2209TpowerdownValue(g_driver_runtime.tmc2209));
    x_tmc2209_write_register(config, keyswitch::Tmc2209Register::Sgthrs, keyswitch::makeTmc2209SgthrsValue(g_driver_runtime.tmc2209));
    return x_tmc2209_refresh_verification(config);
}

uint8_t x_tmc_verified(void)
{
    return (g_driver_runtime.uartConfigured != 0U) ? 1U : 0U;
}

void emit_tmc_unverified_blocked(const char *command_name)
{
    char line[96];
    int len = snprintf(line, sizeof(line), "cmd: %s ok=0 reason=tmc_unverified\r\n", command_name);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }
}