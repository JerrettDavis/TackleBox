#include "app_board.h"
#include "app_display.h"
#include "app_panel.h"
#include "boot_mode.h"
#include "app_config.h"
#include "app_runtime_config.h"
#include "app_tmc_link.h"
#include "keyswitch_domain.h"
#include "keyswitch_protocol.h"
#include "keyswitch_tmc2209.h"
#include "load_cell.h"
#include "usb_cdc_bridge.h"
#include "usbd_cdc_if.h"
#include "stm32f4xx_hal.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static PersistedFirmwareConfig g_persisted_config = {};
static ConfigRuntimeState g_config_state = {};
static const Mini12864PanelPins g_panel_pins = mini12864_panel_pins();
static Mini12864PanelInputs g_panel_inputs = {};
static volatile uint8_t g_load_cell_hx711_ready = 0U;
static PinAssignment g_load_cell_hx711_irq_pin = {};
static uint8_t g_load_cell_hx711_irq_configured = 0U;
static void configure_load_cell_io(const LoadCellConfig &config);

static void emit_panel_pins_line(void)
{
    const PinAssignment exp1_01 = {(uint8_t)GpioPortId::C, 5U};
    const PinAssignment exp1_02 = {(uint8_t)GpioPortId::B, 0U};
    const PinAssignment exp1_03 = {(uint8_t)GpioPortId::B, 1U};
    const PinAssignment exp1_04 = {(uint8_t)GpioPortId::E, 9U};
    const PinAssignment exp1_05 = {(uint8_t)GpioPortId::E, 10U};
    const PinAssignment exp1_06 = {(uint8_t)GpioPortId::E, 11U};
    const PinAssignment exp1_07 = {(uint8_t)GpioPortId::E, 12U};
    const PinAssignment exp1_08 = {(uint8_t)GpioPortId::E, 13U};
    const PinAssignment exp2_01 = {(uint8_t)GpioPortId::A, 6U};
    const PinAssignment exp2_02 = {(uint8_t)GpioPortId::A, 5U};
    const PinAssignment exp2_03 = {(uint8_t)GpioPortId::E, 7U};
    const PinAssignment exp2_04 = {(uint8_t)GpioPortId::A, 4U};
    const PinAssignment exp2_05 = {(uint8_t)GpioPortId::B, 2U};
    const PinAssignment exp2_06 = {(uint8_t)GpioPortId::A, 7U};
    const PinAssignment exp2_07 = {(uint8_t)GpioPortId::C, 4U};

    enable_gpio_clock((uint8_t)GpioPortId::A);
    enable_gpio_clock((uint8_t)GpioPortId::B);
    enable_gpio_clock((uint8_t)GpioPortId::C);
    enable_gpio_clock((uint8_t)GpioPortId::E);
    __DSB();

    char line[320];
    const int len = snprintf(
        line,
        sizeof(line),
        "panel exp1_01_pc5=%lu exp1_02_pb0=%lu exp1_03_pb1=%lu exp1_04_pe9=%lu exp1_05_pe10=%lu exp1_06_pe11=%lu exp1_07_pe12=%lu exp1_08_pe13=%lu exp2_01_pa6=%lu exp2_02_pa5=%lu exp2_03_pe7=%lu exp2_04_pa4=%lu exp2_05_pb2=%lu exp2_06_pa7=%lu exp2_07_pc4=%lu\r\n",
        (unsigned long)pin_read(exp1_01),
        (unsigned long)pin_read(exp1_02),
        (unsigned long)pin_read(exp1_03),
        (unsigned long)pin_read(exp1_04),
        (unsigned long)pin_read(exp1_05),
        (unsigned long)pin_read(exp1_06),
        (unsigned long)pin_read(exp1_07),
        (unsigned long)pin_read(exp1_08),
        (unsigned long)pin_read(exp2_01),
        (unsigned long)pin_read(exp2_02),
        (unsigned long)pin_read(exp2_03),
        (unsigned long)pin_read(exp2_04),
        (unsigned long)pin_read(exp2_05),
        (unsigned long)pin_read(exp2_06),
        (unsigned long)pin_read(exp2_07));
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }
}

static uint8_t same_text_case_sensitive(const char *lhs, const char *rhs)
{
    return strcmp(lhs, rhs) == 0 ? 1U : 0U;
}

static uint32_t exti_line_mask(uint8_t pin)
{
    return 1UL << pin;
}

static uint32_t gpio_port_exti_code(uint8_t port_id)
{
    switch ((GpioPortId)port_id)
    {
    case GpioPortId::A: return 0U;
    case GpioPortId::B: return 1U;
    case GpioPortId::C: return 2U;
    case GpioPortId::D: return 3U;
    case GpioPortId::E: return 4U;
    case GpioPortId::F: return 5U;
    case GpioPortId::G: return 6U;
    case GpioPortId::H: return 7U;
    default: return 0U;
    }
}

static IRQn_Type exti_irqn_from_pin(uint8_t pin)
{
    if (pin <= 4U) return (IRQn_Type)((int32_t)EXTI0_IRQn + (int32_t)pin);
    if (pin <= 9U) return EXTI9_5_IRQn;
    return EXTI15_10_IRQn;
}

static void load_cell_hx711_ready_interrupt_disable(void)
{
    if (g_load_cell_hx711_irq_configured == 0U)
    {
        g_load_cell_hx711_ready = 0U;
        return;
    }

    const uint32_t mask = exti_line_mask(g_load_cell_hx711_irq_pin.pin);
    EXTI->IMR &= ~mask;
    EXTI->FTSR &= ~mask;
    EXTI->RTSR &= ~mask;
    EXTI->PR = mask;
    g_load_cell_hx711_irq_configured = 0U;
    g_load_cell_hx711_ready = 0U;
}

static void load_cell_hx711_ready_interrupt_configure(PinAssignment pin)
{
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    __DSB();

    const uint32_t exti_index = pin.pin / 4U;
    const uint32_t exti_shift = (pin.pin & 0x3U) * 4U;
    const uint32_t exti_mask = 0xFUL << exti_shift;
    const uint32_t line_mask = exti_line_mask(pin.pin);

    SYSCFG->EXTICR[exti_index] = (SYSCFG->EXTICR[exti_index] & ~exti_mask) | (gpio_port_exti_code(pin.portId) << exti_shift);
    EXTI->IMR &= ~line_mask;
    EXTI->RTSR &= ~line_mask;
    EXTI->FTSR |= line_mask;
    EXTI->PR = line_mask;
    EXTI->IMR |= line_mask;

    const IRQn_Type irq = exti_irqn_from_pin(pin.pin);
    HAL_NVIC_SetPriority(irq, 4U, 0U);
    HAL_NVIC_EnableIRQ(irq);

    g_load_cell_hx711_irq_pin = pin;
    g_load_cell_hx711_irq_configured = 1U;
    g_load_cell_hx711_ready = (pin_read(pin) == 0U) ? 1U : 0U;
}

static void handle_load_cell_hx711_exti(uint8_t pin)
{
    if ((g_load_cell_hx711_irq_configured == 0U) || (pin != g_load_cell_hx711_irq_pin.pin))
    {
        return;
    }

    const uint32_t mask = exti_line_mask(pin);
    if ((EXTI->PR & mask) == 0U)
    {
        return;
    }

    EXTI->PR = mask;
    g_load_cell_hx711_ready = 1U;
}

extern "C" void EXTI0_IRQHandler(void) { handle_load_cell_hx711_exti(0U); }
extern "C" void EXTI1_IRQHandler(void) { handle_load_cell_hx711_exti(1U); }
extern "C" void EXTI2_IRQHandler(void) { handle_load_cell_hx711_exti(2U); }
extern "C" void EXTI3_IRQHandler(void) { handle_load_cell_hx711_exti(3U); }
extern "C" void EXTI4_IRQHandler(void) { handle_load_cell_hx711_exti(4U); }
extern "C" void EXTI9_5_IRQHandler(void)
{
    for (uint8_t pin = 5U; pin <= 9U; ++pin)
    {
        handle_load_cell_hx711_exti(pin);
    }
}
extern "C" void EXTI15_10_IRQHandler(void)
{
    for (uint8_t pin = 10U; pin <= 15U; ++pin)
    {
        handle_load_cell_hx711_exti(pin);
    }
}

enum TmcSyncRequestFlags : uint8_t {
    TmcSyncNone = 0U,
    TmcSyncApply = 1U << 0,
    TmcSyncVerify = 1U << 1,
};

static uint8_t g_tmc_sync_pending = TmcSyncNone;

static uint8_t parse_u32_cstr(const char *text, uint32_t *value)
{
    uint32_t parsed = 0U;
    uint32_t index = 0U;
    if ((text == 0) || (text[0] == 0) || (value == 0))
    {
        return 0U;
    }
    while (text[index] != 0)
    {
        if ((text[index] < '0') || (text[index] > '9'))
        {
            return 0U;
        }
        parsed = (parsed * 10U) + (uint32_t)(text[index] - '0');
        ++index;
    }
    *value = parsed;
    return 1U;
}

static uint8_t parse_i32_cstr(const char *text, int32_t *value)
{
    uint32_t parsed = 0U;
    uint32_t index = 0U;
    uint8_t negative = 0U;

    if ((text == 0) || (text[0] == 0) || (value == 0))
    {
        return 0U;
    }

    if (text[index] == '-')
    {
        negative = 1U;
        ++index;
    }

    if (text[index] == 0)
    {
        return 0U;
    }

    while (text[index] != 0)
    {
        if ((text[index] < '0') || (text[index] > '9'))
        {
            return 0U;
        }
        parsed = (parsed * 10U) + (uint32_t)(text[index] - '0');
        if (((negative == 0U) && (parsed > (uint32_t)INT32_MAX)) || ((negative != 0U) && (parsed > 2147483648UL)))
        {
            return 0U;
        }
        ++index;
    }

    *value = (negative != 0U) ? -(int32_t)parsed : (int32_t)parsed;
    return 1U;
}

static uint8_t parse_bool_cstr(const char *text, uint8_t *value)
{
    if ((text == 0) || (value == 0))
    {
        return 0U;
    }
    if ((strcmp(text, "1") == 0) || (strcmp(text, "ON") == 0) || (strcmp(text, "TRUE") == 0))
    {
        *value = 1U;
        return 1U;
    }
    if ((strcmp(text, "0") == 0) || (strcmp(text, "OFF") == 0) || (strcmp(text, "FALSE") == 0))
    {
        *value = 0U;
        return 1U;
    }
    return 0U;
}

static uint8_t command_absolute_target_steps(const keyswitch::Command &command, int32_t *target_steps)
{
    if (target_steps == 0)
    {
        return 0U;
    }

    if (command.valueUnit == keyswitch::CommandValueUnit::Millimeters)
    {
        const int64_t target_um = (int64_t)command.value * 1000LL;
        if ((target_um < (int64_t)INT32_MIN) || (target_um > (int64_t)INT32_MAX))
        {
            return 0U;
        }

        *target_steps = axis_steps_from_signed_um(g_persisted_config, (int32_t)target_um);
        return 1U;
    }

    *target_steps = command.value;
    return 1U;
}

static uint8_t parse_channel_label_cstr(const char *text, char *value)
{
    uint32_t index = 0U;

    if ((text == 0) || (value == 0) || (text[0] == 0))
    {
        return 0U;
    }

    while ((text[index] != 0) && (index < (MOTION_CHANNEL_LABEL_LENGTH - 1U)))
    {
        const char c = text[index];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '_') || (c == '-')))
        {
            return 0U;
        }
        value[index] = c;
        ++index;
    }

    if ((text[index] != 0) || (index == 0U))
    {
        return 0U;
    }

    value[index] = 0;
    return 1U;
}

static uint8_t parse_channel_transport_cstr(const char *text, uint8_t *value)
{
    if ((text == 0) || (value == 0))
    {
        return 0U;
    }

    if (same_text_case_sensitive(text, "LOCAL_GPIO"))
    {
        *value = (uint8_t)MotionChannelTransportKind::LocalGpio;
        return 1U;
    }
    if (same_text_case_sensitive(text, "REMOTE_BUS"))
    {
        *value = (uint8_t)MotionChannelTransportKind::RemoteBus;
        return 1U;
    }
    if (same_text_case_sensitive(text, "VIRTUAL"))
    {
        *value = (uint8_t)MotionChannelTransportKind::Virtual;
        return 1U;
    }

    return 0U;
}

static uint8_t parse_scoped_channel_key(const char *key, uint8_t *channel_index, char *suffix, uint32_t suffix_length)
{
    uint32_t offset = 0U;
    uint32_t parsed_index = 0U;

    if ((key == 0) || (channel_index == 0) || (suffix == 0) || (suffix_length == 0U))
    {
        return 0U;
    }

    if ((key[0] == 'C') && (key[1] == 'H') && (key[2] == 'A') && (key[3] == 'N') && (key[4] == 'N') && (key[5] == 'E') && (key[6] == 'L') && (key[7] == '.'))
    {
        offset = 8U;
    }
    else if ((key[0] == 'C') && (key[1] == 'H'))
    {
        offset = 2U;
    }
    else
    {
        return 0U;
    }

    if ((key[offset] < '0') || (key[offset] > '9'))
    {
        return 0U;
    }

    while ((key[offset] >= '0') && (key[offset] <= '9'))
    {
        parsed_index = (parsed_index * 10U) + (uint32_t)(key[offset] - '0');
        ++offset;
    }

    if ((key[offset] != '.') || (parsed_index >= MOTION_CHANNEL_CAPACITY))
    {
        return 0U;
    }

    ++offset;
    if (key[offset] == 0)
    {
        return 0U;
    }

    strncpy(suffix, key + offset, suffix_length - 1U);
    suffix[suffix_length - 1U] = 0;
    *channel_index = (uint8_t)parsed_index;
    return 1U;
}

static const char *channel_transport_name_local(uint8_t kind)
{
    switch ((MotionChannelTransportKind)kind)
    {
    case MotionChannelTransportKind::RemoteBus: return "remote_bus";
    case MotionChannelTransportKind::Virtual: return "virtual";
    default: return "local_gpio";
    }
}

static const char *load_cell_source_name_local(uint8_t source)
{
    return load_cell_source_name(source);
}

static const char *load_cell_connector_name_local(uint8_t connector)
{
    return load_cell_connector_name(connector);
}

static void emit_channel_inventory_line(const PersistedFirmwareConfig &config, uint8_t index)
{
    const MotionChannelConfig &channel = motion_channel(config, index);
    char line[160];
    int len = snprintf(
        line,
        sizeof(line),
        "config channel[%lu] label=%s enabled=%lu transport=%s bus=%lu address=%lu active=%lu\r\n",
        (unsigned long)index,
        channel.label,
        (unsigned long)channel.enabled,
        channel_transport_name_local(channel.transportKind),
        (unsigned long)channel.transportIndex,
        (unsigned long)channel.transportAddress,
        (unsigned long)(config.activeMotionChannel == index));
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }
}

static void emit_indexed_channel_summary(const PersistedFirmwareConfig &config, uint8_t index)
{
    PersistedFirmwareConfig scoped = config;
    const MotionChannelConfig &channel = motion_channel(config, index);
    char line[192];
    char a[8];
    char b[8];
    char c[8];
    char d[8];
    char e[8];
    int len = 0;

    scoped.activeMotionChannel = index;

    len = snprintf(
        line,
        sizeof(line),
        "config loaded=%lu dirty=%lu reboot=%lu\r\n",
        (unsigned long)g_config_state.loadedFromFlash,
        (unsigned long)g_config_state.dirty,
        (unsigned long)g_config_state.requiresReboot);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }

    len = snprintf(
        line,
        sizeof(line),
        "config inspect_channel=%lu channels.count=%lu channels.active=%lu\r\n",
        (unsigned long)index,
        (unsigned long)config.motionChannelCount,
        (unsigned long)config.activeMotionChannel);
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
        channel_transport_name_local(channel.transportKind),
        (unsigned long)channel.transportIndex,
        (unsigned long)channel.transportAddress);
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
        load_cell_source_name_local(config.loadCell.source),
        load_cell_connector_name_local(config.loadCell.connector),
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
        (unsigned long)axis_steps_from_um(scoped, channel.travelLimitUm),
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
        (long)axis_travel_max_um(scoped),
        (unsigned long)channel.travelLimitUm,
        (unsigned long)channel.defaultPressUm);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }

    len = snprintf(
        line,
        sizeof(line),
        "config telemetry.status_interval_ms=%lu telemetry.heartbeat_interval_ms=%lu boot.host_wait_ms=%lu sim.load_threshold=%lu tmc.allow_unverified_motion=%lu panel.color_rgb=%u,%u,%u\r\n",
        (unsigned long)config.statusIntervalMs,
        (unsigned long)config.heartbeatIntervalMs,
        (unsigned long)config.bootHostWaitMs,
        (unsigned long)config.loadCell.threshold,
        (unsigned long)config.allowUnverifiedTmcMotion,
        (unsigned int)config.panelColorRed,
        (unsigned int)config.panelColorGreen,
        (unsigned int)config.panelColorBlue);
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

static uint8_t tmc_motion_allowed(void)
{
    return (x_tmc_verified() != 0U) || (g_persisted_config.allowUnverifiedTmcMotion != 0U);
}

static uint8_t motion_state_busy_local(const keyswitch::MotionState &state)
{
    return ((state.homingState == keyswitch::HomingState::Seek) ||
            (state.homingState == keyswitch::HomingState::Backoff) ||
            (state.homingState == keyswitch::HomingState::MoveToTarget) ||
            (state.homingState == keyswitch::HomingState::CycleToPress) ||
            (state.homingState == keyswitch::HomingState::CycleToHome))
               ? 1U
               : 0U;
}

static void request_tmc_sync(uint8_t flags)
{
    g_tmc_sync_pending = (uint8_t)(g_tmc_sync_pending | flags);
}

static void service_tmc_sync(keyswitch::MotionState *motion_state)
{
    if ((motion_state == 0) || (g_tmc_sync_pending == TmcSyncNone) || (motion_state_busy_local(*motion_state) != 0U))
    {
        return;
    }

    const uint8_t pending = g_tmc_sync_pending;
    g_tmc_sync_pending = TmcSyncNone;

    if ((pending & TmcSyncApply) != 0U)
    {
        x_tmc2209_apply_config(g_persisted_config);
    }
    else if ((pending & TmcSyncVerify) != 0U)
    {
        x_tmc2209_refresh_verification(g_persisted_config);
    }

    if (tmc_motion_allowed() == 0U)
    {
        keyswitch::setHoldEnabled(motion_state, 0U);
    }
}

static uint8_t parse_pin_assignment(const char *text, PinAssignment *pin)
{
    if ((text == 0) || (pin == 0) || (text[0] != 'P'))
    {
        return 0U;
    }
    if ((text[1] < 'A') || (text[1] > 'H'))
    {
        return 0U;
    }
    uint32_t pin_number = 0U;
    if (parse_u32_cstr(text + 2U, &pin_number) == 0U)
    {
        return 0U;
    }
    pin->portId = (uint8_t)((text[1] - 'A') + (uint8_t)GpioPortId::A);
    pin->pin = (uint8_t)pin_number;
    return pin_assignment_valid(*pin);
}

static uint8_t same_pin_assignment(const PinAssignment &lhs, const PinAssignment &rhs)
{
    return (lhs.portId == rhs.portId) && (lhs.pin == rhs.pin) ? 1U : 0U;
}

static uint8_t led_pin_conflicts_with_load_cell(const PersistedFirmwareConfig &config)
{
    if ((uint8_t)config.loadCell.source != (uint8_t)LoadCellSourceKind::Hx711)
    {
        return 0U;
    }

    if ((pin_assignment_valid(config.loadCell.pins.data) == 0U) ||
        (pin_assignment_valid(config.loadCell.pins.clock) == 0U))
    {
        return 0U;
    }

    return (same_pin_assignment(config.pins.led, config.loadCell.pins.data) != 0U) ||
           (same_pin_assignment(config.pins.led, config.loadCell.pins.clock) != 0U)
               ? 1U
               : 0U;
}

static uint8_t axis_default_press_within_workspace(int32_t min_um, uint32_t span_um, uint32_t default_press_um)
{
    const int64_t max_um = (int64_t)min_um + (int64_t)span_um;
    return ((int64_t)default_press_um >= (int64_t)min_um) && ((int64_t)default_press_um <= max_um) ? 1U : 0U;
}

static ApplyConfigResult apply_config_key_value(
    PersistedFirmwareConfig *config,
    const char *key,
    const char *value,
    uint8_t apply_live,
    keyswitch::RuntimeConfig *runtime_config,
    keyswitch::MotionConfig *motion_config,
    keyswitch::MotionState *motion_state);

static void usb_write_str(const char *s)
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

struct StepScheduler {
    uint32_t lastStepTickUs;
    uint32_t stepIntervalUs;
    uint32_t pulseWidthUs;
    uint8_t pulseHigh;
    uint8_t issuedCount;
    uint8_t runRequested;
};

struct RuntimeMonitor {
    uint32_t lastLoopUs;
    uint32_t lastLoopDeltaUs;
    uint32_t maxLoopDeltaUs;
    uint32_t emittedStepsTotal;
    uint32_t emittedStepsSinceHeartbeat;
    uint8_t maxStepBurstSinceHeartbeat;
};

static LoadCellRuntime g_load_cell = load_cell_make_default(1000U);

static StepScheduler g_step_scheduler = {
    0U,
    0U,
    1U,
    0U,
    0U,
    0U,
};

static RuntimeMonitor g_runtime_monitor = {
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
};

static uint32_t step_scheduler_timer_now_us(void)
{
    return TIM5->CNT;
}

static void step_scheduler_timer_disarm(void)
{
    TIM5->DIER &= ~TIM_DIER_CC1IE;
}

static void step_scheduler_timer_arm_us(uint32_t delay_us)
{
    const uint32_t now_us = step_scheduler_timer_now_us();
    TIM5->CCR1 = now_us + ((delay_us == 0U) ? 1U : delay_us);
    TIM5->SR &= ~TIM_SR_CC1IF;
    TIM5->DIER |= TIM_DIER_CC1IE;
}

static void step_scheduler_schedule_locked(void)
{
    if ((g_step_scheduler.runRequested == 0U) || (g_step_scheduler.pulseHigh != 0U))
    {
        return;
    }

    const uint32_t now_us = step_scheduler_timer_now_us();
    const uint32_t elapsed_us = now_us - g_step_scheduler.lastStepTickUs;
    const uint32_t delay_us = (elapsed_us >= g_step_scheduler.stepIntervalUs) ? 1U : (g_step_scheduler.stepIntervalUs - elapsed_us);
    step_scheduler_timer_arm_us(delay_us);
}

extern "C" void TIM5_IRQHandler(void)
{
    if ((TIM5->SR & TIM_SR_CC1IF) == 0U)
    {
        return;
    }

    TIM5->SR &= ~TIM_SR_CC1IF;

    if (g_step_scheduler.pulseHigh != 0U)
    {
        pin_write_low(active_motion_channel(g_persisted_config).pins.step);
        g_step_scheduler.pulseHigh = 0U;
        if (g_step_scheduler.runRequested != 0U)
        {
            step_scheduler_schedule_locked();
        }
        else
        {
            step_scheduler_timer_disarm();
        }
        return;
    }

    if (g_step_scheduler.runRequested == 0U)
    {
        step_scheduler_timer_disarm();
        return;
    }

    pin_write_high(active_motion_channel(g_persisted_config).pins.step);
    g_step_scheduler.pulseHigh = 1U;
    if (g_step_scheduler.issuedCount < 0xFFU)
    {
        ++g_step_scheduler.issuedCount;
    }
    g_step_scheduler.lastStepTickUs = step_scheduler_timer_now_us();
    step_scheduler_timer_arm_us(g_step_scheduler.pulseWidthUs);
}

static uint32_t step_interval_us_from_feedrate_mm_per_min(const PersistedFirmwareConfig &config, uint32_t feedrate_mm_per_min)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    if ((feedrate_mm_per_min == 0U) || (channel.stepsPerRotation == 0U) || (channel.travelUmPerRotation == 0U))
    {
        return 1U;
    }

    const uint64_t numerator = (60000ULL * (uint64_t)channel.travelUmPerRotation) + (((uint64_t)feedrate_mm_per_min * (uint64_t)channel.stepsPerRotation) / 2ULL);
    const uint64_t denominator = (uint64_t)feedrate_mm_per_min * (uint64_t)channel.stepsPerRotation;
    const uint32_t interval_us = (uint32_t)(numerator / denominator);
    return (interval_us == 0U) ? 1U : interval_us;
}

static uint32_t feedrate_mm_per_min_from_step_interval_us(const PersistedFirmwareConfig &config, uint32_t step_interval_us)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    if ((step_interval_us == 0U) || (channel.stepsPerRotation == 0U) || (channel.travelUmPerRotation == 0U))
    {
        return 0U;
    }

    const uint64_t numerator = (60000ULL * (uint64_t)channel.travelUmPerRotation) + (((uint64_t)step_interval_us * (uint64_t)channel.stepsPerRotation) / 2ULL);
    const uint64_t denominator = (uint64_t)step_interval_us * (uint64_t)channel.stepsPerRotation;
    const uint32_t feedrate = (uint32_t)(numerator / denominator);
    return (feedrate == 0U) ? 1U : feedrate;
}

static uint32_t active_step_interval_us_for_state(const PersistedFirmwareConfig &config, const keyswitch::MotionState &motion_state)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    const uint32_t feedrate_mm_per_min =
        ((motion_state.homingState == keyswitch::HomingState::Seek) || (motion_state.homingState == keyswitch::HomingState::Backoff))
            ? channel.homeFeedrateMmPerMin
            : channel.moveFeedrateMmPerMin;
    return step_interval_us_from_feedrate_mm_per_min(config, feedrate_mm_per_min);
}

static const MotionChannelConfig &current_motion_channel_const()
{
    return active_motion_channel(g_persisted_config);
}

static inline uint32_t x_stop_pressed_from_raw(uint32_t raw_level)
{
    return current_motion_channel_const().stopSignalActiveHigh ? (raw_level != 0U) : (raw_level == 0U);
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

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK)
    {
        return 0U;
    }

    return 1U;
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

static uint32_t cycles_from_us(uint32_t microseconds)
{
    const uint64_t cycles = ((uint64_t)SystemCoreClock * (uint64_t)microseconds) / 1000000ULL;
    return (cycles == 0ULL) ? 1U : (uint32_t)cycles;
}

static uint32_t timer_clock_hz_apb1(void)
{
    const uint32_t pclk1_hz = HAL_RCC_GetPCLK1Freq();
    return ((RCC->CFGR & RCC_CFGR_PPRE1) == 0U) ? pclk1_hz : (pclk1_hz * 2U);
}

static void step_scheduler_timer_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
    __DSB();

    TIM5->CR1 = 0U;
    TIM5->CR2 = 0U;
    TIM5->SMCR = 0U;
    TIM5->DIER = 0U;
    TIM5->CCER = 0U;
    TIM5->CCMR1 = 0U;
    TIM5->CCMR2 = 0U;
    TIM5->PSC = (uint32_t)((timer_clock_hz_apb1() / 1000000U) - 1U);
    TIM5->ARR = 0xFFFFFFFFU;
    TIM5->CNT = 0U;
    TIM5->EGR = TIM_EGR_UG;
    TIM5->SR = 0U;
    TIM5->CR1 = TIM_CR1_CEN;

    HAL_NVIC_SetPriority(TIM5_IRQn, 3U, 0U);
    HAL_NVIC_EnableIRQ(TIM5_IRQn);
}

static void step_scheduler_init(void)
{
    g_step_scheduler.lastStepTickUs = step_scheduler_timer_now_us();
    g_step_scheduler.stepIntervalUs = step_interval_us_from_feedrate_mm_per_min(g_persisted_config, current_motion_channel_const().moveFeedrateMmPerMin);
    g_step_scheduler.pulseWidthUs = g_persisted_config.stepPulseWidthUs;
    g_step_scheduler.pulseHigh = 0U;
    g_step_scheduler.issuedCount = 0U;
    g_step_scheduler.runRequested = 0U;
    step_scheduler_timer_disarm();
    pin_write_low(current_motion_channel_const().pins.step);
}

static void step_scheduler_service(uint32_t now_cycles)
{
    (void)now_cycles;
}

static uint8_t step_scheduler_take_issued_step(void)
{
    __disable_irq();
    const uint8_t issued = g_step_scheduler.issuedCount;
    g_step_scheduler.issuedCount = 0U;
    __enable_irq();
    return issued;
}

static void step_scheduler_reset(void)
{
    __disable_irq();
    step_scheduler_timer_disarm();
    pin_write_low(current_motion_channel_const().pins.step);
    g_step_scheduler.pulseHigh = 0U;
    g_step_scheduler.issuedCount = 0U;
    g_step_scheduler.runRequested = 0U;
    g_step_scheduler.lastStepTickUs = step_scheduler_timer_now_us();
    __enable_irq();
}

static void step_scheduler_set_run_requested(uint8_t run_requested)
{
    __disable_irq();
    g_step_scheduler.runRequested = (uint8_t)(run_requested != 0U ? 1U : 0U);
    if (g_step_scheduler.runRequested != 0U)
    {
        step_scheduler_schedule_locked();
    }
    else if (g_step_scheduler.pulseHigh == 0U)
    {
        step_scheduler_timer_disarm();
    }
    __enable_irq();
}

static void runtime_monitor_record_loop(uint32_t now_us)
{
    if (g_runtime_monitor.lastLoopUs != 0U)
    {
        const uint32_t loop_delta_us = now_us - g_runtime_monitor.lastLoopUs;
        g_runtime_monitor.lastLoopDeltaUs = loop_delta_us;
        if (loop_delta_us > g_runtime_monitor.maxLoopDeltaUs)
        {
            g_runtime_monitor.maxLoopDeltaUs = loop_delta_us;
        }
    }

    g_runtime_monitor.lastLoopUs = now_us;
}

static void runtime_monitor_record_steps(uint8_t step_count)
{
    if (step_count == 0U)
    {
        return;
    }

    g_runtime_monitor.emittedStepsTotal += step_count;
    g_runtime_monitor.emittedStepsSinceHeartbeat += step_count;
    if (step_count > g_runtime_monitor.maxStepBurstSinceHeartbeat)
    {
        g_runtime_monitor.maxStepBurstSinceHeartbeat = step_count;
    }
}

static void runtime_monitor_roll_heartbeat_window(void)
{
    g_runtime_monitor.emittedStepsSinceHeartbeat = 0U;
    g_runtime_monitor.maxLoopDeltaUs = 0U;
    g_runtime_monitor.maxStepBurstSinceHeartbeat = 0U;
}

static void apply_motion_outputs(const keyswitch::MotionOutputs &outputs, uint32_t now_cycles)
{
    (void)now_cycles;
    if (outputs.dirHigh != 0U)
    {
        pin_write_high(current_motion_channel_const().pins.dir);
    }
    else
    {
        pin_write_low(current_motion_channel_const().pins.dir);
    }

    if (outputs.driverEnable != 0U)
    {
        x_driver_enable(g_persisted_config);
    }
    else
    {
        x_driver_disable(g_persisted_config);
        step_scheduler_reset();
        return;
    }

    step_scheduler_set_run_requested(outputs.issueStep);

    if (led_pin_conflicts_with_load_cell(g_persisted_config) == 0U)
    {
        if (outputs.ledOn != 0U)
        {
            pin_write_high(g_persisted_config.pins.led);
        }
        else
        {
            pin_write_low(g_persisted_config.pins.led);
        }
    }
}

static keyswitch::MotionConfig make_motion_config(const PersistedFirmwareConfig &config)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    keyswitch::MotionConfig motion_config = {};
    motion_config.seekStepLimit = axis_steps_from_um(config, channel.travelLimitUm);
    motion_config.minPosition = axis_steps_from_signed_um(config, channel.travelMinUm);
    motion_config.maxPosition = axis_steps_from_signed_um(config, axis_travel_max_um(config));
    motion_config.debounceCount = config.stopDebounceCount;
    motion_config.backoffSteps = config.backoffSteps;
    motion_config.statusIntervalMs = config.statusIntervalMs;
    motion_config.heartbeatIntervalMs = config.heartbeatIntervalMs;
    return motion_config;
}

static void apply_default_press_target(
    keyswitch::MotionState *motion_state,
    const PersistedFirmwareConfig &config,
    const keyswitch::MotionConfig &motion_config)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    if (motion_state == 0)
    {
        return;
    }

    const uint32_t default_press_steps = axis_steps_from_um(config, channel.defaultPressUm);
    if (default_press_steps > 0U)
    {
        keyswitch::setPressTarget(motion_state, (int32_t)default_press_steps, motion_config);
    }
}

static ApplyConfigResult apply_config_key_value(
    PersistedFirmwareConfig *config,
    const char *key,
    const char *value,
    uint8_t apply_live,
    keyswitch::RuntimeConfig *runtime_config,
    keyswitch::MotionConfig *motion_config,
    keyswitch::MotionState *motion_state)
{
    ApplyConfigResult result = {0U, 0U};
    uint8_t bool_value = 0U;
    uint8_t enum_value = 0U;
    uint32_t parsed_u32 = 0U;
    int32_t parsed_i32 = 0;
    PinAssignment pin = {};
    char label[MOTION_CHANNEL_LABEL_LENGTH] = {};

    if ((config == 0) || (key == 0) || (value == 0))
    {
        return result;
    }

    uint8_t scoped_channel_index = 0U;
    char scoped_suffix[32] = {0};
    if (parse_scoped_channel_key(key, &scoped_channel_index, scoped_suffix, sizeof(scoped_suffix)) != 0U)
    {
        const uint8_t original_active = config->activeMotionChannel;
        if (scoped_channel_index >= config->motionChannelCount)
        {
            return result;
        }
        config->activeMotionChannel = scoped_channel_index;
        result = apply_config_key_value(config, scoped_suffix, value, apply_live, runtime_config, motion_config, motion_state);
        config->activeMotionChannel = original_active;
        return result;
    }

    MotionChannelConfig &channel = active_motion_channel(*config);

    if (same_text_case_sensitive(key, "CHANNEL.COUNT") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 > 0U) && (parsed_u32 <= MOTION_CHANNEL_CAPACITY) && (config->activeMotionChannel < parsed_u32))
    {
        config->motionChannelCount = (uint8_t)parsed_u32;
        result.accepted = 1U;
    }
    else if (same_text_case_sensitive(key, "CHANNEL.ACTIVE") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 < config->motionChannelCount) && (motion_channel(*config, (uint8_t)parsed_u32).transportKind == (uint8_t)MotionChannelTransportKind::LocalGpio))
    {
        config->activeMotionChannel = (uint8_t)parsed_u32;
        result.accepted = 1U;
        result.rebootRequired = 1U;
        if ((apply_live != 0U) && (motion_config != 0))
        {
            *motion_config = make_motion_config(*config);
            apply_default_press_target(motion_state, *config, *motion_config);
        }
    }
    else if ((same_text_case_sensitive(key, "PIN.X_UART") || same_text_case_sensitive(key, "PIN.UART")) && parse_pin_assignment(value, &pin))
    {
        channel.pins.uart = pin;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if ((same_text_case_sensitive(key, "PIN.X_DIR") || same_text_case_sensitive(key, "PIN.DIR")) && parse_pin_assignment(value, &pin))
    {
        channel.pins.dir = pin;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if ((same_text_case_sensitive(key, "PIN.X_STEP") || same_text_case_sensitive(key, "PIN.STEP")) && parse_pin_assignment(value, &pin))
    {
        channel.pins.step = pin;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if ((same_text_case_sensitive(key, "PIN.X_ENABLE") || same_text_case_sensitive(key, "PIN.ENABLE")) && parse_pin_assignment(value, &pin))
    {
        channel.pins.enable = pin;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if ((same_text_case_sensitive(key, "PIN.X_STOP") || same_text_case_sensitive(key, "PIN.STOP")) && parse_pin_assignment(value, &pin))
    {
        channel.pins.stop = pin;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if (same_text_case_sensitive(key, "PIN.DIAG0") && parse_pin_assignment(value, &pin))
    {
        config->pins.diag0 = pin;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if (same_text_case_sensitive(key, "PIN.DIAG2") && parse_pin_assignment(value, &pin))
    {
        config->pins.diag2 = pin;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if (same_text_case_sensitive(key, "PIN.PS_ON") && parse_pin_assignment(value, &pin))
    {
        config->pins.psOn = pin;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if (same_text_case_sensitive(key, "PIN.SAFE_POWER") && parse_pin_assignment(value, &pin))
    {
        config->pins.safePower = pin;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if (same_text_case_sensitive(key, "PIN.LED") && parse_pin_assignment(value, &pin))
    {
        config->pins.led = pin;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if (same_text_case_sensitive(key, "PIN.LOADCELL_DATA") && parse_pin_assignment(value, &pin))
    {
        config->loadCell.pins.data = pin;
        config->loadCell.connector = (uint8_t)LoadCellConnectorKind::Custom;
        result.accepted = 1U;
        result.rebootRequired = 1U;
        if (apply_live != 0U)
        {
            load_cell_clear(&g_load_cell);
            load_cell_apply_config(&g_load_cell, config->loadCell);
            configure_load_cell_io(config->loadCell);
        }
    }
    else if (same_text_case_sensitive(key, "PIN.LOADCELL_CLOCK") && parse_pin_assignment(value, &pin))
    {
        config->loadCell.pins.clock = pin;
        config->loadCell.connector = (uint8_t)LoadCellConnectorKind::Custom;
        result.accepted = 1U;
        result.rebootRequired = 1U;
        if (apply_live != 0U)
        {
            load_cell_clear(&g_load_cell);
            load_cell_apply_config(&g_load_cell, config->loadCell);
            configure_load_cell_io(config->loadCell);
        }
    }
    else if (same_text_case_sensitive(key, "LOGIC.STOP_ACTIVE_HIGH") && parse_bool_cstr(value, &bool_value))
    {
        channel.stopSignalActiveHigh = bool_value;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if (same_text_case_sensitive(key, "LOGIC.DIR_INVERTED") && parse_bool_cstr(value, &bool_value))
    {
        channel.dirInverted = bool_value;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if (same_text_case_sensitive(key, "LOGIC.ENABLE_ACTIVE_LOW") && parse_bool_cstr(value, &bool_value))
    {
        channel.enableActiveLow = bool_value;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if (same_text_case_sensitive(key, "LOGIC.HOME_TOWARDS_POSITIVE") && parse_bool_cstr(value, &bool_value))
    {
        channel.homeTowardsPositive = bool_value;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if ((same_text_case_sensitive(key, "CHANNEL.ENABLED") || same_text_case_sensitive(key, "ENABLED")) && parse_bool_cstr(value, &bool_value))
    {
        channel.enabled = bool_value;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if ((same_text_case_sensitive(key, "CHANNEL.LABEL") || same_text_case_sensitive(key, "LABEL")) && parse_channel_label_cstr(value, label))
    {
        strncpy(channel.label, label, MOTION_CHANNEL_LABEL_LENGTH - 1U);
        channel.label[MOTION_CHANNEL_LABEL_LENGTH - 1U] = 0;
        result.accepted = 1U;
    }
    else if ((same_text_case_sensitive(key, "CHANNEL.TRANSPORT") || same_text_case_sensitive(key, "TRANSPORT")) && parse_channel_transport_cstr(value, &enum_value))
    {
        channel.transportKind = enum_value;
        result.accepted = 1U;
        result.rebootRequired = 1U;
    }
    else if ((same_text_case_sensitive(key, "CHANNEL.BUS_INDEX") || same_text_case_sensitive(key, "BUS_INDEX")) && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 <= 255U))
    {
        channel.transportIndex = (uint8_t)parsed_u32;
        result.accepted = 1U;
    }
    else if ((same_text_case_sensitive(key, "CHANNEL.ADDRESS") || same_text_case_sensitive(key, "ADDRESS")) && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 <= 255U))
    {
        channel.transportAddress = (uint8_t)parsed_u32;
        result.accepted = 1U;
    }
    else if (same_text_case_sensitive(key, "TMC.ALLOW_UNVERIFIED_MOTION") && parse_bool_cstr(value, &bool_value))
    {
        config->allowUnverifiedTmcMotion = bool_value;
        result.accepted = 1U;
    }
    else if (same_text_case_sensitive(key, "LOADCELL.SOURCE") && load_cell_source_from_cstr(value, &enum_value))
    {
        config->loadCell.source = enum_value;
        result.accepted = 1U;
        result.rebootRequired = 1U;
        if (apply_live != 0U)
        {
            load_cell_clear(&g_load_cell);
            load_cell_apply_config(&g_load_cell, config->loadCell);
            configure_load_cell_io(config->loadCell);
        }
    }
    else if (same_text_case_sensitive(key, "LOADCELL.CONNECTOR") && load_cell_connector_from_cstr(value, &enum_value))
    {
        load_cell_apply_connector_profile(&config->loadCell, enum_value);
        result.accepted = 1U;
        result.rebootRequired = 1U;
        if (apply_live != 0U)
        {
            load_cell_clear(&g_load_cell);
            load_cell_apply_config(&g_load_cell, config->loadCell);
            configure_load_cell_io(config->loadCell);
        }
    }
    else if ((same_text_case_sensitive(key, "LOADCELL.THRESHOLD") || same_text_case_sensitive(key, "SIM.LOAD_THRESHOLD")) && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 > 0U))
    {
        config->loadCell.threshold = parsed_u32;
        result.accepted = 1U;
        if (apply_live != 0U)
        {
            load_cell_apply_config(&g_load_cell, config->loadCell);
        }
    }
    else if (same_text_case_sensitive(key, "MOTION.SEEK_LIMIT_STEPS") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 > 0U))
    {
        const uint32_t span_um = axis_um_from_steps(*config, parsed_u32);
        const int64_t max_um = (int64_t)channel.travelMinUm + (int64_t)span_um;
        if (max_um <= INT32_MAX)
        {
            channel.seekStepLimit = parsed_u32;
            channel.travelLimitUm = span_um;
            result.accepted = 1U;
            if ((apply_live != 0U) && (motion_config != 0))
            {
                *motion_config = make_motion_config(*config);
            }
        }
    }
    else if (same_text_case_sensitive(key, "MOTION.STOP_DEBOUNCE_COUNT") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 > 0U))
    {
        config->stopDebounceCount = (uint16_t)((parsed_u32 > 65535U) ? 65535U : parsed_u32);
        result.accepted = 1U;
    }
    else if (same_text_case_sensitive(key, "MOTION.BACKOFF_STEPS") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 > 0U))
    {
        config->backoffSteps = (uint16_t)((parsed_u32 > 65535U) ? 65535U : parsed_u32);
        result.accepted = 1U;
        if ((apply_live != 0U) && (motion_config != 0))
        {
            *motion_config = make_motion_config(*config);
        }
    }
    else if (same_text_case_sensitive(key, "MOTION.STEP_INTERVAL_US") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 > 0U))
    {
        config->stepIntervalUs = (uint16_t)((parsed_u32 > 65535U) ? 65535U : parsed_u32);
        channel.homeFeedrateMmPerMin = feedrate_mm_per_min_from_step_interval_us(*config, config->stepIntervalUs);
        channel.moveFeedrateMmPerMin = channel.homeFeedrateMmPerMin;
        result.accepted = 1U;
        if ((apply_live != 0U) && (motion_state != 0))
        {
            g_step_scheduler.stepIntervalUs = active_step_interval_us_for_state(*config, *motion_state);
        }
    }
    else if (same_text_case_sensitive(key, "MOTION.HOME_FEEDRATE_MM_PER_MIN") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 > 0U))
    {
        channel.homeFeedrateMmPerMin = parsed_u32;
        result.accepted = 1U;
        if ((apply_live != 0U) && (motion_state != 0))
        {
            g_step_scheduler.stepIntervalUs = active_step_interval_us_for_state(*config, *motion_state);
        }
    }
    else if (same_text_case_sensitive(key, "MOTION.MOVE_FEEDRATE_MM_PER_MIN") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 > 0U))
    {
        channel.moveFeedrateMmPerMin = parsed_u32;
        result.accepted = 1U;
        if ((apply_live != 0U) && (motion_state != 0))
        {
            g_step_scheduler.stepIntervalUs = active_step_interval_us_for_state(*config, *motion_state);
        }
    }
    else if (same_text_case_sensitive(key, "MOTION.STEP_PULSE_US") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 > 0U))
    {
        config->stepPulseWidthUs = (uint16_t)((parsed_u32 > 65535U) ? 65535U : parsed_u32);
        result.accepted = 1U;
        if (apply_live != 0U)
        {
            g_step_scheduler.pulseWidthUs = config->stepPulseWidthUs;
        }
    }
    else if (same_text_case_sensitive(key, "BOOT.HOST_WAIT_MS") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 <= 30000U))
    {
        config->bootHostWaitMs = parsed_u32;
        result.accepted = 1U;
    }
    else if (same_text_case_sensitive(key, "PANEL.COLOR_RED") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 <= 255U))
    {
        config->panelColorRed = (uint8_t)parsed_u32;
        result.accepted = 1U;
        if (apply_live != 0U)
        {
            mini12864_panel_set_color(g_panel_pins, config->panelColorRed, config->panelColorGreen, config->panelColorBlue);
        }
    }
    else if (same_text_case_sensitive(key, "PANEL.COLOR_GREEN") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 <= 255U))
    {
        config->panelColorGreen = (uint8_t)parsed_u32;
        result.accepted = 1U;
        if (apply_live != 0U)
        {
            mini12864_panel_set_color(g_panel_pins, config->panelColorRed, config->panelColorGreen, config->panelColorBlue);
        }
    }
    else if (same_text_case_sensitive(key, "PANEL.COLOR_BLUE") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 <= 255U))
    {
        config->panelColorBlue = (uint8_t)parsed_u32;
        result.accepted = 1U;
        if (apply_live != 0U)
        {
            mini12864_panel_set_color(g_panel_pins, config->panelColorRed, config->panelColorGreen, config->panelColorBlue);
        }
    }
    else if (same_text_case_sensitive(key, "AXIS.STEPS_PER_ROTATION") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 > 0U))
    {
        channel.stepsPerRotation = parsed_u32;
        channel.seekStepLimit = axis_steps_from_um(*config, channel.travelLimitUm);
        result.accepted = 1U;
        if ((apply_live != 0U) && (motion_config != 0))
        {
            *motion_config = make_motion_config(*config);
            apply_default_press_target(motion_state, *config, *motion_config);
        }
    }
    else if (same_text_case_sensitive(key, "AXIS.TRAVEL_UM_PER_ROTATION") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 > 0U))
    {
        channel.travelUmPerRotation = parsed_u32;
        channel.seekStepLimit = axis_steps_from_um(*config, channel.travelLimitUm);
        result.accepted = 1U;
        if ((apply_live != 0U) && (motion_config != 0))
        {
            *motion_config = make_motion_config(*config);
            apply_default_press_target(motion_state, *config, *motion_config);
        }
    }
    else if (same_text_case_sensitive(key, "AXIS.TRAVEL_LIMIT_UM") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 > 0U))
    {
        const int64_t max_um = (int64_t)channel.travelMinUm + (int64_t)parsed_u32;
        if ((max_um <= INT32_MAX) && (axis_default_press_within_workspace(channel.travelMinUm, parsed_u32, channel.defaultPressUm) != 0U))
        {
            channel.travelLimitUm = parsed_u32;
            channel.seekStepLimit = axis_steps_from_um(*config, channel.travelLimitUm);
            result.accepted = 1U;
            if ((apply_live != 0U) && (motion_config != 0))
            {
                *motion_config = make_motion_config(*config);
                apply_default_press_target(motion_state, *config, *motion_config);
            }
        }
    }
    else if (same_text_case_sensitive(key, "AXIS.TRAVEL_MIN_UM") && parse_i32_cstr(value, &parsed_i32))
    {
        const int64_t max_um = (int64_t)parsed_i32 + (int64_t)channel.travelLimitUm;
        if ((max_um > parsed_i32) && (max_um <= INT32_MAX) && (axis_default_press_within_workspace(parsed_i32, channel.travelLimitUm, channel.defaultPressUm) != 0U))
        {
            channel.travelMinUm = parsed_i32;
            result.accepted = 1U;
            if ((apply_live != 0U) && (motion_config != 0))
            {
                *motion_config = make_motion_config(*config);
                apply_default_press_target(motion_state, *config, *motion_config);
            }
        }
    }
    else if (same_text_case_sensitive(key, "AXIS.TRAVEL_MAX_UM") && parse_i32_cstr(value, &parsed_i32) && (parsed_i32 > channel.travelMinUm))
    {
        const uint32_t span_um = (uint32_t)(parsed_i32 - channel.travelMinUm);
        if (axis_default_press_within_workspace(channel.travelMinUm, span_um, channel.defaultPressUm) != 0U)
        {
            channel.travelLimitUm = span_um;
            channel.seekStepLimit = axis_steps_from_um(*config, channel.travelLimitUm);
            result.accepted = 1U;
            if ((apply_live != 0U) && (motion_config != 0))
            {
                *motion_config = make_motion_config(*config);
                apply_default_press_target(motion_state, *config, *motion_config);
            }
        }
    }
    else if (same_text_case_sensitive(key, "AXIS.DEFAULT_PRESS_UM") && parse_u32_cstr(value, &parsed_u32))
    {
        channel.defaultPressUm = parsed_u32;
        result.accepted = 1U;
        if ((apply_live != 0U) && (motion_config != 0))
        {
            *motion_config = make_motion_config(*config);
            apply_default_press_target(motion_state, *config, *motion_config);
        }
    }
    else if (same_text_case_sensitive(key, "TELEMETRY.STATUS_INTERVAL_MS") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 > 0U) && (parsed_u32 <= 60000U))
    {
        config->statusIntervalMs = parsed_u32;
        result.accepted = 1U;
        if ((apply_live != 0U) && (motion_config != 0))
        {
            motion_config->statusIntervalMs = parsed_u32;
        }
    }
    else if (same_text_case_sensitive(key, "TELEMETRY.HEARTBEAT_INTERVAL_MS") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 > 0U) && (parsed_u32 <= 60000U))
    {
        config->heartbeatIntervalMs = parsed_u32;
        result.accepted = 1U;
        if ((apply_live != 0U) && (motion_config != 0))
        {
            motion_config->heartbeatIntervalMs = parsed_u32;
        }
    }
    else if (same_text_case_sensitive(key, "TMC.UART_BIT_US") && parse_u32_cstr(value, &parsed_u32) && (parsed_u32 > 0U))
    {
        config->tmcUartBitUs = (uint16_t)((parsed_u32 > 65535U) ? 65535U : parsed_u32);
        result.accepted = 1U;
    }
    else if (same_text_case_sensitive(key, "TMC.IRUN") && parse_u32_cstr(value, &parsed_u32))
    {
        channel.tmc2209.irun = keyswitch::clampTmcCurrentScale((int32_t)parsed_u32);
        result.accepted = 1U;
        if (apply_live != 0U)
        {
            tmc_driver_runtime().tmc2209.irun = channel.tmc2209.irun;
            request_tmc_sync(TmcSyncApply);
        }
    }
    else if (same_text_case_sensitive(key, "TMC.IHOLD") && parse_u32_cstr(value, &parsed_u32))
    {
        channel.tmc2209.ihold = keyswitch::clampTmcCurrentScale((int32_t)parsed_u32);
        result.accepted = 1U;
        if (apply_live != 0U)
        {
            tmc_driver_runtime().tmc2209.ihold = channel.tmc2209.ihold;
            request_tmc_sync(TmcSyncApply);
        }
    }
    else if (same_text_case_sensitive(key, "TMC.IHOLDDELAY") && parse_u32_cstr(value, &parsed_u32))
    {
        channel.tmc2209.iholddelay = (uint8_t)((parsed_u32 > 15U) ? 15U : parsed_u32);
        result.accepted = 1U;
        if (apply_live != 0U)
        {
            tmc_driver_runtime().tmc2209.iholddelay = channel.tmc2209.iholddelay;
            request_tmc_sync(TmcSyncApply);
        }
    }
    else if (same_text_case_sensitive(key, "TMC.TPOWERDOWN") && parse_u32_cstr(value, &parsed_u32))
    {
        channel.tmc2209.tpowerdown = (uint8_t)((parsed_u32 > 255U) ? 255U : parsed_u32);
        result.accepted = 1U;
        if (apply_live != 0U)
        {
            tmc_driver_runtime().tmc2209.tpowerdown = channel.tmc2209.tpowerdown;
            request_tmc_sync(TmcSyncApply);
        }
    }
    else if (same_text_case_sensitive(key, "TMC.SGTHRS") && parse_u32_cstr(value, &parsed_u32))
    {
        channel.tmc2209.sgthrs = (uint8_t)((parsed_u32 > 255U) ? 255U : parsed_u32);
        result.accepted = 1U;
        if (apply_live != 0U)
        {
            tmc_driver_runtime().tmc2209.sgthrs = channel.tmc2209.sgthrs;
            request_tmc_sync(TmcSyncApply);
        }
    }

    if ((result.accepted != 0U) && (apply_live != 0U) && (runtime_config != 0))
    {
        runtime_config->stopSignalActiveHigh = active_motion_channel(*config).stopSignalActiveHigh;
        runtime_config->invertXDir = active_motion_channel(*config).dirInverted;
        runtime_config->homeTowardsPositive = active_motion_channel(*config).homeTowardsPositive;
        tmc_driver_runtime().tmc2209 = active_motion_channel(*config).tmc2209;
    }

    return result;
}

static void emit_driver_line(void)
{
    char line[192];
    int len = snprintf(
        line,
        sizeof(line),
        "driver uart=%lu irun=%lu ihold=%lu iholddelay=%lu tpowerdown=%lu sgthrs=%lu\r\n",
        (unsigned long)tmc_driver_runtime().uartConfigured,
        (unsigned long)tmc_driver_runtime().tmc2209.irun,
        (unsigned long)tmc_driver_runtime().tmc2209.ihold,
        (unsigned long)tmc_driver_runtime().tmc2209.iholddelay,
        (unsigned long)tmc_driver_runtime().tmc2209.tpowerdown,
        (unsigned long)tmc_driver_runtime().tmc2209.sgthrs);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }

    len = snprintf(
        line,
        sizeof(line),
        "driver verify=%lu ifcnt_valid=%lu ifcnt=%lu gconf=0x%08lX ihold_irun=0x%08lX tpowerdown_reg=0x%08lX sgthrs_reg=0x%08lX\r\n",
        (unsigned long)tmc_driver_runtime().uartConfigured,
        (unsigned long)tmc_driver_runtime().ifcntValid,
        (unsigned long)tmc_driver_runtime().ifcnt,
        (unsigned long)tmc_driver_runtime().verifiedGconf,
        (unsigned long)tmc_driver_runtime().verifiedIholdIrun,
        (unsigned long)tmc_driver_runtime().verifiedTpowerdown,
        (unsigned long)tmc_driver_runtime().verifiedSgthrs);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }
}

static void emit_status_line(
    const keyswitch::MotionInputs &inputs,
    const keyswitch::MotionState &state,
    const keyswitch::MotionOutputs &outputs)
{
    char line[448];
    int len = snprintf(
        line,
        sizeof(line),
        "diag0=%lu xstop=%lu diag2=%lu pressed=%lu conf=%lu load=%lu mech=%lu stall=%lu source=%lu force=%lu state=%lu homed=%lu hold=%lu pos=%ld target=%ld press=%ld cycles=%lu done=%lu backoff=%lu seek=%lu fault=%lu ui_click=%lu ui_a=%lu ui_b=%lu loop_last_us=%lu loop_max_us=%lu steps_total=%lu steps_hb=%lu steps_burst=%u tmc_sync=%u\r\n",
        (unsigned long)inputs.rawDiag0,
        (unsigned long)inputs.rawXStop,
        (unsigned long)inputs.rawDiag2,
        (unsigned long)outputs.xStopPressed,
        (unsigned long)outputs.xStopConfirmed,
        (unsigned long)outputs.loadCellTriggered,
        (unsigned long)outputs.mechanicalFallbackTriggered,
        (unsigned long)outputs.stallDetected,
        (unsigned long)outputs.stopSource,
        (unsigned long)inputs.loadCellRaw,
        (unsigned long)state.homingState,
        (unsigned long)state.homed,
        (unsigned long)state.holdEnabled,
        (long)state.currentPosition,
        (long)state.targetPosition,
        (long)state.pressTargetPosition,
        (unsigned long)state.cycleCountRemaining,
        (unsigned long)state.completedCycles,
        (unsigned long)state.backoffStepsRemaining,
        (unsigned long)state.seekSteps,
        (unsigned long)state.faultLatch,
        (unsigned long)g_panel_inputs.clickPressed,
        (unsigned long)g_panel_inputs.encoderAActive,
        (unsigned long)g_panel_inputs.encoderBActive,
        (unsigned long)g_runtime_monitor.lastLoopDeltaUs,
        (unsigned long)g_runtime_monitor.maxLoopDeltaUs,
        (unsigned long)g_runtime_monitor.emittedStepsTotal,
        (unsigned long)g_runtime_monitor.emittedStepsSinceHeartbeat,
        (unsigned int)g_runtime_monitor.maxStepBurstSinceHeartbeat,
        (unsigned int)g_tmc_sync_pending);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }
}

static void emit_heartbeat_line(
    uint32_t now_ms,
    const keyswitch::MotionState &state,
    const keyswitch::MotionOutputs &outputs)
{
    char line[352];
    int len = snprintf(
        line,
        sizeof(line),
        "heartbeat count=%lu ms=%lu state=%lu pressed=%lu load=%lu mech=%lu stall=%lu source=%lu homed=%lu hold=%lu pos=%ld target=%ld fault=%lu ui_click=%lu ui_a=%lu ui_b=%lu loop_last_us=%lu loop_max_us=%lu steps_total=%lu steps_hb=%lu steps_burst=%u tmc_sync=%u\r\n",
        (unsigned long)state.heartbeatCount,
        (unsigned long)now_ms,
        (unsigned long)state.homingState,
        (unsigned long)outputs.xStopPressed,
        (unsigned long)outputs.loadCellTriggered,
        (unsigned long)outputs.mechanicalFallbackTriggered,
        (unsigned long)outputs.stallDetected,
        (unsigned long)state.lastStopSource,
        (unsigned long)state.homed,
        (unsigned long)state.holdEnabled,
        (long)state.currentPosition,
        (long)state.targetPosition,
        (unsigned long)state.faultLatch,
        (unsigned long)g_panel_inputs.clickPressed,
        (unsigned long)g_panel_inputs.encoderAActive,
        (unsigned long)g_panel_inputs.encoderBActive,
        (unsigned long)g_runtime_monitor.lastLoopDeltaUs,
        (unsigned long)g_runtime_monitor.maxLoopDeltaUs,
        (unsigned long)g_runtime_monitor.emittedStepsTotal,
        (unsigned long)g_runtime_monitor.emittedStepsSinceHeartbeat,
        (unsigned int)g_runtime_monitor.maxStepBurstSinceHeartbeat,
        (unsigned int)g_tmc_sync_pending);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }

    runtime_monitor_roll_heartbeat_window();
}

static void emit_baseline_line(uint32_t raw_diag0, uint32_t raw_x_stop, uint32_t raw_diag2)
{
    const MotionChannelConfig &channel = current_motion_channel_const();
    char line[96];
    int len = snprintf(
        line,
        sizeof(line),
        "baseline diag0=%lu xstop=%lu diag2=%lu dir=%lu\r\n",
        (unsigned long)raw_diag0,
        (unsigned long)raw_x_stop,
        (unsigned long)raw_diag2,
        (unsigned long)channel.dirInverted);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }
}

static void emit_simulation_line(uint8_t load_triggered)
{
    char line[128];
    int len = snprintf(
        line,
        sizeof(line),
        "sim source=%s raw=%lu thresh=%lu load=%lu mech=%lu stall=%lu\r\n",
        load_cell_source_name_local(load_cell_source(g_load_cell)),
        (unsigned long)g_load_cell.raw,
        (unsigned long)g_load_cell.threshold,
        (unsigned long)load_triggered,
        (unsigned long)g_load_cell.mechanicalFallbackOverride,
        (unsigned long)g_load_cell.stallOverride);
    if (len > 0)
    {
        usb_cdc_bridge_write(line, (uint16_t)len);
    }
}

static void apply_ui_intent(
    const Mini12864UiIntent &intent,
    const keyswitch::MotionInputs &inputs,
    keyswitch::RuntimeConfig *runtime_config,
    keyswitch::MotionConfig *motion_config,
    keyswitch::MotionState *motion_state)
{
    if ((runtime_config == 0) || (motion_config == 0) || (motion_state == 0))
    {
        return;
    }

    char value_buffer[24];

    switch (intent.type)
    {
    case Mini12864UiIntentType::None:
        break;
    case Mini12864UiIntentType::Home:
        if (tmc_motion_allowed() == 0U) emit_tmc_unverified_blocked("ui home");
        else keyswitch::resetForHome(motion_state, inputs.nowMs);
        break;
    case Mini12864UiIntentType::Stop:
        keyswitch::forceStop(motion_state);
        break;
    case Mini12864UiIntentType::Tare:
        load_cell_tare(&g_load_cell);
        emit_simulation_line(load_cell_triggered(g_load_cell));
        break;
    case Mini12864UiIntentType::Backoff:
        if (tmc_motion_allowed() == 0U) emit_tmc_unverified_blocked("ui backoff");
        else keyswitch::startBackoff(motion_state, inputs.nowMs, *motion_config);
        break;
    case Mini12864UiIntentType::SetHold:
        if ((intent.value != 0) && (tmc_motion_allowed() == 0U)) emit_tmc_unverified_blocked("ui hold");
        else keyswitch::setHoldEnabled(motion_state, (uint8_t)(intent.value != 0 ? 1U : 0U));
        break;
    case Mini12864UiIntentType::MoveRelativeUm:
        if (tmc_motion_allowed() == 0U) emit_tmc_unverified_blocked("ui jog");
        else
        {
            int32_t delta_steps = axis_steps_from_signed_um(g_persisted_config, intent.value);
            keyswitch::queueRelativeMove(motion_state, delta_steps, *motion_config);
        }
        break;
    case Mini12864UiIntentType::SetDefaultPressUm:
        snprintf(value_buffer, sizeof(value_buffer), "%ld", (long)intent.value);
        apply_config_key_value(&g_persisted_config, "AXIS.DEFAULT_PRESS_UM", value_buffer, 1U, runtime_config, motion_config, motion_state);
        g_config_state.dirty = 1U;
        break;
    case Mini12864UiIntentType::StartCycle:
        if (tmc_motion_allowed() == 0U) emit_tmc_unverified_blocked("ui cycle");
        else keyswitch::startCycleRoutine(motion_state, (uint32_t)((intent.value <= 0) ? 1 : intent.value), *motion_config);
        break;
    case Mini12864UiIntentType::SetLoadThreshold:
        snprintf(value_buffer, sizeof(value_buffer), "%ld", (long)intent.value);
        apply_config_key_value(&g_persisted_config, "SIM.LOAD_THRESHOLD", value_buffer, 1U, runtime_config, motion_config, motion_state);
        g_config_state.dirty = 1U;
        break;
    case Mini12864UiIntentType::SetIrun:
        snprintf(value_buffer, sizeof(value_buffer), "%ld", (long)intent.value);
        apply_config_key_value(&g_persisted_config, "TMC.IRUN", value_buffer, 1U, runtime_config, motion_config, motion_state);
        g_config_state.dirty = 1U;
        break;
    case Mini12864UiIntentType::SetIhold:
        snprintf(value_buffer, sizeof(value_buffer), "%ld", (long)intent.value);
        apply_config_key_value(&g_persisted_config, "TMC.IHOLD", value_buffer, 1U, runtime_config, motion_config, motion_state);
        g_config_state.dirty = 1U;
        break;
    case Mini12864UiIntentType::SetIholdDelay:
        snprintf(value_buffer, sizeof(value_buffer), "%ld", (long)intent.value);
        apply_config_key_value(&g_persisted_config, "TMC.IHOLDDELAY", value_buffer, 1U, runtime_config, motion_config, motion_state);
        g_config_state.dirty = 1U;
        break;
    case Mini12864UiIntentType::SetSgthrs:
        snprintf(value_buffer, sizeof(value_buffer), "%ld", (long)intent.value);
        apply_config_key_value(&g_persisted_config, "TMC.SGTHRS", value_buffer, 1U, runtime_config, motion_config, motion_state);
        g_config_state.dirty = 1U;
        break;
    case Mini12864UiIntentType::SetAllowUnverifiedMotion:
        snprintf(value_buffer, sizeof(value_buffer), "%ld", (long)(intent.value != 0 ? 1 : 0));
        apply_config_key_value(&g_persisted_config, "TMC.ALLOW_UNVERIFIED_MOTION", value_buffer, 1U, runtime_config, motion_config, motion_state);
        if (tmc_motion_allowed() == 0U)
        {
            keyswitch::setHoldEnabled(motion_state, 0U);
        }
        g_config_state.dirty = 1U;
        break;
    case Mini12864UiIntentType::SetMoveFeedrate:
        snprintf(value_buffer, sizeof(value_buffer), "%ld", (long)intent.value);
        apply_config_key_value(&g_persisted_config, "MOTION.MOVE_FEEDRATE_MM_PER_MIN", value_buffer, 1U, runtime_config, motion_config, motion_state);
        g_config_state.dirty = 1U;
        break;
    case Mini12864UiIntentType::SetHomeFeedrate:
        snprintf(value_buffer, sizeof(value_buffer), "%ld", (long)intent.value);
        apply_config_key_value(&g_persisted_config, "MOTION.HOME_FEEDRATE_MM_PER_MIN", value_buffer, 1U, runtime_config, motion_config, motion_state);
        g_config_state.dirty = 1U;
        break;
    case Mini12864UiIntentType::SetBackoffSteps:
        snprintf(value_buffer, sizeof(value_buffer), "%ld", (long)intent.value);
        apply_config_key_value(&g_persisted_config, "MOTION.BACKOFF_STEPS", value_buffer, 1U, runtime_config, motion_config, motion_state);
        g_config_state.dirty = 1U;
        break;
    case Mini12864UiIntentType::SetDebounceCount:
        snprintf(value_buffer, sizeof(value_buffer), "%ld", (long)intent.value);
        apply_config_key_value(&g_persisted_config, "MOTION.STOP_DEBOUNCE_COUNT", value_buffer, 1U, runtime_config, motion_config, motion_state);
        g_config_state.dirty = 1U;
        break;
    case Mini12864UiIntentType::SetPanelColorRed:
        snprintf(value_buffer, sizeof(value_buffer), "%ld", (long)intent.value);
        apply_config_key_value(&g_persisted_config, "PANEL.COLOR_RED", value_buffer, 1U, runtime_config, motion_config, motion_state);
        g_config_state.dirty = 1U;
        break;
    case Mini12864UiIntentType::SetPanelColorGreen:
        snprintf(value_buffer, sizeof(value_buffer), "%ld", (long)intent.value);
        apply_config_key_value(&g_persisted_config, "PANEL.COLOR_GREEN", value_buffer, 1U, runtime_config, motion_config, motion_state);
        g_config_state.dirty = 1U;
        break;
    case Mini12864UiIntentType::SetPanelColorBlue:
        snprintf(value_buffer, sizeof(value_buffer), "%ld", (long)intent.value);
        apply_config_key_value(&g_persisted_config, "PANEL.COLOR_BLUE", value_buffer, 1U, runtime_config, motion_config, motion_state);
        g_config_state.dirty = 1U;
        break;
    case Mini12864UiIntentType::SaveConfig:
        if (save_persisted_config(g_persisted_config) != 0U)
        {
            g_config_state.dirty = 0U;
            g_config_state.loadedFromFlash = 1U;
        }
        break;
    case Mini12864UiIntentType::ResetConfig:
        g_persisted_config = make_default_persisted_config();
        *motion_config = make_motion_config(g_persisted_config);
        runtime_config->stopSignalActiveHigh = active_motion_channel(g_persisted_config).stopSignalActiveHigh;
        runtime_config->invertXDir = active_motion_channel(g_persisted_config).dirInverted;
        runtime_config->homeTowardsPositive = active_motion_channel(g_persisted_config).homeTowardsPositive;
        apply_default_press_target(motion_state, g_persisted_config, *motion_config);
        g_config_state.dirty = 1U;
        g_config_state.requiresReboot = 1U;
        break;
    case Mini12864UiIntentType::Reboot:
        bootloader_request_application_boot();
        NVIC_SystemReset();
        break;
    }
}

static uint8_t read_load_cell_triggered(void)
{
    return load_cell_triggered(g_load_cell);
}

static uint32_t read_load_cell_raw(void)
{
    return load_cell_raw(g_load_cell);
}

static uint8_t load_cell_hx711_pins_valid(const LoadCellConfig &config)
{
    return ((pin_assignment_valid(config.pins.data) != 0U) && (pin_assignment_valid(config.pins.clock) != 0U)) ? 1U : 0U;
}

static void configure_load_cell_io(const LoadCellConfig &config)
{
    load_cell_hx711_ready_interrupt_disable();

    if ((uint8_t)config.source != (uint8_t)LoadCellSourceKind::Hx711)
    {
        return;
    }

    if (load_cell_hx711_pins_valid(config) == 0U)
    {
        return;
    }

    enable_gpio_clock(config.pins.data.portId);
    enable_gpio_clock(config.pins.clock.portId);
    __DSB();

    pin_set_input_pull(config.pins.data, 1U);
    pin_set_output(config.pins.clock);
    pin_write_low(config.pins.clock);
    load_cell_hx711_ready_interrupt_configure(config.pins.data);
}

static void refresh_load_cell_input(void)
{
    const LoadCellConfig &config = g_persisted_config.loadCell;
    if ((uint8_t)config.source != (uint8_t)LoadCellSourceKind::Hx711)
    {
        return;
    }

    if (load_cell_hx711_pins_valid(config) == 0U)
    {
        return;
    }

    if (g_load_cell_hx711_ready == 0U)
    {
        return;
    }

    if (pin_read(config.pins.data) != 0U)
    {
        g_load_cell_hx711_ready = 0U;
        return;
    }

    g_load_cell_hx711_ready = 0U;

    uint32_t sample = 0U;
    const uint32_t pulse_cycles = cycles_from_us(1U);
    for (uint32_t index = 0U; index < 24U; ++index)
    {
        pin_write_high(config.pins.clock);
        delay_cycles(pulse_cycles);
        sample = (sample << 1U) | (pin_read(config.pins.data) & 0x1U);
        pin_write_low(config.pins.clock);
        delay_cycles(pulse_cycles);
    }

    pin_write_high(config.pins.clock);
    delay_cycles(pulse_cycles);
    pin_write_low(config.pins.clock);
    delay_cycles(pulse_cycles);

    load_cell_set_hx711_sample(&g_load_cell, load_cell_hx711_decode_u24(sample));
}

static uint8_t read_stallguard_triggered(void)
{
    return g_load_cell.stallOverride;
}

static uint8_t read_mechanical_fallback_triggered(uint8_t raw_x_stop)
{
    return (g_load_cell.mechanicalFallbackOverride != 0U) ? 1U : (uint8_t)x_stop_pressed_from_raw(raw_x_stop);
}

static ApplyConfigResult apply_boot_config_key_value(PersistedFirmwareConfig *config, const char *key, const char *value)
{
    return apply_config_key_value(config, key, value, 0U, 0, 0, 0);
}

int main()
{
    SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;

    // Initialize HAL (critical for clock configuration)
    HAL_Init();

    if (!system_clock_config_hse())
    {
        system_clock_config_hsi_safe();
    }

    step_scheduler_timer_init();

    enable_gpio_clock((uint8_t)GpioPortId::A);
    __DSB();

    usb_cdc_bridge_init();
    usb_cdc_bridge_wait_until_ready(3000U);

    g_config_state = select_boot_config(&g_persisted_config, apply_boot_config_key_value);

    load_cell_apply_config(&g_load_cell, g_persisted_config.loadCell);
    tmc_driver_runtime().tmc2209 = current_motion_channel_const().tmc2209;

    enable_gpio_clocks_for_config(g_persisted_config);

    pin_set_output(g_persisted_config.pins.psOn);
    pin_set_output(g_persisted_config.pins.safePower);
    if (led_pin_conflicts_with_load_cell(g_persisted_config) == 0U)
    {
        pin_set_output(g_persisted_config.pins.led);
    }
    pin_set_output(current_motion_channel_const().pins.uart);
    pin_set_output(current_motion_channel_const().pins.dir);
    pin_set_output(current_motion_channel_const().pins.step);
    pin_set_output(current_motion_channel_const().pins.enable);

    pin_set_input_pull(g_persisted_config.pins.diag0, 1U);
    pin_set_input_pull(g_persisted_config.pins.diag2, 1U);
    pin_set_input_pull(current_motion_channel_const().pins.stop, (uint8_t)(current_motion_channel_const().stopSignalActiveHigh ? 0U : 1U));
    configure_load_cell_io(g_persisted_config.loadCell);
    mini12864_panel_init_inputs(g_panel_pins);
    mini12864_display_init(g_panel_pins);

    // Enable board power domains required for stepper drivers.
    pin_write_high(g_persisted_config.pins.psOn);
    pin_write_high(g_persisted_config.pins.safePower);

    // Leave X driver disabled until we actually emit step pulses.
    x_driver_disable(g_persisted_config);
    pin_write_high(current_motion_channel_const().pins.uart);
    step_scheduler_init();
    if (x_tmc2209_apply_config(g_persisted_config) == 0U)
    {
        usb_write_str("boot: tmc verify failed\r\n");
    }
    emit_boot_source_line(g_config_state.source);

    // Single confirmation pulse after USB init returns.
    if (led_pin_conflicts_with_load_cell(g_persisted_config) == 0U)
    {
        pin_write_high(g_persisted_config.pins.led);
        delay_cycles(2000000U);
        pin_write_low(g_persisted_config.pins.led);
        delay_cycles(2000000U);
    }
    usb_write_str("SKR2 X endstop homing test start\r\n");

    const uint32_t x_stop_baseline_diag0 = sample_pin_baseline(g_persisted_config.pins.diag0);
    const uint32_t x_stop_baseline_xstop = sample_pin_baseline(current_motion_channel_const().pins.stop);
    const uint32_t x_stop_baseline_diag2 = sample_pin_baseline(g_persisted_config.pins.diag2);
    emit_baseline_line(x_stop_baseline_diag0, x_stop_baseline_xstop, x_stop_baseline_diag2);
    if (led_pin_conflicts_with_load_cell(g_persisted_config) == 0U)
    {
        pin_write_low(g_persisted_config.pins.led);
    }

    usb_write_str("homing: seeking towards switch\r\n");

    keyswitch::MotionConfig motion_config = make_motion_config(g_persisted_config);
    keyswitch::RuntimeConfig runtime_config = {
        current_motion_channel_const().stopSignalActiveHigh,
        current_motion_channel_const().dirInverted,
        current_motion_channel_const().homeTowardsPositive,
    };
    keyswitch::MotionState motion_state = keyswitch::makeInitialState(HAL_GetTick());
    apply_default_press_target(&motion_state, g_persisted_config, motion_config);
    if (tmc_motion_allowed() == 0U)
    {
        keyswitch::setHoldEnabled(&motion_state, 0U);
        usb_write_str("boot: motion locked until tmc verify succeeds\r\n");
    }
    else if (x_tmc_verified() == 0U)
    {
        usb_write_str("boot: motion override enabled without tmc verify\r\n");
    }
    char command_buf[64];

    while (1)
    {
        const uint32_t now_ms = HAL_GetTick();
        const uint32_t now_cycles = 0U;
        const uint32_t now_us = step_scheduler_timer_now_us();
        runtime_monitor_record_loop(now_us);
        step_scheduler_service(now_cycles);
        refresh_load_cell_input();
        g_panel_inputs = mini12864_panel_read_inputs(g_panel_pins);

        const uint8_t raw_x_stop = (uint8_t)pin_read(current_motion_channel_const().pins.stop);
        const uint8_t load_triggered = read_load_cell_triggered();
        const uint8_t step_issued = step_scheduler_take_issued_step();
    runtime_monitor_record_steps(step_issued);

        const keyswitch::MotionInputs inputs = {
            (uint8_t)pin_read(g_persisted_config.pins.diag0),
            raw_x_stop,
            (uint8_t)pin_read(g_persisted_config.pins.diag2),
            load_triggered,
            read_mechanical_fallback_triggered(raw_x_stop),
            read_stallguard_triggered(),
            step_issued,
            read_load_cell_raw(),
            now_ms,
        };

        keyswitch::MotionOutputs outputs = keyswitch::tickMotion(
            &motion_state,
            inputs,
            motion_config,
            runtime_config);

        g_step_scheduler.stepIntervalUs = active_step_interval_us_for_state(g_persisted_config, motion_state);

        if (tmc_motion_allowed() == 0U)
        {
            outputs.driverEnable = 0U;
            outputs.issueStep = 0U;
            outputs.holdEnabled = 0U;
        }

        // Motion timing gets first claim on each loop iteration; UI, USB, and display
        // work run afterward so they cannot delay due step issuance.
        apply_motion_outputs(outputs, now_cycles);

        usb_cdc_bridge_poll();

        const Mini12864UiIntent ui_intent = mini12864_ui_update(g_panel_inputs, g_persisted_config, g_config_state, motion_state);
        apply_ui_intent(ui_intent, inputs, &runtime_config, &motion_config, &motion_state);

        if (CDC_ReadCommand_FS(command_buf, sizeof(command_buf)) > 0U)
        {
            const keyswitch::Command command = keyswitch::parseCommand(command_buf);

            if ((command.type == keyswitch::CommandType::Status) || (command.type == keyswitch::CommandType::Safety))
            {
                usb_write_str((command.type == keyswitch::CommandType::Safety) ? "cmd: safety\r\n" : "cmd: status\r\n");
                keyswitch::MotionOutputs snapshot = {};
                snapshot.xStopPressed = keyswitch::xStopPressedFromRaw(inputs.rawXStop, runtime_config.stopSignalActiveHigh);
                snapshot.xStopConfirmed = (motion_state.stopDebounce >= motion_config.debounceCount) ? 1U : 0U;
                snapshot.loadCellTriggered = inputs.loadCellTriggered;
                snapshot.mechanicalFallbackTriggered = inputs.mechanicalFallbackTriggered;
                snapshot.stallDetected = inputs.stallDetected;
                snapshot.homed = motion_state.homed;
                snapshot.holdEnabled = motion_state.holdEnabled;
                snapshot.stopSource = motion_state.lastStopSource;
                emit_status_line(
                    inputs,
                    motion_state,
                    snapshot);
                if (command.type == keyswitch::CommandType::Safety)
                {
                    emit_simulation_line(snapshot.loadCellTriggered);
                }
            }
            else if (command.type == keyswitch::CommandType::Config)
            {
                usb_write_str("cmd: config\r\n");
                if (same_text_case_sensitive(command.key, "SOURCES"))
                {
                    emit_config_sources_line(g_config_state);
                }
                else if (same_text_case_sensitive(command.key, "CHANNELS"))
                {
                    for (uint8_t index = 0U; index < g_persisted_config.motionChannelCount; ++index)
                    {
                        emit_channel_inventory_line(g_persisted_config, index);
                    }
                }
                else
                {
                    uint8_t scoped_channel_index = 0U;
                    char scoped_suffix[32] = {0};
                    if ((parse_scoped_channel_key(command.key, &scoped_channel_index, scoped_suffix, sizeof(scoped_suffix)) != 0U) && (scoped_channel_index < g_persisted_config.motionChannelCount))
                    {
                        emit_indexed_channel_summary(g_persisted_config, scoped_channel_index);
                    }
                    else
                    {
                        emit_config_summary(g_persisted_config, g_config_state);
                        emit_config_sources_line(g_config_state);
                    }
                }
            }
            else if (command.type == keyswitch::CommandType::SetConfig)
            {
                char line[160];
                const ApplyConfigResult result = apply_config_key_value(&g_persisted_config, command.key, command.text, 1U, &runtime_config, &motion_config, &motion_state);

                if (result.accepted != 0U)
                {
                    if (tmc_motion_allowed() == 0U)
                    {
                        keyswitch::setHoldEnabled(&motion_state, 0U);
                    }
                    g_config_state.dirty = 1U;
                    if (result.rebootRequired != 0U)
                    {
                        g_config_state.requiresReboot = 1U;
                    }
                    int len = snprintf(line, sizeof(line), "cmd: set key=%s value=%s ok=1 reboot=%lu\r\n", command.key, command.text, (unsigned long)result.rebootRequired);
                    if (len > 0)
                    {
                        usb_cdc_bridge_write(line, (uint16_t)len);
                    }
                }
                else
                {
                    int len = snprintf(line, sizeof(line), "cmd: set key=%s value=%s ok=0\r\n", command.key, command.text);
                    if (len > 0)
                    {
                        usb_cdc_bridge_write(line, (uint16_t)len);
                    }
                }
            }
            else if (command.type == keyswitch::CommandType::SaveConfig)
            {
                const uint8_t saved = save_persisted_config(g_persisted_config);
                char line[64];
                int len = snprintf(line, sizeof(line), "cmd: save ok=%lu reboot=%lu\r\n", (unsigned long)saved, (unsigned long)g_config_state.requiresReboot);
                if (len > 0)
                {
                    usb_cdc_bridge_write(line, (uint16_t)len);
                }
                if (saved != 0U)
                {
                    g_config_state.dirty = 0U;
                    g_config_state.loadedFromFlash = 1U;
                }
            }
            else if (command.type == keyswitch::CommandType::ResetConfig)
            {
                g_persisted_config = make_default_persisted_config();
                g_config_state.dirty = 1U;
                g_config_state.requiresReboot = 1U;
                usb_write_str("cmd: resetcfg ok=1 reboot=1\r\n");
                emit_config_summary(g_persisted_config, g_config_state);
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
                bootloader_request_stay_in_bootloader();
                NVIC_SystemReset();
            }
            else if (command.type == keyswitch::CommandType::Boot)
            {
                usb_write_str("cmd: boot ok=0 stage=runtime\r\n");
            }
            else if (command.type == keyswitch::CommandType::Driver)
            {
                request_tmc_sync(TmcSyncVerify);
                service_tmc_sync(&motion_state);
                usb_write_str("cmd: driver\r\n");
                emit_driver_line();
            }
            else if (command.type == keyswitch::CommandType::Enable)
            {
                if (tmc_motion_allowed() == 0U)
                {
                    keyswitch::setHoldEnabled(&motion_state, 0U);
                    emit_tmc_unverified_blocked("enable");
                }
                else
                {
                    keyswitch::setHoldEnabled(&motion_state, 1U);
                    usb_write_str("cmd: enable\r\n");
                }
            }
            else if (command.type == keyswitch::CommandType::Disable)
            {
                keyswitch::setHoldEnabled(&motion_state, 0U);
                usb_write_str("cmd: disable\r\n");
            }
            else if (command.type == keyswitch::CommandType::Hold)
            {
                char line[48];
                if ((command.value != 0) && (tmc_motion_allowed() == 0U))
                {
                    keyswitch::setHoldEnabled(&motion_state, 0U);
                    emit_tmc_unverified_blocked("hold");
                }
                else
                {
                    keyswitch::setHoldEnabled(&motion_state, (uint8_t)(command.value != 0 ? 1U : 0U));
                    int len = snprintf(line, sizeof(line), "cmd: hold value=%ld\r\n", (long)command.value);
                    if (len > 0)
                    {
                        usb_cdc_bridge_write(line, (uint16_t)len);
                    }
                }
                emit_driver_line();
            }
            else if (command.type == keyswitch::CommandType::RunCurrent)
            {
                char line[64];
                tmc_driver_runtime().tmc2209.irun = keyswitch::clampTmcCurrentScale(command.value);
                active_motion_channel(g_persisted_config).tmc2209.irun = tmc_driver_runtime().tmc2209.irun;
                g_config_state.dirty = 1U;
                request_tmc_sync(TmcSyncApply);
                service_tmc_sync(&motion_state);
                int len = snprintf(line, sizeof(line), "cmd: irun value=%lu\r\n", (unsigned long)tmc_driver_runtime().tmc2209.irun);
                if (len > 0)
                {
                    usb_cdc_bridge_write(line, (uint16_t)len);
                }
                emit_driver_line();
            }
            else if (command.type == keyswitch::CommandType::HoldCurrent)
            {
                char line[64];
                tmc_driver_runtime().tmc2209.ihold = keyswitch::clampTmcCurrentScale(command.value);
                active_motion_channel(g_persisted_config).tmc2209.ihold = tmc_driver_runtime().tmc2209.ihold;
                g_config_state.dirty = 1U;
                request_tmc_sync(TmcSyncApply);
                service_tmc_sync(&motion_state);
                int len = snprintf(line, sizeof(line), "cmd: ihold value=%lu\r\n", (unsigned long)tmc_driver_runtime().tmc2209.ihold);
                if (len > 0)
                {
                    usb_cdc_bridge_write(line, (uint16_t)len);
                }
                emit_driver_line();
            }
            else if (command.type == keyswitch::CommandType::HoldDelay)
            {
                char line[72];
                tmc_driver_runtime().tmc2209.iholddelay = (uint8_t)((command.value < 0) ? 0 : ((command.value > 15) ? 15 : command.value));
                active_motion_channel(g_persisted_config).tmc2209.iholddelay = tmc_driver_runtime().tmc2209.iholddelay;
                g_config_state.dirty = 1U;
                request_tmc_sync(TmcSyncApply);
                service_tmc_sync(&motion_state);
                int len = snprintf(line, sizeof(line), "cmd: iholddelay value=%lu\r\n", (unsigned long)tmc_driver_runtime().tmc2209.iholddelay);
                if (len > 0)
                {
                    usb_cdc_bridge_write(line, (uint16_t)len);
                }
                emit_driver_line();
            }
            else if (command.type == keyswitch::CommandType::StallThreshold)
            {
                char line[64];
                tmc_driver_runtime().tmc2209.sgthrs = (uint8_t)((command.value < 0) ? 0 : ((command.value > 255) ? 255 : command.value));
                active_motion_channel(g_persisted_config).tmc2209.sgthrs = tmc_driver_runtime().tmc2209.sgthrs;
                g_config_state.dirty = 1U;
                request_tmc_sync(TmcSyncApply);
                service_tmc_sync(&motion_state);
                int len = snprintf(line, sizeof(line), "cmd: sgthrs value=%lu\r\n", (unsigned long)tmc_driver_runtime().tmc2209.sgthrs);
                if (len > 0)
                {
                    usb_cdc_bridge_write(line, (uint16_t)len);
                }
                emit_driver_line();
            }
            else if (command.type == keyswitch::CommandType::MoveAbsolute)
            {
                char line[64];
                if (tmc_motion_allowed() == 0U)
                {
                    emit_tmc_unverified_blocked("moveabs");
                }
                else
                {
                    int32_t target_steps = 0;
                    const uint8_t parsed = command_absolute_target_steps(command, &target_steps);
                    const uint8_t accepted = (parsed != 0U) ? keyswitch::queueAbsoluteMove(&motion_state, target_steps, motion_config) : 0U;
                    int len = snprintf(line, sizeof(line), "cmd: moveabs target=%ld ok=%lu\r\n", (long)target_steps, (unsigned long)accepted);
                    if (len > 0)
                    {
                        usb_cdc_bridge_write(line, (uint16_t)len);
                    }
                }
            }
            else if (command.type == keyswitch::CommandType::MoveRelative)
            {
                char line[64];
                if (tmc_motion_allowed() == 0U)
                {
                    emit_tmc_unverified_blocked("moverel");
                }
                else
                {
                    const uint8_t accepted = keyswitch::queueRelativeMove(&motion_state, (int32_t)command.value, motion_config);
                    int len = snprintf(line, sizeof(line), "cmd: moverel delta=%ld ok=%lu\r\n", (long)command.value, (unsigned long)accepted);
                    if (len > 0)
                    {
                        usb_cdc_bridge_write(line, (uint16_t)len);
                    }
                }
            }
            else if (command.type == keyswitch::CommandType::SetPosition)
            {
                char line[64];
                keyswitch::setCurrentPosition(&motion_state, (int32_t)command.value);
                int len = snprintf(line, sizeof(line), "cmd: setpos pos=%ld\r\n", (long)command.value);
                if (len > 0)
                {
                    usb_cdc_bridge_write(line, (uint16_t)len);
                }
            }
            else if (command.type == keyswitch::CommandType::Cycle)
            {
                char line[64];
                if (tmc_motion_allowed() == 0U)
                {
                    emit_tmc_unverified_blocked("cycle");
                }
                else
                {
                    const uint8_t accepted = (command.value > 0) ? keyswitch::startCycleRoutine(&motion_state, (uint32_t)command.value, motion_config) : 0U;
                    int len = snprintf(line, sizeof(line), "cmd: cycle count=%ld ok=%lu\r\n", (long)command.value, (unsigned long)accepted);
                    if (len > 0)
                    {
                        usb_cdc_bridge_write(line, (uint16_t)len);
                    }
                }
            }
            else if (command.type == keyswitch::CommandType::PressTarget)
            {
                char line[64];
                const uint8_t accepted = keyswitch::setPressTarget(&motion_state, (int32_t)command.value, motion_config);
                int len = snprintf(line, sizeof(line), "cmd: presspos pos=%ld ok=%lu\r\n", (long)command.value, (unsigned long)accepted);
                if (len > 0)
                {
                    usb_cdc_bridge_write(line, (uint16_t)len);
                }
            }
            else if (command.type == keyswitch::CommandType::SimLoad)
            {
                char line[64];
                load_cell_set_raw(&g_load_cell, (uint32_t)command.value);
                int len = snprintf(line, sizeof(line), "cmd: simload raw=%lu\r\n", (unsigned long)g_load_cell.raw);
                if (len > 0)
                {
                    usb_cdc_bridge_write(line, (uint16_t)len);
                }
                emit_simulation_line(read_load_cell_triggered());
            }
            else if (command.type == keyswitch::CommandType::SimThreshold)
            {
                char line[72];
                load_cell_set_threshold(&g_load_cell, (uint32_t)command.value);
                g_persisted_config.loadCell.threshold = g_load_cell.threshold;
                g_config_state.dirty = 1U;
                int len = snprintf(line, sizeof(line), "cmd: simthresh thresh=%lu\r\n", (unsigned long)g_load_cell.threshold);
                if (len > 0)
                {
                    usb_cdc_bridge_write(line, (uint16_t)len);
                }
                emit_simulation_line(read_load_cell_triggered());
            }
            else if (command.type == keyswitch::CommandType::SimMechanical)
            {
                char line[64];
                load_cell_set_mechanical_fallback(&g_load_cell, (uint8_t)(command.value != 0U ? 1U : 0U));
                int len = snprintf(line, sizeof(line), "cmd: simmech value=%lu\r\n", (unsigned long)g_load_cell.mechanicalFallbackOverride);
                if (len > 0)
                {
                    usb_cdc_bridge_write(line, (uint16_t)len);
                }
                emit_simulation_line(read_load_cell_triggered());
            }
            else if (command.type == keyswitch::CommandType::SimStall)
            {
                char line[64];
                load_cell_set_stall_override(&g_load_cell, (uint8_t)(command.value != 0U ? 1U : 0U));
                int len = snprintf(line, sizeof(line), "cmd: simstall value=%lu\r\n", (unsigned long)g_load_cell.stallOverride);
                if (len > 0)
                {
                    usb_cdc_bridge_write(line, (uint16_t)len);
                }
                emit_simulation_line(read_load_cell_triggered());
            }
            else if (command.type == keyswitch::CommandType::SimClear)
            {
                load_cell_clear(&g_load_cell);
                usb_write_str("cmd: simclear\r\n");
                emit_simulation_line(read_load_cell_triggered());
            }
            else if (command.type == keyswitch::CommandType::Tare)
            {
                load_cell_tare(&g_load_cell);
                usb_write_str("cmd: tare\r\n");
                emit_simulation_line(read_load_cell_triggered());
            }
            else if (command.type == keyswitch::CommandType::PanelPins)
            {
                usb_write_str("cmd: panelpins\r\n");
                emit_panel_pins_line();
            }
            else if (command.type == keyswitch::CommandType::Home)
            {
                if (tmc_motion_allowed() == 0U)
                {
                    emit_tmc_unverified_blocked("home");
                }
                else
                {
                    keyswitch::resetForHome(&motion_state, inputs.nowMs);
                    usb_write_str("cmd: home\r\n");
                }
            }
            else if (command.type == keyswitch::CommandType::Stop)
            {
                keyswitch::forceStop(&motion_state);
                usb_write_str("cmd: stop\r\n");
            }
            else if (command.type == keyswitch::CommandType::Backoff)
            {
                if (tmc_motion_allowed() == 0U)
                {
                    emit_tmc_unverified_blocked("backoff");
                }
                else
                {
                    keyswitch::startBackoff(&motion_state, inputs.nowMs, motion_config);
                    usb_write_str("cmd: backoff\r\n");
                }
            }
            else if (command.type == keyswitch::CommandType::Help)
            {
                usb_write_str(keyswitch::commandHelpText());
            }
            else
            {
                usb_write_str("cmd: unknown\r\n");
            }
        }

        mini12864_display_render(g_panel_pins, now_ms, inputs, motion_state, outputs, g_panel_inputs, g_persisted_config, g_config_state);

        if (outputs.eventMessage != 0)
        {
            usb_write_str(outputs.eventMessage);
        }

        service_tmc_sync(&motion_state);

        if (outputs.emitStatus != 0U)
        {
            emit_status_line(inputs, motion_state, outputs);
        }

        if (outputs.emitHeartbeat != 0U)
        {
            emit_heartbeat_line(inputs.nowMs, motion_state, outputs);
        }
    }
}
