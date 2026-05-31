#include "boot_panel_splash.h"

#include "app_config.h"

#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

#include <string.h>

namespace {

struct Mini12864BootPins {
    PinAssignment cs;
    PinAssignment a0;
    PinAssignment reset;
    PinAssignment neopixel;
    PinAssignment sck;
    PinAssignment mosi;
};

static const Mini12864BootPins kPanelPins = {
    {(uint8_t)GpioPortId::B, 1U},
    {(uint8_t)GpioPortId::E, 9U},
    {(uint8_t)GpioPortId::E, 10U},
    {(uint8_t)GpioPortId::E, 11U},
    {(uint8_t)GpioPortId::A, 5U},
    {(uint8_t)GpioPortId::A, 7U},
};

static uint8_t g_framebuffer[128U * 8U] = {0};
static uint8_t g_panel_ready = 0U;
static DisplayValidationReport *g_active_validation_report = 0;

static GPIO_TypeDef *gpio_port_from_id(uint8_t port_id)
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

static void enable_gpio_clock(uint8_t port_id)
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

static void delay_cycles(volatile uint32_t cycles)
{
    while (cycles--) __NOP();
}

static void pin_set_output(PinAssignment pin)
{
    GPIO_TypeDef *port = gpio_port_from_id(pin.portId);
    if (port == 0)
    {
        return;
    }

    port->MODER &= ~(3U << (pin.pin * 2U));
    port->MODER |= (1U << (pin.pin * 2U));
    port->OTYPER &= ~(1U << pin.pin);
    port->PUPDR &= ~(3U << (pin.pin * 2U));
    port->OSPEEDR &= ~(3U << (pin.pin * 2U));
}

static void pin_write_high(PinAssignment pin)
{
    GPIO_TypeDef *port = gpio_port_from_id(pin.portId);
    if (port != 0)
    {
        port->BSRR = (1U << pin.pin);
    }
}

static void pin_write_low(PinAssignment pin)
{
    GPIO_TypeDef *port = gpio_port_from_id(pin.portId);
    if (port != 0)
    {
        port->BSRR = (1U << (pin.pin + 16U));
    }
}

static void display_delay(void)
{
    delay_cycles(64U);
}

static void display_select(void)
{
    pin_write_low(kPanelPins.cs);
    display_delay();
}

static void display_deselect(void)
{
    display_delay();
    pin_write_high(kPanelPins.cs);
}

static void display_write_byte(uint8_t value)
{
    for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U)
    {
        if ((value & mask) != 0U)
        {
            pin_write_high(kPanelPins.mosi);
        }
        else
        {
            pin_write_low(kPanelPins.mosi);
        }

        display_delay();
        pin_write_high(kPanelPins.sck);
        display_delay();
        pin_write_low(kPanelPins.sck);
    }
}

static void display_write_command(uint8_t command)
{
    pin_write_low(kPanelPins.a0);
    display_select();
    display_write_byte(command);
    display_deselect();
}

static void display_write_data(const uint8_t *data, uint32_t length)
{
    pin_write_high(kPanelPins.a0);
    display_select();
    for (uint32_t index = 0U; index < length; ++index)
    {
        display_write_byte(data[index]);
    }
    display_deselect();
}

static void display_flush(void)
{
    for (uint8_t page = 0U; page < 8U; ++page)
    {
        display_write_command((uint8_t)(0xB0U | page));
        display_write_command(0x10U);
        display_write_command(0x00U);
        display_write_data(&g_framebuffer[(uint32_t)page * 128U], 128U);
    }
}

static void framebuffer_clear(void)
{
    memset(g_framebuffer, 0, sizeof(g_framebuffer));
}

static void framebuffer_set_pixel(uint8_t x, uint8_t y)
{
    if ((x >= 128U) || (y >= 64U))
    {
        return;
    }

    g_framebuffer[(uint32_t)(y / 8U) * 128U + x] |= (uint8_t)(1U << (y & 0x7U));
}

static void framebuffer_draw_hline(uint8_t x, uint8_t y, uint8_t length)
{
    for (uint8_t offset = 0U; offset < length; ++offset)
    {
        framebuffer_set_pixel((uint8_t)(x + offset), y);
    }
}

static void framebuffer_draw_vline(uint8_t x, uint8_t y, uint8_t length)
{
    for (uint8_t offset = 0U; offset < length; ++offset)
    {
        framebuffer_set_pixel(x, (uint8_t)(y + offset));
    }
}

static void framebuffer_draw_keyswitch_logo(uint8_t x, uint8_t y);

static void framebuffer_draw_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    if ((width == 0U) || (height == 0U))
    {
        return;
    }

    framebuffer_draw_hline(x, y, width);
    framebuffer_draw_hline(x, (uint8_t)(y + height - 1U), width);
    framebuffer_draw_vline(x, y, height);
    framebuffer_draw_vline((uint8_t)(x + width - 1U), y, height);
}

static const uint8_t *glyph_for_char(char c)
{
    static const uint8_t blank[5] = {0, 0, 0, 0, 0};
    static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
    static const uint8_t plus[5] = {0x08, 0x08, 0x3E, 0x08, 0x08};
    static const uint8_t digits[10][5] = {
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
        {0x62, 0x51, 0x49, 0x49, 0x46}, {0x22, 0x49, 0x49, 0x49, 0x36},
        {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x2F, 0x49, 0x49, 0x49, 0x31},
        {0x3E, 0x49, 0x49, 0x49, 0x32}, {0x01, 0x71, 0x09, 0x05, 0x03},
        {0x36, 0x49, 0x49, 0x49, 0x36}, {0x26, 0x49, 0x49, 0x49, 0x3E},
    };
    static const uint8_t letters[26][5] = {
        {0x7E, 0x09, 0x09, 0x09, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36},
        {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x22, 0x1C},
        {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
        {0x3E, 0x41, 0x49, 0x49, 0x3A}, {0x7F, 0x08, 0x08, 0x08, 0x7F},
        {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3F, 0x01},
        {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
        {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F},
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06},
        {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
        {0x26, 0x49, 0x49, 0x49, 0x32}, {0x01, 0x01, 0x7F, 0x01, 0x01},
        {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F},
        {0x7F, 0x20, 0x18, 0x20, 0x7F}, {0x63, 0x14, 0x08, 0x14, 0x63},
        {0x03, 0x04, 0x78, 0x04, 0x03}, {0x61, 0x51, 0x49, 0x45, 0x43},
    };

    if ((c >= '0') && (c <= '9')) return digits[c - '0'];
    if ((c >= 'A') && (c <= 'Z')) return letters[c - 'A'];
    if (c == '-') return dash;
    if (c == '+') return plus;
    if (c == ' ') return blank;
    return blank;
}

static void framebuffer_draw_char(uint8_t x, uint8_t y, char c)
{
    const uint8_t *glyph = glyph_for_char(c);
    for (uint8_t col = 0U; col < 5U; ++col)
    {
        const uint8_t bits = glyph[col];
        for (uint8_t row = 0U; row < 7U; ++row)
        {
            if ((bits & (1U << row)) != 0U)
            {
                framebuffer_set_pixel((uint8_t)(x + col), (uint8_t)(y + row));
            }
        }
    }
}

static void framebuffer_draw_text(uint8_t x, uint8_t y, const char *text)
{
    uint8_t cursor_x = x;
    for (uint32_t index = 0U; (text != 0) && (text[index] != 0); ++index)
    {
        framebuffer_draw_char(cursor_x, y, text[index]);
        cursor_x = (uint8_t)(cursor_x + 6U);
        if (cursor_x >= 122U)
        {
            break;
        }
    }
}

static void framebuffer_draw_centered_text(uint8_t y, const char *text)
{
    uint32_t length = 0U;
    uint8_t x = 0U;

    if (text == 0)
    {
        return;
    }

    while (text[length] != 0)
    {
        ++length;
    }

    if (length < 21U)
    {
        x = (uint8_t)((128U - (length * 6U)) / 2U);
    }

    framebuffer_draw_text(x, y, text);
}

static uint8_t text_pixel_width(const char *text)
{
    uint32_t length = 0U;
    if (text == 0)
    {
        return 0U;
    }

    while (text[length] != 0)
    {
        ++length;
    }

    return (uint8_t)(length * 6U);
}

#ifdef KEYSWITCH_HOST_TEST
static void validation_begin(DisplayValidationReport *report)
{
    g_active_validation_report = report;
    if (report != 0)
    {
        display_validation_reset(report);
    }
}

static void validation_end(void)
{
    g_active_validation_report = 0;
}
#endif

static int16_t validation_add_rect(const char *name, uint8_t x, uint8_t y, uint8_t width, uint8_t height, int16_t parent_index, uint8_t allow_overlap)
{
    if (g_active_validation_report == 0)
    {
        return -1;
    }

    return display_validation_add_rect(g_active_validation_report, name, x, y, width, height, parent_index, allow_overlap);
}

static int16_t validation_add_text(const char *name, uint8_t x, uint8_t y, const char *text, int16_t parent_index)
{
    return validation_add_rect(name, x, y, text_pixel_width(text), 7U, parent_index, 0U);
}

static int16_t validation_add_centered_text(const char *name, uint8_t y, const char *text, int16_t parent_index)
{
    const uint8_t width = text_pixel_width(text);
    const uint8_t x = (width < 126U) ? (uint8_t)((128U - width) / 2U) : 0U;
    return validation_add_text(name, x, y, text, parent_index);
}

static void render_boot_splash_frame(const char *title, const char *subtitle)
{
    const int16_t outer_rect = validation_add_rect("boot.outer", 0U, 0U, 128U, 64U, -1, 0U);
    const int16_t inner_rect = validation_add_rect("boot.inner", 4U, 4U, 120U, 56U, outer_rect, 0U);
    validation_add_rect("boot.logo", 49U, 8U, 30U, 22U, inner_rect, 0U);
    validation_add_rect("boot.rule.top", 14U, 34U, 100U, 1U, inner_rect, 0U);
    validation_add_rect("boot.rule.bottom", 20U, 50U, 88U, 1U, inner_rect, 0U);
    validation_add_centered_text("boot.title", 38U, (title != 0) ? title : "BOOT", inner_rect);
    validation_add_centered_text("boot.subtitle", 52U, (subtitle != 0) ? subtitle : "STARTING", inner_rect);
    validation_add_text("boot.skr2", 10U, 22U, "SKR2", inner_rect);
    validation_add_text("boot.key", 81U, 22U, "KEY", inner_rect);

    framebuffer_clear();
    framebuffer_draw_rect(0U, 0U, 128U, 64U);
    framebuffer_draw_rect(4U, 4U, 120U, 56U);
    framebuffer_draw_keyswitch_logo(49U, 8U);
    framebuffer_draw_hline(14U, 34U, 100U);
    framebuffer_draw_hline(20U, 50U, 88U);
    framebuffer_draw_centered_text(38U, (title != 0) ? title : "BOOT");
    framebuffer_draw_centered_text(52U, (subtitle != 0) ? subtitle : "STARTING");
    framebuffer_draw_text(10U, 22U, "SKR2");
    framebuffer_draw_text(81U, 22U, "KEY");
}

static void framebuffer_fill_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    for (uint8_t row = 0U; row < height; ++row)
    {
        for (uint8_t col = 0U; col < width; ++col)
        {
            framebuffer_set_pixel((uint8_t)(x + col), (uint8_t)(y + row));
        }
    }
}

static void framebuffer_draw_keyswitch_logo(uint8_t x, uint8_t y)
{
    framebuffer_draw_rect(x, y, 30U, 22U);
    framebuffer_draw_rect((uint8_t)(x + 3U), (uint8_t)(y + 3U), 24U, 16U);
    framebuffer_fill_rect((uint8_t)(x + 11U), (uint8_t)(y + 5U), 8U, 8U);
    framebuffer_draw_hline((uint8_t)(x + 6U), (uint8_t)(y + 16U), 18U);
    framebuffer_draw_vline((uint8_t)(x + 9U), (uint8_t)(y + 15U), 5U);
    framebuffer_draw_vline((uint8_t)(x + 20U), (uint8_t)(y + 15U), 5U);
    framebuffer_draw_hline((uint8_t)(x + 2U), (uint8_t)(y + 21U), 26U);
}

static void neopixel_write_bit(uint8_t one)
{
    pin_write_high(kPanelPins.neopixel);
    if (one != 0U)
    {
        delay_cycles(100U);
        pin_write_low(kPanelPins.neopixel);
        delay_cycles(70U);
    }
    else
    {
        delay_cycles(35U);
        pin_write_low(kPanelPins.neopixel);
        delay_cycles(120U);
    }
}

static void neopixel_write_byte(uint8_t value)
{
    for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U)
    {
        neopixel_write_bit((value & mask) != 0U ? 1U : 0U);
    }
}

static void neopixel_write_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    neopixel_write_byte(green);
    neopixel_write_byte(red);
    neopixel_write_byte(blue);
}

static void panel_set_lighting(void)
{
    __disable_irq();
    for (uint8_t index = 0U; index < 8U; ++index)
    {
        neopixel_write_rgb(0x08U, 0x18U, 0x20U);
    }
    __enable_irq();
    delay_cycles(12000U);
}

}

void boot_panel_splash_init(void)
{
    enable_gpio_clock(kPanelPins.cs.portId);
    enable_gpio_clock(kPanelPins.a0.portId);
    enable_gpio_clock(kPanelPins.reset.portId);
    enable_gpio_clock(kPanelPins.neopixel.portId);
    enable_gpio_clock(kPanelPins.sck.portId);
    enable_gpio_clock(kPanelPins.mosi.portId);
    __DSB();

    pin_set_output(kPanelPins.cs);
    pin_set_output(kPanelPins.a0);
    pin_set_output(kPanelPins.reset);
    pin_set_output(kPanelPins.neopixel);
    pin_set_output(kPanelPins.sck);
    pin_set_output(kPanelPins.mosi);

    pin_write_high(kPanelPins.cs);
    pin_write_low(kPanelPins.sck);
    pin_write_low(kPanelPins.mosi);
    pin_write_high(kPanelPins.a0);
    pin_write_low(kPanelPins.neopixel);
    delay_cycles(12000U);
    panel_set_lighting();

    pin_write_high(kPanelPins.reset);
    HAL_Delay(2U);
    pin_write_low(kPanelPins.reset);
    HAL_Delay(2U);
    pin_write_high(kPanelPins.reset);
    HAL_Delay(8U);

    display_write_command(0xE2U);
    display_write_command(0xAEU);
    display_write_command(0x40U);
    display_write_command(0xA0U);
    display_write_command(0xC8U);
    display_write_command(0xA6U);
    display_write_command(0xA2U);
    display_write_command(0x2FU);
    display_write_command(0xF8U);
    display_write_command(0x00U);
    display_write_command(0x23U);
    display_write_command(0x81U);
    display_write_command(0x3FU);
    display_write_command(0xACU);
    display_write_command(0xA4U);
    display_write_command(0xAFU);

    g_panel_ready = 1U;
}

void boot_panel_splash_show(const char *title, const char *subtitle)
{
    if (g_panel_ready == 0U)
    {
        return;
    }

    render_boot_splash_frame(title, subtitle);
    display_flush();
}

#ifdef KEYSWITCH_HOST_TEST
void boot_panel_splash_host_render(const char *title, const char *subtitle, BootPanelRenderSnapshot *snapshot)
{
    if (snapshot == 0)
    {
        return;
    }

    validation_begin(&snapshot->validation);
    render_boot_splash_frame(title, subtitle);
    memcpy(snapshot->framebuffer, g_framebuffer, sizeof(g_framebuffer));
    validation_end();
}
#endif