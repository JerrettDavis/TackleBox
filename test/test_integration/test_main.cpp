#include <iostream>
#include <stdexcept>

#include "keyswitch_domain.h"
#include "keyswitch_protocol.h"

namespace {

keyswitch::MotionConfig make_config()
{
    keyswitch::MotionConfig config = {};
    config.seekStepLimit = 10U;
    config.probeContactThresholdRaw = 100U;
    config.minPosition = 0;
    config.maxPosition = 10;
    config.debounceCount = 3U;
    config.backoffSteps = 3U;
    config.statusIntervalMs = 1000U;
    config.heartbeatIntervalMs = 1000U;
    return config;
}

keyswitch::RuntimeConfig make_runtime()
{
    keyswitch::RuntimeConfig runtime = {};
    runtime.stopSignalActiveHigh = 0U;
    runtime.invertXDir = 1U;
    return runtime;
}

keyswitch::MotionInputs make_inputs(uint32_t now_ms, uint8_t raw_pc1)
{
    keyswitch::MotionInputs inputs = {};
    inputs.rawDiag0 = 1U;
    inputs.rawXStop = raw_pc1;
    inputs.rawDiag2 = 1U;
    inputs.loadCellTriggered = 0U;
    inputs.mechanicalFallbackTriggered = 0U;
    inputs.stallDetected = 0U;
    inputs.stepIssued = 1U;
    inputs.loadCellRaw = 0U;
    inputs.nowMs = now_ms;
    return inputs;
}

void require_true(bool condition, const char *message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void apply_command_to_domain(
    const keyswitch::Command &command,
    keyswitch::MotionState *state,
    const keyswitch::MotionConfig &config,
    uint32_t now_ms)
{
    switch (command.type)
    {
    case keyswitch::CommandType::Home:
        keyswitch::resetForHome(state, now_ms);
        break;
    case keyswitch::CommandType::Stop:
        keyswitch::forceStop(state);
        break;
    case keyswitch::CommandType::Enable:
        keyswitch::setHoldEnabled(state, 1U);
        break;
    case keyswitch::CommandType::Disable:
        keyswitch::setHoldEnabled(state, 0U);
        break;
    case keyswitch::CommandType::Hold:
        require_true(command.hasValue != 0U, "hold command should carry a boolean value");
        keyswitch::setHoldEnabled(state, (uint8_t)command.value);
        break;
    case keyswitch::CommandType::SetPosition:
        keyswitch::setCurrentPosition(state, command.value);
        break;
    case keyswitch::CommandType::MoveAbsolute:
        require_true(keyswitch::queueAbsoluteMove(state, command.value, config) != 0U, "absolute move command should be accepted");
        break;
    case keyswitch::CommandType::MoveRelative:
        require_true(keyswitch::queueRelativeMove(state, command.value, config) != 0U, "relative move command should be accepted");
        break;
    case keyswitch::CommandType::PressTarget:
        require_true(keyswitch::setPressTarget(state, command.value, config) != 0U, "press target command should be accepted");
        break;
    case keyswitch::CommandType::Cycle:
        require_true(command.value > 0, "cycle command should request a positive count");
        require_true(keyswitch::startCycleRoutine(state, (uint32_t)command.value, config) != 0U, "cycle command should be accepted");
        break;
    default:
        throw std::runtime_error("unexpected command in integration scenario");
    }
}

void tick_until_done(
    keyswitch::MotionState *state,
    const keyswitch::MotionConfig &config,
    const keyswitch::RuntimeConfig &runtime,
    uint32_t start_ms,
    uint32_t max_ticks)
{
    for (uint32_t i = 0U; i < max_ticks; ++i)
    {
        keyswitch::MotionInputs inputs = make_inputs(start_ms + i, 1U);
        keyswitch::MotionOutputs outputs = keyswitch::tickMotion(state, inputs, config, runtime);
        if ((state->homingState == keyswitch::HomingState::Done) && (outputs.issueStep == 0U))
        {
            return;
        }
    }

    throw std::runtime_error("motion did not settle within the allowed ticks");
}

void test_integration_home_command_restarts_motion_model(void)
{
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);

    keyswitch::forceStop(&state);
    require_true(state.homingState == keyswitch::HomingState::Done, "forceStop should enter Done state");

    keyswitch::Command command = keyswitch::parseCommand("HOME");
    require_true(command.type == keyswitch::CommandType::Home, "HOME should parse to Home command");

    apply_command_to_domain(command, &state, config, 10U);
    keyswitch::MotionOutputs outputs = keyswitch::tickMotion(&state, make_inputs(20U, 1U), config, runtime);

    require_true(state.homingState == keyswitch::HomingState::Seek, "HOME should restart seeking state");
    require_true(outputs.issueStep == 1U, "HOME should resume stepping once switch is open");
}

void test_integration_home_command_ignores_load_cell_until_switch(void)
{
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);

    apply_command_to_domain(keyswitch::parseCommand("HOME"), &state, config, 10U);

    keyswitch::MotionInputs inputs = make_inputs(20U, 1U);
    inputs.loadCellTriggered = 1U;
    keyswitch::MotionOutputs outputs = keyswitch::tickMotion(&state, inputs, config, runtime);

    require_true(state.homingState == keyswitch::HomingState::Seek, "HOME should ignore load-cell trips while seeking the endstop");
    require_true(outputs.issueStep == 1U, "HOME should keep stepping until the endstop is reached");
    require_true(state.lastStopSource == keyswitch::StopSource::None, "HOME should not record the load cell as the stop source");

    outputs = keyswitch::tickMotion(&state, make_inputs(30U, 0U), config, runtime);
    outputs = keyswitch::tickMotion(&state, make_inputs(40U, 0U), config, runtime);
    outputs = keyswitch::tickMotion(&state, make_inputs(50U, 0U), config, runtime);

    require_true(state.homingState == keyswitch::HomingState::Backoff, "HOME should still transition when the endstop is confirmed");
    require_true(outputs.stopSource == keyswitch::StopSource::MechanicalFallback, "HOME should report the endstop-backed stop source");
}

void test_integration_absolute_move_command_reaches_target(void)
{
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);

    apply_command_to_domain(keyswitch::parseCommand("SETPOS 0"), &state, config, 1U);
    keyswitch::Command move = keyswitch::parseCommand("MOVEABS 4");
    require_true(move.type == keyswitch::CommandType::MoveAbsolute, "MOVEABS n should parse to MoveAbsolute");

    apply_command_to_domain(move, &state, config, 2U);
    tick_until_done(&state, config, runtime, 10U, 8U);

    require_true(state.currentPosition == 4, "MOVEABS 4 should move to absolute position 4");
    require_true(state.homingState == keyswitch::HomingState::Done, "move should settle back into Done state");
}

void test_integration_relative_move_alias_reaches_target(void)
{
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);

    apply_command_to_domain(keyswitch::parseCommand("SETPOS 3"), &state, config, 1U);
    keyswitch::Command move = keyswitch::parseCommand("JOG -2");
    require_true(move.type == keyswitch::CommandType::MoveRelative, "JOG should parse to MoveRelative");

    apply_command_to_domain(move, &state, config, 2U);
    tick_until_done(&state, config, runtime, 20U, 8U);

    require_true(state.currentPosition == 1, "JOG -2 should end at position 1 from position 3");
}

void test_integration_cycle_command_completes_press_and_return_home(void)
{
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);

    apply_command_to_domain(keyswitch::parseCommand("SETPOS 0"), &state, config, 1U);
    apply_command_to_domain(keyswitch::parseCommand("PRESSPOS 2"), &state, config, 2U);
    keyswitch::Command cycle = keyswitch::parseCommand("CYCLE 2");
    require_true(cycle.type == keyswitch::CommandType::Cycle, "CYCLE should parse to Cycle command");

    apply_command_to_domain(cycle, &state, config, 3U);
    tick_until_done(&state, config, runtime, 30U, 16U);

    require_true(state.completedCycles == 2U, "cycle command should complete the requested number of cycles");
    require_true(state.currentPosition == 0, "cycle routine should return to home position");
}

void test_integration_cycle_command_returns_home_after_load_cell_trip(void)
{
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);

    apply_command_to_domain(keyswitch::parseCommand("SETPOS 0"), &state, config, 1U);
    apply_command_to_domain(keyswitch::parseCommand("PRESSPOS 4"), &state, config, 2U);
    apply_command_to_domain(keyswitch::parseCommand("CYCLE 1"), &state, config, 3U);

    keyswitch::MotionInputs inputs = make_inputs(10U, 1U);
    inputs.loadCellTriggered = 1U;
    keyswitch::MotionOutputs outputs = keyswitch::tickMotion(&state, inputs, config, runtime);

    require_true(state.homingState == keyswitch::HomingState::CycleToHome, "load-cell trip during cycle press should reverse back toward home");
    require_true(state.lastStopSource == keyswitch::StopSource::LoadCell, "cycle stop source should record the load cell");
    require_true(outputs.stopSource == keyswitch::StopSource::LoadCell, "cycle outputs should report the load-cell stop source");

    tick_until_done(&state, config, runtime, 20U, 16U);

    require_true(state.completedCycles == 1U, "interrupted cycle should still count as a completed probe cycle");
    require_true(state.currentPosition == 0, "interrupted cycle should settle back at home");
    require_true(state.homingState == keyswitch::HomingState::Done, "interrupted cycle should settle back into Done state");
}

void test_integration_cycle_command_latches_contact_before_hard_stop(void)
{
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);

    apply_command_to_domain(keyswitch::parseCommand("SETPOS 0"), &state, config, 1U);
    apply_command_to_domain(keyswitch::parseCommand("PRESSPOS 4"), &state, config, 2U);
    apply_command_to_domain(keyswitch::parseCommand("CYCLE 1"), &state, config, 3U);

    keyswitch::MotionInputs inputs = make_inputs(10U, 1U);
    inputs.loadCellRaw = 120U;
    keyswitch::MotionOutputs outputs = keyswitch::tickMotion(&state, inputs, config, runtime);

    require_true(state.probeContactActive == 1U, "cycle command should latch first contact from raw force before the hard threshold trips");
    require_true(state.homingState == keyswitch::HomingState::CycleToPress, "first contact should not end the probe press leg");
    require_true(outputs.issueStep == 1U, "first contact should keep the press leg moving");

    inputs = make_inputs(11U, 1U);
    inputs.loadCellRaw = 1000U;
    inputs.loadCellTriggered = 1U;
    outputs = keyswitch::tickMotion(&state, inputs, config, runtime);

    require_true(state.homingState == keyswitch::HomingState::CycleToHome, "load-cell threshold should still start the release leg");
    require_true(outputs.stopSource == keyswitch::StopSource::LoadCell, "load-cell threshold should remain the recorded stop source");
}

void test_integration_stop_command_cancels_active_motion(void)
{
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);

    apply_command_to_domain(keyswitch::parseCommand("SETPOS 0"), &state, config, 1U);
    apply_command_to_domain(keyswitch::parseCommand("MOVEABS 5"), &state, config, 2U);

    keyswitch::MotionOutputs moving = keyswitch::tickMotion(&state, make_inputs(10U, 1U), config, runtime);
    require_true(moving.issueStep == 1U, "active move should request steps before stop");

    apply_command_to_domain(keyswitch::parseCommand("STOP"), &state, config, 11U);
    keyswitch::MotionOutputs stopped = keyswitch::tickMotion(&state, make_inputs(12U, 1U), config, runtime);

    require_true(state.homingState == keyswitch::HomingState::Done, "STOP should return the domain to Done state");
    require_true(stopped.issueStep == 0U, "STOP should suppress further step requests");
}

void test_integration_motion_faults_when_load_cell_triggers_mid_move(void)
{
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);

    apply_command_to_domain(keyswitch::parseCommand("SETPOS 0"), &state, config, 1U);
    apply_command_to_domain(keyswitch::parseCommand("MOVEABS 4"), &state, config, 2U);

    keyswitch::MotionInputs inputs = make_inputs(10U, 1U);
    inputs.loadCellTriggered = 1U;
    keyswitch::MotionOutputs outputs = keyswitch::tickMotion(&state, inputs, config, runtime);

    require_true(state.homingState == keyswitch::HomingState::Fault, "load-cell trigger during a move should fault the motion model");
    require_true(state.lastStopSource == keyswitch::StopSource::LoadCell, "fault stop source should record the load cell");
    require_true(outputs.driverEnable == 0U, "faulted move should disable the driver");
}

void test_integration_homed_idle_state_stays_disabled_until_explicit_hold_enable(void)
{
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);

    apply_command_to_domain(keyswitch::parseCommand("SETPOS 0"), &state, config, 1U);

    keyswitch::MotionOutputs idle = keyswitch::tickMotion(&state, make_inputs(2U, 1U), config, runtime);

    require_true(state.homingState == keyswitch::HomingState::Done, "SETPOS should leave the domain in Done state");
    require_true(idle.driverEnable == 0U, "homed idle state should leave the driver off until hold is explicitly enabled");
}

void test_integration_enable_disable_and_hold_commands_control_idle_driver_state(void)
{
    keyswitch::MotionConfig config = make_config();
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);

    apply_command_to_domain(keyswitch::parseCommand("SETPOS 0"), &state, config, 1U);

    apply_command_to_domain(keyswitch::parseCommand("DISABLE"), &state, config, 2U);
    keyswitch::MotionOutputs disabled = keyswitch::tickMotion(&state, make_inputs(3U, 1U), config, runtime);
    require_true(disabled.driverEnable == 0U, "DISABLE should leave the idle driver off");

    apply_command_to_domain(keyswitch::parseCommand("ENABLE"), &state, config, 4U);
    keyswitch::MotionOutputs enabled = keyswitch::tickMotion(&state, make_inputs(5U, 1U), config, runtime);
    require_true(enabled.driverEnable == 1U, "ENABLE should leave the idle driver on");

    apply_command_to_domain(keyswitch::parseCommand("HOLD OFF"), &state, config, 6U);
    keyswitch::MotionOutputs hold_off = keyswitch::tickMotion(&state, make_inputs(7U, 1U), config, runtime);
    require_true(hold_off.driverEnable == 0U, "HOLD OFF should disable idle hold");

    apply_command_to_domain(keyswitch::parseCommand("HOLD ON"), &state, config, 8U);
    keyswitch::MotionOutputs hold_on = keyswitch::tickMotion(&state, make_inputs(9U, 1U), config, runtime);
    require_true(hold_on.driverEnable == 1U, "HOLD ON should re-enable idle hold");
}

} // namespace

int main()
{
    try
    {
        test_integration_home_command_restarts_motion_model();
        test_integration_home_command_ignores_load_cell_until_switch();
        test_integration_absolute_move_command_reaches_target();
        test_integration_relative_move_alias_reaches_target();
        test_integration_cycle_command_completes_press_and_return_home();
        test_integration_cycle_command_returns_home_after_load_cell_trip();
        test_integration_cycle_command_latches_contact_before_hard_stop();
        test_integration_stop_command_cancels_active_motion();
        test_integration_motion_faults_when_load_cell_triggers_mid_move();
        test_integration_homed_idle_state_stays_disabled_until_explicit_hold_enable();
        test_integration_enable_disable_and_hold_commands_control_idle_driver_state();
        std::cout << "PASS test_integration" << std::endl;
        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "FAIL test_integration: " << ex.what() << std::endl;
        return 1;
    }
}