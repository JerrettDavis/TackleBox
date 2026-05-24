#ifndef APP_BOARD_H
#define APP_BOARD_H

#include "app_config.h"

#include "stm32f4xx.h"

extern "C" void SysTick_Handler(void);

void delay_cycles(volatile uint32_t cycles);
GPIO_TypeDef *gpio_port_from_id(uint8_t port_id);
void enable_gpio_clock(uint8_t port_id);
uint8_t pin_assignment_valid(PinAssignment pin);
void pin_set_output(PinAssignment pin);
void pin_set_output_open_drain(PinAssignment pin);
void set_pin_high(GPIO_TypeDef *port, uint32_t pin);
void set_pin_low(GPIO_TypeDef *port, uint32_t pin);
uint32_t read_pin(GPIO_TypeDef *port, uint32_t pin);
void pin_write_high(PinAssignment pin);
void pin_write_low(PinAssignment pin);
uint32_t pin_read(PinAssignment pin);
void pin_set_input_pull(PinAssignment pin, uint8_t pull_up);
uint32_t sample_pin_baseline(PinAssignment pin);
uint32_t checksum32(const void *data, uint32_t length);
void enable_gpio_clocks_for_config(const PersistedFirmwareConfig &config);
void format_pin_assignment(PinAssignment pin, char *buffer, uint32_t length);

#endif