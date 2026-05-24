# Architecture

## Product Shape

TackleBox is intended to sit between Marlin-style standalone firmware and Klipper-style host-assisted control.

The embedded target must remain independently operable:

- safe boot without a host
- local motion and homing execution
- persistent runtime configuration
- direct sensor and driver handling
- deterministic safety behavior even if the host is absent or disconnected

At the same time, a host companion should be able to add:

- richer orchestration and tuning
- device profile management
- calibration workflows
- data capture and regression analysis
- remote or fleet automation
- future UI and web experiences where supported hardware permits

## Core Architectural Rules

1. Safety-critical behavior must stay executable on-device.
2. Host assistance must be additive, not required for safe operation.
3. Board dependencies must be isolated behind narrow hardware modules.
4. Protocol and motion semantics must remain testable on the host.
5. Configuration must describe capabilities, mappings, and limits rather than one fixed board.
6. The flash-resident board image should change less often than the updateable runtime behavior layered on top of it.

## Named Stack Layers

### BootAnchors

Optional bootloaders and recovery environments for boards that need a better field update story.

Responsibilities:

- USB or serial flashing enablement where the vendor board lacks it
- diagnostic shell and recovery entrypoints
- safe handoff into the main board image
- preserving a restore path during bring-up and field servicing

### KeelWare

The board package and hardware abstraction layer that is flashed directly onto the mainboard.

Responsibilities:

- clocks, startup, interrupt glue, flash layout, and transport initialization
- board pin roles and capability descriptors
- storage, removable media, and peripheral bus binding
- narrow device adapters exposed upward to Boatswain

### Boatswain

The on-device supervisor/runtime that executes control behavior using the KeelWare abstraction contract.

Responsibilities:

- command execution
- motion and homing state machines
- safety policy and fallback handling
- configuration application and runtime telemetry
- updateable macros and higher-level board-resident functionality

### Semaphorio

The host or coprocessor API/orchestration layer that coordinates with the device runtime.

Responsibilities:

- transport/session handling
- configuration injection and synchronization
- automation, calibration, and scripted workflows
- release/update tooling and fleet-safe operational behavior

### SpyGlass

The observational and operator-facing product layer.

Responsibilities:

- dashboards and operator UI
- telemetry visualization and capture
- diagnostics and troubleshooting flows
- future local web portal support for boards or companions that can host it

## Current Layers In This Repo

### Product logic

`lib/keyswitch_core/`

- current Boatswain command parsing
- current Boatswain motion and homing state machine
- TMC frame helpers
- future reusable safety and planning primitives

### Firmware shell

`src/`

- current KeelWare startup and clock configuration
- USB CDC bridge
- persisted config storage
- board pin bindings
- TMC transport and verification
- SDIO and other board-facing integration
- current BootAnchors implementation for SKR 2 CDC recovery

### Host tooling

`tools/` and selected `test/` scripts

- early Semaphorio transport helpers and flashing tools
- early SpyGlass operator surfaces and validation utilities

## Target Layering

The long-term target should converge toward these slices:

1. `bootanchors/`
   Optional bootloader packages and recovery contracts per board family.
2. `keelware/`
   MCU and board packages for clocks, GPIO, timers, flash, USB, SD, UART, and interrupt glue.
3. `boatswain/`
   Pure board-agnostic supervisor logic, command semantics, safety decisions, units, macros, and profile models.
4. `drivers/`
   Chip-family or device-family adapters such as TMC, load-cell front ends, displays, and bus helpers.
5. `semaphorio/`
   Desktop, coprocessor, and automation clients.
6. `spyglass/`
   Operator UIs, dashboards, and observational tooling.
7. `products/`
   Final firmware compositions for supported board targets and deployment bundles.

## Runtime Model

The runtime configuration system should eventually represent:

- board pin mappings
- transport capabilities
- motor and sensor calibration
- motion envelopes and safety thresholds
- optional host companion policy
- installed peripherals and supported features

That lets the same firmware family scale from a single bench fixture to alternate control boards without recompiling the whole behavior model each time.

## Update Model

TackleBox should support three update paths:

1. `BootAnchors` update or recovery when the board needs transport/bootstrap changes.
2. `KeelWare` reflash when the board contract, flash map, or hardware-facing implementation changes.
3. `Boatswain` and host-side package updates without forcing a board-specific rebuild when the change is above the hardware abstraction line.

## Immediate Refactor Priorities

1. Continue shrinking `src/main.cpp` into a KeelWare app shell.
2. Move config parsing and persistence into dedicated Boatswain-facing modules.
3. Separate telemetry formatting from command dispatch and define a stable Semaphorio contract.
4. Formalize board capability descriptors so alternate boards are additive instead of fork-driven.
5. Split the current host scripts into deliberate Semaphorio and SpyGlass surfaces.
6. Expand host tests around safety lockout, config source precedence, and command/telemetry contracts.