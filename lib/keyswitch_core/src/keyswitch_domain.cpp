#include "keyswitch_domain.h"

namespace keyswitch {

namespace {

const char *const EVENT_SWITCH_HIT = "homing: switch hit, backing off\r\n";
const char *const EVENT_LOAD_CELL_HIT = "homing: load cell threshold hit, backing off\r\n";
const char *const EVENT_MECH_HIT = "homing: mechanical fallback hit, backing off\r\n";
const char *const EVENT_STALL_HIT = "homing: stall detected, backing off\r\n";
const char *const EVENT_COMPLETE = "homing: complete\r\n";
const char *const EVENT_FAULT = "homing: fault, switch not found\r\n";
const char *const EVENT_WAIT_RELEASE = "homing: waiting for switch release\r\n";
const char *const EVENT_MOVE_COMPLETE = "move: target reached\r\n";
const char *const EVENT_CYCLE_COMPLETE = "cycle: complete\r\n";
const char *const EVENT_MOVE_FAULT = "move: safety stop triggered\r\n";

static uint8_t within_workspace(int32_t position, const MotionConfig &config)
{
    return ((position >= config.minPosition) && (position <= config.maxPosition)) ? 1U : 0U;
}

static uint8_t moving_towards_positive(const MotionState &state)
{
    return (state.targetPosition > state.currentPosition) ? 1U : 0U;
}

static uint8_t direction_to_dir_high(uint8_t towards_positive, const RuntimeConfig &runtime)
{
    return (towards_positive != 0U) ? runtime.invertXDir : ((runtime.invertXDir == 0U) ? 1U : 0U);
}

static uint8_t homing_seek_dir_high(const RuntimeConfig &runtime)
{
    return direction_to_dir_high(runtime.homeTowardsPositive, runtime);
}

static uint8_t homing_backoff_dir_high(const RuntimeConfig &runtime)
{
    return direction_to_dir_high((runtime.homeTowardsPositive == 0U) ? 1U : 0U, runtime);
}

static void stop_active_motion(MotionState *state)
{
    state->targetPosition = state->currentPosition;
    state->cycleCountRemaining = 0U;
    state->homingState = HomingState::Done;
}

} // namespace

uint8_t xStopPressedFromRaw(uint8_t rawLevel, uint8_t stopSignalActiveHigh)
{
    return stopSignalActiveHigh ? (rawLevel != 0U) : (rawLevel == 0U);
}

MotionState makeInitialState(uint32_t nowMs)
{
    MotionState state = {};
    state.homingState = HomingState::Seek;
    state.lastStatusMs = nowMs;
    state.lastHeartbeatMs = nowMs;
    state.holdEnabled = 0U;
    state.pressTargetPosition = 250;
    state.requireReleaseBeforeSeek = 0U;
    state.lastStopSource = StopSource::None;
    return state;
}

void resetForHome(MotionState *state, uint32_t nowMs)
{
    if (state == 0)
    {
        return;
    }

    state->homingState = HomingState::Seek;
    state->backoffStepsRemaining = 0U;
    state->seekSteps = 0U;
    state->stopDebounce = 0U;
    state->faultLatch = 0U;
    state->lastStatusMs = nowMs;
    state->lastHeartbeatMs = nowMs;
    state->requireReleaseBeforeSeek = 1U;
    state->homed = 0U;
    state->targetPosition = state->currentPosition;
    state->cycleCountRemaining = 0U;
    state->lastStopSource = StopSource::None;
}

void forceStop(MotionState *state)
{
    if (state == 0)
    {
        return;
    }

    stop_active_motion(state);
    state->backoffStepsRemaining = 0U;
    state->stopDebounce = 0U;
    state->requireReleaseBeforeSeek = 0U;
    state->lastStopSource = StopSource::None;
}

void startBackoff(MotionState *state, uint32_t nowMs, const MotionConfig &config)
{
    if (state == 0)
    {
        return;
    }

    state->homingState = HomingState::Backoff;
    state->backoffStepsRemaining = config.backoffSteps;
    state->stopDebounce = 0U;
    state->lastStatusMs = nowMs;
    state->requireReleaseBeforeSeek = 0U;
    state->cycleCountRemaining = 0U;
    state->lastStopSource = StopSource::MechanicalFallback;
}

void setHoldEnabled(MotionState *state, uint8_t enabled)
{
    if (state == 0)
    {
        return;
    }

    state->holdEnabled = (enabled != 0U) ? 1U : 0U;

    if (state->holdEnabled == 0U)
    {
        stop_active_motion(state);
    }
}

void setCurrentPosition(MotionState *state, int32_t position)
{
    if (state == 0)
    {
        return;
    }

    state->currentPosition = position;
    state->targetPosition = position;
    state->homed = 1U;
    if (state->faultLatch == 0U)
    {
        state->homingState = HomingState::Done;
    }
}

uint8_t queueAbsoluteMove(MotionState *state, int32_t position, const MotionConfig &config)
{
    if ((state == 0) || (state->homed == 0U) || (state->faultLatch != 0U) || (within_workspace(position, config) == 0U))
    {
        return 0U;
    }

    state->cycleCountRemaining = 0U;
    state->targetPosition = position;
    state->homingState = (state->targetPosition == state->currentPosition) ? HomingState::Done : HomingState::MoveToTarget;
    return 1U;
}

uint8_t queueRelativeMove(MotionState *state, int32_t delta, const MotionConfig &config)
{
    if (state == 0)
    {
        return 0U;
    }

    return queueAbsoluteMove(state, state->currentPosition + delta, config);
}

uint8_t setPressTarget(MotionState *state, int32_t position, const MotionConfig &config)
{
    if ((state == 0) || (within_workspace(position, config) == 0U))
    {
        return 0U;
    }

    state->pressTargetPosition = position;
    return 1U;
}

uint8_t startCycleRoutine(MotionState *state, uint32_t count, const MotionConfig &config)
{
    if ((state == 0) || (state->homed == 0U) || (state->faultLatch != 0U) || (count == 0U) || (setPressTarget(state, state->pressTargetPosition, config) == 0U))
    {
        return 0U;
    }

    state->cycleCountRemaining = count;
    state->targetPosition = state->pressTargetPosition;
    state->homingState = (state->targetPosition == state->currentPosition) ? HomingState::CycleToHome : HomingState::CycleToPress;
    return 1U;
}

MotionOutputs tickMotion(
    MotionState *state,
    const MotionInputs &inputs,
    const MotionConfig &config,
    const RuntimeConfig &runtime)
{
    MotionOutputs outputs = {};
    const uint8_t xStopPressed = xStopPressedFromRaw(inputs.rawXStop, runtime.stopSignalActiveHigh);

    outputs.xStopPressed = xStopPressed;
    outputs.loadCellTriggered = inputs.loadCellTriggered;
    outputs.mechanicalFallbackTriggered = inputs.mechanicalFallbackTriggered;
    outputs.stallDetected = inputs.stallDetected;
    outputs.homed = state->homed;
    outputs.holdEnabled = state->holdEnabled;
    outputs.stopSource = state->lastStopSource;

    if (xStopPressed)
    {
        if (state->stopDebounce < config.debounceCount)
        {
            ++state->stopDebounce;
        }
    }
    else
    {
        state->stopDebounce = 0U;
        if (state->requireReleaseBeforeSeek != 0U)
        {
            state->requireReleaseBeforeSeek = 0U;
        }
    }

    outputs.xStopConfirmed = (state->stopDebounce >= config.debounceCount) ? 1U : 0U;

    switch (state->homingState)
    {
    case HomingState::Seek:
        outputs.dirHigh = homing_seek_dir_high(runtime);
        outputs.driverEnable = 1U;

        if ((state->requireReleaseBeforeSeek != 0U) && (xStopPressed != 0U))
        {
            outputs.eventMessage = EVENT_WAIT_RELEASE;
            outputs.issueStep = 0U;
        }
        else if (inputs.loadCellTriggered != 0U)
        {
            state->homingState = HomingState::Backoff;
            state->backoffStepsRemaining = config.backoffSteps;
            state->stopDebounce = 0U;
            state->lastStopSource = StopSource::LoadCell;
            outputs.stopSource = state->lastStopSource;
            outputs.eventMessage = EVENT_LOAD_CELL_HIT;
        }
        else if (inputs.stallDetected != 0U)
        {
            state->homingState = HomingState::Backoff;
            state->backoffStepsRemaining = config.backoffSteps;
            state->stopDebounce = 0U;
            state->lastStopSource = StopSource::StallGuard;
            outputs.stopSource = state->lastStopSource;
            outputs.eventMessage = EVENT_STALL_HIT;
        }
        else if ((inputs.mechanicalFallbackTriggered != 0U) || (outputs.xStopConfirmed != 0U))
        {
            state->homingState = HomingState::Backoff;
            state->backoffStepsRemaining = config.backoffSteps;
            state->stopDebounce = 0U;
            state->lastStopSource = StopSource::MechanicalFallback;
            outputs.stopSource = state->lastStopSource;
            outputs.eventMessage = (outputs.xStopConfirmed != 0U) ? EVENT_SWITCH_HIT : EVENT_MECH_HIT;
        }
        else
        {
            if (inputs.stepIssued != 0U)
            {
                ++state->seekSteps;
                if (state->seekSteps > config.seekStepLimit)
                {
                    state->homingState = HomingState::Fault;
                    state->faultLatch = 1U;
                    state->lastStopSource = StopSource::SeekLimitFault;
                    outputs.driverEnable = 0U;
                    outputs.issueStep = 0U;
                    outputs.stopSource = state->lastStopSource;
                    outputs.eventMessage = EVENT_FAULT;
                }
            }

            if (state->homingState == HomingState::Seek)
            {
                outputs.issueStep = 1U;
            }
        }
        break;

    case HomingState::Backoff:
        outputs.driverEnable = 1U;
        outputs.dirHigh = homing_backoff_dir_high(runtime);

        if ((inputs.stepIssued != 0U) && (state->backoffStepsRemaining > 0U))
        {
            --state->backoffStepsRemaining;
        }

        if (state->backoffStepsRemaining > 0U)
        {
            outputs.issueStep = 1U;
        }
        else
        {
            state->homingState = HomingState::Done;
            state->faultLatch = 0U;
            state->homed = 1U;
            state->currentPosition = 0;
            state->targetPosition = 0;
            outputs.driverEnable = state->holdEnabled;
            outputs.stopSource = state->lastStopSource;
            outputs.eventMessage = EVENT_COMPLETE;
        }
        break;

    case HomingState::Done:
        outputs.driverEnable = state->holdEnabled;
        outputs.dirHigh = homing_seek_dir_high(runtime);
        break;

    case HomingState::MoveToTarget:
    case HomingState::CycleToPress:
    case HomingState::CycleToHome:
    {
        const uint8_t towards_positive = moving_towards_positive(*state);
        outputs.driverEnable = 1U;
        outputs.dirHigh = direction_to_dir_high(towards_positive, runtime);

        if (inputs.stallDetected != 0U)
        {
            state->homingState = HomingState::Fault;
            state->faultLatch = 1U;
            state->lastStopSource = StopSource::StallGuard;
            outputs.driverEnable = 0U;
            outputs.issueStep = 0U;
            outputs.stopSource = state->lastStopSource;
            outputs.eventMessage = EVENT_MOVE_FAULT;
            break;
        }

        if ((state->homingState == HomingState::MoveToTarget) &&
            ((inputs.loadCellTriggered != 0U) || (inputs.mechanicalFallbackTriggered != 0U) || (outputs.xStopConfirmed != 0U)))
        {
            state->homingState = HomingState::Fault;
            state->faultLatch = 1U;
            state->lastStopSource = (inputs.loadCellTriggered != 0U) ? StopSource::LoadCell : StopSource::MechanicalFallback;
            outputs.driverEnable = 0U;
            outputs.issueStep = 0U;
            outputs.stopSource = state->lastStopSource;
            outputs.eventMessage = EVENT_MOVE_FAULT;
            break;
        }

        if (inputs.stepIssued != 0U)
        {
            state->currentPosition += (towards_positive != 0U) ? 1 : -1;
        }

        if (state->currentPosition == state->targetPosition)
        {
            if (state->homingState == HomingState::MoveToTarget)
            {
                state->homingState = HomingState::Done;
                outputs.driverEnable = state->holdEnabled;
                outputs.eventMessage = EVENT_MOVE_COMPLETE;
            }
            else if (state->homingState == HomingState::CycleToPress)
            {
                state->targetPosition = 0;
                state->homingState = HomingState::CycleToHome;
            }
            else
            {
                if (state->cycleCountRemaining > 0U)
                {
                    --state->cycleCountRemaining;
                    ++state->completedCycles;
                }

                if (state->cycleCountRemaining > 0U)
                {
                    state->targetPosition = state->pressTargetPosition;
                    state->homingState = HomingState::CycleToPress;
                }
                else
                {
                    state->homingState = HomingState::Done;
                    outputs.driverEnable = state->holdEnabled;
                    outputs.eventMessage = EVENT_CYCLE_COMPLETE;
                }
            }
        }
        else if ((state->homingState == HomingState::CycleToPress) &&
                 ((inputs.loadCellTriggered != 0U) || (inputs.mechanicalFallbackTriggered != 0U) || (outputs.xStopConfirmed != 0U)))
        {
            state->lastStopSource = (inputs.loadCellTriggered != 0U) ? StopSource::LoadCell : StopSource::MechanicalFallback;
            outputs.stopSource = state->lastStopSource;
            state->targetPosition = 0;
            state->homingState = HomingState::CycleToHome;
        }

        if ((state->homingState == HomingState::MoveToTarget) || (state->homingState == HomingState::CycleToPress) || (state->homingState == HomingState::CycleToHome))
        {
            outputs.issueStep = 1U;
        }
        break;
    }

    case HomingState::Fault:
    default:
        outputs.driverEnable = 0U;
        outputs.dirHigh = runtime.invertXDir;
        break;
    }

    outputs.ledOn = (state->faultLatch != 0U) || (xStopPressed != 0U);

    if (((state->homingState == HomingState::Seek) || (state->homingState == HomingState::Backoff)) &&
        ((inputs.nowMs - state->lastStatusMs) >= config.statusIntervalMs))
    {
        state->lastStatusMs = inputs.nowMs;
        outputs.emitStatus = 1U;
    }

    if ((inputs.nowMs - state->lastHeartbeatMs) >= config.heartbeatIntervalMs)
    {
        state->lastHeartbeatMs = inputs.nowMs;
        ++state->heartbeatCount;
        outputs.emitHeartbeat = 1U;
    }

    return outputs;
}

} // namespace keyswitch