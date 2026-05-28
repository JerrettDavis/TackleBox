#ifndef APP_PANEL_H
#define APP_PANEL_H

#include "app_config.h"

struct Mini12864PanelPins {
    PinAssignment beeper;
    PinAssignment click;
    PinAssignment encoderA;
    PinAssignment encoderB;
    PinAssignment cs;
    PinAssignment a0;
    PinAssignment reset;
    PinAssignment neopixel;
    PinAssignment sck;
    PinAssignment miso;
    PinAssignment mosi;
};

struct Mini12864PanelInputs {
    uint8_t clickPressed;
    uint8_t encoderAActive;
    uint8_t encoderBActive;
};

Mini12864PanelPins mini12864_panel_pins(void);
void mini12864_panel_init_inputs(const Mini12864PanelPins &pins);
Mini12864PanelInputs mini12864_panel_read_inputs(const Mini12864PanelPins &pins);
void mini12864_panel_set_color(const Mini12864PanelPins &pins, uint8_t red, uint8_t green, uint8_t blue);

#endif