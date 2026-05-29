#include "app_display.h"

#include "app_board.h"
#include "load_cell.h"

#include <stdio.h>
#include <string.h>

namespace {

enum class UiScreen : uint8_t {
    Dashboard = 0,
    Root,
    Motion,
    Measure,
    Driver,
    Config,
};

struct UiState {
    UiScreen screen;
    uint8_t dashboardPage;
    uint8_t cursor;
    uint8_t scroll;
    uint8_t editing;
    int32_t editValue;
    int32_t jogUm;
    int32_t cycleCount;
    uint8_t initialized;
};

static uint8_t g_framebuffer[128U * 8U] = {0};
static uint32_t g_last_render_ms = 0U;
static uint32_t g_splash_deadline_ms = 0U;
static uint8_t g_display_ready = 0U;
static uint8_t g_prev_click = 0U;
static uint8_t g_prev_encoder_state = 0U;
static int8_t g_encoder_accumulator = 0;
static uint8_t g_display_probe_mode = 0U;
static uint8_t g_last_panel_red = 0xFFU;
static uint8_t g_last_panel_green = 0xFFU;
static uint8_t g_last_panel_blue = 0xFFU;
static uint32_t g_load_display_zero_raw = 0U;
static uint8_t g_load_display_zero_valid = 0U;
static uint8_t g_load_graph_samples[60] = {0};
static uint8_t g_load_graph_head = 0U;
static uint8_t g_display_flush_page = 0U;
static uint8_t g_display_frame_pending = 0U;
static UiState g_ui = {UiScreen::Dashboard, 0U, 0U, 0U, 0U, 0, 1000, 1, 0U};

static const uint8_t ROOT_ITEM_COUNT = 7U;
static const uint8_t MOTION_ITEM_COUNT = 10U;
static const uint8_t MEASURE_ITEM_COUNT = 4U;
static const uint8_t DRIVER_ITEM_COUNT = 6U;
static const uint8_t CONFIG_ITEM_COUNT = 11U;
static const uint8_t MENU_VISIBLE_LINES = 7U;

static void reset_load_display_state(void)
{
    g_load_display_zero_raw = 0U;
    g_load_display_zero_valid = 0U;
    g_load_graph_head = 0U;
    memset(g_load_graph_samples, 0, sizeof(g_load_graph_samples));
}

static void display_delay(void)
{
    delay_cycles(64U);
}

static void display_select(const Mini12864PanelPins &pins)
{
    pin_write_low(pins.cs);
    display_delay();
}

static void display_deselect(const Mini12864PanelPins &pins)
{
    display_delay();
    pin_write_high(pins.cs);
}

static void display_write_byte(const Mini12864PanelPins &pins, uint8_t value)
{
    for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U)
    {
        if ((value & mask) != 0U)
        {
            pin_write_high(pins.mosi);
        }
        else
        {
            pin_write_low(pins.mosi);
        }

        display_delay();
        pin_write_high(pins.sck);
        display_delay();
        pin_write_low(pins.sck);
    }
}

static void display_write_command(const Mini12864PanelPins &pins, uint8_t command)
{
    pin_write_low(pins.a0);
    display_select(pins);
    display_write_byte(pins, command);
    display_deselect(pins);
}

static void display_write_data(const Mini12864PanelPins &pins, const uint8_t *data, uint32_t length)
{
    pin_write_high(pins.a0);
    display_select(pins);
    for (uint32_t index = 0U; index < length; ++index)
    {
        display_write_byte(pins, data[index]);
    }
    display_deselect(pins);
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

static void framebuffer_clear_pixel(uint8_t x, uint8_t y)
{
    if ((x >= 128U) || (y >= 64U))
    {
        return;
    }

    g_framebuffer[(uint32_t)(y / 8U) * 128U + x] &= (uint8_t)~(uint8_t)(1U << (y & 0x7U));
}

static void framebuffer_draw_text(uint8_t x, uint8_t y, const char *text);

static void wheel_color(uint8_t phase, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    if ((red == 0) || (green == 0) || (blue == 0))
    {
        return;
    }

    if (phase < 85U)
    {
        *red = (uint8_t)(255U - (phase * 3U));
        *green = (uint8_t)(phase * 3U);
        *blue = 0U;
    }
    else if (phase < 170U)
    {
        phase = (uint8_t)(phase - 85U);
        *red = 0U;
        *green = (uint8_t)(255U - (phase * 3U));
        *blue = (uint8_t)(phase * 3U);
    }
    else
    {
        phase = (uint8_t)(phase - 170U);
        *red = (uint8_t)(phase * 3U);
        *green = 0U;
        *blue = (uint8_t)(255U - (phase * 3U));
    }
}

static void update_panel_lighting(const Mini12864PanelPins &pins, uint32_t now_ms, const PersistedFirmwareConfig &config)
{
    uint8_t red = config.panelColorRed;
    uint8_t green = config.panelColorGreen;
    uint8_t blue = config.panelColorBlue;

    if ((g_splash_deadline_ms != 0U) && (((int32_t)(now_ms - g_splash_deadline_ms) < 0) != 0))
    {
        const uint8_t phase = (uint8_t)((now_ms / 12U) & 0xFFU);
        wheel_color(phase, &red, &green, &blue);
    }

    if ((red == g_last_panel_red) && (green == g_last_panel_green) && (blue == g_last_panel_blue))
    {
        return;
    }

    mini12864_panel_set_color(pins, red, green, blue);
    g_last_panel_red = red;
    g_last_panel_green = green;
    g_last_panel_blue = blue;
}

static uint8_t motion_state_busy(const keyswitch::MotionState &state)
{
    switch (state.homingState)
    {
    case keyswitch::HomingState::Seek:
    case keyswitch::HomingState::Backoff:
    case keyswitch::HomingState::MoveToTarget:
    case keyswitch::HomingState::CycleToPress:
    case keyswitch::HomingState::CycleToHome:
        return 1U;
    default:
        return 0U;
    }
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

    if (length >= 21U)
    {
        x = 0U;
    }
    else
    {
        const uint32_t width = length * 6U;
        x = (uint8_t)((128U - width) / 2U);
    }

    framebuffer_draw_text(x, y, text);
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

    if (length == 0U)
    {
        return 0U;
    }

    return (uint8_t)(length * 6U);
}

static const uint8_t *glyph_for_char(char c)
{
    static const uint8_t blank[5] = {0, 0, 0, 0, 0};
    static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
    static const uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
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

    if (c >= '0' && c <= '9') return digits[c - '0'];
    if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
    if (c == '-') return dash;
    if (c == ':') return colon;
    if (c == ' ') return blank;
    return blank;
}

static void framebuffer_draw_char_mode(uint8_t x, uint8_t y, char c, uint8_t inverted)
{
    const uint8_t *glyph = glyph_for_char(c);
    for (uint8_t col = 0U; col < 5U; ++col)
    {
        const uint8_t bits = glyph[col];
        for (uint8_t row = 0U; row < 7U; ++row)
        {
            if ((bits & (1U << row)) != 0U)
            {
                if (inverted != 0U)
                {
                    framebuffer_clear_pixel((uint8_t)(x + col), (uint8_t)(y + row));
                }
                else
                {
                    framebuffer_set_pixel((uint8_t)(x + col), (uint8_t)(y + row));
                }
            }
        }
    }
}

static void framebuffer_draw_text_mode(uint8_t x, uint8_t y, const char *text, uint8_t inverted)
{
    uint8_t cursor_x = x;
    for (uint32_t index = 0U; text[index] != 0; ++index)
    {
        framebuffer_draw_char_mode(cursor_x, y, text[index], inverted);
        cursor_x = (uint8_t)(cursor_x + 6U);
        if (cursor_x >= 122U)
        {
            break;
        }
    }
}

static void framebuffer_draw_text(uint8_t x, uint8_t y, const char *text)
{
    framebuffer_draw_text_mode(x, y, text, 0U);
}

static void framebuffer_draw_badge(uint8_t x, uint8_t y, const char *text, uint8_t active)
{
    const uint8_t width = (uint8_t)(text_pixel_width(text) + 6U);
    if (width < 8U)
    {
        return;
    }

    if (active != 0U)
    {
        framebuffer_fill_rect(x, y, width, 9U);
        framebuffer_draw_text_mode((uint8_t)(x + 3U), (uint8_t)(y + 1U), text, 1U);
    }
    else
    {
        framebuffer_draw_rect(x, y, width, 9U);
        framebuffer_draw_text((uint8_t)(x + 3U), (uint8_t)(y + 1U), text);
    }
}

static void framebuffer_draw_value_box(uint8_t x, uint8_t y, uint8_t width, const char *label, const char *value)
{
    framebuffer_draw_rect(x, y, width, 18U);
    framebuffer_draw_text((uint8_t)(x + 3U), (uint8_t)(y + 2U), label);
    framebuffer_draw_hline((uint8_t)(x + 2U), (uint8_t)(y + 9U), (uint8_t)(width - 4U));
    framebuffer_draw_text((uint8_t)(x + 3U), (uint8_t)(y + 11U), value);
}

static void framebuffer_draw_load_graph(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    if ((width < 4U) || (height < 4U))
    {
        return;
    }

    framebuffer_draw_rect(x, y, width, height);
    framebuffer_draw_hline((uint8_t)(x + 1U), (uint8_t)(y + (height / 2U)), (uint8_t)(width - 2U));

    const uint8_t graph_width = (uint8_t)(width - 2U);
    const uint8_t graph_height = (uint8_t)(height - 2U);
    const uint8_t sample_count = (uint8_t)(sizeof(g_load_graph_samples) / sizeof(g_load_graph_samples[0]));
    const uint8_t start_index = (g_load_graph_head + sample_count - ((graph_width < sample_count) ? graph_width : sample_count)) % sample_count;

    for (uint8_t col = 0U; col < graph_width; ++col)
    {
        const uint8_t sample = g_load_graph_samples[(start_index + col) % sample_count];
        const uint8_t bar_height = (uint8_t)(((uint16_t)sample * graph_height) / 100U);
        for (uint8_t row = 0U; row < bar_height; ++row)
        {
            framebuffer_set_pixel((uint8_t)(x + 1U + col), (uint8_t)(y + height - 2U - row));
        }
    }
}

static void framebuffer_draw_dashboard_header(const char *title, const char *status)
{
    framebuffer_draw_badge(0U, 0U, title, 1U);
    if ((status != 0) && (status[0] != 0))
    {
        const uint8_t status_width = (uint8_t)(text_pixel_width(status) + 6U);
        const uint8_t status_x = (status_width < 126U) ? (uint8_t)(128U - status_width) : 0U;
        framebuffer_draw_badge(status_x, 0U, status, 0U);
    }
    framebuffer_draw_hline(0U, 11U, 128U);
}

static void framebuffer_draw_page_dots(uint8_t active_page, uint8_t page_count)
{
    if (page_count == 0U)
    {
        return;
    }

    const uint8_t total_width = (uint8_t)((page_count * 6U) - 2U);
    uint8_t x = (uint8_t)((128U - total_width) / 2U);
    for (uint8_t index = 0U; index < page_count; ++index)
    {
        if (index == active_page)
        {
            framebuffer_fill_rect(x, 59U, 4U, 4U);
        }
        else
        {
            framebuffer_draw_rect(x, 59U, 4U, 4U);
        }
        x = (uint8_t)(x + 6U);
    }
}

static void framebuffer_draw_menu_line(uint8_t line_index, uint8_t selected, uint8_t editing, const char *text)
{
    const uint8_t row_y = (uint8_t)(8U + (line_index * 8U));
    if (selected != 0U)
    {
        framebuffer_fill_rect(0U, row_y, 122U, 8U);
        framebuffer_draw_text_mode(4U, (uint8_t)(row_y + 1U), text, 1U);
        if (editing != 0U)
        {
            framebuffer_draw_badge(103U, row_y, "E", 0U);
        }
    }
    else
    {
        framebuffer_draw_hline(0U, row_y, 122U);
        framebuffer_draw_text(4U, (uint8_t)(row_y + 1U), text);
        if (editing != 0U)
        {
            framebuffer_draw_badge(103U, row_y, "E", 1U);
        }
    }
}

static const char *motion_state_label(const keyswitch::MotionState &state)
{
    switch (state.homingState)
    {
    case keyswitch::HomingState::Seek: return "SEEK";
    case keyswitch::HomingState::Backoff: return "BACK";
    case keyswitch::HomingState::Done: return "READY";
    case keyswitch::HomingState::Fault: return "FAULT";
    case keyswitch::HomingState::MoveToTarget: return "MOVE";
    case keyswitch::HomingState::CycleToPress: return "PRESS";
    case keyswitch::HomingState::CycleToHome: return "RETURN";
    default: return "IDLE";
    }
}

static uint32_t update_load_display_relative_raw(uint32_t raw, const PersistedFirmwareConfig &config)
{
    const uint32_t settle_window = ((config.loadCell.threshold > 0U) ? (config.loadCell.threshold / 8U) : 0U) + 16U;

    if (g_load_display_zero_valid == 0U)
    {
        g_load_display_zero_raw = raw;
        g_load_display_zero_valid = 1U;
    }

    if (raw <= g_load_display_zero_raw)
    {
        g_load_display_zero_raw = raw;
    }
    else if ((raw - g_load_display_zero_raw) <= settle_window)
    {
        g_load_display_zero_raw = ((g_load_display_zero_raw * 31U) + raw) / 32U;
    }

    return (raw > g_load_display_zero_raw) ? (raw - g_load_display_zero_raw) : 0U;
}

static uint32_t load_display_estimated_grams(uint32_t relative_raw, const PersistedFirmwareConfig &config)
{
    return load_cell_calibrated_grams(relative_raw, config.loadCell);
}

static void load_graph_push_sample(uint32_t estimated_grams)
{
    g_load_graph_samples[g_load_graph_head] = (uint8_t)((estimated_grams > 100U) ? 100U : estimated_grams);
    g_load_graph_head = (uint8_t)((g_load_graph_head + 1U) % (sizeof(g_load_graph_samples) / sizeof(g_load_graph_samples[0])));
}

static uint8_t ui_item_count(UiScreen screen)
{
    switch (screen)
    {
    case UiScreen::Root: return ROOT_ITEM_COUNT;
    case UiScreen::Motion: return MOTION_ITEM_COUNT;
    case UiScreen::Measure: return MEASURE_ITEM_COUNT;
    case UiScreen::Driver: return DRIVER_ITEM_COUNT;
    case UiScreen::Config: return CONFIG_ITEM_COUNT;
    default: return 0U;
    }
}

static void ui_reset_list_position(void)
{
    g_ui.cursor = 0U;
    g_ui.scroll = 0U;
    g_ui.editing = 0U;
}

static void ui_enter_screen(UiScreen screen)
{
    g_ui.screen = screen;
    ui_reset_list_position();
}

static void ui_ensure_cursor_visible(void)
{
    if (g_ui.cursor < g_ui.scroll)
    {
        g_ui.scroll = g_ui.cursor;
    }
    else if (g_ui.cursor >= (uint8_t)(g_ui.scroll + MENU_VISIBLE_LINES))
    {
        g_ui.scroll = (uint8_t)(g_ui.cursor - (MENU_VISIBLE_LINES - 1U));
    }
}

static int32_t clamp_i32_local(int32_t value, int32_t min_value, int32_t max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int32_t ui_edit_step(UiScreen screen, uint8_t cursor)
{
    if ((screen == UiScreen::Motion) && (cursor == 4U)) return 100U;
    if ((screen == UiScreen::Motion) && (cursor == 7U)) return 100U;
    if ((screen == UiScreen::Motion) && (cursor == 8U)) return 1U;
    if ((screen == UiScreen::Measure) && (cursor == 0U)) return 50U;
    if ((screen == UiScreen::Driver) && (cursor <= 1U)) return 1U;
    if ((screen == UiScreen::Driver) && (cursor == 2U)) return 1U;
    if ((screen == UiScreen::Driver) && (cursor == 3U)) return 1U;
    if ((screen == UiScreen::Config) && (cursor <= 1U)) return 50U;
    if ((screen == UiScreen::Config) && (cursor == 2U)) return 10U;
    if ((screen == UiScreen::Config) && (cursor == 3U)) return 1U;
    if ((screen == UiScreen::Config) && (cursor == 4U)) return 100U;
    if ((screen == UiScreen::Config) && (cursor >= 5U) && (cursor <= 7U)) return 4U;
    return 1U;
}

static int32_t ui_edit_min(UiScreen screen, uint8_t cursor)
{
    if ((screen == UiScreen::Motion) && (cursor == 4U)) return 100;
    if ((screen == UiScreen::Motion) && (cursor == 7U)) return 0;
    if ((screen == UiScreen::Motion) && (cursor == 8U)) return 1;
    if ((screen == UiScreen::Measure) && (cursor == 0U)) return 1;
    if ((screen == UiScreen::Driver) && (cursor <= 1U)) return 0;
    if ((screen == UiScreen::Driver) && (cursor == 2U)) return 0;
    if ((screen == UiScreen::Driver) && (cursor == 3U)) return 0;
    if ((screen == UiScreen::Config) && (cursor <= 1U)) return 1;
    if ((screen == UiScreen::Config) && (cursor == 2U)) return 1;
    if ((screen == UiScreen::Config) && (cursor == 3U)) return 1;
    if ((screen == UiScreen::Config) && (cursor == 4U)) return 0;
    if ((screen == UiScreen::Config) && (cursor >= 5U) && (cursor <= 7U)) return 0;
    return 0;
}

static int32_t ui_edit_max(UiScreen screen, uint8_t cursor)
{
    if ((screen == UiScreen::Motion) && (cursor == 4U)) return 50000;
    if ((screen == UiScreen::Motion) && (cursor == 7U)) return 50000;
    if ((screen == UiScreen::Motion) && (cursor == 8U)) return 999;
    if ((screen == UiScreen::Measure) && (cursor == 0U)) return 2000000;
    if ((screen == UiScreen::Driver) && (cursor <= 1U)) return 31;
    if ((screen == UiScreen::Driver) && (cursor == 2U)) return 15;
    if ((screen == UiScreen::Driver) && (cursor == 3U)) return 255;
    if ((screen == UiScreen::Config) && (cursor <= 1U)) return 20000;
    if ((screen == UiScreen::Config) && (cursor == 2U)) return 65535;
    if ((screen == UiScreen::Config) && (cursor == 3U)) return 65535;
    if ((screen == UiScreen::Config) && (cursor == 4U)) return 50000;
    if ((screen == UiScreen::Config) && (cursor >= 5U) && (cursor <= 7U)) return 255;
    return 0;
}

static int32_t ui_current_edit_value(UiScreen screen, uint8_t cursor, const PersistedFirmwareConfig &config)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    if ((screen == UiScreen::Motion) && (cursor == 4U)) return g_ui.jogUm;
    if ((screen == UiScreen::Motion) && (cursor == 7U)) return (int32_t)channel.defaultPressUm;
    if ((screen == UiScreen::Motion) && (cursor == 8U)) return g_ui.cycleCount;
    if ((screen == UiScreen::Measure) && (cursor == 0U)) return (int32_t)config.loadCell.threshold;
    if ((screen == UiScreen::Driver) && (cursor == 0U)) return (int32_t)channel.tmc2209.irun;
    if ((screen == UiScreen::Driver) && (cursor == 1U)) return (int32_t)channel.tmc2209.ihold;
    if ((screen == UiScreen::Driver) && (cursor == 2U)) return (int32_t)channel.tmc2209.iholddelay;
    if ((screen == UiScreen::Driver) && (cursor == 3U)) return (int32_t)channel.tmc2209.sgthrs;
    if ((screen == UiScreen::Config) && (cursor == 0U)) return (int32_t)channel.moveFeedrateMmPerMin;
    if ((screen == UiScreen::Config) && (cursor == 1U)) return (int32_t)channel.homeFeedrateMmPerMin;
    if ((screen == UiScreen::Config) && (cursor == 2U)) return (int32_t)config.backoffSteps;
    if ((screen == UiScreen::Config) && (cursor == 3U)) return (int32_t)config.stopDebounceCount;
    if ((screen == UiScreen::Config) && (cursor == 4U)) return (int32_t)channel.defaultPressUm;
    if ((screen == UiScreen::Config) && (cursor == 5U)) return (int32_t)config.panelColorRed;
    if ((screen == UiScreen::Config) && (cursor == 6U)) return (int32_t)config.panelColorGreen;
    if ((screen == UiScreen::Config) && (cursor == 7U)) return (int32_t)config.panelColorBlue;
    return 0;
}

static Mini12864UiIntent make_intent(Mini12864UiIntentType type, int32_t value)
{
    Mini12864UiIntent intent = {type, value};
    return intent;
}

static Mini12864UiIntent ui_commit_edit(UiScreen screen, uint8_t cursor, int32_t value)
{
    if ((screen == UiScreen::Motion) && (cursor == 4U))
    {
        g_ui.jogUm = value;
        return make_intent(Mini12864UiIntentType::None, 0);
    }
    if ((screen == UiScreen::Motion) && (cursor == 7U)) return make_intent(Mini12864UiIntentType::SetDefaultPressUm, value);
    if ((screen == UiScreen::Motion) && (cursor == 8U)) return make_intent(Mini12864UiIntentType::StartCycle, value);
    if ((screen == UiScreen::Measure) && (cursor == 0U)) return make_intent(Mini12864UiIntentType::SetLoadThreshold, value);
    if ((screen == UiScreen::Driver) && (cursor == 0U)) return make_intent(Mini12864UiIntentType::SetIrun, value);
    if ((screen == UiScreen::Driver) && (cursor == 1U)) return make_intent(Mini12864UiIntentType::SetIhold, value);
    if ((screen == UiScreen::Driver) && (cursor == 2U)) return make_intent(Mini12864UiIntentType::SetIholdDelay, value);
    if ((screen == UiScreen::Driver) && (cursor == 3U)) return make_intent(Mini12864UiIntentType::SetSgthrs, value);
    if ((screen == UiScreen::Config) && (cursor == 0U)) return make_intent(Mini12864UiIntentType::SetMoveFeedrate, value);
    if ((screen == UiScreen::Config) && (cursor == 1U)) return make_intent(Mini12864UiIntentType::SetHomeFeedrate, value);
    if ((screen == UiScreen::Config) && (cursor == 2U)) return make_intent(Mini12864UiIntentType::SetBackoffSteps, value);
    if ((screen == UiScreen::Config) && (cursor == 3U)) return make_intent(Mini12864UiIntentType::SetDebounceCount, value);
    if ((screen == UiScreen::Config) && (cursor == 4U)) return make_intent(Mini12864UiIntentType::SetDefaultPressUm, value);
    if ((screen == UiScreen::Config) && (cursor == 5U)) return make_intent(Mini12864UiIntentType::SetPanelColorRed, value);
    if ((screen == UiScreen::Config) && (cursor == 6U)) return make_intent(Mini12864UiIntentType::SetPanelColorGreen, value);
    if ((screen == UiScreen::Config) && (cursor == 7U)) return make_intent(Mini12864UiIntentType::SetPanelColorBlue, value);
    return make_intent(Mini12864UiIntentType::None, 0);
}

static Mini12864UiIntent ui_click_action(const PersistedFirmwareConfig &config, const keyswitch::MotionState &state)
{
    switch (g_ui.screen)
    {
    case UiScreen::Dashboard:
        if (g_ui.dashboardPage == 0U)
        {
            reset_load_display_state();
            return make_intent(Mini12864UiIntentType::Tare, 0);
        }
        ui_enter_screen(UiScreen::Root);
        return make_intent(Mini12864UiIntentType::None, 0);
    case UiScreen::Root:
        switch (g_ui.cursor)
        {
        case 0U: ui_enter_screen(UiScreen::Dashboard); break;
        case 1U: ui_enter_screen(UiScreen::Motion); break;
        case 2U: ui_enter_screen(UiScreen::Measure); break;
        case 3U: ui_enter_screen(UiScreen::Driver); break;
        case 4U: ui_enter_screen(UiScreen::Config); break;
        case 5U: return make_intent(Mini12864UiIntentType::SaveConfig, 0);
        case 6U: return make_intent(Mini12864UiIntentType::Reboot, 0);
        default: break;
        }
        return make_intent(Mini12864UiIntentType::None, 0);
    case UiScreen::Motion:
        switch (g_ui.cursor)
        {
        case 0U: return make_intent(Mini12864UiIntentType::Home, 0);
        case 1U: return make_intent(Mini12864UiIntentType::Stop, 0);
        case 2U: return make_intent(Mini12864UiIntentType::Backoff, 0);
        case 3U: return make_intent(Mini12864UiIntentType::SetHold, state.holdEnabled == 0U ? 1 : 0);
        case 4U:
        case 7U:
        case 8U:
            g_ui.editing = 1U;
            g_ui.editValue = ui_current_edit_value(g_ui.screen, g_ui.cursor, config);
            return make_intent(Mini12864UiIntentType::None, 0);
        case 5U: return make_intent(Mini12864UiIntentType::MoveRelativeUm, -g_ui.jogUm);
        case 6U: return make_intent(Mini12864UiIntentType::MoveRelativeUm, g_ui.jogUm);
        case 9U: ui_enter_screen(UiScreen::Root); return make_intent(Mini12864UiIntentType::None, 0);
        default: break;
        }
        return make_intent(Mini12864UiIntentType::None, 0);
    case UiScreen::Measure:
        switch (g_ui.cursor)
        {
        case 0U:
            g_ui.editing = 1U;
            g_ui.editValue = ui_current_edit_value(g_ui.screen, g_ui.cursor, config);
            break;
        case 1U:
            reset_load_display_state();
            return make_intent(Mini12864UiIntentType::Tare, 0);
        case 2U: return make_intent(Mini12864UiIntentType::Stop, 0);
        case 3U: ui_enter_screen(UiScreen::Root); break;
        default: break;
        }
        return make_intent(Mini12864UiIntentType::None, 0);
    case UiScreen::Driver:
        switch (g_ui.cursor)
        {
        case 0U:
        case 1U:
        case 2U:
        case 3U:
            g_ui.editing = 1U;
            g_ui.editValue = ui_current_edit_value(g_ui.screen, g_ui.cursor, config);
            break;
        case 4U: return make_intent(Mini12864UiIntentType::SetAllowUnverifiedMotion, config.allowUnverifiedTmcMotion == 0U ? 1 : 0);
        case 5U: ui_enter_screen(UiScreen::Root); break;
        default: break;
        }
        return make_intent(Mini12864UiIntentType::None, 0);
    case UiScreen::Config:
        switch (g_ui.cursor)
        {
        case 0U:
        case 1U:
        case 2U:
        case 3U:
        case 4U:
        case 5U:
        case 6U:
        case 7U:
            g_ui.editing = 1U;
            g_ui.editValue = ui_current_edit_value(g_ui.screen, g_ui.cursor, config);
            break;
        case 8U: return make_intent(Mini12864UiIntentType::SaveConfig, 0);
        case 9U: return make_intent(Mini12864UiIntentType::ResetConfig, 0);
        case 10U: ui_enter_screen(UiScreen::Root); break;
        default: break;
        }
        return make_intent(Mini12864UiIntentType::None, 0);
    default:
        return make_intent(Mini12864UiIntentType::None, 0);
    }
}

static void ui_apply_encoder_delta(int8_t steps, const PersistedFirmwareConfig &config)
{
    if (steps == 0)
    {
        return;
    }

    if (g_ui.screen == UiScreen::Dashboard)
    {
        const int32_t next_page = clamp_i32_local((int32_t)g_ui.dashboardPage + steps, 0, 3);
        g_ui.dashboardPage = (uint8_t)next_page;
        return;
    }

    if (g_ui.editing != 0U)
    {
        const int32_t next_value = g_ui.editValue + ((int32_t)steps * ui_edit_step(g_ui.screen, g_ui.cursor));
        g_ui.editValue = clamp_i32_local(next_value, ui_edit_min(g_ui.screen, g_ui.cursor), ui_edit_max(g_ui.screen, g_ui.cursor));
        return;
    }

    const uint8_t item_count = ui_item_count(g_ui.screen);
    if (item_count == 0U)
    {
        return;
    }

    const int32_t next_cursor = clamp_i32_local((int32_t)g_ui.cursor + steps, 0, (int32_t)item_count - 1);
    g_ui.cursor = (uint8_t)next_cursor;
    ui_ensure_cursor_visible();
}

static void ui_render_dashboard(
    const keyswitch::MotionInputs &inputs,
    const keyswitch::MotionState &state,
    const keyswitch::MotionOutputs &outputs,
    const PersistedFirmwareConfig &config,
    const ConfigRuntimeState &config_state)
{
    char line[22];
    char line2[22];
    const MotionChannelConfig &channel = active_motion_channel(config);
    const uint32_t load_relative_raw = update_load_display_relative_raw(inputs.loadCellRaw, config);
    const uint32_t load_estimated_grams = load_display_estimated_grams(load_relative_raw, config);
    load_graph_push_sample(load_estimated_grams);
    switch (g_ui.dashboardPage)
    {
    case 0U:
        framebuffer_draw_dashboard_header("LOAD", outputs.loadCellTriggered != 0U ? "TRIP" : "IDLE");
        snprintf(line, sizeof(line), "%lug", (unsigned long)load_estimated_grams);
        framebuffer_draw_value_box(0U, 15U, 62U, "EQ GRAMS", line);
        snprintf(line, sizeof(line), "%lu", (unsigned long)load_relative_raw);
        framebuffer_draw_value_box(66U, 15U, 62U, "RAW DELTA", line);
        framebuffer_draw_badge(0U, 36U, state.homed != 0U ? "HOME" : "UNHOME", state.homed != 0U ? 1U : 0U);
        framebuffer_draw_badge(44U, 36U, state.holdEnabled != 0U ? "HOLD" : "DRIVE", state.holdEnabled != 0U ? 1U : 0U);
        framebuffer_draw_badge(88U, 36U, state.faultLatch != 0U ? "FAULT" : "ZERO", state.faultLatch != 0U ? 1U : 0U);
        framebuffer_draw_text(0U, 47U, "0G");
        framebuffer_draw_text(102U, 47U, "100G");
        framebuffer_draw_load_graph(16U, 44U, 96U, 13U);
        framebuffer_draw_page_dots(g_ui.dashboardPage, 4U);
        break;
    case 1U:
        framebuffer_draw_dashboard_header("MOTION", motion_state_label(state));
        snprintf(line, sizeof(line), "%ld", (long)state.pressTargetPosition);
        framebuffer_draw_value_box(0U, 15U, 62U, "PRESS", line);
        snprintf(line, sizeof(line), "%lu", (unsigned long)channel.moveFeedrateMmPerMin);
        framebuffer_draw_value_box(66U, 15U, 62U, "MOVE MM", line);
        snprintf(line, sizeof(line), "CYCLE %lu", (unsigned long)state.cycleCountRemaining);
        framebuffer_draw_text(0U, 38U, line);
        snprintf(line2, sizeof(line2), "DONE %lu", (unsigned long)state.completedCycles);
        framebuffer_draw_text(66U, 38U, line2);
        snprintf(line, sizeof(line), "SEEK %lu", (unsigned long)state.seekSteps);
        framebuffer_draw_text(0U, 47U, line);
        snprintf(line2, sizeof(line2), "BACK %lu", (unsigned long)state.backoffStepsRemaining);
        framebuffer_draw_text(66U, 47U, line2);
        framebuffer_draw_page_dots(g_ui.dashboardPage, 4U);
        break;
    case 2U:
        framebuffer_draw_dashboard_header("DRIVER", config.allowUnverifiedTmcMotion != 0U ? "UNVER" : "LOCKED");
        snprintf(line, sizeof(line), "%u", (unsigned int)channel.tmc2209.irun);
        framebuffer_draw_value_box(0U, 15U, 40U, "IRUN", line);
        snprintf(line, sizeof(line), "%u", (unsigned int)channel.tmc2209.ihold);
        framebuffer_draw_value_box(44U, 15U, 40U, "IHOLD", line);
        snprintf(line, sizeof(line), "%u", (unsigned int)channel.tmc2209.iholddelay);
        framebuffer_draw_value_box(88U, 15U, 40U, "DELAY", line);
        snprintf(line, sizeof(line), "%u", (unsigned int)channel.tmc2209.sgthrs);
        framebuffer_draw_value_box(0U, 37U, 62U, "SGTHRS", line);
        snprintf(line, sizeof(line), "%u", (unsigned int)config.tmcUartBitUs);
        framebuffer_draw_value_box(66U, 37U, 62U, "UART US", line);
        framebuffer_draw_page_dots(g_ui.dashboardPage, 4U);
        break;
    default:
        framebuffer_draw_dashboard_header("CONFIG", config_state.dirty != 0U ? "DIRTY" : "SAVED");
        snprintf(line, sizeof(line), "%lu", (unsigned long)channel.moveFeedrateMmPerMin);
        framebuffer_draw_value_box(0U, 15U, 62U, "MOVE", line);
        snprintf(line, sizeof(line), "%lu", (unsigned long)channel.homeFeedrateMmPerMin);
        framebuffer_draw_value_box(66U, 15U, 62U, "HOME", line);
        snprintf(line, sizeof(line), "BACK %u DEB %u", (unsigned int)config.backoffSteps, (unsigned int)config.stopDebounceCount);
        framebuffer_draw_text(0U, 39U, line);
        snprintf(line, sizeof(line), "LOAD %lu", (unsigned long)config.loadCell.threshold);
        framebuffer_draw_text(0U, 47U, line);
        snprintf(line, sizeof(line), "RGB %u %u %u", (unsigned int)config.panelColorRed, (unsigned int)config.panelColorGreen, (unsigned int)config.panelColorBlue);
        framebuffer_draw_text(0U, 55U, line);
        framebuffer_draw_page_dots(g_ui.dashboardPage, 4U);
        break;
    }
}

static void ui_menu_text(UiScreen screen, uint8_t item_index, char *buffer, uint32_t length, const PersistedFirmwareConfig &config, const ConfigRuntimeState &config_state, const keyswitch::MotionState &state, const keyswitch::MotionInputs &inputs)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    if ((buffer == 0) || (length == 0U))
    {
        return;
    }
    buffer[0] = 0;
    switch (screen)
    {
    case UiScreen::Root:
        switch (item_index)
        {
        case 0U: snprintf(buffer, length, "LIVE"); break;
        case 1U: snprintf(buffer, length, "MOTION"); break;
        case 2U: snprintf(buffer, length, "MEASURE"); break;
        case 3U: snprintf(buffer, length, "DRIVER"); break;
        case 4U: snprintf(buffer, length, "CONFIG"); break;
        case 5U: snprintf(buffer, length, "SAVE CFG"); break;
        case 6U: snprintf(buffer, length, "REBOOT"); break;
        default: break;
        }
        break;
    case UiScreen::Motion:
        switch (item_index)
        {
        case 0U: snprintf(buffer, length, "HOME AXIS"); break;
        case 1U: snprintf(buffer, length, "STOP NOW"); break;
        case 2U: snprintf(buffer, length, "BACKOFF"); break;
        case 3U: snprintf(buffer, length, "HOLD:%u", (unsigned int)state.holdEnabled); break;
        case 4U: snprintf(buffer, length, "JOG UM:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 4U ? g_ui.editValue : g_ui.jogUm)); break;
        case 5U: snprintf(buffer, length, "MOVE NEG"); break;
        case 6U: snprintf(buffer, length, "MOVE POS"); break;
        case 7U: snprintf(buffer, length, "PRESS:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 7U ? g_ui.editValue : (int32_t)channel.defaultPressUm)); break;
        case 8U: snprintf(buffer, length, "CYCLE:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 8U ? g_ui.editValue : g_ui.cycleCount)); break;
        case 9U: snprintf(buffer, length, "BACK"); break;
        default: break;
        }
        break;
    case UiScreen::Measure:
        switch (item_index)
        {
        case 0U: snprintf(buffer, length, "THRESH:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 0U ? g_ui.editValue : (int32_t)config.loadCell.threshold)); break;
        case 1U: snprintf(buffer, length, "TARE ZERO"); break;
        case 2U: snprintf(buffer, length, "STOP SRC:%u", (unsigned int)state.lastStopSource); break;
        case 3U: snprintf(buffer, length, "BACK"); break;
        default: break;
        }
        break;
    case UiScreen::Driver:
        switch (item_index)
        {
        case 0U: snprintf(buffer, length, "IRUN:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 0U ? g_ui.editValue : (int32_t)channel.tmc2209.irun)); break;
        case 1U: snprintf(buffer, length, "IHOLD:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 1U ? g_ui.editValue : (int32_t)channel.tmc2209.ihold)); break;
        case 2U: snprintf(buffer, length, "IHOLDD:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 2U ? g_ui.editValue : (int32_t)channel.tmc2209.iholddelay)); break;
        case 3U: snprintf(buffer, length, "SGTHRS:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 3U ? g_ui.editValue : (int32_t)channel.tmc2209.sgthrs)); break;
        case 4U: snprintf(buffer, length, "UNVER:%u", (unsigned int)config.allowUnverifiedTmcMotion); break;
        case 5U: snprintf(buffer, length, "BACK"); break;
        default: break;
        }
        break;
    case UiScreen::Config:
        switch (item_index)
        {
        case 0U: snprintf(buffer, length, "MOVE:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 0U ? g_ui.editValue : (int32_t)channel.moveFeedrateMmPerMin)); break;
        case 1U: snprintf(buffer, length, "HOME:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 1U ? g_ui.editValue : (int32_t)channel.homeFeedrateMmPerMin)); break;
        case 2U: snprintf(buffer, length, "BACK:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 2U ? g_ui.editValue : (int32_t)config.backoffSteps)); break;
        case 3U: snprintf(buffer, length, "DEB:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 3U ? g_ui.editValue : (int32_t)config.stopDebounceCount)); break;
        case 4U: snprintf(buffer, length, "DPRESS:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 4U ? g_ui.editValue : (int32_t)channel.defaultPressUm)); break;
        case 5U: snprintf(buffer, length, "LED R:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 5U ? g_ui.editValue : (int32_t)config.panelColorRed)); break;
        case 6U: snprintf(buffer, length, "LED G:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 6U ? g_ui.editValue : (int32_t)config.panelColorGreen)); break;
        case 7U: snprintf(buffer, length, "LED B:%ld", (long)(g_ui.editing != 0U && g_ui.cursor == 7U ? g_ui.editValue : (int32_t)config.panelColorBlue)); break;
        case 8U: snprintf(buffer, length, "SAVE DIRTY:%u", (unsigned int)config_state.dirty); break;
        case 9U: snprintf(buffer, length, "RESET CFG"); break;
        case 10U: snprintf(buffer, length, "BACK"); break;
        default: break;
        }
        break;
    default:
        break;
    }
}

static void ui_render_menu(UiScreen screen, const PersistedFirmwareConfig &config, const ConfigRuntimeState &config_state, const keyswitch::MotionState &state, const keyswitch::MotionInputs &inputs)
{
    char title[22] = {0};
    switch (screen)
    {
    case UiScreen::Root: snprintf(title, sizeof(title), "MAIN MENU"); break;
    case UiScreen::Motion: snprintf(title, sizeof(title), "MOTION MENU"); break;
    case UiScreen::Measure: snprintf(title, sizeof(title), "MEASURE MENU"); break;
    case UiScreen::Driver: snprintf(title, sizeof(title), "DRIVER MENU"); break;
    case UiScreen::Config: snprintf(title, sizeof(title), "CONFIG MENU"); break;
    default: snprintf(title, sizeof(title), "MENU"); break;
    }
    framebuffer_draw_dashboard_header(title, g_ui.editing != 0U ? "EDIT" : "BROWSE");
    const uint8_t item_count = ui_item_count(screen);
    for (uint8_t line_index = 0U; line_index < MENU_VISIBLE_LINES; ++line_index)
    {
        const uint8_t item_index = (uint8_t)(g_ui.scroll + line_index);
        if (item_index >= item_count)
        {
            break;
        }
        char line[22] = {0};
        ui_menu_text(screen, item_index, line, sizeof(line), config, config_state, state, inputs);
        framebuffer_draw_menu_line(line_index, item_index == g_ui.cursor ? 1U : 0U, (g_ui.editing != 0U) && (item_index == g_ui.cursor) ? 1U : 0U, line);
    }

    if (item_count > MENU_VISIBLE_LINES)
    {
        const uint8_t rail_y = 15U;
        const uint8_t rail_height = 41U;
        const uint8_t thumb_height = 11U;
        const uint8_t travel = (uint8_t)(rail_height - thumb_height);
        const uint8_t max_scroll = (uint8_t)(item_count - MENU_VISIBLE_LINES);
        const uint8_t thumb_y = (max_scroll == 0U) ? rail_y : (uint8_t)(rail_y + ((uint16_t)g_ui.scroll * travel) / max_scroll);
        framebuffer_draw_rect(124U, rail_y, 4U, rail_height);
        framebuffer_fill_rect(125U, (uint8_t)(thumb_y + 1U), 2U, thumb_height);
    }
}

static void ui_render_splash(const PersistedFirmwareConfig &config)
{
    const MotionChannelConfig &channel = active_motion_channel(config);
    char line[22] = {0};

    framebuffer_draw_rect(0U, 0U, 128U, 64U);
    framebuffer_draw_rect(4U, 4U, 120U, 56U);
    framebuffer_draw_badge(39U, 4U, "KEYSWITCH", 1U);
    framebuffer_draw_keyswitch_logo(49U, 8U);
    framebuffer_draw_hline(14U, 34U, 100U);
    framebuffer_draw_hline(20U, 50U, 88U);
    framebuffer_draw_centered_text(38U, "KEYSWITCH TESTER");
    snprintf(line, sizeof(line), "CHANNEL %s READY", channel.label);
    framebuffer_draw_centered_text(54U, line);
    framebuffer_draw_text(10U, 22U, "SKR2");
    framebuffer_draw_text(87U, 22U, "V4");
    framebuffer_draw_badge(42U, 42U, "TURN OR CLICK", 0U);
}

static void display_flush(const Mini12864PanelPins &pins)
{
    for (uint8_t page = 0U; page < 8U; ++page)
    {
        display_write_command(pins, (uint8_t)(0xB0U | page));
        display_write_command(pins, 0x10U);
        display_write_command(pins, 0x00U);
        display_write_data(pins, &g_framebuffer[(uint32_t)page * 128U], 128U);
    }
}

static void display_flush_page(const Mini12864PanelPins &pins, uint8_t page)
{
    if (page >= 8U)
    {
        return;
    }

    display_write_command(pins, (uint8_t)(0xB0U | page));
    display_write_command(pins, 0x10U);
    display_write_command(pins, 0x00U);
    display_write_data(pins, &g_framebuffer[(uint32_t)page * 128U], 128U);
}

static void display_begin_frame(
    const keyswitch::MotionInputs &inputs,
    const keyswitch::MotionState &state,
    const keyswitch::MotionOutputs &outputs,
    const PersistedFirmwareConfig &config,
    const ConfigRuntimeState &config_state)
{
    framebuffer_clear();
    if ((g_splash_deadline_ms != 0U) && (((int32_t)(HAL_GetTick() - g_splash_deadline_ms) < 0) != 0))
    {
        ui_render_splash(config);
    }
    else if (g_ui.screen == UiScreen::Dashboard)
    {
        ui_render_dashboard(inputs, state, outputs, config, config_state);
    }
    else
    {
        ui_render_menu(g_ui.screen, config, config_state, state, inputs);
    }

    g_display_flush_page = 0U;
    g_display_frame_pending = 1U;
}

static int8_t display_consume_encoder_steps(const Mini12864PanelInputs &panel_inputs)
{
    static const int8_t transition_table[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    const uint8_t encoder_state = (uint8_t)(panel_inputs.encoderAActive | (uint8_t)(panel_inputs.encoderBActive << 1U));
    const uint8_t transition_index = (uint8_t)((g_prev_encoder_state << 2U) | encoder_state);
    g_encoder_accumulator += transition_table[transition_index];
    g_prev_encoder_state = encoder_state;

    if (g_encoder_accumulator >= 4)
    {
        g_encoder_accumulator = 0;
        return 1;
    }
    if (g_encoder_accumulator <= -4)
    {
        g_encoder_accumulator = 0;
        return -1;
    }
    return 0;
}

}

void mini12864_display_init(const Mini12864PanelPins &pins)
{
    enable_gpio_clock(pins.cs.portId);
    enable_gpio_clock(pins.a0.portId);
    if (pin_assignment_valid(pins.reset) != 0U)
    {
        enable_gpio_clock(pins.reset.portId);
    }
    enable_gpio_clock(pins.sck.portId);
    enable_gpio_clock(pins.mosi.portId);
    __DSB();

    pin_set_output(pins.cs);
    pin_set_output(pins.a0);
    if (pin_assignment_valid(pins.reset) != 0U)
    {
        pin_set_output(pins.reset);
    }
    pin_set_output(pins.sck);
    pin_set_output(pins.mosi);

    pin_write_high(pins.cs);
    pin_write_low(pins.sck);
    pin_write_low(pins.mosi);
    pin_write_high(pins.a0);
    if (pin_assignment_valid(pins.reset) != 0U)
    {
        pin_write_high(pins.reset);
        HAL_Delay(2U);
        pin_write_low(pins.reset);
        HAL_Delay(2U);
        pin_write_high(pins.reset);
    }
    HAL_Delay(8U);

    display_write_command(pins, 0xE2U);
    display_write_command(pins, 0xAEU);
    display_write_command(pins, 0x40U);
    display_write_command(pins, 0xA0U);
    display_write_command(pins, 0xC8U);
    display_write_command(pins, 0xA6U);
    display_write_command(pins, 0xA2U);
    display_write_command(pins, 0x2FU);
    display_write_command(pins, 0xF8U);
    display_write_command(pins, 0x00U);
    display_write_command(pins, 0x23U);
    display_write_command(pins, 0x81U);
    display_write_command(pins, 0x3FU);
    display_write_command(pins, 0xACU);
    display_write_command(pins, 0xA4U);
    display_write_command(pins, 0xAFU);

    framebuffer_clear();
    framebuffer_draw_text(0U, 0U, "PG0 STATUS");
    framebuffer_draw_text(0U, 16U, "DISPLAY INIT OK");
    framebuffer_draw_text(0U, 32U, "CLICK OR TURN");
    display_flush(pins);
    g_display_ready = 1U;
    g_last_render_ms = 0U;
    g_splash_deadline_ms = HAL_GetTick() + 2500U;
    g_last_panel_red = 0xFFU;
    g_last_panel_green = 0xFFU;
    g_last_panel_blue = 0xFFU;
    g_load_display_zero_raw = 0U;
    g_load_display_zero_valid = 0U;
    memset(g_load_graph_samples, 0, sizeof(g_load_graph_samples));
    g_load_graph_head = 0U;
    g_display_flush_page = 0U;
    g_display_frame_pending = 0U;
    g_ui = {UiScreen::Dashboard, 0U, 0U, 0U, 0U, 0, 1000, 1, 1U};
    g_prev_click = 0U;
    g_prev_encoder_state = 0U;
    g_encoder_accumulator = 0;
}

Mini12864UiIntent mini12864_ui_update(
    const Mini12864PanelInputs &panel_inputs,
    const PersistedFirmwareConfig &config,
    const ConfigRuntimeState &config_state,
    const keyswitch::MotionState &state)
{
    (void)config_state;
    if (g_ui.initialized == 0U)
    {
        g_ui = {UiScreen::Dashboard, 0U, 0U, 0U, 0U, 0, 1000, 1, 1U};
    }

    const int8_t encoder_steps = display_consume_encoder_steps(panel_inputs);

    if ((g_splash_deadline_ms != 0U) &&
        (((int32_t)(HAL_GetTick() - g_splash_deadline_ms) < 0) != 0))
    {
        if ((encoder_steps != 0) || ((panel_inputs.clickPressed != 0U) && (g_prev_click == 0U)))
        {
            g_splash_deadline_ms = 0U;
        }
        g_prev_click = panel_inputs.clickPressed;
        return {Mini12864UiIntentType::None, 0};
    }

    g_splash_deadline_ms = 0U;
    ui_apply_encoder_delta(encoder_steps, config);

    Mini12864UiIntent intent = {Mini12864UiIntentType::None, 0};
    if ((panel_inputs.clickPressed != 0U) && (g_prev_click == 0U))
    {
        if (g_ui.editing != 0U)
        {
            intent = ui_commit_edit(g_ui.screen, g_ui.cursor, g_ui.editValue);
            g_ui.editing = 0U;
        }
        else
        {
            intent = ui_click_action(config, state);
        }
    }
    g_prev_click = panel_inputs.clickPressed;
    return intent;
}

void mini12864_display_render(
    const Mini12864PanelPins &pins,
    uint32_t now_ms,
    const keyswitch::MotionInputs &inputs,
    const keyswitch::MotionState &state,
    const keyswitch::MotionOutputs &outputs,
    const Mini12864PanelInputs &panel_inputs,
    const PersistedFirmwareConfig &config,
    const ConfigRuntimeState &config_state)
{
    if (g_display_ready == 0U)
    {
        return;
    }

    update_panel_lighting(pins, now_ms, config);

    if (g_display_probe_mode != 0U)
    {
        return;
    }

    if (g_display_frame_pending != 0U)
    {
        display_flush_page(pins, g_display_flush_page);
        ++g_display_flush_page;
        if (g_display_flush_page >= 8U)
        {
            g_display_flush_page = 0U;
            g_display_frame_pending = 0U;
        }
        return;
    }

    const uint32_t render_interval_ms = (motion_state_busy(state) != 0U) ? 200U : 125U;
    if ((now_ms - g_last_render_ms) < render_interval_ms)
    {
        return;
    }
    g_last_render_ms = now_ms;

    display_begin_frame(inputs, state, outputs, config, config_state);
}
