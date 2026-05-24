#ifndef KEYSWITCH_TMC2209_H
#define KEYSWITCH_TMC2209_H

#include <stdint.h>

namespace keyswitch {

enum class Tmc2209Register : uint8_t {
    Gconf = 0x00,
    Ifcnt = 0x02,
    IholdIrun = 0x10,
    Tpowerdown = 0x11,
    Sgthrs = 0x40,
};

struct Tmc2209Config {
    uint8_t irun;
    uint8_t ihold;
    uint8_t iholddelay;
    uint8_t tpowerdown;
    uint8_t sgthrs;
};

uint8_t clampTmcCurrentScale(int32_t value);
uint32_t makeTmc2209GconfValue(void);
uint32_t makeTmc2209IholdIrunValue(const Tmc2209Config &config);
uint32_t makeTmc2209TpowerdownValue(const Tmc2209Config &config);
uint32_t makeTmc2209SgthrsValue(const Tmc2209Config &config);
uint8_t tmc2209Crc8(const uint8_t *data, uint32_t length);
uint32_t encodeTmc2209ReadFrame(uint8_t slaveAddress, Tmc2209Register reg, uint8_t *out, uint32_t outLen);
uint32_t encodeTmc2209WriteFrame(uint8_t slaveAddress, Tmc2209Register reg, uint32_t value, uint8_t *out, uint32_t outLen);
uint8_t decodeTmc2209ReadReply(Tmc2209Register reg, const uint8_t *frame, uint32_t length, uint32_t *value);

} // namespace keyswitch

#endif