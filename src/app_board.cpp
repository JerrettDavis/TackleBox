#include "app_board.h"

#include "stm32f4xx_hal.h"

#include <stdio.h>

extern "C" void SysTick_Handler(void)
{
    HAL_IncTick();
}

void delay_cycles(volatile uint32_t cycles)
{
    while (cycles--) __NOP();
}

GPIO_TypeDef *gpio_port_from_id(uint8_t port_id)
{
    switch ((GpioPortId)port_id)
    {
    case GpioPortId::A: return GPIOA;
    case GpioPortId::B: return GPIOB;
    case GpioPortId::C: return GPIOC;
    case GpioPortId::D: return GPIOD;
    case GpioPortId::E: return GPIOE;
    case GpioPortId::F: return GPIOF;
    case GpioPortId::G: return GPIOG;
    case GpioPortId::H: return GPIOH;
    default: return 0;
    }
}

void enable_gpio_clock(uint8_t port_id)
{
    switch ((GpioPortId)port_id)
    {
    case GpioPortId::A: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; break;
    case GpioPortId::B: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; break;
    case GpioPortId::C: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN; break;
    case GpioPortId::D: RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN; break;
    case GpioPortId::E: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN; break;
    case GpioPortId::F: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN; break;
    case GpioPortId::G: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOGEN; break;
    case GpioPortId::H: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN; break;
    default: break;
    }
}

uint8_t pin_assignment_valid(PinAssignment pin)
{
    return (gpio_port_from_id(pin.portId) != 0) && (pin.pin < 16U);
}

void pin_set_output(PinAssignment pin)
{
    GPIO_TypeDef *port = gpio_port_from_id(pin.portId);
    if (port == 0)
    {
        return;
    }

    port->MODER &= ~(3U << (pin.pin * 2U));
    port->MODER |=  (1U << (pin.pin * 2U));
    port->OTYPER &= ~(1U << pin.pin);
    port->PUPDR &= ~(3U << (pin.pin * 2U));
    port->OSPEEDR &= ~(3U << (pin.pin * 2U));
}

void pin_set_output_open_drain(PinAssignment pin)
{
    GPIO_TypeDef *port = gpio_port_from_id(pin.portId);
    if (port == 0)
    {
        return;
    }

    port->MODER &= ~(3U << (pin.pin * 2U));
    port->MODER |=  (1U << (pin.pin * 2U));
    port->OTYPER |= (1U << pin.pin);
    port->PUPDR &= ~(3U << (pin.pin * 2U));
    port->PUPDR |=  (1U << (pin.pin * 2U));
    port->OSPEEDR &= ~(3U << (pin.pin * 2U));
}

void set_pin_high(GPIO_TypeDef *port, uint32_t pin)
{
    port->BSRR = (1U << pin);
}

void set_pin_low(GPIO_TypeDef *port, uint32_t pin)
{
    port->BSRR = (1U << (pin + 16U));
}

uint32_t read_pin(GPIO_TypeDef *port, uint32_t pin)
{
    return (port->IDR >> pin) & 0x1U;
}

void pin_write_high(PinAssignment pin)
{
    GPIO_TypeDef *port = gpio_port_from_id(pin.portId);
    if (port != 0)
    {
        set_pin_high(port, pin.pin);
    }
}

void pin_write_low(PinAssignment pin)
{
    GPIO_TypeDef *port = gpio_port_from_id(pin.portId);
    if (port != 0)
    {
        set_pin_low(port, pin.pin);
    }
}

uint32_t pin_read(PinAssignment pin)
{
    GPIO_TypeDef *port = gpio_port_from_id(pin.portId);
    return (port == 0) ? 0U : read_pin(port, pin.pin);
}

void pin_set_input_pull(PinAssignment pin, uint8_t pull_up)
{
    GPIO_TypeDef *port = gpio_port_from_id(pin.portId);
    if (port == 0)
    {
        return;
    }

    port->MODER &= ~(3U << (pin.pin * 2U));
    port->PUPDR &= ~(3U << (pin.pin * 2U));
    port->PUPDR |= ((pull_up != 0U) ? 1U : 2U) << (pin.pin * 2U);
}

uint32_t sample_pin_baseline(PinAssignment pin)
{
    uint32_t sum = 0U;
    for (uint32_t i = 0U; i < 32U; ++i)
    {
        sum += pin_read(pin);
        delay_cycles(5000U);
    }
    return (sum >= 16U) ? 1U : 0U;
}

uint32_t checksum32(const void *data, uint32_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t hash = 2166136261UL;

    for (uint32_t i = 0U; i < length; ++i)
    {
        hash ^= bytes[i];
        hash *= 16777619UL;
    }

    return hash;
}

void enable_gpio_clocks_for_config(const PersistedFirmwareConfig &config)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    enable_gpio_clock((uint8_t)GpioPortId::A);
    enable_gpio_clock(channel.pins.uart.portId);
    enable_gpio_clock(channel.pins.dir.portId);
    enable_gpio_clock(channel.pins.step.portId);
    enable_gpio_clock(channel.pins.enable.portId);
    enable_gpio_clock(channel.pins.stop.portId);
    enable_gpio_clock(config.pins.diag0.portId);
    enable_gpio_clock(config.pins.diag2.portId);
    enable_gpio_clock(config.pins.psOn.portId);
    enable_gpio_clock(config.pins.safePower.portId);
    enable_gpio_clock(config.pins.led.portId);
    __DSB();
}

void format_pin_assignment(PinAssignment pin, char *buffer, uint32_t length)
{
    const char port_letter = (pin.portId >= (uint8_t)GpioPortId::A) ? (char)('A' + (pin.portId - (uint8_t)GpioPortId::A)) : '?';
    snprintf(buffer, length, "P%c%lu", port_letter, (unsigned long)pin.pin);
}