#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "app_display.h"
#include "app_runtime_config.h"
#include "boot_panel_splash.h"

namespace {

void require_true(bool condition, const char *message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

uint32_t lit_pixel_count(const uint8_t *framebuffer, uint32_t length)
{
    uint32_t count = 0U;
    for (uint32_t index = 0U; index < length; ++index)
    {
        uint8_t value = framebuffer[index];
        for (uint8_t bit = 0U; bit < 8U; ++bit)
        {
            count += (value >> bit) & 0x1U;
        }
    }
    return count;
}

void require_no_layout_issues(const DisplayValidationReport &report, const char *label)
{
    if (report.issueCount != 0U)
    {
        const DisplayValidationIssue &issue = report.issues[0];
        const char *rect_name = (issue.rectIndex >= 0 && (uint16_t)issue.rectIndex < report.rectCount && report.rects[issue.rectIndex].name != nullptr)
            ? report.rects[issue.rectIndex].name
            : "unknown";
        const char *other_name = (issue.otherRectIndex >= 0 && (uint16_t)issue.otherRectIndex < report.rectCount && report.rects[issue.otherRectIndex].name != nullptr)
            ? report.rects[issue.otherRectIndex].name
            : "none";
        throw std::runtime_error(std::string(label) + " should have no layout issues; first issue rect=" + rect_name + " other=" + other_name);
    }
}

void write_pbm(const std::filesystem::path &path, const uint8_t *framebuffer)
{
    std::ofstream output(path, std::ios::binary);
    output << "P1\n128 64\n";
    for (uint32_t y = 0U; y < 64U; ++y)
    {
        for (uint32_t x = 0U; x < 128U; ++x)
        {
            const uint8_t pixel = (framebuffer[(y / 8U) * 128U + x] >> (y & 0x7U)) & 0x1U;
            output << (pixel != 0U ? '1' : '0');
            if (x + 1U < 128U)
            {
                output << ' ';
            }
        }
        output << '\n';
    }
}

template <typename Snapshot>
void validate_snapshot(const Snapshot &snapshot, const char *label)
{
    require_no_layout_issues(snapshot.validation, label);
    require_true(lit_pixel_count(snapshot.framebuffer, 128U * 8U) > 0U, (std::string(label) + " should draw pixels").c_str());
}

}

int main(int argc, char **argv)
{
    (void)argc;

    const std::filesystem::path exe_path = std::filesystem::absolute(argv[0]);
    const std::filesystem::path output_dir = exe_path.parent_path() / "display-sim";
    std::filesystem::create_directories(output_dir);

    PersistedFirmwareConfig config = make_default_persisted_config();
    config.loadCell.source = (uint8_t)LoadCellSourceKind::Hx711;
    ConfigRuntimeState config_state = {};
    keyswitch::MotionInputs inputs = {};
    inputs.loadCellRaw = 1200U;
    keyswitch::MotionState state = {};
    state.homingState = keyswitch::HomingState::Done;
    state.homed = 1U;
    state.holdEnabled = 1U;
    state.pressTargetPosition = 2500;
    state.completedCycles = 3U;
    state.cycleCountRemaining = 1U;
    state.seekSteps = 42U;
    state.backoffStepsRemaining = 7U;
    state.lastStopSource = keyswitch::StopSource::LoadCell;
    state.probeContactPosition = 1234;
    keyswitch::MotionOutputs outputs = {};
    outputs.loadCellTriggered = 1U;

    BootPanelRenderSnapshot boot_snapshot = {};
    boot_panel_splash_host_render("KEYSWITCH BOOT", "AUTO START APP", &boot_snapshot);
    validate_snapshot(boot_snapshot, "boot splash");
    write_pbm(output_dir / "boot_auto_start_app.pbm", boot_snapshot.framebuffer);

    struct ScreenCase {
        Mini12864HostScreen screen;
        const char *name;
    };

    const ScreenCase screens[] = {
        {Mini12864HostScreen::Splash, "app_splash"},
        {Mini12864HostScreen::DashboardLoad, "dashboard_load"},
        {Mini12864HostScreen::DashboardMotion, "dashboard_motion"},
        {Mini12864HostScreen::DashboardDriver, "dashboard_driver"},
        {Mini12864HostScreen::DashboardConfig, "dashboard_config"},
        {Mini12864HostScreen::RootMenu, "menu_root"},
        {Mini12864HostScreen::MotionMenu, "menu_motion"},
        {Mini12864HostScreen::MeasureMenu, "menu_measure"},
        {Mini12864HostScreen::DriverMenu, "menu_driver"},
        {Mini12864HostScreen::ConfigMenu, "menu_config"},
    };

    for (const ScreenCase &screen_case : screens)
    {
        Mini12864RenderSnapshot snapshot = {};
        mini12864_display_host_render(screen_case.screen, inputs, state, outputs, config, config_state, &snapshot);
        validate_snapshot(snapshot, screen_case.name);
        write_pbm(output_dir / (std::string(screen_case.name) + ".pbm"), snapshot.framebuffer);
    }

    return 0;
}