#ifndef BOOTLOADER_PROTOCOL_H
#define BOOTLOADER_PROTOCOL_H

#include <stdint.h>

enum class BootloaderCommandType : uint8_t {
    None = 0,
    Ping,
    Help,
    Info,
    Status,
    Flash,
    Read,
    Erase,
    Write,
    Crc,
    Boot,
    Reset,
    Unknown,
};

struct BootloaderCommand {
    BootloaderCommandType type;
    uint32_t offset;
    uint32_t size;
    uint8_t hasOffset;
    uint8_t hasSize;
    char text[260];
};

BootloaderCommand bootloader_parse_command(const char *text);
const char *bootloader_command_help_text(void);

#endif