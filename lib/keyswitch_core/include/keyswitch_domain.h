#ifndef KEYSWITCH_DOMAIN_H
#define KEYSWITCH_DOMAIN_H

#include <stdint.h>

namespace keyswitch {

enum class HomingState : uint8_t {
    Seek = 0,
    Backoff = 1,
    Done = 2,
    Fault = 3,
    MoveToTarget = 4,
    CycleToPress = 5,
    CycleToHome = 6,
};

enum class StopSource : uint8_t {
    None = 0,
    LoadCell = 1,
    MechanicalFallback = 2,
    StallGuard = 3,
    SeekLimitFault = 4,
    TravelFault = 5,
};

struct MotionConfig {
    uint32_t seekStepLimit;
    int32_t minPosition;
    int32_t maxPosition;
    uint32_t debounceCount;
    uint32_t backoffSteps;
    uint32_t statusIntervalMs;
    uint32_t heartbeatIntervalMs;
};

struct RuntimeConfig {
    uint8_t stopSignalActiveHigh;
    uint8_t invertXDir;
    uint8_t homeTowardsPositive;
};

struct MotionInputs {
    uint8_t rawDiag0;
    uint8_t rawXStop;
    uint8_t rawDiag2;
    uint8_t loadCellTriggered;
    uint8_t mechanicalFallbackTriggered;
    uint8_t stallDetected;
    uint8_t stepIssued;
    uint32_t loadCellRaw;
    uint32_t nowMs;
};

struct MotionState {
    HomingState homingState;
    uint32_t backoffStepsRemaining;
    uint32_t seekSteps;
    uint32_t stopDebounce;
    uint8_t faultLatch;
    uint32_t lastStatusMs;
    uint32_t lastHeartbeatMs;
    uint32_t heartbeatCount;
    uint8_t requireReleaseBeforeSeek;
    uint8_t homed;
    uint8_t holdEnabled;
    int32_t currentPosition;
    int32_t targetPosition;
    int32_t pressTargetPosition;
    uint32_t cycleCountRemaining;
    uint32_t completedCycles;
    StopSource lastStopSource;
};

struct MotionOutputs {
    uint8_t driverEnable;
    uint8_t dirHigh;
    uint8_t issueStep;
    uint8_t ledOn;
    uint8_t xStopPressed;
    uint8_t xStopConfirmed;
    uint8_t loadCellTriggered;
    uint8_t mechanicalFallbackTriggered;
    uint8_t stallDetected;
    uint8_t homed;
    uint8_t holdEnabled;
    uint8_t emitStatus;
    uint8_t emitHeartbeat;
    StopSource stopSource;
    const char *eventMessage;
};

uint8_t xStopPressedFromRaw(uint8_t rawLevel, uint8_t stopSignalActiveHigh);
MotionState makeInitialState(uint32_t nowMs);
void resetForHome(MotionState *state, uint32_t nowMs);
void forceStop(MotionState *state);
void startBackoff(MotionState *state, uint32_t nowMs, const MotionConfig &config);
void setHoldEnabled(MotionState *state, uint8_t enabled);
void setCurrentPosition(MotionState *state, int32_t position);
uint8_t queueAbsoluteMove(MotionState *state, int32_t position, const MotionConfig &config);
uint8_t queueRelativeMove(MotionState *state, int32_t delta, const MotionConfig &config);
uint8_t setPressTarget(MotionState *state, int32_t position, const MotionConfig &config);
uint8_t startCycleRoutine(MotionState *state, uint32_t count, const MotionConfig &config);
MotionOutputs tickMotion(
    MotionState *state,
    const MotionInputs &inputs,
    const MotionConfig &config,
    const RuntimeConfig &runtime);

} // namespace keyswitch

#endif