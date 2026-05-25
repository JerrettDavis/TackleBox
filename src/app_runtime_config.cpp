#include "app_runtime_config.h"

#include "app_board.h"
#include "boot_mode.h"
#include "keyswitch_protocol.h"
#include "load_cell.h"
#include "sdcard_fatfs.h"
#include "usb_cdc_bridge.h"
#include "usbd_cdc_if.h"
#include "stm32f4xx_hal.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static_assert((sizeof(StoredFirmwareConfig) % sizeof(uint32_t)) == 0U, "Stored config must align to flash word writes");

namespace {

const uint32_t CONFIG_FLASH_ADDRESS = 0x080E0000UL;
const uint32_t CONFIG_MAGIC = 0x4B535743UL;
const uint16_t CONFIG_VERSION = 9U;

uint8_t axis_workspace_valid(const PersistedFirmwareConfig &config)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    const int64_t max_um = (int64_t)channel.travelMinUm + (int64_t)channel.travelLimitUm;
    return (max_um > (int64_t)channel.travelMinUm) && (max_um <= INT32_MAX) ? 1U : 0U;
}

MotionChannelConfig make_default_motion_channel(
    const char *label,
    PinAssignment uart,
    PinAssignment dir,
    PinAssignment step,
    PinAssignment enable,
    PinAssignment stop,
    uint8_t dir_inverted,
    uint8_t home_towards_positive)
{
    MotionChannelConfig channel = {};
    strncpy(channel.label, (label != 0) ? label : "CH", MOTION_CHANNEL_LABEL_LENGTH - 1U);
    channel.label[MOTION_CHANNEL_LABEL_LENGTH - 1U] = 0;
    channel.enabled = 1U;
    channel.transportKind = (uint8_t)MotionChannelTransportKind::LocalGpio;
    channel.transportIndex = 0U;
    channel.transportAddress = 0U;
    channel.pins.uart = uart;
    channel.pins.dir = dir;
    channel.pins.step = step;
    channel.pins.enable = enable;
    channel.pins.stop = stop;
    channel.stopSignalActiveHigh = 0U;
    channel.dirInverted = dir_inverted;
    channel.enableActiveLow = 1U;
    channel.homeTowardsPositive = home_towards_positive;
    channel.stepsPerRotation = 3200U;
    channel.travelUmPerRotation = 8000U;
    channel.travelMinUm = 0;
    channel.travelLimitUm = 17500U;
    channel.defaultPressUm = 625U;
    channel.homeFeedrateMmPerMin = 600U;
    channel.moveFeedrateMmPerMin = 600U;
    channel.seekStepLimit = 7000U;
    channel.tmc2209 = {5U, 0U, 4U, 4U, 0U};
    return channel;
}

const char *channel_transport_name(uint8_t kind)
{
    switch ((MotionChannelTransportKind)kind)
    {
    case MotionChannelTransportKind::RemoteBus: return "remote_bus";
    case MotionChannelTransportKind::Virtual: return "virtual";
    default: return "local_gpio";
    }
}

uint8_t same_text_case_sensitive(const char *lhs, const char *rhs)
{
    return strcmp(lhs, rhs) == 0 ? 1U : 0U;
}

void usb_write_str(const char *s)
{
    if (s == 0)
    {
        return;
    }

    uint16_t len = 0U;
    while ((s[len] != 0) && (len < 1024U))
    {
        ++len;
    }

    usb_cdc_bridge_write(s, len);
}

const char *boot_source_name(ConfigBootSource source)
{
    switch (source)
    {
    case ConfigBootSource::Flash: return "flash";
    case ConfigBootSource::UsbHost: return "usb";
    case ConfigBootSource::MicroSd: return "microsd";
    default: return "defaults";
    }
}

void uppercase_inplace(char *text)
{
    if (text == 0)
    {
        return;
    }

    for (uint32_t i = 0U; text[i] != 0; ++i)
    {
        if ((text[i] >= 'a') && (text[i] <= 'z'))
        {
            text[i] = (char)(text[i] - ('a' - 'A'));
        }
    }
}

char *trim_ascii(char *text)
{
    if (text == 0)
    {
        return text;
    }

    while ((*text == ' ') || (*text == '\t') || (*text == '\r') || (*text == '\n'))
    {
        ++text;
    }

    uint32_t length = (uint32_t)strlen(text);
    while (length > 0U)
    {
        const char c = text[length - 1U];
        if ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'))
        {
            text[length - 1U] = 0;
            --length;
        }
        else
        {
            break;
        }
    }

    return text;
}

uint8_t parse_config_assignment_line(char *line, char *key, uint32_t key_length, char *value, uint32_t value_length)
{
    char *trimmed = trim_ascii(line);
    if ((trimmed == 0) || (trimmed[0] == 0) || (trimmed[0] == '#') || (trimmed[0] == ';'))
    {
        return 0U;
    }

    char *equals = strchr(trimmed, '=');
    if (equals != 0)
    {
        *equals = 0;
        strncpy(key, trim_ascii(trimmed), key_length - 1U);
        strncpy(value, trim_ascii(equals + 1U), value_length - 1U);
        key[key_length - 1U] = 0;
        value[value_length - 1U] = 0;
        uppercase_inplace(key);
        uppercase_inplace(value);
        return (key[0] != 0) && (value[0] != 0);
    }

    const keyswitch::Command command = keyswitch::parseCommand(trimmed);
    if (command.type == keyswitch::CommandType::SetConfig)
    {
        strncpy(key, command.key, key_length - 1U);
        strncpy(value, command.text, value_length - 1U);
        key[key_length - 1U] = 0;
        value[value_length - 1U] = 0;
        return 1U;
    }

    return 0U;
}

uint8_t try_load_config_from_microsd(PersistedFirmwareConfig *config, ConfigRuntimeState *state, BootConfigApplyFn apply_fn)
{
    char file_buffer[2048];
    uint32_t length = 0U;
    uint8_t applied_any = 0U;
    PersistedFirmwareConfig working = *config;

    state->microsdCardPresent = sdcard_detected();
    if (state->microsdCardPresent == 0U)
    {
        usb_write_str("boot: microsd missing\r\n");
        state->microsdConfigAvailable = 0U;
        return 0U;
    }

    if (sdcard_read_text_file("0:device.cfg", file_buffer, sizeof(file_buffer), &length) == 0U)
    {
        usb_write_str("boot: microsd config missing\r\n");
        state->microsdConfigAvailable = 0U;
        return 0U;
    }

    char *cursor = file_buffer;
    while (*cursor != 0)
    {
        char *line_start = cursor;
        while ((*cursor != 0) && (*cursor != '\n'))
        {
            ++cursor;
        }
        if (*cursor == '\n')
        {
            *cursor = 0;
            ++cursor;
        }

        char key[32] = {0};
        char value[32] = {0};
        if (parse_config_assignment_line(line_start, key, sizeof(key), value, sizeof(value)) == 0U)
        {
            continue;
        }

        const ApplyConfigResult result = apply_fn(&working, key, value);
        if (result.accepted == 0U)
        {
            usb_write_str("boot: microsd config invalid\r\n");
            state->microsdConfigAvailable = 0U;
            return 0U;
        }
        applied_any = 1U;
    }

    if ((applied_any == 0U) || (persisted_config_valid(working) == 0U))
    {
        usb_write_str("boot: microsd config invalid\r\n");
        state->microsdConfigAvailable = 0U;
        return 0U;
    }

    *config = working;
    state->microsdConfigAvailable = 1U;
    usb_write_str("boot: microsd config loaded\r\n");
    return 1U;
}

uint8_t boot_wait_for_usb_config(PersistedFirmwareConfig *working_config, PersistedFirmwareConfig defaults_config, ConfigRuntimeState *state, BootConfigApplyFn apply_fn)
{
    char command_buf[64];
    uint8_t received_config = 0U;
    const uint32_t wait_ms = working_config->bootHostWaitMs;
    const uint32_t deadline = HAL_GetTick() + wait_ms;

    char line[64];
    int len = snprintf(line, sizeof(line), "boot: wait_usb ms=%lu\r\n", (unsigned long)wait_ms);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }

    while ((uint32_t)(HAL_GetTick() - deadline) > 0x80000000UL)
    {
        usb_cdc_bridge_poll();

        if (CDC_ReadCommand_FS(command_buf, sizeof(command_buf)) == 0U)
        {
            continue;
        }

        const keyswitch::Command command = keyswitch::parseCommand(command_buf);

        if (command.type == keyswitch::CommandType::Help)
        {
            usb_write_str(keyswitch::commandHelpText());
        }
        else if (command.type == keyswitch::CommandType::Config)
        {
            usb_write_str("cmd: config\r\n");
            if (same_text_case_sensitive(command.key, "SOURCES"))
            {
                emit_config_sources_line(*state);
            }
            else
            {
                emit_config_summary(*working_config, *state);
                emit_config_sources_line(*state);
            }
        }
        else if (command.type == keyswitch::CommandType::ResetConfig)
        {
            *working_config = defaults_config;
            received_config = 1U;
            usb_write_str("cmd: resetcfg ok=1 reboot=0\r\n");
        }
        else if (command.type == keyswitch::CommandType::SaveConfig)
        {
            const uint8_t saved = save_persisted_config(*working_config);
            len = snprintf(line, sizeof(line), "cmd: save ok=%lu reboot=0\r\n", (unsigned long)saved);
            if (len > 0)
            {
                usb_cdc_bridge_write(line, (uint16_t)len);
            }
        }
        else if (command.type == keyswitch::CommandType::Reboot)
        {
            usb_write_str("cmd: reboot\r\n");
            usb_cdc_bridge_poll();
            bootloader_request_application_boot();
            NVIC_SystemReset();
        }
        else if (command.type == keyswitch::CommandType::Bootloader)
        {
            usb_write_str("cmd: bootloader\r\n");
            usb_cdc_bridge_poll();
            bootloader_clear_application_boot_request();
            NVIC_SystemReset();
        }
        else if (command.type == keyswitch::CommandType::Boot)
        {
            usb_write_str("cmd: boot\r\n");
            return received_config;
        }
        else if (command.type == keyswitch::CommandType::SetConfig)
        {
            const ApplyConfigResult result = apply_fn(working_config, command.key, command.text);

            len = snprintf(line, sizeof(line), "cmd: set key=%s value=%s ok=%lu reboot=0\r\n", command.key, command.text, (unsigned long)result.accepted);
            if (len > 0)
            {
                usb_cdc_bridge_write(line, (uint16_t)len);
            }

            if (result.accepted != 0U)
            {
                received_config = 1U;
            }
        }
    }

    return received_config;
}

}  // namespace

PersistedFirmwareConfig make_default_persisted_config(void)
{
    PersistedFirmwareConfig config = {};
    config.motionChannelCount = 5U;
    config.activeMotionChannel = 0U;
    config.motionChannels[0] = make_default_motion_channel("X", {(uint8_t)GpioPortId::E, 0U}, {(uint8_t)GpioPortId::E, 1U}, {(uint8_t)GpioPortId::E, 2U}, {(uint8_t)GpioPortId::E, 3U}, {(uint8_t)GpioPortId::C, 1U}, 1U, 1U);
    config.motionChannels[1] = make_default_motion_channel("Y", {(uint8_t)GpioPortId::D, 3U}, {(uint8_t)GpioPortId::D, 4U}, {(uint8_t)GpioPortId::D, 5U}, {(uint8_t)GpioPortId::D, 6U}, {(uint8_t)GpioPortId::C, 3U}, 0U, 0U);
    config.motionChannels[2] = make_default_motion_channel("Z", {(uint8_t)GpioPortId::D, 0U}, {(uint8_t)GpioPortId::A, 8U}, {(uint8_t)GpioPortId::A, 15U}, {(uint8_t)GpioPortId::D, 1U}, {(uint8_t)GpioPortId::C, 0U}, 0U, 0U);
    config.motionChannels[3] = make_default_motion_channel("E0", {(uint8_t)GpioPortId::C, 6U}, {(uint8_t)GpioPortId::D, 14U}, {(uint8_t)GpioPortId::D, 15U}, {(uint8_t)GpioPortId::C, 7U}, {(uint8_t)GpioPortId::C, 2U}, 0U, 0U);
    config.motionChannels[4] = make_default_motion_channel("E1", {(uint8_t)GpioPortId::D, 12U}, {(uint8_t)GpioPortId::D, 10U}, {(uint8_t)GpioPortId::D, 11U}, {(uint8_t)GpioPortId::D, 13U}, {(uint8_t)GpioPortId::A, 0U}, 0U, 0U);
    for (uint8_t index = 5U; index < MOTION_CHANNEL_CAPACITY; ++index)
    {
        char label[8] = {};
        snprintf(label, sizeof(label), "CH%u", (unsigned)index);
        config.motionChannels[index] = make_default_motion_channel(label, {(uint8_t)GpioPortId::A, 0U}, {(uint8_t)GpioPortId::A, 0U}, {(uint8_t)GpioPortId::A, 0U}, {(uint8_t)GpioPortId::A, 0U}, {(uint8_t)GpioPortId::A, 0U}, 0U, 0U);
        config.motionChannels[index].enabled = 0U;
    }
    config.pins.diag0 = {(uint8_t)GpioPortId::C, 0U};
    config.pins.diag2 = {(uint8_t)GpioPortId::C, 2U};
    config.pins.psOn = {(uint8_t)GpioPortId::E, 8U};
    config.pins.safePower = {(uint8_t)GpioPortId::C, 13U};
    config.pins.led = {(uint8_t)GpioPortId::E, 5U};
    config.loadCell.source = (uint8_t)LoadCellSourceKind::Simulation;
    config.loadCell.connector = (uint8_t)LoadCellConnectorKind::Custom;
    config.loadCell.pins.data = {0U, 0U};
    config.loadCell.pins.clock = {0U, 0U};
    config.loadCell.threshold = 1000U;
    config.stopDebounceCount = 3U;
    config.backoffSteps = 600U;
    config.stepIntervalUs = 250U;
    config.stepPulseWidthUs = 4U;
    config.tmcUartBitUs = 52U;
    config.allowUnverifiedTmcMotion = 0U;
    config.statusIntervalMs = 1000U;
    config.heartbeatIntervalMs = 1000U;
    config.bootHostWaitMs = 4000U;
    for (uint8_t index = 0U; index < MOTION_CHANNEL_CAPACITY; ++index)
    {
        config.motionChannels[index].seekStepLimit = axis_steps_from_um(config, config.motionChannels[index].travelLimitUm);
    }
    return config;
}

uint8_t persisted_config_valid(const PersistedFirmwareConfig &config)
{
    if ((config.motionChannelCount == 0U) || (config.motionChannelCount > MOTION_CHANNEL_CAPACITY) || (config.activeMotionChannel >= config.motionChannelCount))
    {
        return 0U;
    }

    if (active_motion_channel(config).transportKind != (uint8_t)MotionChannelTransportKind::LocalGpio)
    {
        return 0U;
    }

    for (uint8_t index = 0U; index < config.motionChannelCount; ++index)
    {
        const MotionChannelConfig &channel = config.motionChannels[index];
        const int64_t channel_max_um = (int64_t)channel.travelMinUm + (int64_t)channel.travelLimitUm;
        if ((channel.label[0] == 0) ||
            (channel.transportKind > (uint8_t)MotionChannelTransportKind::Virtual) ||
            ((channel.enabled != 0U) && !pin_assignment_valid(channel.pins.uart)) ||
            ((channel.enabled != 0U) && !pin_assignment_valid(channel.pins.dir)) ||
            ((channel.enabled != 0U) && !pin_assignment_valid(channel.pins.step)) ||
            ((channel.enabled != 0U) && !pin_assignment_valid(channel.pins.enable)) ||
            ((channel.enabled != 0U) && !pin_assignment_valid(channel.pins.stop)) ||
            (channel.stepsPerRotation == 0U) ||
            (channel.travelUmPerRotation == 0U) ||
            (channel.travelLimitUm == 0U) ||
            (channel.homeFeedrateMmPerMin == 0U) ||
            (channel.moveFeedrateMmPerMin == 0U) ||
            (channel_max_um <= (int64_t)channel.travelMinUm) ||
            (channel_max_um > INT32_MAX) ||
            ((int64_t)channel.defaultPressUm < (int64_t)channel.travelMinUm) ||
            ((int64_t)channel.defaultPressUm > channel_max_um) ||
            (axis_steps_from_um(config, channel.travelLimitUm) == 0U))
        {
            return 0U;
        }
    }

    return
           pin_assignment_valid(config.pins.diag0) &&
           pin_assignment_valid(config.pins.diag2) &&
           pin_assignment_valid(config.pins.psOn) &&
           pin_assignment_valid(config.pins.safePower) &&
           pin_assignment_valid(config.pins.led) &&
                     (config.loadCell.source <= (uint8_t)LoadCellSourceKind::AnalogAdc) &&
                     (config.loadCell.connector <= (uint8_t)LoadCellConnectorKind::Skr2Tb) &&
                     (config.loadCell.threshold > 0U) &&
                     (((config.loadCell.source != (uint8_t)LoadCellSourceKind::AnalogAdc) ||
                         pin_assignment_valid(config.loadCell.pins.data))) &&
                     (((config.loadCell.source != (uint8_t)LoadCellSourceKind::Hx711) ||
                         (pin_assignment_valid(config.loadCell.pins.data) && pin_assignment_valid(config.loadCell.pins.clock)))) &&
           axis_workspace_valid(config) &&
           (config.stopDebounceCount > 0U) &&
           (config.backoffSteps > 0U) &&
           (config.stepIntervalUs > 0U) &&
           (config.stepPulseWidthUs > 0U) &&
           (config.tmcUartBitUs > 0U) &&
           (config.statusIntervalMs > 0U) && (config.statusIntervalMs <= 60000U) &&
           (config.heartbeatIntervalMs > 0U) && (config.heartbeatIntervalMs <= 60000U) &&
           (config.bootHostWaitMs <= 30000U);
}

uint32_t axis_steps_from_um(const PersistedFirmwareConfig &config, uint32_t travel_um)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    if ((channel.stepsPerRotation == 0U) || (channel.travelUmPerRotation == 0U) || (travel_um == 0U))
    {
        return 0U;
    }

    const uint64_t numerator = ((uint64_t)travel_um * (uint64_t)channel.stepsPerRotation) + ((uint64_t)channel.travelUmPerRotation / 2ULL);
    const uint32_t steps = (uint32_t)(numerator / (uint64_t)channel.travelUmPerRotation);
    return (steps == 0U) ? 1U : steps;
}

uint32_t axis_um_from_steps(const PersistedFirmwareConfig &config, uint32_t steps)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    if ((channel.stepsPerRotation == 0U) || (channel.travelUmPerRotation == 0U) || (steps == 0U))
    {
        return 0U;
    }

    const uint64_t numerator = ((uint64_t)steps * (uint64_t)channel.travelUmPerRotation) + ((uint64_t)channel.stepsPerRotation / 2ULL);
    return (uint32_t)(numerator / (uint64_t)channel.stepsPerRotation);
}

int32_t axis_steps_from_signed_um(const PersistedFirmwareConfig &config, int32_t travel_um)
{
    const uint32_t magnitude_steps = axis_steps_from_um(config, (travel_um < 0) ? (uint32_t)(-travel_um) : (uint32_t)travel_um);
    return (travel_um < 0) ? -(int32_t)magnitude_steps : (int32_t)magnitude_steps;
}

int32_t axis_travel_max_um(const PersistedFirmwareConfig &config)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    const int64_t max_um = (int64_t)channel.travelMinUm + (int64_t)channel.travelLimitUm;
    if (max_um > INT32_MAX)
    {
        return INT32_MAX;
    }

    return (int32_t)max_um;
}

uint8_t load_persisted_config(PersistedFirmwareConfig *config)
{
    const StoredFirmwareConfig *stored = (const StoredFirmwareConfig *)CONFIG_FLASH_ADDRESS;
    if (stored->magic != CONFIG_MAGIC)
    {
        return 0U;
    }
    if ((stored->version != CONFIG_VERSION) || (stored->length != sizeof(PersistedFirmwareConfig)))
    {
        return 0U;
    }
    if (stored->checksum != checksum32(&stored->config, sizeof(stored->config)))
    {
        return 0U;
    }
    if (persisted_config_valid(stored->config) == 0U)
    {
        return 0U;
    }

    *config = stored->config;
    return 1U;
}

uint8_t save_persisted_config(const PersistedFirmwareConfig &config)
{
    StoredFirmwareConfig stored = {};
    stored.magic = CONFIG_MAGIC;
    stored.version = CONFIG_VERSION;
    stored.length = sizeof(PersistedFirmwareConfig);
    stored.config = config;
    stored.checksum = checksum32(&stored.config, sizeof(stored.config));

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {};
    uint32_t sector_error = 0U;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.Sector = FLASH_SECTOR_11;
    erase.NbSectors = 1U;

    if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return 0U;
    }

    const uint32_t *words = (const uint32_t *)&stored;
    const uint32_t word_count = sizeof(stored) / sizeof(uint32_t);
    uint32_t address = CONFIG_FLASH_ADDRESS;
    for (uint32_t index = 0U; index < word_count; ++index)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, words[index]) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return 0U;
        }
        address += sizeof(uint32_t);
    }

    HAL_FLASH_Lock();
    return 1U;
}

void emit_config_summary(const PersistedFirmwareConfig &config, const ConfigRuntimeState &state)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    char line[192];
    char a[8];
    char b[8];
    char c[8];
    char d[8];
    char e[8];

    int len = snprintf(
        line,
        sizeof(line),
        "config loaded=%lu dirty=%lu reboot=%lu\r\n",
        (unsigned long)state.loadedFromFlash,
        (unsigned long)state.dirty,
        (unsigned long)state.requiresReboot);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }

    len = snprintf(
        line,
        sizeof(line),
        "config channel.label=%s channel.enabled=%lu channel.transport=%s channel.bus=%lu channel.address=%lu\r\n",
        channel.label,
        (unsigned long)channel.enabled,
        channel_transport_name(channel.transportKind),
        (unsigned long)channel.transportIndex,
        (unsigned long)channel.transportAddress);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }

    len = snprintf(line, sizeof(line), "config channels.count=%lu channels.active=%lu\r\n", (unsigned long)config.motionChannelCount, (unsigned long)config.activeMotionChannel);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }

    format_pin_assignment(channel.pins.step, a, sizeof(a));
    format_pin_assignment(channel.pins.dir, b, sizeof(b));
    format_pin_assignment(channel.pins.enable, c, sizeof(c));
    format_pin_assignment(channel.pins.uart, d, sizeof(d));
    format_pin_assignment(channel.pins.stop, e, sizeof(e));
    len = snprintf(line, sizeof(line), "config pin.x_step=%s pin.x_dir=%s pin.x_enable=%s pin.x_uart=%s pin.x_stop=%s\r\n", a, b, c, d, e);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }

    format_pin_assignment(config.pins.diag0, a, sizeof(a));
    format_pin_assignment(config.pins.diag2, b, sizeof(b));
    format_pin_assignment(config.pins.psOn, c, sizeof(c));
    format_pin_assignment(config.pins.safePower, d, sizeof(d));
    format_pin_assignment(config.pins.led, e, sizeof(e));
    len = snprintf(line, sizeof(line), "config pin.diag0=%s pin.diag2=%s pin.ps_on=%s pin.safe_power=%s pin.led=%s\r\n", a, b, c, d, e);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }

    format_pin_assignment(config.loadCell.pins.data, a, sizeof(a));
    format_pin_assignment(config.loadCell.pins.clock, b, sizeof(b));
    len = snprintf(
        line,
        sizeof(line),
        "config loadcell.source=%s loadcell.connector=%s loadcell.threshold=%lu pin.loadcell_data=%s pin.loadcell_clock=%s\r\n",
        load_cell_source_name(config.loadCell.source),
        load_cell_connector_name(config.loadCell.connector),
        (unsigned long)config.loadCell.threshold,
        a,
        b);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }

    len = snprintf(
        line,
        sizeof(line),
        "config logic.stop_active_high=%lu logic.dir_inverted=%lu logic.enable_active_low=%lu logic.home_towards_positive=%lu\r\n",
        (unsigned long)channel.stopSignalActiveHigh,
        (unsigned long)channel.dirInverted,
        (unsigned long)channel.enableActiveLow,
        (unsigned long)channel.homeTowardsPositive);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }

    len = snprintf(
        line,
        sizeof(line),
        "config motion.step_pulse_us=%lu motion.seek_limit_steps=%lu motion.stop_debounce_count=%lu motion.backoff_steps=%lu motion.home_feedrate_mm_per_min=%lu motion.move_feedrate_mm_per_min=%lu\r\n",
        (unsigned long)config.stepPulseWidthUs,
        (unsigned long)axis_steps_from_um(config, channel.travelLimitUm),
        (unsigned long)config.stopDebounceCount,
        (unsigned long)config.backoffSteps,
        (unsigned long)channel.homeFeedrateMmPerMin,
        (unsigned long)channel.moveFeedrateMmPerMin);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }

    len = snprintf(
        line,
        sizeof(line),
        "config axis.steps_per_rotation=%lu axis.travel_um_per_rotation=%lu axis.travel_min_um=%ld axis.travel_max_um=%ld axis.travel_limit_um=%lu axis.default_press_um=%lu\r\n",
        (unsigned long)channel.stepsPerRotation,
        (unsigned long)channel.travelUmPerRotation,
        (long)channel.travelMinUm,
        (long)axis_travel_max_um(config),
        (unsigned long)channel.travelLimitUm,
        (unsigned long)channel.defaultPressUm);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }

    len = snprintf(
        line,
        sizeof(line),
        "config telemetry.status_interval_ms=%lu telemetry.heartbeat_interval_ms=%lu boot.host_wait_ms=%lu sim.load_threshold=%lu tmc.allow_unverified_motion=%lu\r\n",
        (unsigned long)config.statusIntervalMs,
        (unsigned long)config.heartbeatIntervalMs,
        (unsigned long)config.bootHostWaitMs,
        (unsigned long)config.loadCell.threshold,
        (unsigned long)config.allowUnverifiedTmcMotion);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }

    len = snprintf(
        line,
        sizeof(line),
        "config tmc.irun=%lu tmc.ihold=%lu tmc.iholddelay=%lu tmc.tpowerdown=%lu tmc.sgthrs=%lu tmc.uart_bit_us=%lu\r\n",
        (unsigned long)channel.tmc2209.irun,
        (unsigned long)channel.tmc2209.ihold,
        (unsigned long)channel.tmc2209.iholddelay,
        (unsigned long)channel.tmc2209.tpowerdown,
        (unsigned long)channel.tmc2209.sgthrs,
        (unsigned long)config.tmcUartBitUs);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }
}

void emit_config_sources_line(const ConfigRuntimeState &state)
{
    char line[160];
    int len = snprintf(
        line,
        sizeof(line),
        "config sources selected=%s usb=1 microsd_card=%lu microsd_cfg=%lu flash=%lu defaults=1\r\n",
        boot_source_name(state.source),
        (unsigned long)state.microsdCardPresent,
        (unsigned long)state.microsdConfigAvailable,
        (unsigned long)state.flashConfigAvailable);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }
}

void emit_boot_source_line(ConfigBootSource source)
{
    char line[64];
    int len = snprintf(line, sizeof(line), "boot: source=%s\r\n", boot_source_name(source));
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }
}

ConfigRuntimeState select_boot_config(PersistedFirmwareConfig *config, BootConfigApplyFn apply_fn)
{
    const PersistedFirmwareConfig default_config = make_default_persisted_config();
    PersistedFirmwareConfig saved_config = default_config;
    const uint8_t has_saved_config = load_persisted_config(&saved_config);

    ConfigRuntimeState state = {};
    state.source = ConfigBootSource::Defaults;
    state.flashConfigAvailable = has_saved_config;

    PersistedFirmwareConfig boot_config = (has_saved_config != 0U) ? saved_config : default_config;
    const uint8_t usb_config_received = boot_wait_for_usb_config(&boot_config, default_config, &state, apply_fn);

    if (usb_config_received != 0U)
    {
        *config = boot_config;
        state.source = ConfigBootSource::UsbHost;
        state.loadedFromFlash = 0U;
    }
    else if (try_load_config_from_microsd(&boot_config, &state, apply_fn) != 0U)
    {
        *config = boot_config;
        state.source = ConfigBootSource::MicroSd;
        state.loadedFromFlash = 0U;
    }
    else if (has_saved_config != 0U)
    {
        *config = saved_config;
        state.source = ConfigBootSource::Flash;
        state.loadedFromFlash = 1U;
    }
    else
    {
        *config = default_config;
        state.source = ConfigBootSource::Defaults;
        state.loadedFromFlash = 0U;
    }

    return state;
}