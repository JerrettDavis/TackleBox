#include "load_cell.h"

#include <limits.h>
#include <string.h>

static const uint8_t kHx711TriggerDebounceCount = 3U;
static const uint8_t kHx711WarmupSampleCount = 16U;
static const uint8_t kHx711AutoTareSampleCount = 16U;

static uint32_t hx711_idle_force_floor(uint32_t threshold)
{
    return (threshold > 0U) ? ((threshold / 16U) + 1U) : 16U;
}

static void load_cell_update_hx711_trigger_state(LoadCellRuntime *runtime)
{
    if (runtime == 0)
    {
        return;
    }

    if (runtime->raw >= runtime->threshold)
    {
        if (runtime->overThresholdCount < 255U)
        {
            ++runtime->overThresholdCount;
        }
        runtime->underThresholdCount = 0U;
        if (runtime->overThresholdCount >= kHx711TriggerDebounceCount)
        {
            runtime->triggerState = 1U;
        }
        return;
    }

    if (runtime->underThresholdCount < 255U)
    {
        ++runtime->underThresholdCount;
    }
    runtime->overThresholdCount = 0U;
    if (runtime->underThresholdCount >= kHx711TriggerDebounceCount)
    {
        runtime->triggerState = 0U;
    }
}

static uint32_t blend_u32(uint32_t current, uint32_t next)
{
    return (current == 0U) ? next : (((current * 3U) + next) / 4U);
}

LoadCellRuntime load_cell_make_default(uint32_t threshold)
{
    LoadCellRuntime runtime = {};
    runtime.source = (uint8_t)LoadCellSourceKind::Simulation;
    runtime.threshold = threshold;
    return runtime;
}

void load_cell_apply_config(LoadCellRuntime *runtime, const LoadCellConfig &config)
{
    if (runtime == 0)
    {
        return;
    }

    if (runtime->source != config.source)
    {
        runtime->raw = 0U;
        runtime->triggerState = 0U;
        runtime->overThresholdCount = 0U;
        runtime->underThresholdCount = 0U;
        runtime->hx711WarmupSamples = 0U;
        runtime->hx711TareSamples = 0U;
        runtime->hx711TareReady = 0U;
        runtime->hx711Baseline = 0;
        runtime->hx711Accumulator = 0;
    }

    runtime->source = config.source;
    runtime->threshold = config.threshold;
}

void load_cell_set_raw(LoadCellRuntime *runtime, uint32_t raw)
{
    if (runtime == 0)
    {
        return;
    }

    runtime->raw = raw;
    if (runtime->source == (uint8_t)LoadCellSourceKind::Hx711)
    {
        load_cell_update_hx711_trigger_state(runtime);
    }
    else
    {
        runtime->triggerState = (runtime->raw >= runtime->threshold) ? 1U : 0U;
    }
}

void load_cell_set_hx711_sample(LoadCellRuntime *runtime, int32_t sample)
{
    if (runtime == 0)
    {
        return;
    }

    if (runtime->source != (uint8_t)LoadCellSourceKind::Hx711)
    {
        load_cell_set_raw(runtime, load_cell_force_from_signed(sample));
        return;
    }

    if (runtime->hx711TareReady == 0U)
    {
        if (runtime->hx711WarmupSamples < kHx711WarmupSampleCount)
        {
            ++runtime->hx711WarmupSamples;
            runtime->raw = 0U;
            runtime->triggerState = 0U;
            runtime->overThresholdCount = 0U;
            runtime->underThresholdCount = 0U;
            return;
        }

        runtime->hx711Accumulator += sample;
        ++runtime->hx711TareSamples;
        runtime->raw = 0U;
        runtime->triggerState = 0U;
        runtime->overThresholdCount = 0U;
        runtime->underThresholdCount = 0U;
        if (runtime->hx711TareSamples >= kHx711AutoTareSampleCount)
        {
            runtime->hx711Baseline = runtime->hx711Accumulator / (int32_t)runtime->hx711TareSamples;
            runtime->hx711Accumulator = 0;
            runtime->hx711TareReady = 1U;
        }
        return;
    }

    const int32_t relative_sample = sample - runtime->hx711Baseline;
    const uint32_t relative_force = load_cell_force_from_signed(relative_sample);
    runtime->raw = blend_u32(runtime->raw, relative_force);

    const uint32_t idle_force_floor = hx711_idle_force_floor(runtime->threshold);
    if ((relative_force < idle_force_floor) && (runtime->raw < idle_force_floor))
    {
        runtime->raw = 0U;
    }

    if (relative_force < ((runtime->threshold > 0U) ? ((runtime->threshold / 8U) + 1U) : 32U))
    {
        runtime->hx711Baseline = runtime->hx711Baseline + (relative_sample / 8);
    }

    load_cell_update_hx711_trigger_state(runtime);
}

void load_cell_set_threshold(LoadCellRuntime *runtime, uint32_t threshold)
{
    if (runtime == 0)
    {
        return;
    }

    runtime->threshold = threshold;
    runtime->triggerState = 0U;
    runtime->overThresholdCount = 0U;
    runtime->underThresholdCount = 0U;
}

void load_cell_tare(LoadCellRuntime *runtime)
{
    if (runtime == 0)
    {
        return;
    }

    runtime->raw = 0U;
    runtime->triggerState = 0U;
    runtime->overThresholdCount = 0U;
    runtime->underThresholdCount = 0U;

    if (runtime->source == (uint8_t)LoadCellSourceKind::Hx711)
    {
        runtime->hx711WarmupSamples = 0U;
        runtime->hx711TareSamples = 0U;
        runtime->hx711TareReady = 0U;
        runtime->hx711Baseline = 0;
        runtime->hx711Accumulator = 0;
    }
}

void load_cell_set_mechanical_fallback(LoadCellRuntime *runtime, uint8_t enabled)
{
    if (runtime == 0)
    {
        return;
    }

    runtime->mechanicalFallbackOverride = enabled != 0U ? 1U : 0U;
}

void load_cell_set_stall_override(LoadCellRuntime *runtime, uint8_t enabled)
{
    if (runtime == 0)
    {
        return;
    }

    runtime->stallOverride = enabled != 0U ? 1U : 0U;
}

void load_cell_clear(LoadCellRuntime *runtime)
{
    if (runtime == 0)
    {
        return;
    }

    runtime->raw = 0U;
    runtime->triggerState = 0U;
    runtime->overThresholdCount = 0U;
    runtime->underThresholdCount = 0U;
    runtime->hx711WarmupSamples = 0U;
    runtime->hx711TareSamples = 0U;
    runtime->hx711TareReady = 0U;
    runtime->hx711Baseline = 0;
    runtime->hx711Accumulator = 0;
    runtime->mechanicalFallbackOverride = 0U;
    runtime->stallOverride = 0U;
}

uint8_t load_cell_triggered(const LoadCellRuntime &runtime)
{
    if ((runtime.source != (uint8_t)LoadCellSourceKind::Simulation) &&
        (runtime.source != (uint8_t)LoadCellSourceKind::Hx711))
    {
        return 0U;
    }

    return runtime.triggerState;
}

uint32_t load_cell_raw(const LoadCellRuntime &runtime)
{
    if ((runtime.source != (uint8_t)LoadCellSourceKind::Simulation) &&
        (runtime.source != (uint8_t)LoadCellSourceKind::Hx711))
    {
        return 0U;
    }

    return runtime.raw;
}

uint8_t load_cell_source(const LoadCellRuntime &runtime)
{
    return runtime.source;
}

int32_t load_cell_hx711_decode_u24(uint32_t sample)
{
    sample &= 0x00FFFFFFUL;
    if ((sample & 0x00800000UL) != 0U)
    {
        sample |= 0xFF000000UL;
    }
    return (int32_t)sample;
}

uint32_t load_cell_force_from_signed(int32_t sample)
{
    if (sample >= 0)
    {
        return (uint32_t)sample;
    }

    if (sample == INT32_MIN)
    {
        return 2147483648UL;
    }

    return (uint32_t)(-sample);
}

const char *load_cell_source_name(uint8_t source)
{
    switch ((LoadCellSourceKind)source)
    {
    case LoadCellSourceKind::Hx711: return "hx711";
    case LoadCellSourceKind::AnalogAdc: return "adc";
    default: return "simulation";
    }
}

uint8_t load_cell_source_from_cstr(const char *text, uint8_t *source)
{
    if ((text == 0) || (source == 0))
    {
        return 0U;
    }

    if ((strcmp(text, "SIM") == 0) || (strcmp(text, "SIMULATION") == 0))
    {
        *source = (uint8_t)LoadCellSourceKind::Simulation;
        return 1U;
    }
    if (strcmp(text, "HX711") == 0)
    {
        *source = (uint8_t)LoadCellSourceKind::Hx711;
        return 1U;
    }
    if ((strcmp(text, "ADC") == 0) || (strcmp(text, "ANALOG_ADC") == 0))
    {
        *source = (uint8_t)LoadCellSourceKind::AnalogAdc;
        return 1U;
    }

    return 0U;
}

const char *load_cell_connector_name(uint8_t connector)
{
    switch ((LoadCellConnectorKind)connector)
    {
    case LoadCellConnectorKind::Skr2Bltouch: return "skr2_bltouch";
    case LoadCellConnectorKind::Skr2Det: return "skr2_det";
    case LoadCellConnectorKind::Skr2Th1: return "skr2_th1";
    case LoadCellConnectorKind::Skr2Th0: return "skr2_th0";
    case LoadCellConnectorKind::Skr2Tb: return "skr2_tb";
    default: return "custom";
    }
}

uint8_t load_cell_connector_from_cstr(const char *text, uint8_t *connector)
{
    if ((text == 0) || (connector == 0))
    {
        return 0U;
    }

    if ((strcmp(text, "CUSTOM") == 0) || (strcmp(text, "MANUAL") == 0))
    {
        *connector = (uint8_t)LoadCellConnectorKind::Custom;
        return 1U;
    }
    if ((strcmp(text, "SKR2_BLTOUCH") == 0) || (strcmp(text, "BLTOUCH") == 0) || (strcmp(text, "SERVO") == 0))
    {
        *connector = (uint8_t)LoadCellConnectorKind::Skr2Bltouch;
        return 1U;
    }
    if ((strcmp(text, "SKR2_DET") == 0) || (strcmp(text, "DET") == 0) || (strcmp(text, "RUNOUT") == 0))
    {
        *connector = (uint8_t)LoadCellConnectorKind::Skr2Det;
        return 1U;
    }
    if ((strcmp(text, "SKR2_TH1") == 0) || (strcmp(text, "TH1") == 0))
    {
        *connector = (uint8_t)LoadCellConnectorKind::Skr2Th1;
        return 1U;
    }
    if ((strcmp(text, "SKR2_TH0") == 0) || (strcmp(text, "TH0") == 0))
    {
        *connector = (uint8_t)LoadCellConnectorKind::Skr2Th0;
        return 1U;
    }
    if ((strcmp(text, "SKR2_TB") == 0) || (strcmp(text, "TB") == 0) || (strcmp(text, "BED") == 0))
    {
        *connector = (uint8_t)LoadCellConnectorKind::Skr2Tb;
        return 1U;
    }

    return 0U;
}

void load_cell_apply_connector_profile(LoadCellConfig *config, uint8_t connector)
{
    if (config == 0)
    {
        return;
    }

    config->connector = connector;

    switch ((LoadCellConnectorKind)connector)
    {
    case LoadCellConnectorKind::Skr2Bltouch:
        config->source = (uint8_t)LoadCellSourceKind::Hx711;
        config->pins.data = {(uint8_t)GpioPortId::E, 4U};
        config->pins.clock = {(uint8_t)GpioPortId::E, 5U};
        break;
    case LoadCellConnectorKind::Skr2Det:
        config->source = (uint8_t)LoadCellSourceKind::Hx711;
        config->pins.data = {(uint8_t)GpioPortId::C, 2U};
        config->pins.clock = {(uint8_t)GpioPortId::A, 0U};
        break;
    case LoadCellConnectorKind::Skr2Th1:
        config->source = (uint8_t)LoadCellSourceKind::AnalogAdc;
        config->pins.data = {(uint8_t)GpioPortId::A, 3U};
        config->pins.clock = {0U, 0U};
        break;
    case LoadCellConnectorKind::Skr2Th0:
        config->source = (uint8_t)LoadCellSourceKind::AnalogAdc;
        config->pins.data = {(uint8_t)GpioPortId::A, 2U};
        config->pins.clock = {0U, 0U};
        break;
    case LoadCellConnectorKind::Skr2Tb:
        config->source = (uint8_t)LoadCellSourceKind::AnalogAdc;
        config->pins.data = {(uint8_t)GpioPortId::A, 1U};
        config->pins.clock = {0U, 0U};
        break;
    default:
        break;
    }
}