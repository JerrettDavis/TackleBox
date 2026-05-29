#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "keyswitch_domain.h"
#include "keyswitch_protocol.h"
#include "load_cell.h"

namespace {

struct ProbeWorkflowOptions {
    uint32_t threshold = 1000U;
    uint32_t contactDetectRaw = 125U;
    int32_t pressTarget = 40;
    uint32_t cycleCount = 3U;
    int32_t contactPosition = 18;
    uint32_t forceSlope = 170U;
    uint32_t forceBias = 60U;
    uint32_t homeTripSeekSteps = 6U;
};

struct ProbeSample {
    uint32_t tick;
    const char *phase;
    int32_t position;
    int32_t target;
    uint32_t force;
    uint8_t load;
    uint8_t homed;
    uint8_t hold;
    uint32_t cyclesRemaining;
    uint32_t completedCycles;
    uint8_t stopSource;
    uint8_t contact;
    double normalizedPosition;
};

struct ProbeWorkflowResult {
    std::vector<ProbeSample> samples;
    uint32_t contactSamples = 0U;
    uint32_t loadCellHits = 0U;
    uint32_t maxForce = 0U;
    int32_t firstContactPosition = -1;
    int32_t firstTriggerPosition = -1;
    int32_t finalPosition = 0;
    uint32_t completedCycles = 0U;
};

keyswitch::MotionConfig make_config()
{
    keyswitch::MotionConfig config = {};
    config.seekStepLimit = 64U;
    config.probeContactThresholdRaw = 125U;
    config.minPosition = 0;
    config.maxPosition = 80;
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
    runtime.homeTowardsPositive = 0U;
    return runtime;
}

void require_true(bool condition, const char *message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

const char *phase_name(keyswitch::HomingState state)
{
    switch (state)
    {
    case keyswitch::HomingState::Seek: return "seek";
    case keyswitch::HomingState::Backoff: return "backoff";
    case keyswitch::HomingState::Done: return "done";
    case keyswitch::HomingState::Fault: return "fault";
    case keyswitch::HomingState::MoveToTarget: return "move";
    case keyswitch::HomingState::CycleToPress: return "cycle_press";
    case keyswitch::HomingState::CycleToHome: return "cycle_home";
    default: return "unknown";
    }
}

void apply_command_to_domain(
    const keyswitch::Command &command,
    keyswitch::MotionState *state,
    const keyswitch::MotionConfig &config,
    uint32_t now_ms,
    LoadCellRuntime *load_cell)
{
    switch (command.type)
    {
    case keyswitch::CommandType::Home:
        keyswitch::resetForHome(state, now_ms);
        break;
    case keyswitch::CommandType::Hold:
        require_true(command.hasValue != 0U, "hold command should carry a boolean value");
        keyswitch::setHoldEnabled(state, (uint8_t)command.value);
        break;
    case keyswitch::CommandType::PressTarget:
        require_true(keyswitch::setPressTarget(state, command.value, config) != 0U, "press target should be accepted");
        break;
    case keyswitch::CommandType::Cycle:
        require_true(command.value > 0, "cycle count should be positive");
        require_true(keyswitch::startCycleRoutine(state, (uint32_t)command.value, config) != 0U, "cycle command should be accepted");
        break;
    case keyswitch::CommandType::SimThreshold:
        require_true(load_cell != 0, "sim threshold requires a load-cell runtime");
        load_cell_set_threshold(load_cell, (uint32_t)command.value);
        break;
    case keyswitch::CommandType::SimClear:
        require_true(load_cell != 0, "sim clear requires a load-cell runtime");
        load_cell_clear(load_cell);
        break;
    default:
        throw std::runtime_error("unexpected command in probe workflow simulation");
    }
}

uint32_t simulated_force_raw(
    const keyswitch::MotionState &state,
    const ProbeWorkflowOptions &options)
{
    if ((state.homingState != keyswitch::HomingState::CycleToPress) && (state.homingState != keyswitch::HomingState::CycleToHome))
    {
        return 0U;
    }

    const int32_t compression = state.currentPosition - options.contactPosition;
    if (compression <= 0)
    {
        return 0U;
    }

    return options.forceBias + ((uint32_t)compression * options.forceSlope);
}

void capture_sample(
    ProbeWorkflowResult *result,
    uint32_t tick,
    const keyswitch::MotionState &state,
    const LoadCellRuntime &load_cell)
{
    ProbeSample sample = {};
    sample.tick = tick;
    sample.phase = phase_name(state.homingState);
    sample.position = state.currentPosition;
    sample.target = state.targetPosition;
    sample.force = load_cell_raw(load_cell);
    sample.load = load_cell_triggered(load_cell);
    sample.homed = state.homed;
    sample.hold = state.holdEnabled;
    sample.cyclesRemaining = state.cycleCountRemaining;
    sample.completedCycles = state.completedCycles;
    sample.stopSource = (uint8_t)state.lastStopSource;
    sample.contact = state.probeContactActive;
    sample.normalizedPosition = (state.pressTargetPosition > 0)
        ? ((double)state.currentPosition / (double)state.pressTargetPosition)
        : 0.0;
    result->samples.push_back(sample);
    if (sample.contact != 0U)
    {
        ++result->contactSamples;
        if (result->firstContactPosition < 0)
        {
            result->firstContactPosition = state.probeContactPosition;
        }
    }
    if (sample.force > result->maxForce)
    {
        result->maxForce = sample.force;
    }
}

void run_until_homed(
    keyswitch::MotionState *state,
    const keyswitch::MotionConfig &config,
    const keyswitch::RuntimeConfig &runtime,
    LoadCellRuntime *load_cell,
    const ProbeWorkflowOptions &options,
    ProbeWorkflowResult *result,
    uint32_t *tick)
{
    for (uint32_t iteration = 0U; iteration < 128U; ++iteration)
    {
        keyswitch::MotionInputs inputs = {};
        inputs.rawDiag0 = 1U;
        inputs.rawDiag2 = 1U;
        inputs.rawXStop = (state->seekSteps >= options.homeTripSeekSteps) ? 0U : 1U;
        inputs.stepIssued = 1U;
        inputs.nowMs = *tick;

        load_cell_set_raw(load_cell, 0U);
        inputs.loadCellTriggered = load_cell_triggered(*load_cell);
        inputs.loadCellRaw = load_cell_raw(*load_cell);

        const keyswitch::MotionOutputs outputs = keyswitch::tickMotion(state, inputs, config, runtime);
        capture_sample(result, *tick, *state, *load_cell);
        ++(*tick);

        if ((state->homingState == keyswitch::HomingState::Done) && (state->homed != 0U) && (outputs.issueStep == 0U))
        {
            return;
        }
    }

    throw std::runtime_error("home routine did not settle in the simulated probe workflow");
}

void run_cycle_routine(
    keyswitch::MotionState *state,
    const keyswitch::MotionConfig &config,
    const keyswitch::RuntimeConfig &runtime,
    LoadCellRuntime *load_cell,
    const ProbeWorkflowOptions &options,
    ProbeWorkflowResult *result,
    uint32_t *tick)
{
    for (uint32_t iteration = 0U; iteration < 512U; ++iteration)
    {
        const keyswitch::HomingState phase_before_tick = state->homingState;
        const uint32_t raw_force = simulated_force_raw(*state, options);
        load_cell_set_raw(load_cell, raw_force);

        keyswitch::MotionInputs inputs = {};
        inputs.rawDiag0 = 1U;
        inputs.rawDiag2 = 1U;
        inputs.rawXStop = 1U;
        inputs.stepIssued = 1U;
        inputs.loadCellTriggered = load_cell_triggered(*load_cell);
        inputs.loadCellRaw = load_cell_raw(*load_cell);
        inputs.nowMs = *tick;

        const keyswitch::MotionOutputs outputs = keyswitch::tickMotion(state, inputs, config, runtime);
        if ((phase_before_tick == keyswitch::HomingState::CycleToPress) &&
            (state->homingState == keyswitch::HomingState::CycleToHome) &&
            (outputs.stopSource == keyswitch::StopSource::LoadCell))
        {
            ++result->loadCellHits;
            if (result->firstTriggerPosition < 0)
            {
                result->firstTriggerPosition = state->currentPosition;
            }
        }

        capture_sample(result, *tick, *state, *load_cell);
        ++(*tick);

        if ((state->homingState == keyswitch::HomingState::Done) && (state->completedCycles == options.cycleCount) && (outputs.issueStep == 0U))
        {
            return;
        }
    }

    throw std::runtime_error("cycle routine did not settle in the simulated probe workflow");
}

ProbeWorkflowResult run_probe_workflow(const ProbeWorkflowOptions &options)
{
    keyswitch::MotionConfig config = make_config();
    config.probeContactThresholdRaw = options.contactDetectRaw;
    keyswitch::RuntimeConfig runtime = make_runtime();
    keyswitch::MotionState state = keyswitch::makeInitialState(0U);

    LoadCellConfig load_cell_config = {};
    load_cell_config.source = (uint8_t)LoadCellSourceKind::Simulation;
    load_cell_config.threshold = options.threshold;
    LoadCellRuntime load_cell = load_cell_make_default(options.threshold);
    load_cell_apply_config(&load_cell, load_cell_config);

    ProbeWorkflowResult result = {};
    uint32_t tick = 0U;

    apply_command_to_domain(keyswitch::parseCommand("SIMCLEAR"), &state, config, tick, &load_cell);
    apply_command_to_domain(keyswitch::parseCommand("SIMTHRESH 1000"), &state, config, tick, &load_cell);
    load_cell_set_threshold(&load_cell, options.threshold);
    apply_command_to_domain(keyswitch::parseCommand("HOME"), &state, config, tick, &load_cell);
    run_until_homed(&state, config, runtime, &load_cell, options, &result, &tick);

    apply_command_to_domain(keyswitch::parseCommand("HOLD ON"), &state, config, tick, &load_cell);

    const std::string press_command = std::string("PRESSPOS ") + std::to_string(options.pressTarget);
    apply_command_to_domain(keyswitch::parseCommand(press_command.c_str()), &state, config, tick, &load_cell);

    const std::string cycle_command = std::string("CYCLE ") + std::to_string(options.cycleCount);
    apply_command_to_domain(keyswitch::parseCommand(cycle_command.c_str()), &state, config, tick, &load_cell);
    run_cycle_routine(&state, config, runtime, &load_cell, options, &result, &tick);

    result.finalPosition = state.currentPosition;
    result.completedCycles = state.completedCycles;
    return result;
}

void write_export_json(const char *path, const ProbeWorkflowOptions &options, const ProbeWorkflowResult &result)
{
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open())
    {
        throw std::runtime_error("failed to open export path");
    }

    output << "{\n";
    output << "  \"meta\": {\n";
    output << "    \"threshold\": " << options.threshold << ",\n";
    output << "    \"contactDetectRaw\": " << options.contactDetectRaw << ",\n";
    output << "    \"pressTarget\": " << options.pressTarget << ",\n";
    output << "    \"cycleCount\": " << options.cycleCount << ",\n";
    output << "    \"contactPosition\": " << options.contactPosition << ",\n";
    output << "    \"firstContactPosition\": " << result.firstContactPosition << ",\n";
    output << "    \"forceSlope\": " << options.forceSlope << ",\n";
    output << "    \"forceBias\": " << options.forceBias << ",\n";
    output << "    \"contactSamples\": " << result.contactSamples << ",\n";
    output << "    \"loadCellHits\": " << result.loadCellHits << ",\n";
    output << "    \"maxForce\": " << result.maxForce << ",\n";
    output << "    \"completedCycles\": " << result.completedCycles << "\n";
    output << "  },\n";
    output << "  \"samples\": [\n";
    for (size_t index = 0U; index < result.samples.size(); ++index)
    {
        const ProbeSample &sample = result.samples[index];
        output << "    {\"tick\": " << sample.tick
               << ", \"phase\": \"" << sample.phase
               << "\", \"position\": " << sample.position
               << ", \"target\": " << sample.target
               << ", \"force\": " << sample.force
               << ", \"load\": " << (unsigned long)sample.load
               << ", \"homed\": " << (unsigned long)sample.homed
               << ", \"hold\": " << (unsigned long)sample.hold
               << ", \"cyclesRemaining\": " << sample.cyclesRemaining
               << ", \"completedCycles\": " << sample.completedCycles
               << ", \"stopSource\": " << (unsigned long)sample.stopSource
               << ", \"contact\": " << (unsigned long)sample.contact
               << ", \"normalizedPosition\": " << sample.normalizedPosition
               << "}";
        if ((index + 1U) < result.samples.size())
        {
            output << ",";
        }
        output << "\n";
    }
    output << "  ]\n";
    output << "}\n";
}

void test_probe_workflow_completes_requested_cycles_with_load_cell_trips(void)
{
    ProbeWorkflowOptions options = {};
    const ProbeWorkflowResult result = run_probe_workflow(options);

    require_true(result.completedCycles == options.cycleCount, "probe workflow should complete every requested cycle");
    require_true(result.loadCellHits == options.cycleCount, "each cycle press should trip the simulated load cell before the hard press target");
    require_true(result.finalPosition == 0, "probe workflow should return to home after the simulated routine");
    require_true(result.maxForce >= options.threshold, "simulated workflow should reach the configured threshold");
    require_true(result.contactSamples > 0U, "simulated workflow should enter the fine probe region before the hard threshold trips");
    require_true(result.firstContactPosition >= 0, "simulated workflow should record the first contact position");
    require_true(result.firstTriggerPosition >= 0, "simulated workflow should record the first trigger position");
    require_true(result.firstContactPosition < result.firstTriggerPosition, "fine probe contact should occur before the hard threshold stop");
    require_true(result.firstTriggerPosition < options.pressTarget, "threshold trigger should occur before the configured press target");
}

void test_probe_workflow_produces_curve_samples_for_visualization(void)
{
    ProbeWorkflowOptions options = {};
    options.threshold = 900U;
    options.cycleCount = 2U;
    const ProbeWorkflowResult result = run_probe_workflow(options);

    require_true(result.samples.size() > 16U, "simulated probe workflow should emit a useful number of chart samples");

    uint32_t loaded_samples = 0U;
    uint32_t contact_samples = 0U;
    for (const ProbeSample &sample : result.samples)
    {
        if (sample.load != 0U)
        {
            ++loaded_samples;
        }
        if (sample.contact != 0U)
        {
            ++contact_samples;
        }
    }

    require_true(loaded_samples > 0U, "response-curve export should contain threshold-crossing samples");
    require_true(contact_samples > loaded_samples, "response-curve export should retain a fine-probe region before the hard stop samples");
}

ProbeWorkflowOptions parse_options(int argc, char **argv, const char **export_path)
{
    ProbeWorkflowOptions options = {};
    *export_path = 0;

    for (int index = 1; index < argc; ++index)
    {
        const std::string arg = argv[index];
        if ((arg == "--export") && ((index + 1) < argc))
        {
            *export_path = argv[++index];
        }
        else if ((arg == "--threshold") && ((index + 1) < argc))
        {
            options.threshold = (uint32_t)std::stoul(argv[++index]);
        }
        else if ((arg == "--contact-detect-raw") && ((index + 1) < argc))
        {
            options.contactDetectRaw = (uint32_t)std::stoul(argv[++index]);
        }
        else if ((arg == "--press-target") && ((index + 1) < argc))
        {
            options.pressTarget = (int32_t)std::stol(argv[++index]);
        }
        else if ((arg == "--cycle-count") && ((index + 1) < argc))
        {
            options.cycleCount = (uint32_t)std::stoul(argv[++index]);
        }
        else
        {
            throw std::runtime_error("unknown argument");
        }
    }

    return options;
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        const char *export_path = 0;
        const ProbeWorkflowOptions options = parse_options(argc, argv, &export_path);
        if (export_path != 0)
        {
            const ProbeWorkflowResult result = run_probe_workflow(options);
            write_export_json(export_path, options, result);
            std::cout << "PASS test_probe_workflow export" << std::endl;
            return 0;
        }

        test_probe_workflow_completes_requested_cycles_with_load_cell_trips();
        test_probe_workflow_produces_curve_samples_for_visualization();
        std::cout << "PASS test_probe_workflow" << std::endl;
        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "FAIL test_probe_workflow: " << ex.what() << std::endl;
        return 1;
    }
}