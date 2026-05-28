#include "keyswitch_protocol.h"

namespace keyswitch {

namespace {

static uint8_t is_space(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

static char to_upper(char c)
{
    if ((c >= 'a') && (c <= 'z'))
    {
        return (char)(c - ('a' - 'A'));
    }
    return c;
}

static uint8_t same_text(const char *lhs, const char *rhs)
{
    uint32_t i = 0U;
    while ((lhs[i] != 0) && (rhs[i] != 0))
    {
        if (lhs[i] != rhs[i])
        {
            return 0U;
        }
        ++i;
    }
    return (lhs[i] == 0) && (rhs[i] == 0);
}

static void copy_token_upper(const char *text, uint32_t *index, char *dest, uint32_t dest_len)
{
    uint32_t out = 0U;

    while ((text[*index] != 0) && is_space(text[*index]))
    {
        ++(*index);
    }

    while ((text[*index] != 0) && !is_space(text[*index]) && (out + 1U < dest_len))
    {
        dest[out++] = to_upper(text[*index]);
        ++(*index);
    }

    dest[out] = 0;
}

static uint8_t parse_u32_token(const char *text, uint32_t *value)
{
    uint32_t parsed = 0U;
    uint32_t i = 0U;

    if ((text == 0) || (text[0] == 0) || (value == 0))
    {
        return 0U;
    }

    while (text[i] != 0)
    {
        if ((text[i] < '0') || (text[i] > '9'))
        {
            return 0U;
        }

        parsed = (parsed * 10U) + (uint32_t)(text[i] - '0');
        ++i;
    }

    *value = parsed;
    return 1U;
}

static uint8_t parse_i32_token(const char *text, int32_t *value)
{
    uint32_t i = 0U;
    uint8_t negative = 0U;
    int32_t parsed = 0;

    if ((text == 0) || (text[0] == 0) || (value == 0))
    {
        return 0U;
    }

    if (text[i] == '-')
    {
        negative = 1U;
        ++i;
    }
    else if (text[i] == '+')
    {
        ++i;
    }

    if (text[i] == 0)
    {
        return 0U;
    }

    while (text[i] != 0)
    {
        if ((text[i] < '0') || (text[i] > '9'))
        {
            return 0U;
        }

        parsed = (parsed * 10) + (int32_t)(text[i] - '0');
        ++i;
    }

    *value = (negative != 0U) ? -parsed : parsed;
    return 1U;
}

static uint8_t parse_axis_or_i32_token(const char *text, int32_t *value)
{
    if ((text == 0) || (value == 0))
    {
        return 0U;
    }

    if ((text[0] == 'X') && (text[1] != 0))
    {
        return parse_i32_token(text + 1U, value);
    }

    return parse_i32_token(text, value);
}

static uint8_t parse_bool_token(const char *text, uint32_t *value)
{
    if ((text == 0) || (value == 0))
    {
        return 0U;
    }

    if (same_text(text, "1") || same_text(text, "ON") || same_text(text, "TRUE"))
    {
        *value = 1U;
        return 1U;
    }

    if (same_text(text, "0") || same_text(text, "OFF") || same_text(text, "FALSE"))
    {
        *value = 0U;
        return 1U;
    }

    return 0U;
}

} // namespace

Command parseCommand(const char *text)
{
    char normalized[32] = {0};
    char argument[32] = {0};
    char argument2[32] = {0};
    uint32_t i = 0U;
    Command command = {CommandType::None, 0U, 0U, CommandValueUnit::NativeSteps, {0}, {0}};

    if (text == 0)
    {
        return command;
    }

    copy_token_upper(text, &i, normalized, sizeof(normalized));
    copy_token_upper(text, &i, argument, sizeof(argument));
    copy_token_upper(text, &i, argument2, sizeof(argument2));

    if (normalized[0] == 0)
    {
        return command;
    }

    if (same_text(normalized, "STATUS") || same_text(normalized, "M114") || same_text(normalized, "M119"))
    {
        command.type = CommandType::Status;
    }
    else if (same_text(normalized, "SAFETY") || same_text(normalized, "M122"))
    {
        command.type = CommandType::Safety;
    }
    else if (same_text(normalized, "CONFIG") || same_text(normalized, "CFG"))
    {
        command.type = CommandType::Config;
        for (uint32_t idx = 0U; idx < sizeof(command.key); ++idx)
        {
            command.key[idx] = argument[idx];
            if (argument[idx] == 0)
            {
                break;
            }
        }
    }
    else if (same_text(normalized, "SET"))
    {
        if ((argument[0] != 0) && (argument2[0] != 0))
        {
            command.type = CommandType::SetConfig;
            for (uint32_t idx = 0U; idx < sizeof(command.key); ++idx)
            {
                command.key[idx] = argument[idx];
                if (argument[idx] == 0)
                {
                    break;
                }
            }
            for (uint32_t idx = 0U; idx < sizeof(command.text); ++idx)
            {
                command.text[idx] = argument2[idx];
                if (argument2[idx] == 0)
                {
                    break;
                }
            }
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "SAVE") || same_text(normalized, "SAVECFG"))
    {
        command.type = CommandType::SaveConfig;
    }
    else if (same_text(normalized, "RESETCFG") || same_text(normalized, "DEFAULTCFG"))
    {
        command.type = CommandType::ResetConfig;
    }
    else if (same_text(normalized, "REBOOT") || same_text(normalized, "RESET"))
    {
        command.type = CommandType::Reboot;
    }
    else if (same_text(normalized, "BOOTLOADER") || same_text(normalized, "RECOVERY"))
    {
        command.type = CommandType::Bootloader;
    }
    else if (same_text(normalized, "BOOT") || same_text(normalized, "START"))
    {
        command.type = CommandType::Boot;
    }
    else if (same_text(normalized, "ENABLE") || same_text(normalized, "M17"))
    {
        command.type = CommandType::Enable;
    }
    else if (same_text(normalized, "DISABLE") || same_text(normalized, "M18") || same_text(normalized, "M84"))
    {
        command.type = CommandType::Disable;
    }
    else if (same_text(normalized, "HOLD"))
    {
        uint32_t bool_value = 0U;
        if (parse_bool_token(argument, &bool_value) != 0U)
        {
            command.type = CommandType::Hold;
            command.hasValue = 1U;
            command.value = (int32_t)bool_value;
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "DRIVER") || same_text(normalized, "TMC"))
    {
        command.type = CommandType::Driver;
    }
    else if (same_text(normalized, "IRUN"))
    {
        if (parse_i32_token(argument, &command.value) != 0U)
        {
            command.type = CommandType::RunCurrent;
            command.hasValue = 1U;
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "IHOLD"))
    {
        if (parse_i32_token(argument, &command.value) != 0U)
        {
            command.type = CommandType::HoldCurrent;
            command.hasValue = 1U;
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "IHOLDDELAY") || same_text(normalized, "IHOLDD"))
    {
        if (parse_i32_token(argument, &command.value) != 0U)
        {
            command.type = CommandType::HoldDelay;
            command.hasValue = 1U;
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "SGTHRS"))
    {
        if (parse_i32_token(argument, &command.value) != 0U)
        {
            command.type = CommandType::StallThreshold;
            command.hasValue = 1U;
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "MOVEABS"))
    {
        if (parse_axis_or_i32_token(argument, &command.value) != 0U)
        {
            command.type = CommandType::MoveAbsolute;
            command.hasValue = 1U;
            command.valueUnit = CommandValueUnit::NativeSteps;
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "G0") || same_text(normalized, "G1"))
    {
        if (parse_axis_or_i32_token(argument, &command.value) != 0U)
        {
            command.type = CommandType::MoveAbsolute;
            command.hasValue = 1U;
            command.valueUnit = CommandValueUnit::Millimeters;
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "MOVEREL") || same_text(normalized, "JOG"))
    {
        if (parse_i32_token(argument, &command.value) != 0U)
        {
            command.type = CommandType::MoveRelative;
            command.hasValue = 1U;
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "SETPOS"))
    {
        if (parse_i32_token(argument, &command.value) != 0U)
        {
            command.type = CommandType::SetPosition;
            command.hasValue = 1U;
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "CYCLE"))
    {
        if (parse_i32_token(argument, &command.value) != 0U)
        {
            command.type = CommandType::Cycle;
            command.hasValue = 1U;
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "PRESSPOS"))
    {
        if (parse_i32_token(argument, &command.value) != 0U)
        {
            command.type = CommandType::PressTarget;
            command.hasValue = 1U;
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "SIMLOAD") || same_text(normalized, "SIMRAW"))
    {
        uint32_t parsed = 0U;
        if (parse_u32_token(argument, &parsed) != 0U)
        {
            command.type = CommandType::SimLoad;
            command.hasValue = 1U;
            command.value = (int32_t)parsed;
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "SIMTHRESH") || same_text(normalized, "LOADTHRESH"))
    {
        uint32_t parsed = 0U;
        if (parse_u32_token(argument, &parsed) != 0U)
        {
            command.type = CommandType::SimThreshold;
            command.hasValue = 1U;
            command.value = (int32_t)parsed;
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "SIMMECH"))
    {
        uint32_t bool_value = 0U;
        if (parse_bool_token(argument, &bool_value) != 0U)
        {
            command.type = CommandType::SimMechanical;
            command.hasValue = 1U;
            command.value = (int32_t)bool_value;
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "SIMSTALL"))
    {
        uint32_t bool_value = 0U;
        if (parse_bool_token(argument, &bool_value) != 0U)
        {
            command.type = CommandType::SimStall;
            command.hasValue = 1U;
            command.value = (int32_t)bool_value;
        }
        else
        {
            command.type = CommandType::Unknown;
        }
    }
    else if (same_text(normalized, "SIMCLEAR"))
    {
        command.type = CommandType::SimClear;
    }
    else if (same_text(normalized, "ZERO") || same_text(normalized, "TARE"))
    {
        command.type = CommandType::Tare;
    }
    else if (same_text(normalized, "PANELPINS") || same_text(normalized, "PANEL"))
    {
        command.type = CommandType::PanelPins;
    }
    else if (same_text(normalized, "HOME") || same_text(normalized, "G28"))
    {
        command.type = CommandType::Home;
    }
    else if (same_text(normalized, "STOP") || same_text(normalized, "M112"))
    {
        command.type = CommandType::Stop;
    }
    else if (same_text(normalized, "BACKOFF"))
    {
        command.type = CommandType::Backoff;
    }
    else if (same_text(normalized, "HELP") || same_text(normalized, "?"))
    {
        command.type = CommandType::Help;
    }
    else
    {
        command.type = CommandType::Unknown;
    }

    return command;
}

const char *commandHelpText()
{
    return "cmds: STATUS/M114/M119 SAFETY/M122 CONFIG/CFG [KEY] SET KEY VALUE SAVE/SAVECFG RESETCFG REBOOT BOOTLOADER/RECOVERY BOOT/START DRIVER/TMC IRUN n IHOLD n IHOLDDELAY n SGTHRS n ENABLE/M17 DISABLE/M18/M84 HOLD on|off MOVEABS steps|G0 Xmm MOVEREL steps|JOG steps SETPOS n CYCLE n PRESSPOS n SIMLOAD n SIMTHRESH n SIMMECH on|off SIMSTALL on|off SIMCLEAR ZERO/TARE PANELPINS/PANEL HOME/G28 STOP/M112 BACKOFF HELP/?\r\n";
}

} // namespace keyswitch