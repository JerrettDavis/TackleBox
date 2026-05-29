#include "load_cell.h"

#include <limits.h>
#include <string.h>

static const uint8_t kHx711TriggerDebounceCount = 3U;
static const uint8_t kHx711CompositeSampleCount = 3U;
static const uint8_t kHx711WarmupSampleCount = 16U;
static const uint8_t kHx711AutoTareSampleCount = 16U;
static const uint8_t kHx711DefaultReleaseRate = 2U;
static const uint8_t kHx711DefaultRiseRate = 4U;

static uint32_t load_cell_default_display_full_scale_raw(uint32_t threshold)
{
    uint32_t full_scale_raw = threshold;
    if (full_scale_raw == 0U)
    {
        return 4000U;
    }

    if (full_scale_raw <= (UINT32_MAX / 4U))
    {
        full_scale_raw *= 4U;
    }

    return (full_scale_raw < 4000U) ? 4000U : full_scale_raw;
}

static uint32_t hx711_idle_force_floor(uint32_t threshold)
{
    return (threshold > 0U) ? ((threshold / 16U) + 1U) : 16U;
}

static uint8_t hx711_release_rate_or_default(uint8_t configured_rate)
{
    return (configured_rate == 0U) ? kHx711DefaultReleaseRate : configured_rate;
}

static uint8_t hx711_rise_rate_or_default(uint8_t configured_rate)
{
    return (configured_rate == 0U) ? kHx711DefaultRiseRate : configured_rate;
}

static uint32_t hx711_composite_span_limit(uint32_t threshold)
{
    uint32_t span_limit = (threshold / 8U) + 32U;
    if (span_limit < 64U)
    {
        span_limit = 64U;
    }
    if (span_limit > 512U)
    {
        span_limit = 512U;
    }
    return span_limit;
}

static void hx711_reset_composite_window(LoadCellRuntime *runtime)
{
    if (runtime == 0)
    {
        return;
    }

    runtime->hx711CompositeCount = 0U;
    memset(runtime->hx711CompositeSamples, 0, sizeof(runtime->hx711CompositeSamples));
}

static void hx711_push_composite_sample(LoadCellRuntime *runtime, int32_t sample)
{
    if (runtime == 0)
    {
        return;
    }

    if (runtime->hx711CompositeCount < kHx711CompositeSampleCount)
    {
        runtime->hx711CompositeSamples[runtime->hx711CompositeCount] = sample;
        ++runtime->hx711CompositeCount;
        return;
    }

    runtime->hx711CompositeSamples[0] = runtime->hx711CompositeSamples[1];
    runtime->hx711CompositeSamples[1] = runtime->hx711CompositeSamples[2];
    runtime->hx711CompositeSamples[2] = sample;
}

static int32_t hx711_median3(int32_t a, int32_t b, int32_t c)
{
    if (a > b)
    {
        const int32_t temp = a;
        a = b;
        b = temp;
    }
    if (b > c)
    {
        const int32_t temp = b;
        b = c;
        c = temp;
    }
    if (a > b)
    {
        const int32_t temp = a;
        a = b;
        b = temp;
    }
    return b;
}

static uint8_t hx711_try_get_composite_sample(const LoadCellRuntime *runtime, uint32_t threshold, int32_t *sample)
{
    if ((runtime == 0) || (sample == 0) || (runtime->hx711CompositeCount < kHx711CompositeSampleCount))
    {
        return 0U;
    }

    int32_t min_sample = runtime->hx711CompositeSamples[0];
    int32_t max_sample = runtime->hx711CompositeSamples[0];
    for (uint8_t index = 1U; index < kHx711CompositeSampleCount; ++index)
    {
        const int32_t candidate = runtime->hx711CompositeSamples[index];
        if (candidate < min_sample)
        {
            min_sample = candidate;
        }
        if (candidate > max_sample)
        {
            max_sample = candidate;
        }
    }

    if ((uint32_t)(max_sample - min_sample) > hx711_composite_span_limit(threshold))
    {
        return 0U;
    }

    *sample = hx711_median3(
        runtime->hx711CompositeSamples[0],
        runtime->hx711CompositeSamples[1],
        runtime->hx711CompositeSamples[2]);
    return 1U;
}

static uint8_t saturating_add_u8(uint8_t value, uint8_t amount)
{
    return (uint8_t)((value > (uint8_t)(255U - amount)) ? 255U : (value + amount));
}

static void load_cell_update_hx711_trigger_state_weighted(LoadCellRuntime *runtime, uint8_t sample_weight)
{
    if (runtime == 0)
    {
        return;
    }

    if (sample_weight == 0U)
    {
        sample_weight = 1U;
    }

    if (runtime->raw >= runtime->threshold)
    {
        runtime->overThresholdCount = saturating_add_u8(runtime->overThresholdCount, sample_weight);
        runtime->underThresholdCount = 0U;
        if (runtime->overThresholdCount >= kHx711TriggerDebounceCount)
        {
            runtime->triggerState = 1U;
        }
        return;
    }

    runtime->underThresholdCount = saturating_add_u8(runtime->underThresholdCount, sample_weight);
    runtime->overThresholdCount = 0U;
    if (runtime->underThresholdCount >= kHx711TriggerDebounceCount)
    {
        runtime->triggerState = 0U;
    }
}

static void load_cell_update_hx711_trigger_state(LoadCellRuntime *runtime)
{
    load_cell_update_hx711_trigger_state_weighted(runtime, 1U);
}

static uint32_t blend_u32_rise(uint32_t current, uint32_t next, uint8_t rise_rate)
{
    if ((current == 0U) || (current >= next))
    {
        return next;
    }

    const uint8_t divisor = hx711_rise_rate_or_default(rise_rate);
    if (divisor <= 1U)
    {
        return next;
    }

    return current + ((next - current + (uint32_t)divisor - 1U) / (uint32_t)divisor);
}

static uint32_t blend_u32_fall(uint32_t current, uint32_t next, uint8_t release_rate)
{
    if (current <= next)
    {
        return next;
    }

    const uint8_t divisor = hx711_release_rate_or_default(release_rate);
    if (divisor <= 1U)
    {
        return next;
    }

    const uint32_t delta = current - next;
    const uint32_t step = (delta + (uint32_t)divisor - 1U) / (uint32_t)divisor;
    return current - step;
}

LoadCellRuntime load_cell_make_default(uint32_t threshold)
{
    LoadCellRuntime runtime = {};
    runtime.source = (uint8_t)LoadCellSourceKind::Simulation;
    runtime.threshold = threshold;
    runtime.hx711ReleaseRate = kHx711DefaultReleaseRate;
    runtime.hx711RiseRate = kHx711DefaultRiseRate;
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
        hx711_reset_composite_window(runtime);
    }

    runtime->source = config.source;
    runtime->threshold = config.threshold;
    runtime->hx711ReleaseRate = hx711_release_rate_or_default(config.hx711ReleaseRate);
    runtime->hx711RiseRate = hx711_rise_rate_or_default(config.hx711RiseRate);
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
            hx711_reset_composite_window(runtime);
            return;
        }

        runtime->hx711Accumulator += sample;
        ++runtime->hx711TareSamples;
        runtime->raw = 0U;
        runtime->triggerState = 0U;
        runtime->overThresholdCount = 0U;
        runtime->underThresholdCount = 0U;
        hx711_reset_composite_window(runtime);
        if (runtime->hx711TareSamples >= kHx711AutoTareSampleCount)
        {
            runtime->hx711Baseline = runtime->hx711Accumulator / (int32_t)runtime->hx711TareSamples;
            runtime->hx711Accumulator = 0;
            runtime->hx711TareReady = 1U;
        }
        return;
    }

    hx711_push_composite_sample(runtime, sample - runtime->hx711Baseline);

    int32_t relative_sample = 0;
    if (hx711_try_get_composite_sample(runtime, runtime->threshold, &relative_sample) == 0U)
    {
        return;
    }

    const uint32_t relative_force = load_cell_force_from_signed(relative_sample);
    runtime->raw = (relative_force >= runtime->raw)
        ? blend_u32_rise(runtime->raw, relative_force, runtime->hx711RiseRate)
        : blend_u32_fall(runtime->raw, relative_force, runtime->hx711ReleaseRate);

    const uint32_t idle_force_floor = hx711_idle_force_floor(runtime->threshold);
    if ((relative_force < idle_force_floor) && (runtime->raw < idle_force_floor))
    {
        runtime->raw = 0U;
    }
    else if (relative_force < idle_force_floor)
    {
        const uint32_t quiet_snap_limit = ((runtime->threshold > 0U) ? (runtime->threshold / 8U) : 0U) + idle_force_floor;
        if (runtime->raw <= quiet_snap_limit)
        {
            runtime->raw = 0U;
        }
    }

    if (relative_force < ((runtime->threshold > 0U) ? ((runtime->threshold / 8U) + 1U) : 32U))
    {
        runtime->hx711Baseline = runtime->hx711Baseline + (relative_sample / 8);
    }

    load_cell_update_hx711_trigger_state_weighted(runtime, kHx711CompositeSampleCount);
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
        hx711_reset_composite_window(runtime);
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
    hx711_reset_composite_window(runtime);
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

uint32_t load_cell_calibrated_grams(uint32_t raw, const LoadCellConfig &config)
    {
        uint32_t reference_raw = config.calibrationRaw;
        uint32_t reference_grams = config.calibrationGrams;

        if ((reference_raw == 0U) || (reference_grams == 0U))
        {
            reference_raw = load_cell_default_display_full_scale_raw(config.threshold);
            reference_grams = 100U;
        }

        uint64_t scaled = ((uint64_t)raw * (uint64_t)reference_grams) + ((uint64_t)reference_raw / 2ULL);
        uint32_t grams = (uint32_t)(scaled / (uint64_t)reference_raw);
        return (grams > 1999U) ? 1999U : grams;
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