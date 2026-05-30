#include <iostream>
#include <stdexcept>
#include <string>

#include "bootloader_protocol.h"

static void require_true(bool condition, const char *message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

static void test_bootloader_protocol_parses_core_commands()
{
    require_true(bootloader_parse_command("ping").type == BootloaderCommandType::Ping, "ping should map to Ping");
    require_true(bootloader_parse_command("help").type == BootloaderCommandType::Help, "help should map to Help");
    require_true(bootloader_parse_command("info").type == BootloaderCommandType::Info, "info should map to Info");
    require_true(bootloader_parse_command("diag").type == BootloaderCommandType::Status, "diag should map to Status");
    require_true(bootloader_parse_command("map").type == BootloaderCommandType::Flash, "map should map to Flash");
    require_true(bootloader_parse_command("boot").type == BootloaderCommandType::Boot, "boot should map to Boot");
    require_true(bootloader_parse_command("reset").type == BootloaderCommandType::Reset, "reset should map to Reset");
    require_true(bootloader_parse_command("estop").type == BootloaderCommandType::EmergencyStop, "estop should map to EmergencyStop");
    require_true(bootloader_parse_command("estopclear").type == BootloaderCommandType::EmergencyClear, "estopclear should map to EmergencyClear");
}

static void test_bootloader_protocol_parses_argument_commands()
{
    const BootloaderCommand read = bootloader_parse_command("read 128 16");
    require_true(read.type == BootloaderCommandType::Read, "read should map to Read");
    require_true(read.hasOffset == 1U, "read should carry an offset");
    require_true(read.hasSize == 1U, "read should carry a size");
    require_true(read.offset == 128U, "read should parse the requested offset");
    require_true(read.size == 16U, "read should parse the requested size");

    const BootloaderCommand erase = bootloader_parse_command("erase 4096");
    require_true(erase.type == BootloaderCommandType::Erase, "erase should map to Erase");
    require_true(erase.size == 4096U, "erase should parse size");

    const BootloaderCommand write = bootloader_parse_command("write 64 deadbeef");
    require_true(write.type == BootloaderCommandType::Write, "write should map to Write");
    require_true(write.offset == 64U, "write should parse offset");
    require_true(std::string(write.text) == "DEADBEEF", "write should normalize hex payload to uppercase");

    const BootloaderCommand crc = bootloader_parse_command("crc 1024");
    require_true(crc.type == BootloaderCommandType::Crc, "crc should map to Crc");
    require_true(crc.size == 1024U, "crc should parse size");
}

static void test_bootloader_protocol_rejects_bad_commands()
{
    require_true(bootloader_parse_command("").type == BootloaderCommandType::None, "empty input should map to None");
    require_true(bootloader_parse_command("read 10").type == BootloaderCommandType::Unknown, "read requires offset and size");
    require_true(bootloader_parse_command("erase nope").type == BootloaderCommandType::Unknown, "erase requires numeric size");
    require_true(bootloader_parse_command("write 10").type == BootloaderCommandType::Unknown, "write requires payload");
}

int main()
{
    try
    {
        test_bootloader_protocol_parses_core_commands();
        test_bootloader_protocol_parses_argument_commands();
        test_bootloader_protocol_rejects_bad_commands();
        std::cout << "PASS test_bootloader_protocol" << std::endl;
        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "FAIL test_bootloader_protocol: " << ex.what() << std::endl;
        return 1;
    }
}