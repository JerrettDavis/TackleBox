#include <stdexcept>
#include <string>

#include "load_cell.h"

void test_load_cell_defaults_to_zero_raw_and_configured_threshold(void)
{
    LoadCellRuntime runtime = load_cell_make_default(1000U);

    if (runtime.source != (uint8_t)LoadCellSourceKind::Simulation) throw std::runtime_error("default runtime should start in simulation mode");
    if (runtime.raw != 0U) throw std::runtime_error("default runtime should start with raw=0");
    if (runtime.threshold != 1000U) throw std::runtime_error("default runtime should keep the configured threshold");
    if (runtime.mechanicalFallbackOverride != 0U) throw std::runtime_error("default runtime should start with mechanical fallback cleared");
    if (runtime.stallOverride != 0U) throw std::runtime_error("default runtime should start with stall override cleared");
    if (load_cell_triggered(runtime) != 0U) throw std::runtime_error("zero raw should not trigger the load cell");
}

void test_load_cell_triggers_when_raw_reaches_threshold(void)
{
    LoadCellRuntime runtime = load_cell_make_default(1000U);

    load_cell_set_raw(&runtime, 999U);
    if (load_cell_triggered(runtime) != 0U) throw std::runtime_error("raw below threshold should not trigger");

    load_cell_set_raw(&runtime, 1000U);
    if (load_cell_triggered(runtime) != 1U) throw std::runtime_error("raw at threshold should trigger");

    load_cell_set_threshold(&runtime, 1200U);
    if (load_cell_triggered(runtime) != 0U) throw std::runtime_error("raising the threshold should clear the trigger when raw is below it");
}

void test_load_cell_hx711_trigger_requires_stable_samples(void)
{
    LoadCellRuntime runtime = load_cell_make_default(1000U);
    LoadCellConfig config = {};
    config.source = (uint8_t)LoadCellSourceKind::Hx711;
    config.threshold = 1000U;

    load_cell_apply_config(&runtime, config);

    load_cell_set_raw(&runtime, 4000U);
    if (load_cell_triggered(runtime) != 0U) throw std::runtime_error("first hx711 spike should not immediately trigger");
    load_cell_set_raw(&runtime, 4000U);
    if (load_cell_triggered(runtime) != 0U) throw std::runtime_error("second hx711 spike should still be debounced");
    load_cell_set_raw(&runtime, 4000U);
    if (load_cell_triggered(runtime) != 1U) throw std::runtime_error("third consecutive hx711 sample should trigger");

    load_cell_set_raw(&runtime, 0U);
    if (load_cell_triggered(runtime) != 1U) throw std::runtime_error("first below-threshold hx711 sample should not immediately clear");
    load_cell_set_raw(&runtime, 0U);
    if (load_cell_triggered(runtime) != 1U) throw std::runtime_error("second below-threshold hx711 sample should still be debounced");
    load_cell_set_raw(&runtime, 0U);
    if (load_cell_triggered(runtime) != 0U) throw std::runtime_error("third below-threshold hx711 sample should clear the trigger");
}

void test_load_cell_clear_preserves_threshold_and_clears_overrides(void)
{
    LoadCellRuntime runtime = load_cell_make_default(1000U);

    load_cell_set_raw(&runtime, 1500U);
    load_cell_set_threshold(&runtime, 900U);
    load_cell_set_mechanical_fallback(&runtime, 1U);
    load_cell_set_stall_override(&runtime, 1U);

    load_cell_clear(&runtime);

    if (runtime.raw != 0U) throw std::runtime_error("clear should reset raw to zero");
    if (runtime.threshold != 900U) throw std::runtime_error("clear should preserve the configured threshold");
    if (runtime.mechanicalFallbackOverride != 0U) throw std::runtime_error("clear should reset the mechanical fallback override");
    if (runtime.stallOverride != 0U) throw std::runtime_error("clear should reset the stall override");
}

void test_load_cell_raw_accessor_tracks_updates(void)
{
    LoadCellRuntime runtime = load_cell_make_default(1000U);

    load_cell_set_raw(&runtime, 321U);

    if (load_cell_raw(runtime) != 321U) throw std::runtime_error("raw accessor should reflect the latest raw value");
}

void test_load_cell_source_names_and_parsing(void)
{
    uint8_t source = 0U;

    if (!load_cell_source_from_cstr("SIMULATION", &source)) throw std::runtime_error("simulation source should parse");
    if (source != (uint8_t)LoadCellSourceKind::Simulation) throw std::runtime_error("simulation source should parse to Simulation");
    if (std::string(load_cell_source_name(source)) != "simulation") throw std::runtime_error("simulation source should format as simulation");

    if (!load_cell_source_from_cstr("HX711", &source)) throw std::runtime_error("HX711 source should parse");
    if (source != (uint8_t)LoadCellSourceKind::Hx711) throw std::runtime_error("HX711 source should parse to Hx711");
    if (std::string(load_cell_source_name(source)) != "hx711") throw std::runtime_error("HX711 source should format as hx711");

    if (!load_cell_source_from_cstr("ADC", &source)) throw std::runtime_error("ADC source should parse");
    if (source != (uint8_t)LoadCellSourceKind::AnalogAdc) throw std::runtime_error("ADC source should parse to AnalogAdc");
    if (std::string(load_cell_source_name(source)) != "adc") throw std::runtime_error("ADC source should format as adc");

    if (load_cell_source_from_cstr("NOPE", &source) != 0U) throw std::runtime_error("unknown source should not parse");
}

void test_load_cell_non_simulation_modes_do_not_reuse_simulated_raw(void)
{
    LoadCellRuntime runtime = load_cell_make_default(1000U);
    LoadCellConfig config = {};
    config.source = (uint8_t)LoadCellSourceKind::AnalogAdc;
    config.threshold = 1000U;

    load_cell_set_raw(&runtime, 1500U);
    load_cell_apply_config(&runtime, config);

    if (load_cell_source(runtime) != (uint8_t)LoadCellSourceKind::AnalogAdc) throw std::runtime_error("runtime should reflect the configured source");
    if (load_cell_raw(runtime) != 0U) throw std::runtime_error("adc mode should stay inactive until acquisition is implemented");
    if (load_cell_triggered(runtime) != 0U) throw std::runtime_error("adc mode should stay inactive until acquisition is implemented");
}

void test_load_cell_hx711_decode_and_thresholding(void)
{
    if (load_cell_hx711_decode_u24(0x007FFFFFUL) != 8388607) throw std::runtime_error("positive HX711 sample should decode as signed 24-bit");
    if (load_cell_hx711_decode_u24(0x00800000UL) != (-8388607 - 1)) throw std::runtime_error("negative HX711 sample should sign-extend from bit 23");
    if (load_cell_force_from_signed(-1234) != 1234U) throw std::runtime_error("signed load-cell samples should map to absolute force magnitude");

    LoadCellRuntime runtime = load_cell_make_default(1000U);
    LoadCellConfig config = {};
    config.source = (uint8_t)LoadCellSourceKind::Hx711;
    config.threshold = 1000U;

    load_cell_apply_config(&runtime, config);
    const uint32_t sample = load_cell_force_from_signed(load_cell_hx711_decode_u24(0x00FFF830UL));
    load_cell_set_raw(&runtime, sample);
    load_cell_set_raw(&runtime, sample);
    load_cell_set_raw(&runtime, sample);

    if (load_cell_raw(runtime) == 0U) throw std::runtime_error("hx711 mode should expose captured force samples");
    if (load_cell_triggered(runtime) != 1U) throw std::runtime_error("hx711 mode should honor the configured force threshold");
}

void test_load_cell_hx711_auto_tare_and_relative_force(void)
{
    LoadCellRuntime runtime = load_cell_make_default(1000U);
    LoadCellConfig config = {};
    config.source = (uint8_t)LoadCellSourceKind::Hx711;
    config.threshold = 1000U;

    load_cell_apply_config(&runtime, config);

    for (uint32_t index = 0U; index < 32U; ++index)
    {
        load_cell_set_hx711_sample(&runtime, 5000);
    }

    if (load_cell_raw(runtime) != 0U) throw std::runtime_error("hx711 auto-tare should zero the initial baseline window");

    load_cell_set_hx711_sample(&runtime, 6500);
    load_cell_set_hx711_sample(&runtime, 6500);
    load_cell_set_hx711_sample(&runtime, 6500);

    if (load_cell_raw(runtime) == 0U) throw std::runtime_error("hx711 relative force should rise when the sample moves away from baseline");
    if (load_cell_triggered(runtime) != 1U) throw std::runtime_error("hx711 relative force should trigger after stable above-threshold samples");
}

void test_load_cell_hx711_idle_noise_floor_zeros_small_residuals(void)
{
    LoadCellRuntime runtime = load_cell_make_default(1000U);
    LoadCellConfig config = {};
    config.source = (uint8_t)LoadCellSourceKind::Hx711;
    config.threshold = 1000U;

    load_cell_apply_config(&runtime, config);

    for (uint32_t index = 0U; index < 32U; ++index)
    {
        load_cell_set_hx711_sample(&runtime, 5000);
    }

    load_cell_set_hx711_sample(&runtime, 5030);
    load_cell_set_hx711_sample(&runtime, 4975);
    load_cell_set_hx711_sample(&runtime, 5025);

    if (load_cell_raw(runtime) != 0U) throw std::runtime_error("small unloaded hx711 residuals should be normalized back to zero");
    if (load_cell_triggered(runtime) != 0U) throw std::runtime_error("small unloaded hx711 residuals should not trigger the load cell");
}

void test_load_cell_connector_names_and_profiles(void)
{
    uint8_t connector = 0U;
    LoadCellConfig config = {};

    if (!load_cell_connector_from_cstr("BLTOUCH", &connector)) throw std::runtime_error("BLTouch connector should parse");
    if (connector != (uint8_t)LoadCellConnectorKind::Skr2Bltouch) throw std::runtime_error("BLTouch connector should parse to Skr2Bltouch");
    if (std::string(load_cell_connector_name(connector)) != "skr2_bltouch") throw std::runtime_error("BLTouch connector should format as skr2_bltouch");
    load_cell_apply_connector_profile(&config, connector);
    if (config.source != (uint8_t)LoadCellSourceKind::Hx711) throw std::runtime_error("BLTouch connector should select HX711 source");
    if ((config.pins.data.portId != (uint8_t)GpioPortId::E) || (config.pins.data.pin != 4U)) throw std::runtime_error("BLTouch connector should map data to PE4");
    if ((config.pins.clock.portId != (uint8_t)GpioPortId::E) || (config.pins.clock.pin != 5U)) throw std::runtime_error("BLTouch connector should map clock to PE5");

    if (!load_cell_connector_from_cstr("TH1", &connector)) throw std::runtime_error("TH1 connector should parse");
    if (connector != (uint8_t)LoadCellConnectorKind::Skr2Th1) throw std::runtime_error("TH1 connector should parse to Skr2Th1");
    load_cell_apply_connector_profile(&config, connector);
    if (config.source != (uint8_t)LoadCellSourceKind::AnalogAdc) throw std::runtime_error("TH1 connector should select analog ADC source");
    if ((config.pins.data.portId != (uint8_t)GpioPortId::A) || (config.pins.data.pin != 3U)) throw std::runtime_error("TH1 connector should map data to PA3");
    if ((config.pins.clock.portId != 0U) || (config.pins.clock.pin != 0U)) throw std::runtime_error("TH1 connector should clear the clock pin");

    if (!load_cell_connector_from_cstr("CUSTOM", &connector)) throw std::runtime_error("custom connector should parse");
    if (std::string(load_cell_connector_name(connector)) != "custom") throw std::runtime_error("custom connector should format as custom");
    if (load_cell_connector_from_cstr("NOPE", &connector) != 0U) throw std::runtime_error("unknown connector should not parse");
}

int main()
{
    test_load_cell_defaults_to_zero_raw_and_configured_threshold();
    test_load_cell_triggers_when_raw_reaches_threshold();
    test_load_cell_hx711_trigger_requires_stable_samples();
    test_load_cell_clear_preserves_threshold_and_clears_overrides();
    test_load_cell_raw_accessor_tracks_updates();
    test_load_cell_source_names_and_parsing();
    test_load_cell_non_simulation_modes_do_not_reuse_simulated_raw();
    test_load_cell_hx711_decode_and_thresholding();
    test_load_cell_hx711_auto_tare_and_relative_force();
    test_load_cell_hx711_idle_noise_floor_zeros_small_residuals();
    test_load_cell_connector_names_and_profiles();
    return 0;
}