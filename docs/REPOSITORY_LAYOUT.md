# Repository Layout

## Goal

This document defines the target TackleBox repository layout and the compatibility migration path from the current SKR 2 bring-up structure.

The purpose is to create explicit landing zones for future refactors without breaking the currently validated build, flash, and recovery flows.

## Target Top-Level Structure

```text
bootanchors/
  stm32/
    skr2-f429/
keelware/
  stm32/
    common/
    skr2-f429/
boatswain/
  include/
  src/
semaphorio/
  cli/
  serial/
  boot/
spyglass/
  web/
  operator/
products/
  skr2-f429/
docs/
plan/
test/
tools/
boards/
```

## Ownership Rules

### `bootanchors/`

Owns optional bootloaders, recovery shells, and transport/bootstrap flows.

Examples of code that belongs here:

- bootloader startup and command loops
- flash erase/write/read primitives used only by the recovery image
- boot handoff and application-jump helpers
- bootloader-specific USB descriptors and protocol glue

### `keelware/`

Owns the flash-resident board package and HAL boundary.

Examples of code that belongs here:

- board clocks, startup, and board init
- GPIO, timers, UART, USB, SDIO, and flash wiring
- board capability descriptors and pin-role resolution
- board-specific bridges that expose narrow contracts to Boatswain

### `boatswain/`

Owns board-agnostic on-device supervisor logic.

Examples of code that belongs here:

- command parsing
- motion and homing state machines
- telemetry/state formatting contracts
- reusable TMC helpers and future macro execution

### `semaphorio/`

Owns host and coprocessor API/orchestration behavior.

Examples of code that belongs here:

- serial session handling
- boot/config orchestration
- flashing and transport helpers
- future desktop or service-side automation entrypoints

### `spyglass/`

Owns observational tooling and operator interfaces.

Examples of code that belongs here:

- dashboards
- operator CLIs and guided diagnostics
- telemetry capture/visualization flows
- future web or desktop UI surfaces

### `products/`

Owns composed, supported deliverables.

Examples of code that belongs here:

- reference board manifests
- release packaging metadata
- product-level onboarding and board smoke-test bundles

## Current-To-Target Mapping

### Current `src/` mapping

| Current file | Target location | Notes |
|------|-------------|------|
| `src/bootloader_cdc_main.cpp` | `bootanchors/stm32/skr2-f429/src/bootanchor_cdc_main.cpp` | Current SKR 2 CDC BootAnchor entrypoint. |
| `src/bootloader_main.cpp` | `bootanchors/stm32/skr2-f429/src/bootanchor_dfu_main.cpp` | Alternate DFU-oriented BootAnchor path. |
| `src/bootloader_flash.cpp` | `bootanchors/stm32/common/src/bootanchor_flash.cpp` | Shared recovery-image flash helpers if kept bootloader-only. |
| `src/bootloader_flash.h` | `bootanchors/stm32/common/include/bootanchor_flash.h` | Shared recovery-image flash API. |
| `src/bootloader_protocol.cpp` | `bootanchors/common/src/bootanchor_protocol.cpp` | Host-testable bootloader command parsing. |
| `src/bootloader_protocol.h` | `bootanchors/common/include/bootanchor_protocol.h` | BootAnchor protocol contract. |
| `src/boot_mode.cpp` | `keelware/stm32/common/src/boot_mode.cpp` | Shared app-to-bootloader handoff contract. |
| `src/boot_mode.h` | `keelware/stm32/common/include/boot_mode.h` | Shared boot-mode coordination header. |
| `src/main.cpp` | `products/skr2-f429/firmware/main.cpp` | Final product composition entrypoint until KeelWare/Boatswain composition is split further. |
| `src/app_board.cpp` | `keelware/stm32/skr2-f429/src/app_board.cpp` | Board GPIO and low-level board helpers. |
| `src/app_runtime_config.cpp` | `keelware/stm32/skr2-f429/src/app_runtime_config.cpp` | Transitional home until config contracts are split between KeelWare and Boatswain. |
| `src/app_tmc_link.cpp` | `keelware/stm32/skr2-f429/src/app_tmc_link.cpp` | Board-specific TMC transport bridge. |
| `src/usb_cdc_bridge.c` | `keelware/stm32/common/src/usb_cdc_bridge.c` | MCU-family transport bridge. |
| `src/usb_cdc_bridge.h` | `keelware/stm32/common/include/usb_cdc_bridge.h` | Shared CDC bridge header. |
| `src/usbd_cdc_if.c` | `keelware/stm32/common/src/usbd_cdc_if.c` | STM32 USB CDC interface glue. |
| `src/usbd_cdc_if.h` | `keelware/stm32/common/include/usbd_cdc_if.h` | STM32 USB CDC interface header. |
| `src/usbd_conf.c` | `keelware/stm32/common/src/usbd_conf.c` | STM32 USB device middleware glue. |
| `src/usbd_conf.h` | `keelware/stm32/common/include/usbd_conf.h` | STM32 USB device middleware header. |
| `src/usbd_desc.c` | `keelware/stm32/common/src/usbd_desc.c` | USB descriptor implementation. |
| `src/usbd_desc.h` | `keelware/stm32/common/include/usbd_desc.h` | USB descriptor header. |
| `src/fatfs_core.c` | `keelware/stm32/common/src/fatfs_core.c` | Filesystem glue owned by the board package layer. |
| `src/sdcard_fatfs.c` | `keelware/stm32/skr2-f429/src/sdcard_fatfs.c` | Board-specific SDIO/FatFs integration. |
| `src/bootloader_dfu_media.c` | `bootanchors/stm32/skr2-f429/src/bootanchor_dfu_media.c` | DFU bootloader media backend. |
| `src/usb_mw/` | `keelware/stm32/common/src/usb_mw/` | Middleware support files. |

### Current `lib/keyswitch_core/` mapping

| Current file | Target location | Notes |
|------|-------------|------|
| `lib/keyswitch_core/include/keyswitch_domain.h` | `boatswain/include/boatswain_domain.h` | Keep transitional compatibility include during migration. |
| `lib/keyswitch_core/src/keyswitch_domain.cpp` | `boatswain/src/boatswain_domain.cpp` | Core domain state machine. |
| `lib/keyswitch_core/include/keyswitch_protocol.h` | `boatswain/include/boatswain_protocol.h` | Device command contract. |
| `lib/keyswitch_core/src/keyswitch_protocol.cpp` | `boatswain/src/boatswain_protocol.cpp` | Device command parser. |
| `lib/keyswitch_core/include/keyswitch_tmc2209.h` | `boatswain/include/boatswain_tmc2209.h` | Reusable TMC helper header. |
| `lib/keyswitch_core/src/keyswitch_tmc2209.cpp` | `boatswain/src/boatswain_tmc2209.cpp` | Reusable TMC helper implementation. |

### Current `tools/` mapping

| Current file or directory | Target location | Notes |
|------|-------------|------|
| `tools/usb_serial_common.ps1` | `semaphorio/serial/usb_serial_common.ps1` | Shared serial/session substrate. |
| `tools/flash-over-cdc.ps1` | `semaphorio/boot/flash-over-cdc.ps1` | Boot and update orchestration tool. |
| `tools/boot-config-service/boot_config_service.ps1` | `semaphorio/boot/boot_config_service.ps1` | Host-provided boot config injection. |
| `tools/boot-config-service/device.cfg.example` | `semaphorio/boot/device.cfg.example` | Boot/session config example. |
| `tools/board-usb.ps1` | `spyglass/operator/board-usb.ps1` | Operator-facing diagnostic/control CLI. |
| `tools/arm-dashboard/` | `spyglass/web/arm-dashboard/` | Web Serial dashboard. |

### Current `test/` mapping

| Current test surface | Target ownership | Notes |
|------|-------------|------|
| `test/test_domain/` | `boatswain` | Host tests for core runtime state machines. |
| `test/test_protocol/` | `boatswain` | Host tests for command parsing. |
| `test/test_bootloader_protocol/` | `bootanchors` | Host tests for bootloader command parsing. |
| `test/run_live_serial_tests.ps1` | `products/skr2-f429` | Reference board live app validation. |
| `test/run_bootloader_serial_tests.ps1` | `products/skr2-f429` | Reference board live bootloader validation. |
| `test/run_ci_checks.ps1` | `products/shared` | Repository-wide validation spine until packaging is split. |

### Current `products/skr2-f429/` surface

| Product wrapper or manifest | Target ownership | Notes |
|------|-------------|------|
| `products/skr2-f429/README.md` | `products/skr2-f429` | Human-readable reference product manifest. |
| `products/skr2-f429/manifest.json` | `products/skr2-f429` | Machine-readable release/package manifest. |
| `products/skr2-f429/discover.ps1` | `products/skr2-f429` | Product-owned board discovery entrypoint. |
| `products/skr2-f429/flash-app.ps1` | `products/skr2-f429` | Product-owned application flash entrypoint. |
| `products/skr2-f429/flash-bootanchor.ps1` | `products/skr2-f429` | Product-owned BootAnchor flash entrypoint. |
| `products/skr2-f429/validate-runtime.ps1` | `products/skr2-f429` | Product-owned live runtime validation entrypoint. |
| `products/skr2-f429/validate-bootanchor.ps1` | `products/skr2-f429` | Product-owned bootloader validation entrypoint. |
| `products/skr2-f429/validate-all.ps1` | `products/skr2-f429` | Product-owned validation spine wrapper. |
| `products/skr2-f429/package-release.ps1` | `products/skr2-f429` | Deterministic release packaging entrypoint. |

## Compatibility Migration Rules

1. Do not move working source files and build scripts in one step.
2. Introduce the target directories first with package-level READMEs and mapping docs.
3. Add compatibility wrappers or mirrored includes before renaming public headers.
4. Move host-testable modules before moving board startup or linker-sensitive code.
5. Update `platformio.ini`, host scripts, and CI only after the compatibility path exists.

## Recommended Migration Sequence

### Sequence 1: Skeleton and ownership

- create the target top-level package directories
- add package READMEs that define ownership and current mapped files
- keep all functional code in place

### Sequence 2: Boatswain compatibility layer

- add `boatswain/include/` compatibility headers that forward to `lib/keyswitch_core/include/`
- add `boatswain/src/` placeholders or wrapper translation units only if needed by the build system
- keep `lib/keyswitch_core/` as the build source of truth until all include paths are updated

### Sequence 3: Host tooling split

- move shared serial helpers and boot services behind a `Semaphorio` contract
- move operator-facing command surfaces and dashboard assets behind `SpyGlass`
- preserve existing PowerShell entrypoints as wrappers until docs and CI are updated

Current status:

- stable `Semaphorio` wrapper entrypoints exist under `semaphorio/serial/` and `semaphorio/boot/`
- a stable `SpyGlass` operator wrapper exists under `spyglass/operator/`
- the existing `tools/` scripts remain the implementation source of truth for now

### Sequence 4: KeelWare and BootAnchors move

- split common STM32 glue from board-specific SKR 2 code
- move bootloader code into `bootanchors/` and board firmware glue into `keelware/`
- leave product composition entrypoints under `products/skr2-f429/`

### Sequence 5: Product packaging

- define release artifact names and output directories per product
- document the supported SKR 2 product bundles
- add onboarding instructions that reference the final package names instead of transitional folders

## Non-Goals For The First Layout Refactor

- replacing PlatformIO in the current repo
- renaming every symbol from `keyswitch_*` to `boatswain_*` immediately
- splitting this repository into multiple Git repositories before the first public release
- changing the validated flash layout or boot offsets as part of a naming refactor