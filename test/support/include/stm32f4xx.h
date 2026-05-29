#ifndef STM32F4XX_H
#define STM32F4XX_H

#include <stdint.h>

typedef struct GPIO_TypeDef {
    uint32_t dummy;
} GPIO_TypeDef;

#ifdef __cplusplus
extern "C" {
#endif

void NVIC_SystemReset(void);

#ifdef __cplusplus
}
#endif

#endif