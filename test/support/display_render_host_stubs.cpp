#include "app_board.h"
#include "app_panel.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

namespace {

GPIO_TypeDef g_gpio_a = {};
GPIO_TypeDef g_gpio_b = {};
GPIO_TypeDef g_gpio_c = {};
GPIO_TypeDef g_gpio_d = {};
GPIO_TypeDef g_gpio_e = {};
GPIO_TypeDef g_gpio_f = {};
GPIO_TypeDef g_gpio_g = {};
GPIO_TypeDef g_gpio_h = {};
RCC_TypeDef g_rcc = {};

}

GPIO_TypeDef *GPIOA = &g_gpio_a;
GPIO_TypeDef *GPIOB = &g_gpio_b;
GPIO_TypeDef *GPIOC = &g_gpio_c;
GPIO_TypeDef *GPIOD = &g_gpio_d;
GPIO_TypeDef *GPIOE = &g_gpio_e;
GPIO_TypeDef *GPIOF = &g_gpio_f;
GPIO_TypeDef *GPIOG = &g_gpio_g;
GPIO_TypeDef *GPIOH = &g_gpio_h;
RCC_TypeDef *RCC = &g_rcc;

void HAL_Delay(uint32_t delay_ms)
{
    (void)delay_ms;
}

void delay_cycles(volatile uint32_t cycles)
{
    while (cycles-- > 0U)
    {
    }
}

void enable_gpio_clock(uint8_t port_id)
{
    (void)port_id;
}

void pin_set_output(PinAssignment pin)
{
    (void)pin;
}

void pin_set_output_open_drain(PinAssignment pin)
{
    (void)pin;
}

void set_pin_high(GPIO_TypeDef *port, uint32_t pin)
{
    (void)port;
    (void)pin;
}

void set_pin_low(GPIO_TypeDef *port, uint32_t pin)
{
    (void)port;
    (void)pin;
}

uint32_t read_pin(GPIO_TypeDef *port, uint32_t pin)
{
    (void)port;
    (void)pin;
    return 0U;
}

void pin_write_high(PinAssignment pin)
{
    (void)pin;
}

void pin_write_low(PinAssignment pin)
{
    (void)pin;
}

uint32_t pin_read(PinAssignment pin)
{
    (void)pin;
    return 0U;
}

void pin_set_input_pull(PinAssignment pin, uint8_t pull_up)
{
    (void)pin;
    (void)pull_up;
}

uint32_t sample_pin_baseline(PinAssignment pin)
{
    (void)pin;
    return 0U;
}

void enable_gpio_clocks_for_config(const PersistedFirmwareConfig &config)
{
    (void)config;
}

Mini12864PanelPins mini12864_panel_pins(void)
{
    return {};
}

void mini12864_panel_init_inputs(const Mini12864PanelPins &pins)
{
    (void)pins;
}

Mini12864PanelInputs mini12864_panel_read_inputs(const Mini12864PanelPins &pins)
{
    (void)pins;
    return {};
}

void mini12864_panel_set_color(const Mini12864PanelPins &pins, uint8_t red, uint8_t green, uint8_t blue)
{
    (void)pins;
    (void)red;
    (void)green;
    (void)blue;
}