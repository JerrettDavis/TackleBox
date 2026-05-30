#include <stdexcept>
#include <iostream>

#include "keyswitch_protocol.h"

static void require_true(bool condition, const char *message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

static void test_protocol_parses_plain_commands_and_aliases()
{
    require_true(keyswitch::parseCommand("home").type == keyswitch::CommandType::Home, "home should map to Home");
    require_true(keyswitch::parseCommand("G28").type == keyswitch::CommandType::Home, "G28 should map to Home");
    require_true(keyswitch::parseCommand("M114").type == keyswitch::CommandType::Status, "M114 should map to Status");
    require_true(keyswitch::parseCommand("M122").type == keyswitch::CommandType::Safety, "M122 should map to Safety");
    require_true(keyswitch::parseCommand("m112").type == keyswitch::CommandType::Stop, "M112 should map to Stop");
    require_true(keyswitch::parseCommand("?").type == keyswitch::CommandType::Help, "? should map to Help");
    require_true(keyswitch::parseCommand("save").type == keyswitch::CommandType::SaveConfig, "save should map to SaveConfig");
    require_true(keyswitch::parseCommand("reboot").type == keyswitch::CommandType::Reboot, "reboot should map to Reboot");
    require_true(keyswitch::parseCommand("bootloader").type == keyswitch::CommandType::Bootloader, "bootloader should map to Bootloader");
    require_true(keyswitch::parseCommand("recovery").type == keyswitch::CommandType::Bootloader, "recovery should map to Bootloader");
    require_true(keyswitch::parseCommand("boot").type == keyswitch::CommandType::Boot, "boot should map to Boot");
    require_true(keyswitch::parseCommand("estop").type == keyswitch::CommandType::EmergencyStop, "estop should map to EmergencyStop");
    require_true(keyswitch::parseCommand("estopclear").type == keyswitch::CommandType::EmergencyClear, "estopclear should map to EmergencyClear");
}

static void test_protocol_parses_configuration_commands()
{
    const keyswitch::Command config = keyswitch::parseCommand("config pin.x_step");
    require_true(config.type == keyswitch::CommandType::Config, "config should map to Config");
    require_true(std::string(config.key) == "PIN.X_STEP", "config should capture the requested key");

    const keyswitch::Command setPin = keyswitch::parseCommand("set pin.x_step pe2");
    require_true(setPin.type == keyswitch::CommandType::SetConfig, "set should map to SetConfig");
    require_true(std::string(setPin.key) == "PIN.X_STEP", "set should capture the config key");
    require_true(std::string(setPin.text) == "PE2", "set should capture the config value text");

    const keyswitch::Command setCurrent = keyswitch::parseCommand("set tmc.irun 5");
    require_true(setCurrent.type == keyswitch::CommandType::SetConfig, "set current should map to SetConfig");
    require_true(std::string(setCurrent.key) == "TMC.IRUN", "set current should preserve the dotted key");
    require_true(std::string(setCurrent.text) == "5", "set current should preserve the numeric text value");

    require_true(keyswitch::parseCommand("resetcfg").type == keyswitch::CommandType::ResetConfig, "resetcfg should map to ResetConfig");
}

static void test_protocol_parses_simulation_commands()
{
    const keyswitch::Command simLoad = keyswitch::parseCommand("simload 1234");
    require_true(simLoad.type == keyswitch::CommandType::SimLoad, "simload should map to SimLoad");
    require_true(simLoad.hasValue == 1U, "simload should capture a numeric value");
    require_true(simLoad.value == 1234U, "simload should parse the provided raw value");

    const keyswitch::Command simThresh = keyswitch::parseCommand("loadthresh 900");
    require_true(simThresh.type == keyswitch::CommandType::SimThreshold, "loadthresh should map to SimThreshold");
    require_true(simThresh.value == 900U, "loadthresh should parse the provided threshold value");

    const keyswitch::Command simMech = keyswitch::parseCommand("simmech on");
    require_true(simMech.type == keyswitch::CommandType::SimMechanical, "simmech should map to SimMechanical");
    require_true(simMech.value == 1U, "simmech on should parse to 1");

    const keyswitch::Command simStall = keyswitch::parseCommand("simstall off");
    require_true(simStall.type == keyswitch::CommandType::SimStall, "simstall should map to SimStall");
    require_true(simStall.value == 0U, "simstall off should parse to 0");

    require_true(keyswitch::parseCommand("simclear").type == keyswitch::CommandType::SimClear, "simclear should map to SimClear");
}

static void test_protocol_parses_actuator_commands()
{
    require_true(keyswitch::parseCommand("m17").type == keyswitch::CommandType::Enable, "M17 should map to Enable");
    require_true(keyswitch::parseCommand("m84").type == keyswitch::CommandType::Disable, "M84 should map to Disable");
    require_true(keyswitch::parseCommand("tmc").type == keyswitch::CommandType::Driver, "tmc should map to Driver");

    const keyswitch::Command hold = keyswitch::parseCommand("hold on");
    require_true(hold.type == keyswitch::CommandType::Hold, "hold should map to Hold");
    require_true(hold.value == 1, "hold on should parse as enabled");

    const keyswitch::Command irun = keyswitch::parseCommand("irun 18");
    require_true(irun.type == keyswitch::CommandType::RunCurrent, "irun should map to RunCurrent");
    require_true(irun.value == 18, "irun should parse the configured current scale");

    const keyswitch::Command ihold = keyswitch::parseCommand("ihold 6");
    require_true(ihold.type == keyswitch::CommandType::HoldCurrent, "ihold should map to HoldCurrent");

    const keyswitch::Command iholddelay = keyswitch::parseCommand("iholddelay 8");
    require_true(iholddelay.type == keyswitch::CommandType::HoldDelay, "iholddelay should map to HoldDelay");

    const keyswitch::Command sgthrs = keyswitch::parseCommand("sgthrs 4");
    require_true(sgthrs.type == keyswitch::CommandType::StallThreshold, "sgthrs should map to StallThreshold");

    const keyswitch::Command absMove = keyswitch::parseCommand("G0 X100");
    require_true(absMove.type == keyswitch::CommandType::MoveAbsolute, "G0 X100 should map to MoveAbsolute");
    require_true(absMove.value == 100, "G0 X100 should parse a 100 mm target");
    require_true(absMove.valueUnit == keyswitch::CommandValueUnit::Millimeters, "G0 X100 should carry millimeter units");

    const keyswitch::Command stepMove = keyswitch::parseCommand("MOVEABS 1200");
    require_true(stepMove.type == keyswitch::CommandType::MoveAbsolute, "MOVEABS 1200 should map to MoveAbsolute");
    require_true(stepMove.value == 1200, "MOVEABS 1200 should preserve the step target");
    require_true(stepMove.valueUnit == keyswitch::CommandValueUnit::NativeSteps, "MOVEABS 1200 should remain step-native");

    const keyswitch::Command relMove = keyswitch::parseCommand("jog -25");
    require_true(relMove.type == keyswitch::CommandType::MoveRelative, "jog should map to MoveRelative");
    require_true(relMove.value == -25, "jog -25 should parse a signed delta");

    const keyswitch::Command setPos = keyswitch::parseCommand("setpos 0");
    require_true(setPos.type == keyswitch::CommandType::SetPosition, "setpos should map to SetPosition");

    const keyswitch::Command cycle = keyswitch::parseCommand("cycle 10000");
    require_true(cycle.type == keyswitch::CommandType::Cycle, "cycle should map to Cycle");
    require_true(cycle.value == 10000, "cycle should parse the routine count");

    const keyswitch::Command pressPos = keyswitch::parseCommand("presspos 320");
    require_true(pressPos.type == keyswitch::CommandType::PressTarget, "presspos should map to PressTarget");
    require_true(pressPos.value == 320, "presspos should parse the configured press target");
}

static void test_protocol_rejects_unknown_commands()
{
    require_true(keyswitch::parseCommand("jogx").type == keyswitch::CommandType::Unknown, "unknown command should be Unknown");
    require_true(keyswitch::parseCommand("").type == keyswitch::CommandType::None, "empty command should be None");
    require_true(keyswitch::parseCommand("simload abc").type == keyswitch::CommandType::Unknown, "simload requires a numeric value");
    require_true(keyswitch::parseCommand("simstall maybe").type == keyswitch::CommandType::Unknown, "simstall requires an on/off style value");
    require_true(keyswitch::parseCommand("g0").type == keyswitch::CommandType::Unknown, "g0 requires a target value");
    require_true(keyswitch::parseCommand("hold maybe").type == keyswitch::CommandType::Unknown, "hold requires an on/off value");
    require_true(keyswitch::parseCommand("irun").type == keyswitch::CommandType::Unknown, "irun requires a numeric value");
    require_true(keyswitch::parseCommand("set pin.x_step").type == keyswitch::CommandType::Unknown, "set requires both a key and value");
}

int main()
{
    try
    {
        test_protocol_parses_plain_commands_and_aliases();
        test_protocol_parses_configuration_commands();
        test_protocol_parses_simulation_commands();
        test_protocol_parses_actuator_commands();
        test_protocol_rejects_unknown_commands();
        std::cout << "PASS test_protocol" << std::endl;
        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "FAIL test_protocol: " << ex.what() << std::endl;
        return 1;
    }
}