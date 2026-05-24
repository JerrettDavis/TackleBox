# Portability

## Goal

The firmware must not be trapped on one BTT board. Supporting another printer controller, a custom STM32 board, or a smaller Arduino-class coprocessor should be a composition exercise, not a rewrite.

## Portability Principles

1. Treat the board as a capability provider.
2. Describe pins, buses, timers, and storage as configuration or platform services.
3. Keep product behavior independent from concrete GPIO ports and register names.
4. Avoid tying user-visible protocol commands to one vendor's board layout.
5. Prefer feature detection and capability flags over preprocessor sprawl.

## Required Seams

Every new hardware-facing feature should answer these questions:

- What part is board-specific?
- What part is MCU-family-specific?
- What part is chip or peripheral specific?
- What part is product logic and therefore reusable?

If a change mixes all four, it needs another abstraction boundary.

## Recommended Platform Contract

Future board packages should expose a small contract for:

- digital input and output roles
- timebase and pulse scheduling
- persistent storage access
- serial or USB transport endpoints
- removable storage support
- optional sensor buses such as I2C, SPI, ADC, and UART

## Board Bring-Up Standard

Each supported board should eventually ship with:

- a board profile document
- a build target in `platformio.ini` or equivalent build system metadata
- a smoke-test checklist
- a safe default runtime config
- explicit notes for bootloader offset and flash procedure

## Anti-Patterns

- hard-coding one board pinout inside domain logic
- mixing HAL register code into parser or motion code
- requiring host control for safety-critical stop behavior
- cloning `main.cpp` per board instead of composing smaller modules
- letting vendor naming dominate the public model