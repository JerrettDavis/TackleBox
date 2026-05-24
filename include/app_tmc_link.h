#ifndef APP_TMC_LINK_H
#define APP_TMC_LINK_H

#include "app_config.h"

struct DriverRuntimeConfig {
    keyswitch::Tmc2209Config tmc2209;
    uint8_t uartConfigured;
    uint8_t ifcntValid;
    uint8_t ifcnt;
    uint32_t verifiedGconf;
    uint32_t verifiedIholdIrun;
    uint32_t verifiedTpowerdown;
    uint32_t verifiedSgthrs;
};

DriverRuntimeConfig &tmc_driver_runtime(void);
void x_driver_enable(const PersistedFirmwareConfig &config);
void x_driver_disable(const PersistedFirmwareConfig &config);
uint8_t x_tmc2209_apply_config(const PersistedFirmwareConfig &config);
uint8_t x_tmc2209_refresh_verification(const PersistedFirmwareConfig &config);
uint8_t x_tmc_verified(void);
void emit_tmc_unverified_blocked(const char *command_name);

#endif