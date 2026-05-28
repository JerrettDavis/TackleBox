#include "app_panel.h"

#include "app_board.h"

namespace {

static uint8_t pin_active_low(PinAssignment pin)
{
    return pin_read(pin) == 0U ? 1U : 0U;
}

static void neopixel_delay_short(uint32_t cycles)
{
    delay_cycles(cycles);
}

static void neopixel_write_bit(PinAssignment pin, uint8_t one)
{
    pin_write_high(pin);
    if (one != 0U)
    {
        neopixel_delay_short(100U);
        pin_write_low(pin);
        neopixel_delay_short(70U);
    }
    else
    {
        neopixel_delay_short(35U);
        pin_write_low(pin);
        neopixel_delay_short(120U);
    }
}

static void neopixel_write_byte(PinAssignment pin, uint8_t value)
{
    for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U)
    {
        neopixel_write_bit(pin, (value & mask) != 0U ? 1U : 0U);
    }
}

static void neopixel_write_rgb(PinAssignment pin, uint8_t red, uint8_t green, uint8_t blue)
{
    neopixel_write_byte(pin, green);
    neopixel_write_byte(pin, red);
    neopixel_write_byte(pin, blue);
}

static uint8_t panel_color_max(uint8_t a, uint8_t b, uint8_t c)
{
    uint8_t value = a;
    if (b > value)
    {
        value = b;
    }
    if (c > value)
    {
        value = c;
    }
    return value;
}

static void mini12864_panel_write_color(PinAssignment pin, uint8_t red, uint8_t green, uint8_t blue)
{
    if (pin_assignment_valid(pin) == 0U)
    {
        return;
    }

    const uint8_t peak = panel_color_max(red, green, blue);
    if (peak != 0U)
    {
        red = (uint8_t)(((uint16_t)red * 255U) / peak);
        green = (uint8_t)(((uint16_t)green * 255U) / peak);
        blue = (uint8_t)(((uint16_t)blue * 255U) / peak);
    }

    __disable_irq();
    for (uint8_t index = 0U; index < 8U; ++index)
    {
        neopixel_write_rgb(pin, red, green, blue);
    }
    __enable_irq();
    delay_cycles(12000U);
}

}

Mini12864PanelPins mini12864_panel_pins(void)
{
    Mini12864PanelPins pins = {};
    pins.beeper = {(uint8_t)GpioPortId::C, 5U};
    pins.click = {(uint8_t)GpioPortId::B, 0U};
    pins.encoderA = {(uint8_t)GpioPortId::E, 7U};
    pins.encoderB = {(uint8_t)GpioPortId::B, 2U};
    pins.cs = {(uint8_t)GpioPortId::B, 1U};
    pins.a0 = {(uint8_t)GpioPortId::E, 9U};
    pins.reset = {(uint8_t)GpioPortId::E, 10U};
    pins.neopixel = {(uint8_t)GpioPortId::E, 11U};
    pins.sck = {(uint8_t)GpioPortId::A, 5U};
    pins.miso = {(uint8_t)GpioPortId::A, 6U};
    pins.mosi = {(uint8_t)GpioPortId::A, 7U};
    return pins;
}

void mini12864_panel_init_inputs(const Mini12864PanelPins &pins)
{
    enable_gpio_clock(pins.beeper.portId);
    enable_gpio_clock(pins.click.portId);
    enable_gpio_clock(pins.encoderA.portId);
    enable_gpio_clock(pins.encoderB.portId);
    enable_gpio_clock(pins.neopixel.portId);
    __DSB();

    pin_set_output(pins.beeper);
    pin_set_output(pins.neopixel);
    pin_write_low(pins.beeper);
    pin_write_low(pins.neopixel);
    delay_cycles(12000U);
    mini12864_panel_write_color(pins.neopixel, 0x30U, 0x30U, 0x30U);

    pin_set_input_pull(pins.click, 1U);
    pin_set_input_pull(pins.encoderA, 1U);
    pin_set_input_pull(pins.encoderB, 1U);
}

Mini12864PanelInputs mini12864_panel_read_inputs(const Mini12864PanelPins &pins)
{
    Mini12864PanelInputs inputs = {};
    inputs.clickPressed = pin_active_low(pins.click);
    inputs.encoderAActive = pin_active_low(pins.encoderA);
    inputs.encoderBActive = pin_active_low(pins.encoderB);
    return inputs;
}

void mini12864_panel_set_color(const Mini12864PanelPins &pins, uint8_t red, uint8_t green, uint8_t blue)
{
    mini12864_panel_write_color(pins.neopixel, red, green, blue);
}