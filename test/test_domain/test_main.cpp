#include <stdexcept>
#include <iostream>

#include "keyswitch_domain.h"

namespace {

keyswitch::MotionConfig make_config()
{
    keyswitch::MotionConfig config = {};
    config.seekStepLimit = 10U;
    config.probeContactThresholdRaw = 100U;
    config.minPosition = 0;
    config.maxPosition = 10;
    config.debounceCount = 3U;
    config.backoffSteps = 5U;
    config.statusIntervalMs = 1000U;
    config.heartbeatIntervalMs = 1000U;
    return config;
}

keyswitch::RuntimeConfig make_runtime()
{
    keyswitch::RuntimeConfig runtime = {};
    runtime.stopSignalActiveHigh = 0U;
    runtime.invertXDir = 1U;
    runtime.homeTowardsPositive = 0U;
    return runtime;
}

keyswitch::MotionInputs make_inputs(uint32_t nowMs, uint8_t rawXStop)
{
    keyswitch::MotionInputs inputs = {};
    inputs.rawDiag0 = 1U;
    inputs.rawXStop = rawXStop;
    inputs.rawDiag2 = 1U;
    inputs.loadCellTriggered = 0U;
    inputs.mechanicalFallbackTriggered = 0U;
    inputs.stallDetected = 0U;
    inputs.stepIssued = 1U;
    inputs.loadCellRaw = 0U;
    inputs.nowMs = nowMs;
    return inputs;
}

} // namespace

void test_domain_seeks_then_enters_backoff_on_confirmed_switch(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionOutputs outputs = {};

    outputs = keyswitch::tickMotion(&state, make_inputs(10U, 1U), config, runtime);
    if (outputs.issueStep != 1U) throw std::runtime_error("seek should step while switch is open");
    if (outputs.dirHigh != 0U) throw std::runtime_error("default homing seek should move toward the negative direction");
    if (state.homingState != keyswitch::HomingState::Seek) throw std::runtime_error("state should still be Seek");

    outputs = keyswitch::tickMotion(&state, make_inputs(20U, 0U), config, runtime);
    if (state.homingState != keyswitch::HomingState::Seek) throw std::runtime_error("first press sample should not backoff yet");
    outputs = keyswitch::tickMotion(&state, make_inputs(30U, 0U), config, runtime);
    if (state.homingState != keyswitch::HomingState::Seek) throw std::runtime_error("second press sample should not backoff yet");
    outputs = keyswitch::tickMotion(&state, make_inputs(40U, 0U), config, runtime);
    if (state.homingState != keyswitch::HomingState::Backoff) throw std::runtime_error("third press sample should enter backoff");
    if (outputs.eventMessage == 0) throw std::runtime_error("backoff transition should emit an event");

    outputs = keyswitch::tickMotion(&state, make_inputs(50U, 1U), config, runtime);
    if (outputs.dirHigh != 1U) throw std::runtime_error("backoff should reverse away from the home direction");
}

void test_domain_homing_direction_is_configurable(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    runtime.homeTowardsPositive = 1U;

    keyswitch::MotionOutputs outputs = keyswitch::tickMotion(&state, make_inputs(10U, 1U), config, runtime);
    if (outputs.dirHigh != 1U) throw std::runtime_error("positive home direction should drive the seek direction high when dir is inverted");

    outputs = keyswitch::tickMotion(&state, make_inputs(20U, 0U), config, runtime);
    outputs = keyswitch::tickMotion(&state, make_inputs(30U, 0U), config, runtime);
    outputs = keyswitch::tickMotion(&state, make_inputs(40U, 0U), config, runtime);
    if (state.homingState != keyswitch::HomingState::Backoff) throw std::runtime_error("configured positive home direction should still enter backoff");

    outputs = keyswitch::tickMotion(&state, make_inputs(50U, 1U), config, runtime);
    if (outputs.dirHigh != 0U) throw std::runtime_error("configured positive home direction should reverse low during backoff");
}

void test_domain_home_requires_release_before_reseek(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionOutputs outputs = {};

    keyswitch::resetForHome(&state, 100U);
    outputs = keyswitch::tickMotion(&state, make_inputs(110U, 0U), config, runtime);
    if (outputs.issueStep != 0U) throw std::runtime_error("home should wait for release before reseeking");
    if (state.homingState != keyswitch::HomingState::Seek) throw std::runtime_error("state should remain Seek after home reset");

    outputs = keyswitch::tickMotion(&state, make_inputs(120U, 1U), config, runtime);
    if (outputs.issueStep != 1U) throw std::runtime_error("seek should resume after switch release");
    if (state.requireReleaseBeforeSeek != 0U) throw std::runtime_error("release gate should clear after switch opens");
}

void test_domain_faults_when_switch_not_found(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionOutputs outputs = {};

    for (uint32_t i = 0U; i < 12U; ++i)
    {
        outputs = keyswitch::tickMotion(&state, make_inputs(10U + i, 1U), config, runtime);
    }

    if (state.homingState != keyswitch::HomingState::Fault) throw std::runtime_error("seek limit should fault when switch is not found");
    if (state.faultLatch != 1U) throw std::runtime_error("fault latch should set when switch is not found");
    if (outputs.driverEnable != 0U) throw std::runtime_error("driver should disable in fault state");
}

void test_domain_seek_progress_requires_accepted_steps(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionInputs inputs = make_inputs(10U, 1U);
    inputs.stepIssued = 0U;

    keyswitch::MotionOutputs outputs = keyswitch::tickMotion(&state, inputs, config, runtime);

    if (outputs.issueStep != 1U) throw std::runtime_error("seek should still request steps when idle");
    if (state.seekSteps != 0U) throw std::runtime_error("seek progress should not advance without an accepted step");
}

void test_domain_home_ignores_load_cell_trigger(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionInputs inputs = make_inputs(10U, 1U);
    inputs.loadCellTriggered = 1U;

    keyswitch::MotionOutputs outputs = keyswitch::tickMotion(&state, inputs, config, runtime);

    if (state.homingState != keyswitch::HomingState::Seek) throw std::runtime_error("load cell trigger should not interrupt homing seek");
    if (outputs.issueStep != 1U) throw std::runtime_error("homing seek should continue stepping when only the load cell is active");
    if (state.lastStopSource != keyswitch::StopSource::None) throw std::runtime_error("load cell should not be recorded as the homing stop source");
}

void test_domain_stallguard_acts_as_fallback_stop_source(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionInputs inputs = make_inputs(10U, 1U);
    inputs.stallDetected = 1U;

    keyswitch::MotionOutputs outputs = keyswitch::tickMotion(&state, inputs, config, runtime);

    if (state.homingState != keyswitch::HomingState::Backoff) throw std::runtime_error("stall detection should enter backoff");
    if (state.lastStopSource != keyswitch::StopSource::StallGuard) throw std::runtime_error("stallguard should be the recorded stop source");
    if (outputs.stopSource != keyswitch::StopSource::StallGuard) throw std::runtime_error("outputs should report stallguard stop source");
}

void test_domain_initial_idle_state_does_not_hold_driver_enabled(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();

    keyswitch::setCurrentPosition(&state, 0);

    keyswitch::MotionOutputs outputs = keyswitch::tickMotion(&state, make_inputs(10U, 1U), config, runtime);

    if (outputs.driverEnable != 0U) throw std::runtime_error("initial idle state should leave the driver disabled until hold is explicitly enabled");
    if (outputs.homed != 1U) throw std::runtime_error("setCurrentPosition should still mark the machine as homed");
}

void test_domain_done_state_holds_when_enabled(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();

    keyswitch::setCurrentPosition(&state, 0);
    keyswitch::setHoldEnabled(&state, 1U);

    keyswitch::MotionOutputs outputs = keyswitch::tickMotion(&state, make_inputs(10U, 1U), config, runtime);

    if (state.homingState != keyswitch::HomingState::Done) throw std::runtime_error("setCurrentPosition should place the machine in Done state");
    if (outputs.driverEnable != 1U) throw std::runtime_error("hold-enabled idle state should keep the driver enabled");
    if (outputs.homed != 1U) throw std::runtime_error("setCurrentPosition should mark the machine as homed");
}

void test_domain_absolute_move_tracks_position(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::setCurrentPosition(&state, 0);

    if (keyswitch::queueAbsoluteMove(&state, 3, config) != 1U) throw std::runtime_error("queueAbsoluteMove should accept an in-range target");

    for (uint32_t i = 0U; i < 4U; ++i)
    {
        keyswitch::MotionInputs inputs = make_inputs(20U + i, 1U);
        inputs.stepIssued = 1U;
        keyswitch::tickMotion(&state, inputs, config, runtime);
    }

    if (state.currentPosition != 3) throw std::runtime_error("absolute move should advance to the commanded position");
    if (state.homingState != keyswitch::HomingState::Done) throw std::runtime_error("absolute move should return to Done when complete");
}

void test_domain_backoff_clears_fault_after_move_stop(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::setCurrentPosition(&state, 0);

    if (keyswitch::queueAbsoluteMove(&state, 4, config) != 1U) throw std::runtime_error("queueAbsoluteMove should accept a small target");

    keyswitch::MotionInputs inputs = make_inputs(10U, 1U);
    inputs.mechanicalFallbackTriggered = 1U;
    keyswitch::MotionOutputs outputs = keyswitch::tickMotion(&state, inputs, config, runtime);

    if (state.homingState != keyswitch::HomingState::Fault) throw std::runtime_error("move stop should enter fault state");
    if (state.faultLatch != 1U) throw std::runtime_error("move stop should latch a fault until recovered");
    if (outputs.stopSource != keyswitch::StopSource::MechanicalFallback) throw std::runtime_error("move stop should report mechanical fallback source");

    keyswitch::startBackoff(&state, 20U, config);
    for (uint32_t i = 0U; i < config.backoffSteps + 1U; ++i)
    {
        inputs = make_inputs(20U + i, 1U);
        inputs.stepIssued = 1U;
        outputs = keyswitch::tickMotion(&state, inputs, config, runtime);
    }

    if (state.homingState != keyswitch::HomingState::Done) throw std::runtime_error("completed backoff should return to Done state");
    if (state.faultLatch != 0U) throw std::runtime_error("completed backoff should clear the recovered fault latch");
    if (state.currentPosition != 0) throw std::runtime_error("completed backoff should restore the home position");
}

void test_domain_cycle_routine_returns_home_and_counts_cycles(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::setCurrentPosition(&state, 0);
    if (keyswitch::setPressTarget(&state, 2, config) != 1U) throw std::runtime_error("setPressTarget should accept a small positive target");
    if (keyswitch::startCycleRoutine(&state, 2U, config) != 1U) throw std::runtime_error("startCycleRoutine should start when homed");

    for (uint32_t i = 0U; i < 24U; ++i)
    {
        keyswitch::MotionInputs inputs = make_inputs(40U + i, 1U);
        inputs.stepIssued = 1U;
        keyswitch::tickMotion(&state, inputs, config, runtime);
    }

    if (state.currentPosition != 0) throw std::runtime_error("cycle routine should return to home position");
    if (state.completedCycles != 2U) throw std::runtime_error("cycle routine should count completed cycles");
    if (state.homingState != keyswitch::HomingState::Done) throw std::runtime_error("cycle routine should return to Done state when complete");
}

void test_domain_cycle_latches_contact_and_keeps_pressing_until_threshold(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::setCurrentPosition(&state, config.maxPosition);
    if (keyswitch::setPressTarget(&state, 4, config) != 1U) throw std::runtime_error("setPressTarget should accept a probe travel target");
    if (keyswitch::startCycleRoutine(&state, 1U, config) != 1U) throw std::runtime_error("startCycleRoutine should start when homed");

    keyswitch::MotionInputs inputs = make_inputs(60U, 1U);
    inputs.loadCellRaw = 120U;
    keyswitch::MotionOutputs outputs = keyswitch::tickMotion(&state, inputs, config, runtime);

    if (state.probeContactActive != 1U) throw std::runtime_error("cycle press should latch first contact once raw force exceeds the probe threshold");
    if (state.probeContactPosition != config.maxPosition) throw std::runtime_error("probe contact should record the position where the first contact sample occurred");
    if (state.homingState != keyswitch::HomingState::CycleToPress) throw std::runtime_error("first contact should keep the probe pressing toward the full travel target");
    if (outputs.issueStep != 1U) throw std::runtime_error("first contact should continue requesting press steps");

    inputs = make_inputs(61U, 1U);
    inputs.loadCellRaw = 180U;
    outputs = keyswitch::tickMotion(&state, inputs, config, runtime);
    if (state.homingState != keyswitch::HomingState::CycleToPress) throw std::runtime_error("sub-threshold contact should not reverse the probe cycle");

    inputs = make_inputs(62U, 1U);
    inputs.loadCellRaw = 1000U;
    inputs.loadCellTriggered = 1U;
    outputs = keyswitch::tickMotion(&state, inputs, config, runtime);
    if (state.homingState != keyswitch::HomingState::CycleToHome) throw std::runtime_error("load-cell threshold should end the press leg and start release");
    if (outputs.stopSource != keyswitch::StopSource::LoadCell) throw std::runtime_error("threshold stop should report the load cell as the stop source");
}

void test_domain_cycle_moves_to_workspace_max_before_pressing(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::setCurrentPosition(&state, 0);
    if (keyswitch::setPressTarget(&state, 4, config) != 1U) throw std::runtime_error("setPressTarget should accept a probe travel target");
    if (keyswitch::startCycleRoutine(&state, 1U, config) != 1U) throw std::runtime_error("startCycleRoutine should start when homed");
    if (state.homingState != keyswitch::HomingState::MoveToTarget) throw std::runtime_error("cycle routine should start with an approach move toward workspace max");
    if (state.targetPosition != config.maxPosition) throw std::runtime_error("cycle approach should target the workspace maximum before probing");

    for (uint32_t i = 0U; i < 16U; ++i)
    {
        keyswitch::MotionInputs inputs = make_inputs(120U + i, 1U);
        inputs.stepIssued = 1U;
        keyswitch::tickMotion(&state, inputs, config, runtime);
        if (state.homingState == keyswitch::HomingState::CycleToPress)
        {
            break;
        }
    }

    if (state.homingState != keyswitch::HomingState::CycleToPress) throw std::runtime_error("cycle routine should enter the probe press leg after reaching the approach position");
    if (state.targetPosition != state.pressTargetPosition) throw std::runtime_error("cycle press leg should retarget the configured press position after approach");
}

void test_domain_cycle_approach_ignores_home_stop_inputs(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::setCurrentPosition(&state, 0);
    if (keyswitch::setPressTarget(&state, 4, config) != 1U) throw std::runtime_error("setPressTarget should accept a probe travel target");
    if (keyswitch::startCycleRoutine(&state, 1U, config) != 1U) throw std::runtime_error("startCycleRoutine should start when homed");

    keyswitch::MotionInputs inputs = make_inputs(140U, 0U);
    inputs.mechanicalFallbackTriggered = 1U;
    keyswitch::MotionOutputs outputs = keyswitch::tickMotion(&state, inputs, config, runtime);

    if (state.homingState != keyswitch::HomingState::MoveToTarget) throw std::runtime_error("cycle approach should keep moving away from home while home-side stop inputs are still active");
    if (state.faultLatch != 0U) throw std::runtime_error("cycle approach should not fault on home-side stop inputs");
    if (outputs.issueStep != 1U) throw std::runtime_error("cycle approach should continue stepping while home-side stop inputs clear");
    if (state.currentPosition != 1) throw std::runtime_error("cycle approach should still advance away from home");
}

void test_domain_cycle_approach_rearms_stop_inputs_after_release(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::setCurrentPosition(&state, 0);
    if (keyswitch::setPressTarget(&state, 4, config) != 1U) throw std::runtime_error("setPressTarget should accept a probe travel target");
    if (keyswitch::startCycleRoutine(&state, 1U, config) != 1U) throw std::runtime_error("startCycleRoutine should start when homed");

    keyswitch::MotionInputs inputs = make_inputs(150U, 1U);
    inputs.stepIssued = 1U;
    keyswitch::MotionOutputs outputs = keyswitch::tickMotion(&state, inputs, config, runtime);
    if (state.cycleApproachStopArmed != 1U) throw std::runtime_error("cycle approach should re-arm stop handling after home-side inputs clear");
    if (outputs.issueStep != 1U) throw std::runtime_error("cycle approach should keep stepping after stop handling rearms");

    inputs = make_inputs(151U, 0U);
    inputs.mechanicalFallbackTriggered = 1U;
    outputs = keyswitch::tickMotion(&state, inputs, config, runtime);

    if (state.homingState != keyswitch::HomingState::CycleToPress) throw std::runtime_error("cycle approach should stop and begin the press leg when a hard stop reappears after release");
    if (state.faultLatch != 0U) throw std::runtime_error("cycle approach hard stop should not latch a fault");
    if (outputs.issueStep != 0U) throw std::runtime_error("cycle approach should not issue another outward step on the stop-detection tick");
}

void test_domain_workspace_range_supports_signed_positions(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    config.minPosition = -4;
    config.maxPosition = 6;
    keyswitch::setCurrentPosition(&state, 0);

    if (keyswitch::queueAbsoluteMove(&state, -3, config) != 1U) throw std::runtime_error("queueAbsoluteMove should accept an in-range negative target");
    if (keyswitch::queueAbsoluteMove(&state, -5, config) != 0U) throw std::runtime_error("queueAbsoluteMove should reject a target below workspace min");
    if (keyswitch::queueAbsoluteMove(&state, 7, config) != 0U) throw std::runtime_error("queueAbsoluteMove should reject a target above workspace max");
    if (keyswitch::setPressTarget(&state, -2, config) != 1U) throw std::runtime_error("setPressTarget should accept an in-range negative target");
}

void test_domain_relative_move_clamps_to_workspace_bounds(void)
{
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();

    keyswitch::setCurrentPosition(&state, 8);
    if (keyswitch::queueRelativeMove(&state, -10, config) != 1U) throw std::runtime_error("queueRelativeMove should clamp instead of rejecting a negative overshoot");

    for (uint32_t i = 0U; i < 10U; ++i)
    {
        keyswitch::MotionInputs inputs = make_inputs(80U + i, 1U);
        inputs.stepIssued = 1U;
        keyswitch::tickMotion(&state, inputs, config, runtime);
    }

    if (state.currentPosition != 0) throw std::runtime_error("negative overshoot should clamp to the workspace minimum");

    if (keyswitch::queueRelativeMove(&state, 20, config) != 1U) throw std::runtime_error("queueRelativeMove should clamp instead of rejecting a positive overshoot");

    for (uint32_t i = 0U; i < 12U; ++i)
    {
        keyswitch::MotionInputs inputs = make_inputs(100U + i, 1U);
        inputs.stepIssued = 1U;
        keyswitch::tickMotion(&state, inputs, config, runtime);
    }

    if (state.currentPosition != config.maxPosition) throw std::runtime_error("positive overshoot should clamp to the workspace maximum");
}

int main()
{
    try
    {
        test_domain_seeks_then_enters_backoff_on_confirmed_switch();
        test_domain_homing_direction_is_configurable();
        test_domain_home_requires_release_before_reseek();
        test_domain_faults_when_switch_not_found();
        test_domain_seek_progress_requires_accepted_steps();
        test_domain_home_ignores_load_cell_trigger();
        test_domain_stallguard_acts_as_fallback_stop_source();
        test_domain_initial_idle_state_does_not_hold_driver_enabled();
        test_domain_done_state_holds_when_enabled();
        test_domain_absolute_move_tracks_position();
        test_domain_backoff_clears_fault_after_move_stop();
        test_domain_cycle_routine_returns_home_and_counts_cycles();
        test_domain_cycle_latches_contact_and_keeps_pressing_until_threshold();
        test_domain_cycle_moves_to_workspace_max_before_pressing();
        test_domain_cycle_approach_ignores_home_stop_inputs();
        test_domain_cycle_approach_rearms_stop_inputs_after_release();
        test_domain_workspace_range_supports_signed_positions();
        test_domain_relative_move_clamps_to_workspace_bounds();
        std::cout << "PASS test_domain" << std::endl;
        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "FAIL test_domain: " << ex.what() << std::endl;
        return 1;
    }
}