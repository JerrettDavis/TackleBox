#ifndef KEYSWITCH_PROTOCOL_H
#define KEYSWITCH_PROTOCOL_H

#include <stdint.h>

namespace keyswitch {

enum class CommandValueUnit : uint8_t {
    NativeSteps = 0,
    Millimeters,
};

enum class CommandType : uint8_t {
    None = 0,
    Status,
    Safety,
    Config,
    SetConfig,
    SaveConfig,
    ResetConfig,
    Reboot,
    Bootloader,
    Boot,
    Enable,
    Disable,
    Hold,
    Driver,
    RunCurrent,
    HoldCurrent,
    HoldDelay,
    StallThreshold,
    MoveAbsolute,
    MoveRelative,
    SetPosition,
    Cycle,
    PressTarget,
    SimLoad,
    SimThreshold,
    SimMechanical,
    SimStall,
    SimClear,
    Home,
    Stop,
    Backoff,
    Help,
    Unknown,
};

struct Command {
    CommandType type;
    int32_t value;
    uint8_t hasValue;
    CommandValueUnit valueUnit;
    char key[32];
    char text[32];
};

Command parseCommand(const char *text);
const char *commandHelpText();

} // namespace keyswitch

#endif