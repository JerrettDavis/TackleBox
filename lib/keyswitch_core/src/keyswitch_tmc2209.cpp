#include "keyswitch_tmc2209.h"

namespace keyswitch {

namespace {

constexpr uint8_t kTmc2209RequestSyncByte = 0xF5U;
constexpr uint8_t kTmc2209ReplySyncByte = 0x05U;

}

uint8_t clampTmcCurrentScale(int32_t value)
{
    if (value < 0)
    {
        return 0U;
    }

    if (value > 31)
    {
        return 31U;
    }

    return (uint8_t)value;
}

uint32_t makeTmc2209GconfValue(void)
{
    return (1UL << 6) | (1UL << 7) | (1UL << 8);
}

uint32_t makeTmc2209IholdIrunValue(const Tmc2209Config &config)
{
    return (uint32_t)(config.ihold & 0x1FU)
        | ((uint32_t)(config.irun & 0x1FU) << 8)
        | ((uint32_t)(config.iholddelay & 0x0FU) << 16);
}

uint32_t makeTmc2209TpowerdownValue(const Tmc2209Config &config)
{
    return (uint32_t)config.tpowerdown;
}

uint32_t makeTmc2209SgthrsValue(const Tmc2209Config &config)
{
    return (uint32_t)config.sgthrs;
}

uint8_t tmc2209Crc8(const uint8_t *data, uint32_t length)
{
    uint8_t crc = 0U;

    if (data == 0)
    {
        return 0U;
    }

    for (uint32_t index = 0U; index < length; ++index)
    {
        uint8_t current = data[index];
        for (uint8_t bit = 0U; bit < 8U; ++bit)
        {
            if (((crc >> 7) ^ (current & 0x01U)) != 0U)
            {
                crc = (uint8_t)((crc << 1) ^ 0x07U);
            }
            else
            {
                crc = (uint8_t)(crc << 1);
            }
            current >>= 1;
        }
    }

    return crc;
}

uint32_t encodeTmc2209ReadFrame(uint8_t slaveAddress, Tmc2209Register reg, uint8_t *out, uint32_t outLen)
{
    if ((out == 0) || (outLen < 4U))
    {
        return 0U;
    }

    out[0] = kTmc2209RequestSyncByte;
    out[1] = slaveAddress;
    out[2] = (uint8_t)reg & 0x7FU;
    out[3] = tmc2209Crc8(out, 3U);
    return 4U;
}

uint32_t encodeTmc2209WriteFrame(uint8_t slaveAddress, Tmc2209Register reg, uint32_t value, uint8_t *out, uint32_t outLen)
{
    if ((out == 0) || (outLen < 8U))
    {
        return 0U;
    }

    out[0] = kTmc2209RequestSyncByte;
    out[1] = slaveAddress;
    out[2] = (uint8_t)reg | 0x80U;
    out[3] = (uint8_t)((value >> 24) & 0xFFU);
    out[4] = (uint8_t)((value >> 16) & 0xFFU);
    out[5] = (uint8_t)((value >> 8) & 0xFFU);
    out[6] = (uint8_t)(value & 0xFFU);
    out[7] = tmc2209Crc8(out, 7U);
    return 8U;
}

uint8_t decodeTmc2209ReadReply(Tmc2209Register reg, const uint8_t *frame, uint32_t length, uint32_t *value)
{
    if ((frame == 0) || (value == 0) || (length < 8U))
    {
        return 0U;
    }

    if ((frame[0] != kTmc2209ReplySyncByte) || (frame[1] != 0xFFU) || (frame[2] != ((uint8_t)reg & 0x7FU)))
    {
        return 0U;
    }

    if (frame[7] != tmc2209Crc8(frame, 7U))
    {
        return 0U;
    }

    *value = ((uint32_t)frame[3] << 24)
        | ((uint32_t)frame[4] << 16)
        | ((uint32_t)frame[5] << 8)
        | (uint32_t)frame[6];
    return 1U;
}

} // namespace keyswitch