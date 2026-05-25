#include <stdexcept>

#include "load_cell.h"

void test_load_cell_defaults_to_zero_raw_and_configured_threshold(void)
{
    LoadCellRuntime runtime = load_cell_make_default(1000U);

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

int main()
{
    test_load_cell_defaults_to_zero_raw_and_configured_threshold();
    test_load_cell_triggers_when_raw_reaches_threshold();
    test_load_cell_clear_preserves_threshold_and_clears_overrides();
    test_load_cell_raw_accessor_tracks_updates();
    return 0;
}