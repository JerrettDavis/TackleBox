#include <cstring>
#include <stdexcept>
#include <string>

#include "app_board.h"
#include "app_runtime_config.h"
#include "app_runtime_config_test_support.h"

namespace {

struct LegacyLoadCellConfigV10 {
    uint8_t source;
    uint8_t connector;
    uint8_t hx711ReleaseRate;
    uint8_t hx711RiseRate;
    LoadCellPins pins;
    uint32_t threshold;
};

struct LegacyPersistedFirmwareConfigV10 {
    uint8_t motionChannelCount;
    uint8_t activeMotionChannel;
    uint8_t allowUnverifiedTmcMotion;
    uint8_t reserved0;
    MotionChannelConfig motionChannels[MOTION_CHANNEL_CAPACITY];
    SharedRuntimePins pins;
    LegacyLoadCellConfigV10 loadCell;
    uint16_t stopDebounceCount;
    uint16_t backoffSteps;
    uint16_t stepIntervalUs;
    uint16_t stepPulseWidthUs;
    uint16_t tmcUartBitUs;
    uint32_t statusIntervalMs;
    uint32_t heartbeatIntervalMs;
    uint32_t bootHostWaitMs;
    uint8_t panelColorRed;
    uint8_t panelColorGreen;
    uint8_t panelColorBlue;
    uint8_t reserved1;
};

struct LegacyStoredFirmwareConfigV10 {
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t checksum;
    LegacyPersistedFirmwareConfigV10 config;
};

void require_true(bool value, const char *message)
{
    if (!value)
    {
        throw std::runtime_error(message);
    }
}

void test_default_config_is_valid_and_has_unset_calibration(void)
{
    const PersistedFirmwareConfig config = make_default_persisted_config();

    require_true(persisted_config_valid(config) != 0U, "default persisted config should be valid");
    require_true(config.loadCell.calibrationRaw == 0U, "default calibration raw should be unset");
    require_true(config.loadCell.calibrationGrams == 0U, "default calibration grams should be unset");
    require_true(config.motionChannels[0].homeTowardsPositive == 1U, "default X homing direction should remain toward positive");
}

void test_calibration_requires_both_raw_and_grams(void)
{
    PersistedFirmwareConfig config = make_default_persisted_config();

    config.loadCell.calibrationRaw = 1234U;
    require_true(persisted_config_valid(config) == 0U, "calibration raw without grams should be rejected");

    config = make_default_persisted_config();
    config.loadCell.calibrationGrams = 250U;
    require_true(persisted_config_valid(config) == 0U, "calibration grams without raw should be rejected");

    config = make_default_persisted_config();
    config.loadCell.source = (uint8_t)LoadCellSourceKind::Hx711;
    config.loadCell.pins.data = {(uint8_t)GpioPortId::E, 4U};
    config.loadCell.pins.clock = {(uint8_t)GpioPortId::E, 5U};
    config.loadCell.calibrationRaw = 2048U;
    config.loadCell.calibrationGrams = 100U;
    require_true(persisted_config_valid(config) != 0U, "complete calibration pair should be accepted");
}

void test_config_save_and_load_round_trip_preserves_calibration(void)
{
    keyswitch_test_reset_runtime_config_host_state();

    PersistedFirmwareConfig config = make_default_persisted_config();
    config.loadCell.source = (uint8_t)LoadCellSourceKind::Hx711;
    config.loadCell.pins.data = {(uint8_t)GpioPortId::E, 4U};
    config.loadCell.pins.clock = {(uint8_t)GpioPortId::E, 5U};
    config.loadCell.calibrationRaw = 3210U;
    config.loadCell.calibrationGrams = 250U;
    config.motionChannels[0].travelMinUm = -1500;
    config.motionChannels[0].travelLimitUm = 20000U;
    config.motionChannels[0].defaultPressUm = 625U;
    config.motionChannels[0].seekStepLimit = axis_steps_from_um(config, config.motionChannels[0].travelLimitUm);

    require_true(save_persisted_config(config) != 0U, "save_persisted_config should succeed against host flash stub");

    PersistedFirmwareConfig loaded = {};
    require_true(load_persisted_config(&loaded) != 0U, "load_persisted_config should succeed after host flash save");
    require_true(loaded.loadCell.calibrationRaw == 3210U, "loaded calibration raw should match saved value");
    require_true(loaded.loadCell.calibrationGrams == 250U, "loaded calibration grams should match saved value");
    require_true(loaded.motionChannels[0].travelMinUm == -1500, "signed workspace minimum should survive persisted round-trip");
}

void test_load_persisted_config_migrates_v10_calibration_defaults(void)
{
    keyswitch_test_reset_runtime_config_host_state();

    const PersistedFirmwareConfig current = make_default_persisted_config();

    LegacyStoredFirmwareConfigV10 legacy = {};
    legacy.magic = 0x4B535743UL;
    legacy.version = 10U;
    legacy.length = sizeof(LegacyPersistedFirmwareConfigV10);
    legacy.config.motionChannelCount = current.motionChannelCount;
    legacy.config.activeMotionChannel = current.activeMotionChannel;
    legacy.config.allowUnverifiedTmcMotion = current.allowUnverifiedTmcMotion;
    std::memcpy(legacy.config.motionChannels, current.motionChannels, sizeof(legacy.config.motionChannels));
    legacy.config.pins = current.pins;
    legacy.config.loadCell.source = (uint8_t)LoadCellSourceKind::Hx711;
    legacy.config.loadCell.connector = (uint8_t)LoadCellConnectorKind::Custom;
    legacy.config.loadCell.hx711ReleaseRate = 2U;
    legacy.config.loadCell.hx711RiseRate = 4U;
    legacy.config.loadCell.pins.data = {(uint8_t)GpioPortId::E, 4U};
    legacy.config.loadCell.pins.clock = {(uint8_t)GpioPortId::E, 5U};
    legacy.config.loadCell.threshold = 1500U;
    legacy.config.stopDebounceCount = current.stopDebounceCount;
    legacy.config.backoffSteps = current.backoffSteps;
    legacy.config.stepIntervalUs = current.stepIntervalUs;
    legacy.config.stepPulseWidthUs = current.stepPulseWidthUs;
    legacy.config.tmcUartBitUs = current.tmcUartBitUs;
    legacy.config.statusIntervalMs = current.statusIntervalMs;
    legacy.config.heartbeatIntervalMs = current.heartbeatIntervalMs;
    legacy.config.bootHostWaitMs = current.bootHostWaitMs;
    legacy.config.panelColorRed = current.panelColorRed;
    legacy.config.panelColorGreen = current.panelColorGreen;
    legacy.config.panelColorBlue = current.panelColorBlue;
    legacy.config.reserved1 = current.reserved1;
    legacy.checksum = checksum32(&legacy.config, sizeof(legacy.config));

    std::memcpy(reinterpret_cast<void *>(keyswitch_test_config_flash_storage_address()), &legacy, sizeof(legacy));

    PersistedFirmwareConfig loaded = {};
    require_true(load_persisted_config(&loaded) != 0U, "v10 persisted config should migrate successfully");
    require_true(loaded.loadCell.calibrationRaw == 0U, "v10 migration should initialize calibration raw to zero");
    require_true(loaded.loadCell.calibrationGrams == 0U, "v10 migration should initialize calibration grams to zero");
    require_true(loaded.loadCell.threshold == 1500U, "v10 migration should preserve existing load-cell threshold");
}

void test_emit_config_summary_includes_calibration_lines(void)
{
    keyswitch_test_reset_runtime_config_host_state();

    PersistedFirmwareConfig config = make_default_persisted_config();
    config.loadCell.calibrationRaw = 4321U;
    config.loadCell.calibrationGrams = 200U;

    ConfigRuntimeState state = {};
    emit_config_summary(config, state);

    const std::string output = keyswitch_test_usb_output();
    require_true(output.find("config loadcell.calibration_raw=4321\r\n") != std::string::npos, "config summary should emit loadcell calibration raw");
    require_true(output.find("config loadcell.calibration_grams=200\r\n") != std::string::npos, "config summary should emit loadcell calibration grams");
}

}  // namespace

int main()
{
    test_default_config_is_valid_and_has_unset_calibration();
    test_calibration_requires_both_raw_and_grams();
    test_config_save_and_load_round_trip_preserves_calibration();
    test_load_persisted_config_migrates_v10_calibration_defaults();
    test_emit_config_summary_includes_calibration_lines();
    return 0;
}