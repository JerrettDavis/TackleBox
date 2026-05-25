#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

#include "keyswitch_tmc2209.h"

enum class GpioPortId : uint8_t {
    None = 0,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
};

struct PinAssignment {
    uint8_t portId;
    uint8_t pin;
};

static const uint8_t MOTION_CHANNEL_CAPACITY = 16U;
static const uint8_t MOTION_CHANNEL_LABEL_LENGTH = 16U;

enum class MotionChannelTransportKind : uint8_t {
    LocalGpio = 0,
    RemoteBus = 1,
    Virtual = 2,
};

enum class LoadCellSourceKind : uint8_t {
    Simulation = 0,
    Hx711 = 1,
    AnalogAdc = 2,
};

enum class LoadCellConnectorKind : uint8_t {
    Custom = 0,
    Skr2Bltouch = 1,
    Skr2Det = 2,
    Skr2Th1 = 3,
    Skr2Th0 = 4,
    Skr2Tb = 5,
};

struct MotionChannelPins {
    PinAssignment uart;
    PinAssignment dir;
    PinAssignment step;
    PinAssignment enable;
    PinAssignment stop;
};

struct MotionChannelConfig {
    char label[MOTION_CHANNEL_LABEL_LENGTH];
    uint8_t enabled;
    uint8_t transportKind;
    uint8_t transportIndex;
    uint8_t transportAddress;
    MotionChannelPins pins;
    uint8_t stopSignalActiveHigh;
    uint8_t dirInverted;
    uint8_t enableActiveLow;
    uint8_t homeTowardsPositive;
    uint32_t stepsPerRotation;
    uint32_t travelUmPerRotation;
    int32_t travelMinUm;
    uint32_t travelLimitUm;
    uint32_t defaultPressUm;
    uint32_t homeFeedrateMmPerMin;
    uint32_t moveFeedrateMmPerMin;
    uint32_t seekStepLimit;
    keyswitch::Tmc2209Config tmc2209;
};

struct SharedRuntimePins {
    PinAssignment diag0;
    PinAssignment diag2;
    PinAssignment psOn;
    PinAssignment safePower;
    PinAssignment led;
};

struct LoadCellPins {
    PinAssignment data;
    PinAssignment clock;
};

struct LoadCellConfig {
    uint8_t source;
    uint8_t connector;
    uint16_t reserved1;
    LoadCellPins pins;
    uint32_t threshold;
};

struct PersistedFirmwareConfig {
    uint8_t motionChannelCount;
    uint8_t activeMotionChannel;
    uint8_t allowUnverifiedTmcMotion;
    uint8_t reserved0;
    MotionChannelConfig motionChannels[MOTION_CHANNEL_CAPACITY];
    SharedRuntimePins pins;
    LoadCellConfig loadCell;
    uint16_t stopDebounceCount;
    uint16_t backoffSteps;
    uint16_t stepIntervalUs;
    uint16_t stepPulseWidthUs;
    uint16_t tmcUartBitUs;
    uint32_t statusIntervalMs;
    uint32_t heartbeatIntervalMs;
    uint32_t bootHostWaitMs;
};

inline uint8_t motion_channel_index_valid(uint8_t index)
{
    return index < MOTION_CHANNEL_CAPACITY ? 1U : 0U;
}

inline MotionChannelConfig &motion_channel(PersistedFirmwareConfig &config, uint8_t index)
{
    return config.motionChannels[(index < MOTION_CHANNEL_CAPACITY) ? index : 0U];
}

inline const MotionChannelConfig &motion_channel(const PersistedFirmwareConfig &config, uint8_t index)
{
    return config.motionChannels[(index < MOTION_CHANNEL_CAPACITY) ? index : 0U];
}

inline MotionChannelConfig &active_motion_channel(PersistedFirmwareConfig &config)
{
    return motion_channel(config, config.activeMotionChannel);
}

inline const MotionChannelConfig &active_motion_channel(const PersistedFirmwareConfig &config)
{
    return motion_channel(config, config.activeMotionChannel);
}

enum class ConfigBootSource : uint8_t {
    Defaults = 0,
    Flash = 1,
    UsbHost = 2,
    MicroSd = 3,
};

struct StoredFirmwareConfig {
    uint32_t magic;
    uint16_t version;
    uint16_t length;
    uint32_t checksum;
    PersistedFirmwareConfig config;
};

#endif