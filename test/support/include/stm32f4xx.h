#ifndef STM32F4XX_H
#define STM32F4XX_H

#include <stdint.h>

typedef struct GPIO_TypeDef {
    uint32_t MODER;
    uint32_t OTYPER;
    uint32_t OSPEEDR;
    uint32_t PUPDR;
    uint32_t IDR;
    uint32_t BSRR;
} GPIO_TypeDef;

typedef struct RCC_TypeDef {
    uint32_t AHB1ENR;
} RCC_TypeDef;

extern GPIO_TypeDef *GPIOA;
extern GPIO_TypeDef *GPIOB;
extern GPIO_TypeDef *GPIOC;
extern GPIO_TypeDef *GPIOD;
extern GPIO_TypeDef *GPIOE;
extern GPIO_TypeDef *GPIOF;
extern GPIO_TypeDef *GPIOG;
extern GPIO_TypeDef *GPIOH;
extern RCC_TypeDef *RCC;

#define RCC_AHB1ENR_GPIOAEN 0x00000001U
#define RCC_AHB1ENR_GPIOBEN 0x00000002U
#define RCC_AHB1ENR_GPIOCEN 0x00000004U
#define RCC_AHB1ENR_GPIODEN 0x00000008U
#define RCC_AHB1ENR_GPIOEEN 0x00000010U
#define RCC_AHB1ENR_GPIOFEN 0x00000020U
#define RCC_AHB1ENR_GPIOGEN 0x00000040U
#define RCC_AHB1ENR_GPIOHEN 0x00000080U

#define __NOP() do { } while (0)
#define __DSB() do { } while (0)
#define __ISB() do { } while (0)
#define __disable_irq() do { } while (0)
#define __enable_irq() do { } while (0)

#ifdef __cplusplus
extern "C" {
#endif

void NVIC_SystemReset(void);

#ifdef __cplusplus
}
#endif

#endif