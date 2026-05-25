#include "load_cell.h"

#include <string.h>

LoadCellRuntime load_cell_make_default(uint32_t threshold)
{
    LoadCellRuntime runtime = {};
    runtime.threshold = threshold;
    return runtime;
}

void load_cell_set_raw(LoadCellRuntime *runtime, uint32_t raw)
{
    if (runtime == 0)
    {
        return;
    }

    runtime->raw = raw;
}

void load_cell_set_threshold(LoadCellRuntime *runtime, uint32_t threshold)
{
    if (runtime == 0)
    {
        return;
    }

    runtime->threshold = threshold;
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
    runtime->mechanicalFallbackOverride = 0U;
    runtime->stallOverride = 0U;
}

uint8_t load_cell_triggered(const LoadCellRuntime &runtime)
{
    return runtime.raw >= runtime.threshold ? 1U : 0U;
}

uint32_t load_cell_raw(const LoadCellRuntime &runtime)
{
    return runtime.raw;
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