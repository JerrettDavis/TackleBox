#include "bootloader_protocol.h"

#include <string.h>

namespace {

static uint8_t is_space(char ch)
{
    return (ch == ' ') || (ch == '\t') || (ch == '\r') || (ch == '\n');
}

static char to_upper(char ch)
{
    if ((ch >= 'a') && (ch <= 'z'))
    {
        return (char)(ch - ('a' - 'A'));
    }
    return ch;
}

static uint8_t same_text(const char *lhs, const char *rhs)
{
    uint32_t index = 0U;

    while ((lhs[index] != 0) && (rhs[index] != 0))
    {
        if (lhs[index] != rhs[index])
        {
            return 0U;
        }
        ++index;
    }

    return (lhs[index] == 0) && (rhs[index] == 0);
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
    uint32_t index = 0U;

    if ((text == 0) || (value == 0) || (text[0] == 0))
    {
        return 0U;
    }

    while (text[index] != 0)
    {
        if ((text[index] < '0') || (text[index] > '9'))
        {
            return 0U;
        }

        parsed = (parsed * 10U) + (uint32_t)(text[index] - '0');
        ++index;
    }

    *value = parsed;
    return 1U;
}

} // namespace

BootloaderCommand bootloader_parse_command(const char *text)
{
    char normalized[32] = {0};
    char argument[32] = {0};
    char argument2[260] = {0};
    uint32_t index = 0U;
    BootloaderCommand command = {BootloaderCommandType::None, 0U, 0U, 0U, 0U, {0}};

    if (text == 0)
    {
        return command;
    }

    copy_token_upper(text, &index, normalized, sizeof(normalized));
    copy_token_upper(text, &index, argument, sizeof(argument));
    copy_token_upper(text, &index, argument2, sizeof(argument2));

    if (normalized[0] == 0)
    {
        return command;
    }

    if (same_text(normalized, "PING"))
    {
        command.type = BootloaderCommandType::Ping;
    }
    else if (same_text(normalized, "HELP") || same_text(normalized, "?"))
    {
        command.type = BootloaderCommandType::Help;
    }
    else if (same_text(normalized, "INFO"))
    {
        command.type = BootloaderCommandType::Info;
    }
    else if (same_text(normalized, "STATUS") || same_text(normalized, "DIAG"))
    {
        command.type = BootloaderCommandType::Status;
    }
    else if (same_text(normalized, "FLASH") || same_text(normalized, "MAP"))
    {
        command.type = BootloaderCommandType::Flash;
    }
    else if (same_text(normalized, "READ") || same_text(normalized, "DUMP"))
    {
        if ((parse_u32_token(argument, &command.offset) != 0U) &&
            (parse_u32_token(argument2, &command.size) != 0U))
        {
            command.type = BootloaderCommandType::Read;
            command.hasOffset = 1U;
            command.hasSize = 1U;
        }
        else
        {
            command.type = BootloaderCommandType::Unknown;
        }
    }
    else if (same_text(normalized, "ERASE"))
    {
        if (parse_u32_token(argument, &command.size) != 0U)
        {
            command.type = BootloaderCommandType::Erase;
            command.hasSize = 1U;
        }
        else
        {
            command.type = BootloaderCommandType::Unknown;
        }
    }
    else if (same_text(normalized, "WRITE"))
    {
        if ((parse_u32_token(argument, &command.offset) != 0U) && (argument2[0] != 0))
        {
            command.type = BootloaderCommandType::Write;
            command.hasOffset = 1U;
            for (uint32_t out = 0U; out < sizeof(command.text); ++out)
            {
                command.text[out] = argument2[out];
                if (argument2[out] == 0)
                {
                    break;
                }
            }
        }
        else
        {
            command.type = BootloaderCommandType::Unknown;
        }
    }
    else if (same_text(normalized, "CRC"))
    {
        if (parse_u32_token(argument, &command.size) != 0U)
        {
            command.type = BootloaderCommandType::Crc;
            command.hasSize = 1U;
        }
        else
        {
            command.type = BootloaderCommandType::Unknown;
        }
    }
    else if (same_text(normalized, "BOOT") || same_text(normalized, "START"))
    {
        command.type = BootloaderCommandType::Boot;
    }
    else if (same_text(normalized, "RESET"))
    {
        command.type = BootloaderCommandType::Reset;
    }
    else
    {
        command.type = BootloaderCommandType::Unknown;
    }

    return command;
}

const char *bootloader_command_help_text(void)
{
    return "BOOTLOADER HELP: PING HELP INFO STATUS/DIAG FLASH/MAP READ <offset> <size> ERASE <size> WRITE <offset> <hex> CRC <size> BOOT RESET\r\n";
}