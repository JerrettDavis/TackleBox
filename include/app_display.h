#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#include "app_panel.h"
#include "display_validation.h"
#include "app_runtime_config.h"
#include "keyswitch_domain.h"

enum class Mini12864UiIntentType : uint8_t {
    None = 0,
    Home,
    Stop,
    Tare,
    Backoff,
    SetHold,
    MoveRelativeUm,
    SetDefaultPressUm,
    StartCycle,
    SetLoadThreshold,
    SetIrun,
    SetIhold,
    SetIholdDelay,
    SetSgthrs,
    SetAllowUnverifiedMotion,
    SetMoveFeedrate,
    SetHomeFeedrate,
    SetBackoffSteps,
    SetDebounceCount,
    SetPanelColorRed,
    SetPanelColorGreen,
    SetPanelColorBlue,
    SaveConfig,
    ResetConfig,
    Reboot,
};

struct Mini12864UiIntent {
    Mini12864UiIntentType type;
    int32_t value;
};

void mini12864_display_init(const Mini12864PanelPins &pins);
Mini12864UiIntent mini12864_ui_update(
    const Mini12864PanelInputs &panel_inputs,
    const PersistedFirmwareConfig &config,
    const ConfigRuntimeState &config_state,
    const keyswitch::MotionState &state);
void mini12864_display_render(
    const Mini12864PanelPins &pins,
    uint32_t now_ms,
    const keyswitch::MotionInputs &inputs,
    const keyswitch::MotionState &state,
    const keyswitch::MotionOutputs &outputs,
    const Mini12864PanelInputs &panel_inputs,
    const PersistedFirmwareConfig &config,
    const ConfigRuntimeState &config_state);

#ifdef KEYSWITCH_HOST_TEST
enum class Mini12864HostScreen : uint8_t {
    Splash = 0,
    DashboardLoad,
    DashboardMotion,
    DashboardDriver,
    DashboardConfig,
    RootMenu,
    MotionMenu,
    MeasureMenu,
    DriverMenu,
    ConfigMenu,
};

struct Mini12864RenderSnapshot {
    uint8_t framebuffer[128U * 8U];
    DisplayValidationReport validation;
};

void mini12864_display_host_render(
    Mini12864HostScreen screen,
    const keyswitch::MotionInputs &inputs,
    const keyswitch::MotionState &state,
    const keyswitch::MotionOutputs &outputs,
    const PersistedFirmwareConfig &config,
    const ConfigRuntimeState &config_state,
    Mini12864RenderSnapshot *snapshot);
#endif

#endif