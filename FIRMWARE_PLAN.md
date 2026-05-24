# BTT SKR2 Keyswitch Tester Firmware Plan

## Current State

The firmware has moved beyond the initial bring-up phase.

What is now working:

- HSE/PLL clock configuration for the SKR2 target
- USB CDC enumeration and bidirectional communication
- homing / backoff domain model extracted into a testable core library
- multi-source stop-source selection in the domain core
- simple command parser with plain-text and trimmed G-code style aliases
- host-side unit and integration tests for the extracted domain logic

## Current Architecture

### Firmware Shell

`src/main.cpp` is now responsible for:

- board clocks and startup
- GPIO wiring and pin toggling
- USB CDC startup and polling
- mapping hardware inputs into domain inputs
- mapping domain outputs into driver / direction / LED actions

### Domain Core

`lib/keyswitch_core/` is responsible for:

- homing state transitions
- debounce handling
- backoff sequencing
- stop-source prioritization
- command parsing
- heartbeat / status cadence decisions

This is the layer intended to grow with the tester product.

## Refactor Goals

### Goal 1: Maintainable Motion Control

- further decouple motion timing from telemetry writes
- make homing resets deterministic and fresh-edge based
- isolate step timing and movement profiles behind a dedicated motion service

### Goal 2: Protocol Growth Path

- keep macro commands for bring-up speed
- add a small curated G-code compatibility layer where it adds value
- avoid importing a full printer command stack too early

### Goal 3: Product Preparation

- create clean seams for load cell acquisition
- create clean seams for button / joystick input
- create clean seams for stall/bind protection
- define device/application/domain boundaries more explicitly

## Planned Functional Areas

1. motion domain
2. protocol / command domain
3. sensor domain
4. execution / scheduler domain
5. reporting / results domain

## Test Strategy

### Unit Tests

- command parsing
- homing state transitions
- debounce behavior
- fault transitions
- stop-source prioritization

### Integration Tests

- command-to-motion interactions
- state reset behavior for `HOME`, `STOP`, and `BACKOFF`

### End-to-End Tests

E2E remains hardware-backed for now and should validate:

- USB enumeration
- heartbeat telemetry
- command acceptance
- homing travel and backoff
- endstop behavior

## Near-Term Implementation Priorities

1. add a real load-cell acquisition path behind the current placeholders
2. add TMC2209 DIAG/UART stall reporting behind the current placeholders
3. move stepping onto a timer-driven cadence so safety sampling and telemetry do not disturb travel
4. add more explicit motion modes and state reporting
5. keep expanding scripted serial integration coverage as the command surface stabilizes

## Stop Model

The seek phase now has an explicit stop-source model so future hardware can slot in without reworking the state machine:

1. load-cell threshold trigger is the preferred primary stop
2. StallGuard trigger is an active fallback for bind/stall protection
3. mechanical stop is the final physical fallback
4. seek-limit timeout remains the terminal fault path

That means the next hardware work is mostly adapter work in `src/main.cpp`, not state-machine redesign.
