#include "load_cell.h"

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