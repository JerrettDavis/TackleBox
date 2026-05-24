#ifndef APP_RUNTIME_CONFIG_H
#define APP_RUNTIME_CONFIG_H

#include "app_config.h"

struct ApplyConfigResult {
    uint8_t accepted;
    uint8_t rebootRequired;
};

struct ConfigRuntimeState {
    uint8_t dirty;
    uint8_t requiresReboot;
    uint8_t loadedFromFlash;
    ConfigBootSource source;
    uint8_t flashConfigAvailable;
    uint8_t microsdCardPresent;
    uint8_t microsdConfigAvailable;
};

typedef ApplyConfigResult (*BootConfigApplyFn)(PersistedFirmwareConfig *config, const char *key, const char *value);

PersistedFirmwareConfig make_default_persisted_config(void);
uint8_t persisted_config_valid(const PersistedFirmwareConfig &config);
uint8_t load_persisted_config(PersistedFirmwareConfig *config);
uint8_t save_persisted_config(const PersistedFirmwareConfig &config);
uint32_t axis_steps_from_um(const PersistedFirmwareConfig &config, uint32_t travel_um);
uint32_t axis_um_from_steps(const PersistedFirmwareConfig &config, uint32_t steps);
int32_t axis_steps_from_signed_um(const PersistedFirmwareConfig &config, int32_t travel_um);
int32_t axis_travel_max_um(const PersistedFirmwareConfig &config);
void emit_config_summary(const PersistedFirmwareConfig &config, const ConfigRuntimeState &state);
void emit_config_sources_line(const ConfigRuntimeState &state);
void emit_boot_source_line(ConfigBootSource source);
ConfigRuntimeState select_boot_config(PersistedFirmwareConfig *config, BootConfigApplyFn apply_fn);

#endif