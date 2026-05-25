#ifndef LOAD_CELL_H
#define LOAD_CELL_H

#include <stdint.h>

#include "app_config.h"

struct LoadCellRuntime {
    uint32_t raw;
    uint32_t threshold;
    uint8_t mechanicalFallbackOverride;
    uint8_t stallOverride;
};

LoadCellRuntime load_cell_make_default(uint32_t threshold);
void load_cell_set_raw(LoadCellRuntime *runtime, uint32_t raw);
void load_cell_set_threshold(LoadCellRuntime *runtime, uint32_t threshold);
void load_cell_set_mechanical_fallback(LoadCellRuntime *runtime, uint8_t enabled);
void load_cell_set_stall_override(LoadCellRuntime *runtime, uint8_t enabled);
void load_cell_clear(LoadCellRuntime *runtime);
uint8_t load_cell_triggered(const LoadCellRuntime &runtime);
uint32_t load_cell_raw(const LoadCellRuntime &runtime);
const char *load_cell_source_name(uint8_t source);
uint8_t load_cell_source_from_cstr(const char *text, uint8_t *source);

#endif