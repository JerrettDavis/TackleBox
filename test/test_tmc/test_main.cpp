#include <stdexcept>
#include <iostream>

#include "keyswitch_tmc2209.h"

static void require_true(bool condition, const char *message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

static void test_tmc2209_config_values()
{
    keyswitch::Tmc2209Config config = {18U, 6U, 8U, 20U, 4U};

    require_true(keyswitch::makeTmc2209GconfValue() == 0x000001C0UL, "GCONF should enable pdn_disable, mstep_reg_select, and multistep_filt");
    require_true(keyswitch::makeTmc2209IholdIrunValue(config) == 0x00081206UL, "IHOLD_IRUN should pack ihold, irun, and iholddelay");
    require_true(keyswitch::makeTmc2209TpowerdownValue(config) == 20U, "TPOWERDOWN should use the configured value");
    require_true(keyswitch::makeTmc2209SgthrsValue(config) == 4U, "SGTHRS should use the configured value");
}

static void test_tmc2209_frame_encoding()
{
    uint8_t frame[8] = {0};
    const uint32_t length = keyswitch::encodeTmc2209WriteFrame(0U, keyswitch::Tmc2209Register::IholdIrun, 0x00081206UL, frame, sizeof(frame));

    require_true(length == 8U, "TMC2209 write frame should be 8 bytes");
    require_true(frame[0] == 0x05U, "frame should start with sync byte");
    require_true(frame[1] == 0x00U, "frame should target slave address 0");
    require_true(frame[2] == 0x90U, "frame should set write bit on the IHOLD_IRUN register");
    require_true(frame[3] == 0x00U && frame[4] == 0x08U && frame[5] == 0x12U && frame[6] == 0x06U, "frame payload should be big-endian register data");
    require_true(frame[7] == keyswitch::tmc2209Crc8(frame, 7U), "frame crc should match helper calculation");
}

static void test_tmc2209_read_frame_and_reply_decoding()
{
    uint8_t request[4] = {0};
    const uint32_t requestLength = keyswitch::encodeTmc2209ReadFrame(0U, keyswitch::Tmc2209Register::Ifcnt, request, sizeof(request));
    require_true(requestLength == 4U, "TMC2209 read frame should be 4 bytes");
    require_true(request[0] == 0x05U, "read frame should start with sync byte");
    require_true(request[1] == 0x00U, "read frame should target slave address 0");
    require_true(request[2] == 0x02U, "read frame should target the IFCNT register");
    require_true(request[3] == keyswitch::tmc2209Crc8(request, 3U), "read frame crc should match helper calculation");

    uint8_t reply[8] = {0x05U, 0xFFU, 0x10U, 0x00U, 0x08U, 0x12U, 0x06U, 0x00U};
    reply[7] = keyswitch::tmc2209Crc8(reply, 7U);

    uint32_t value = 0U;
    require_true(keyswitch::decodeTmc2209ReadReply(keyswitch::Tmc2209Register::IholdIrun, reply, sizeof(reply), &value) == 1U, "valid read reply should decode");
    require_true(value == 0x00081206UL, "decoded read value should preserve the register payload");

    reply[7] ^= 0x01U;
    require_true(keyswitch::decodeTmc2209ReadReply(keyswitch::Tmc2209Register::IholdIrun, reply, sizeof(reply), &value) == 0U, "crc mismatch should fail read reply decoding");
}

int main()
{
    try
    {
        test_tmc2209_config_values();
        test_tmc2209_frame_encoding();
        test_tmc2209_read_frame_and_reply_decoding();
        std::cout << "PASS test_tmc" << std::endl;
        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "FAIL test_tmc: " << ex.what() << std::endl;
        return 1;
    }
}