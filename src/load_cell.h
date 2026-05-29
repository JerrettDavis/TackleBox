#ifndef LOAD_CELL_H
#define LOAD_CELL_H

#include <stdint.h>

#include "app_config.h"

struct LoadCellRuntime {
    uint8_t source;
    uint8_t triggerState;
    uint8_t overThresholdCount;
    uint8_t underThresholdCount;
    uint32_t raw;
    uint32_t threshold;
    uint8_t mechanicalFallbackOverride;
    uint8_t stallOverride;
    uint8_t hx711WarmupSamples;
    uint8_t hx711TareSamples;
    uint8_t hx711TareReady;
    uint8_t hx711ReleaseRate;
    uint8_t hx711RiseRate;
    uint8_t hx711CompositeCount;
    int32_t hx711Baseline;
    int32_t hx711Accumulator;
    int32_t hx711CompositeSamples[3];
};

LoadCellRuntime load_cell_make_default(uint32_t threshold);
void load_cell_apply_config(LoadCellRuntime *runtime, const LoadCellConfig &config);
void load_cell_set_raw(LoadCellRuntime *runtime, uint32_t raw);
void load_cell_set_hx711_sample(LoadCellRuntime *runtime, int32_t sample);
void load_cell_set_threshold(LoadCellRuntime *runtime, uint32_t threshold);
void load_cell_tare(LoadCellRuntime *runtime);
void load_cell_set_mechanical_fallback(LoadCellRuntime *runtime, uint8_t enabled);
void load_cell_set_stall_override(LoadCellRuntime *runtime, uint8_t enabled);
void load_cell_clear(LoadCellRuntime *runtime);
uint8_t load_cell_triggered(const LoadCellRuntime &runtime);
uint32_t load_cell_raw(const LoadCellRuntime &runtime);
uint8_t load_cell_source(const LoadCellRuntime &runtime);
int32_t load_cell_hx711_decode_u24(uint32_t sample);
uint32_t load_cell_force_from_signed(int32_t sample);
uint32_t load_cell_calibrated_grams(uint32_t raw, const LoadCellConfig &config);
const char *load_cell_source_name(uint8_t source);
uint8_t load_cell_source_from_cstr(const char *text, uint8_t *source);
const char *load_cell_connector_name(uint8_t connector);
uint8_t load_cell_connector_from_cstr(const char *text, uint8_t *connector);
void load_cell_apply_connector_profile(LoadCellConfig *config, uint8_t connector);

#endif