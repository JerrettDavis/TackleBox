# TackleBox

## Product Direction

TackleBox is a production-grade dual-mode control stack positioned between Marlin and Klipper.

It must support both operating models at the same time:

- standalone, board-resident control like Marlin
- optional coprocessor-assisted orchestration like Klipper
- a build-once, flash-once board image with post-flash software evolution through stable abstractions

The current SKR 2 STM32F429 work is the first proving ground, not the product boundary. The permanent direction is a portable stack whose board-specific flash image stays thin while higher-level behavior, orchestration, and configuration can evolve independently.

Key reference documents:

- `CONTRIBUTING.md`
- `docs/ARCHITECTURE.md`
- `docs/REPOSITORY_LAYOUT.md`
- `docs/PORTABILITY.md`
- `docs/QUALITY.md`
- `docs/RELEASE_READINESS.md`
- `docs/FIRST_PUBLIC_PUSH_CHECKLIST.md`
- `docs/STLINK_HOOKUP.md`
- `keelware/contracts/skr2-f429.md`
- `products/skr2-f429/README.md`
- `products/skr2-f429/manifest.json`
- `boatswain/PACKAGE_FORMAT_V1.md`
- `plan/architecture-tacklebox-foundation-1.md`

## Product Stack

TackleBox is composed of five named layers.

1. `BootAnchors`
	Optional board bootloaders and recovery environments that add CDC or other field-safe flashing, diagnostics, and recovery flows to boards that do not ship with a sufficient upgrade path.
2. `KeelWare`
	The hardware abstraction and board package layer. This is the board-specific flash image that owns clocks, buses, flash layout, pin roles, boot offsets, and transport wiring.
3. `Boatswain`
	The on-device supervisor/runtime layer. This is the board-resident control plane that executes commands, safety policy, macros, state machines, and updateable logic shipped through the common abstraction layer.
4. `Semaphorio`
	The host and coprocessor API/orchestration layer that talks to TackleBox devices over serial or future transports on Windows, Linux, and macOS.
5. `SpyGlass`
	The operator and observational layer, including dashboards, diagnostics, telemetry capture, and the future off-device or web-served control portal.

## Current Repository Scope

This repository currently contains the first operational slice of TackleBox on the BigTreeTech SKR 2 STM32F429VGT6 board.

What exists today:

- a working resident CDC bootloader path for a board that is otherwise awkward to update over USB
- a board-resident firmware shell with runtime configuration, microSD config loading, TMC controls, and safe motion primitives
- host-side flashing, validation, and operator tooling
- host tests, CI, and hardware-validation workflows

What this repository is evolving toward:

- `BootAnchors` as reusable optional bootloader packages
- `KeelWare` as a stable board package/HAL boundary
- `Boatswain` as an independently updateable device supervisor/runtime contract
- `Semaphorio` and `SpyGlass` as first-class host products rather than ad hoc scripts

The target package skeleton now exists at the repo root in `bootanchors/`, `keelware/`, `boatswain/`, `semaphorio/`, `spyglass/`, and `products/`. Those directories are currently ownership anchors and migration landing zones; the validated implementation still lives in the existing `src/`, `lib/keyswitch_core/`, and `tools/` paths until compatibility moves are completed.

For host tooling, stable wrapper entrypoints now exist under `semaphorio/` and `spyglass/`, while the currently validated implementations remain in `tools/`.

## Present Capabilities

- USB CDC telemetry with heartbeat and status reporting
- USB CDC command parsing
- multi-source homing/safety domain model
- backoff and stop control
- runtime remapping and persisted configuration for active board roles
- boot-time configuration source precedence across USB, microSD, flash, and defaults
- CDC bootloader flashing and app handoff for the SKR 2 bring-up target
- host-runnable unit and integration tests for the domain/protocol layer
- host-assisted flash, mode-switch, and validation workflows
- graph-ready safety telemetry fields for force and stop-source reporting

## Current Device Commands

The current Boatswain command surface accepts both plain-text and a few trimmed-down G-code style aliases:

- `STATUS`
- `M114`
- `M119`
- `CONFIG [key]`
- `CFG [key]`
- `SET <key> <value>`
- `SAVE`
- `SAVECFG`
- `RESETCFG`
- `REBOOT`
- `HOME`
- `G28`
- `SAFETY`
- `M122`
- `DRIVER`
- `TMC`
- `IRUN <0-31>`
- `IHOLD <0-31>`
- `IHOLDDELAY <0-15>`
- `SGTHRS <0-255>`
- `ENABLE`
- `M17`
- `DISABLE`
- `M18`
- `M84`
- `HOLD <on|off>`
- `MOVEABS <steps>`
- `G0 X<mm>`
- `G1 X<mm>`
- `MOVEREL <steps>`
- `JOG <steps>`
- `SETPOS <steps>`
- `PRESSPOS <steps>`
- `CYCLE <count>`
- `SIMLOAD <raw>`
- `SIMTHRESH <raw>`
- `SIMMECH <on|off>`
- `SIMSTALL <on|off>`
- `SIMCLEAR`
- `STOP`
- `M112`
- `BACKOFF`
- `HELP`
- `?`

`STATUS` and `SAFETY` currently share the same structured telemetry payload so host scripts can consume one stable line format while the physical sensor stack is still being wired.

The simulation commands are the current way to validate the load-cell and StallGuard code paths before the load cell is physically attached. They let you drive the safety model over USB and verify state transitions, stop-source selection, and telemetry formatting on the real board.

For a fully local dry run, the repo also now includes `test/run_simulated_probe_workflow.ps1`. That compiles and runs a host-side probe workflow simulator which exercises the same command parser, motion domain, and simulated load-cell threshold path used by the board-facing flow, then exports `tools/arm-dashboard/simulated-probe-curve.json` for visualization in the local arm dashboard.

Current telemetry fields:

- `diag0`: raw DIAG0 input level for the currently configured `pin.diag0` role
- `xstop`: raw mechanical stop input level for the currently configured `pin.x_stop` role
- `diag2`: raw DIAG2 input level for the currently configured `pin.diag2` role
- `pressed`: debounced logical stop state derived from the configured `x_stop` path
- `conf`: confirmed/debounced mechanical stop state
- `load`: load-cell trigger flag
- `mech`: mechanical fallback trigger flag
- `stall`: StallGuard fallback trigger flag
- `source`: last stop source enum (`0 none`, `1 load cell`, `2 mechanical`, `3 stall`, `4 seek-limit fault`)
- `force`: placeholder raw load-cell reading, currently `0` until the frontend is wired
- `homed`: whether the arm has a trusted home/position reference
- `hold`: whether the firmware should keep the driver enabled while idle
- `pos`: current tracked position in steps relative to home
- `target`: current move target in steps
- `press`: configured press target used by the `CYCLE` routine
- `cycles`: remaining routine cycles
- `done`: completed routine cycles
- `loop_last_us`: most recent main-loop interval in microseconds
- `loop_max_us`: largest main-loop interval observed in the current heartbeat window
- `steps_total`: total emitted step pulses since boot
- `steps_hb`: emitted step pulses counted since the last heartbeat line
- `steps_burst`: largest burst of acknowledged timer-driven steps consumed in one loop pass since the last heartbeat line
- `tmc_sync`: whether deferred TMC runtime sync work is currently pending (`0 no`, `1 apply`, `2 verify`, `3 both`)

Driver telemetry:

- `driver uart=<0|1> irun=<0-31> ihold=<0-31> iholddelay=<0-15> tpowerdown=<0-255> sgthrs=<0-255>`

Configuration telemetry:

- `config loaded=<0|1> dirty=<0|1> reboot=<0|1>`
- `config pin.x_step=<Pn> ...`
- `config logic.stop_active_high=<0|1> logic.dir_inverted=<0|1> logic.home_towards_positive=<0|1> ...`
- `config tmc.irun=<n> ...`
- `boot: wait_usb ms=<n>`
- `boot: source=<defaults|flash|usb|microsd>`

The `SAFETY` command also emits a simulation line in this format:

- `sim raw=<n> thresh=<n> load=<0|1> mech=<0|1> stall=<0|1>`

That line reports the active simulation state used by the pre-hardware safety workflow.

## Runtime Configuration

The current KeelWare plus Boatswain image includes a persisted runtime configuration block stored in a reserved internal flash sector, so the board can be retuned and remapped without rebuilding or reflashing the board image.

What is configurable now:

- all GPIO roles currently used by this firmware: `x_uart`, `x_dir`, `x_step`, `x_enable`, `x_stop`, `diag0`, `diag2`, `ps_on`, `safe_power`, and `led`
- motion timing values like `step_pulse_us`, `home_feedrate_mm_per_min`, `move_feedrate_mm_per_min`, and the compatibility `step_interval_us` setter
- indexed channel metadata like labels, enable flags, transport kind, bus index, and transport address
- axis mechanics values like `steps_per_rotation`, `travel_um_per_rotation`, `travel_min_um`, `travel_max_um`, `travel_limit_um`, and `default_press_um` for the active channel
- logic/polarity flags like stop active-high, direction inversion, and whether homing seeks toward the positive or negative coordinate direction
- TMC runtime knobs like `irun`, `ihold`, `iholddelay`, `tpowerdown`, `sgthrs`, and `uart_bit_us`
- persisted simulated load threshold

What still is not firmware-configurable:

- actual supply voltage rails or analog hardware that is not already under software control
- arbitrary alternate peripheral routing beyond simple GPIO role remapping supported by this firmware shell

Workflow:

1. Inspect the current desired config with `CONFIG`.
2. Change values with `SET <key> <value>`.
3. Persist them with `SAVE`.
4. Reboot with `REBOOT` or power-cycle the board when you changed pins or polarity.

Boot-time source order is now:

1. wait for a USB host config session for `BOOT.HOST_WAIT_MS`
2. try microSD config loading
3. fall back to saved internal flash config
4. fall back to safe built-in defaults

The firmware now includes an SDIO + FatFs microSD path and will read `0:device.cfg` when a card is present. Use `CONFIG SOURCES` to see which source was selected for the current boot and whether USB, microSD card/config, flash, and defaults were available.

Examples:

```text
CONFIG
CONFIG SOURCES
CONFIG CHANNELS
CONFIG CH1.LABEL
SET CHANNEL.ACTIVE 0
SET CHANNEL.LABEL X1
SET CH1.LABEL Y1
SET CH1.TRANSPORT LOCAL_GPIO
SET CHANNEL.TRANSPORT LOCAL_GPIO
SET TMC.IRUN 5
SET TMC.IHOLD 0
SET MOTION.STEP_INTERVAL_US 300
SET MOTION.HOME_FEEDRATE_MM_PER_MIN 600
SET MOTION.MOVE_FEEDRATE_MM_PER_MIN 1200
SET MOTION.SEEK_LIMIT_STEPS 8000
SET AXIS.TRAVEL_MIN_UM 0
SET AXIS.TRAVEL_MAX_UM 17500
SET AXIS.TRAVEL_UM_PER_ROTATION 8000
SET AXIS.TRAVEL_LIMIT_UM 17500
SET LOGIC.ENABLE_ACTIVE_LOW 1
SET LOGIC.HOME_TOWARDS_POSITIVE 1
SET TELEMETRY.HEARTBEAT_INTERVAL_MS 500
SET PIN.X_STEP PE2
SAVE
REBOOT
```

Supported `SET` keys:

- `CHANNEL.COUNT`, `CHANNEL.ACTIVE`, `CHANNEL.ENABLED`, `CHANNEL.LABEL`
- `CHANNEL.TRANSPORT`, `CHANNEL.BUS_INDEX`, `CHANNEL.ADDRESS`
- `PIN.X_UART`, `PIN.X_DIR`, `PIN.X_STEP`, `PIN.X_ENABLE`, `PIN.X_STOP`
- `PIN.DIAG0`, `PIN.DIAG2`, `PIN.PS_ON`, `PIN.SAFE_POWER`, `PIN.LED`
- `PIN.LOADCELL_DATA`, `PIN.LOADCELL_CLOCK`
- `LOGIC.STOP_ACTIVE_HIGH`, `LOGIC.DIR_INVERTED`, `LOGIC.ENABLE_ACTIVE_LOW`, `LOGIC.HOME_TOWARDS_POSITIVE`
- `BOOT.HOST_WAIT_MS`
- `MOTION.STEP_INTERVAL_US`, `MOTION.HOME_FEEDRATE_MM_PER_MIN`, `MOTION.MOVE_FEEDRATE_MM_PER_MIN`, `MOTION.STEP_PULSE_US`, `MOTION.SEEK_LIMIT_STEPS`
- `MOTION.STOP_DEBOUNCE_COUNT`, `MOTION.BACKOFF_STEPS`
- `AXIS.STEPS_PER_ROTATION`, `AXIS.TRAVEL_UM_PER_ROTATION`
- `AXIS.TRAVEL_MIN_UM`, `AXIS.TRAVEL_MAX_UM`, `AXIS.TRAVEL_LIMIT_UM`, `AXIS.DEFAULT_PRESS_UM`
- `TELEMETRY.STATUS_INTERVAL_MS`, `TELEMETRY.HEARTBEAT_INTERVAL_MS`
- `LOADCELL.SOURCE`, `LOADCELL.CONNECTOR`, `LOADCELL.THRESHOLD`, `SIM.LOAD_THRESHOLD`
- `TMC.ALLOW_UNVERIFIED_MOTION`
- `TMC.IRUN`, `TMC.IHOLD`, `TMC.IHOLDDELAY`, `TMC.TPOWERDOWN`, `TMC.SGTHRS`, `TMC.UART_BIT_US`

Pin values use the `P<port><pin>` form, for example `PE2` or `PC13`.

The base firmware now stores an indexed channel array. `CHANNEL.ACTIVE` selects which channel the current single-axis runtime binds to, and the channel-scoped `PIN.*`, `LOGIC.*`, `AXIS.*`, and `TMC.*` keys apply to that active channel.

For direct indexed access, `CONFIG CHANNELS` lists the current channel inventory, `CONFIG CH<n>.*` inspects channel `n` without rebinding the runtime, and `SET CH<n>.* <value>` applies channel-scoped metadata or mechanics changes directly to channel `n`.

`AXIS.TRAVEL_MIN_UM` sets the low edge of the current workspace, `AXIS.TRAVEL_MAX_UM` sets the high edge, and `AXIS.TRAVEL_LIMIT_UM` remains the total workspace span. `MOTION.SEEK_LIMIT_STEPS` is kept as a compatibility key, but it now derives and updates that workspace span through the axis mechanics configuration. For new tuning flows, prefer the `AXIS.*` keys.

`CHANNEL.TRANSPORT` currently accepts `LOCAL_GPIO`, `REMOTE_BUS`, and `VIRTUAL`. The current runtime can only execute a `LOCAL_GPIO` active channel; the other transport types exist so the software layer can model future remote MCU and virtual channels without redesigning the config schema again.

For the current `LOCAL_GPIO` runtime, `CHANNEL.ADDRESS` also feeds the active channel's TMC UART slave address. That makes it possible to probe TMC2209 addresses `0..3` live over USB without rebuilding the firmware.

`LOADCELL.SOURCE` currently accepts `SIMULATION`, `HX711`, and `ADC`. `LOADCELL.CONNECTOR` provides board-level presets so the operator can choose a physical landing in firmware config instead of manually entering MCU pins every time. Manual `PIN.LOADCELL_*` edits remain available and automatically switch the connector mode back to `CUSTOM`. `SIM.LOAD_THRESHOLD` remains as a compatibility alias for `LOADCELL.THRESHOLD`.

For the current SKR2 reference package, `LOADCELL.CONNECTOR` accepts:

1. `CUSTOM` for manual `PIN.LOADCELL_*` assignment.
2. `SKR2_BLTOUCH` for the `PE4` + `PE5` BLTouch / servo pair.
3. `SKR2_DET` for the `PC2` + `PA0` detector / runout pair.
4. `SKR2_TH1`, `SKR2_TH0`, or `SKR2_TB` for the thermistor-side analog input presets.

Applying a connector preset also applies the matching default source class for that connector family: `SKR2_BLTOUCH` and `SKR2_DET` select `HX711`, while `SKR2_TH1`, `SKR2_TH0`, and `SKR2_TB` select `ADC`.

For the SKR2 reference board, the right connector choice depends on whether "ADC" means a direct board-side analog input or an external load-cell ADC module such as an HX711:

1. For an external HX711-style module, the best connector set is the BLTouch / servo area because it naturally exposes two logic pins plus nearby power. The SKR2 Marlin pin map assigns `Z_MIN_PROBE_PIN` to `PE4` and `SERVO0_PIN` to `PE5`, so that pair is the cleanest digital landing for `DOUT` and `SCK`.
2. The caveat is that the current bring-up firmware already uses `PE5` as the heartbeat / status LED output. If that indicator must remain intact during load-cell bring-up, the next adequate logic-pin pair is the detector / runout area using `PC2` (`E0DET`) and `PA0` (`E1DET`).
3. For a direct analog voltage path from an already amplified `0-3.3V` signal, the only board-routed ADC-oriented connectors are the thermistor inputs: `TB` on `PA1`, `TH0` on `PA2`, and `TH1` on `PA3`. Treat those as the analog landing only for a conditioned analog signal path, not as the preferred drop-in path for an external digital load-cell ADC board.

Recommended SKR2 connector order:

1. `PE4` + `PE5` in the BLTouch / servo area for an HX711-style external ADC module, if you are willing to reassign the current `PE5` heartbeat LED.
2. `PC2` + `PA0` on the detector / runout headers as the fallback digital pair when you want to preserve `PE5`.
3. `TH1` / `PA3`, then `TH0` / `PA2`, then `TB` / `PA1` only for a true conditioned analog-voltage path.

Do not wire the raw millivolt load-cell bridge directly into any SKR2 header. Use either an external load-cell ADC module such as HX711, or a proper analog front-end that outputs a safe board-compatible voltage.

## Boot Config Service

A small host-side companion script is available in `tools/boot-config-service/`. It is an early `Semaphorio` slice that can watch for the STM32 CDC port, push a `device.cfg` file over USB immediately after boot, and then send `BOOT` so the board starts with the host-provided session config.

That lets you keep everyday tuning off the internal flash and only persist settings when you intentionally send `SAVE`. If no USB host session claims the boot window, the board next checks the onboard microSD card for `device.cfg` before falling back to flash or defaults.

## Repository Structure

The current repository layout is transitional. It contains the future TackleBox slices, but some code still carries the original bring-up naming.

- `src/main.cpp`: firmware entrypoint and hardware adapter
- `src/app_board.cpp`: board GPIO and low-level pin helpers
- `src/app_runtime_config.cpp`: persisted config, boot-source selection, and config telemetry
- `src/app_tmc_link.cpp`: board-side TMC transport and verification
- `src/bootloader_cdc_main.cpp`: current `BootAnchors` CDC recovery shell for the SKR 2
- `src/bootloader_flash.*`: bootloader-side flash layout and application handoff helpers
- `src/boot_mode.*`: reset-persistent boot-mode coordination shared between app and bootloader
- `src/usb_cdc_bridge.*`: USB CDC transport bridge
- `src/usbd_cdc_if.*`: STM32 USB CDC interface glue
- `lib/keyswitch_core/include/keyswitch_domain.h`: current `Boatswain` domain API for homing and motion
- `lib/keyswitch_core/src/keyswitch_domain.cpp`: current `Boatswain` domain state machine implementation
- `lib/keyswitch_core/include/keyswitch_protocol.h`: current device command parsing API
- `lib/keyswitch_core/src/keyswitch_protocol.cpp`: current device command parsing implementation
- `tools/board-usb.ps1`: operator CLI for discovery, mode switching, and direct command execution
- `tools/flash-over-cdc.ps1`: SKR 2 bootloader uploader over CDC
- `tools/usb_serial_common.ps1`: shared host-side serial helpers used by scripts and tests
- `tools/arm-dashboard/`: browser-based Web Serial operator UI
- `test/run_host_tests.ps1`: host test runner
- `test/run_ci_checks.ps1`: local CI-equivalent validation bundle
- `test/run_bootloader_serial_tests.ps1`: live board validation for app and bootloader USB transitions
- `test/test_domain/`: unit tests for homing and motion state
- `test/test_protocol/`: unit tests for command parsing
- `test/test_integration/`: higher-level command-to-domain integration tests

## Roadmap Snapshot

The repo is being prepared in four product phases:

1. stabilize the current SKR 2 reference implementation and board workflows
2. separate `BootAnchors`, `KeelWare`, `Boatswain`, `Semaphorio`, and `SpyGlass` into explicit package and repo boundaries
3. add multi-board descriptors and make board support additive instead of copy-driven
4. package releases, docs, and validation artifacts so outside users can consume the project safely

## CI

GitHub Actions now runs the same validation spine intended for local use:

- host tests via `test/run_host_tests.ps1`
- firmware builds for all declared PlatformIO environments via `test/run_ci_checks.ps1`

The workflow lives in `.github/workflows/ci.yml`.

## Build

Embedded firmware build:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& "C:\Users\jd\.platformio\penv\Scripts\platformio.exe" run -e skr2_f429_usb
```

Flash path:

1. Build the firmware.
2. Copy `.pio\build\skr2_f429_usb\firmware.bin` to the SKR2 microSD card as `firmware.bin`.
3. Power-cycle the board.

## ST-Link Path

The SKR2 board definitions already support `stlink`, so an ST-Link V2 can be used for direct SWD flashing and debugger-assisted bring-up.

Before replacing any resident bootloader image, back up the first 32 KiB boot region:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\backup_bootloader.ps1"
```

Restore a captured bootloader image:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\restore_bootloader.ps1" -InputPath ".\artifacts\bootloader-backups\<your-backup>.bin"
```

On Windows, these OpenOCD-based scripts require the ST-Link probe to use a libusb-compatible driver such as `WinUSB` or `libusbK`. If the backup script reports `ProblemCode: 28`, install the driver for `STM32 STLink` first and then re-run the script.

Typical SWD connections:

- ST-Link `SWDIO` -> target `SWDIO`
- ST-Link `SWCLK` -> target `SWCLK`
- ST-Link `GND` -> target `GND`
- ST-Link `3V3` -> target `3V3` sense or target reference voltage
- optional `NRST` -> target `NRST` for cleaner reset control

For the board-relative hookup guide and signal map, see `docs/STLINK_HOOKUP.md`.

Direct upload with PlatformIO:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& "C:\Users\jd\.platformio\penv\Scripts\platformio.exe" run -e skr2_f429_usb -t upload
```

End-to-end hardware validation over ST-Link plus USB CDC:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_stlink_validation.ps1"
```

Optional variants:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_stlink_validation.ps1" -IncludeMotion
& ".\test\run_stlink_validation.ps1" -IncludePersistence
& ".\test\run_stlink_validation.ps1" -Environment skr2_f429_usb_0
```

This path is useful when:

- the SD-card flash path is inconvenient
- USB boot behavior is unstable and you still need to reflash safely
- you want repeatable hardware-in-the-loop validation before manual console testing
- you want to move toward breakpoint-driven TMC or SDIO bring-up later

## USB Recovery Shell

Once the resident CDC bootloader is installed, normal application reflashing can happen directly over the board USB link without ST-Link:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\tools\flash-over-cdc.ps1" -EnterBootloader -FirmwarePath ".\.pio\build\skr2_f429_usb\firmware.bin"
```

The uploader can now auto-discover the resident bootloader port. If the board is currently running the app, add `-EnterBootloader` and it will issue the mode switch first.

Mode transitions over USB:

- from the app console, `BOOTLOADER` or `RECOVERY` resets into the resident bootloader shell
- from the app console, `REBOOT` requests a clean reset back into the application
- from the bootloader shell, `BOOT` transfers into the installed application
- `RESET` restarts the board and leaves it in the default bootloader entry path

Bootloader shell commands:

- `HELP`: print the supported bootloader command set
- `INFO`: report the application flash base and whether a valid app image is present
- `STATUS` or `DIAG`: report bootloader mode, uptime, USB state, and app-presence status
- `FLASH` or `MAP`: report bootloader/app flash layout boundaries
- `READ <offset> <size>`: bounded hex dump from the application slot for recovery inspection
- `ERASE <size>` / `WRITE <offset> <hex>` / `CRC <size>`: application update primitives used by the CDC uploader
- `BOOT`: request a clean handoff into the application
- `RESET`: restart the MCU

The bounded `READ` command is intended for recovery and diagnostics, not bulk transfer. Keep it for spot checks, header inspection, and validating what the uploader actually wrote.

Live transition validation:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_bootloader_serial_tests.ps1"
```

Operator USB control surface:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\tools\board-usb.ps1" -Action discover
& ".\tools\board-usb.ps1" -Action enter-bootloader
& ".\tools\board-usb.ps1" -Action bootloader-command -Command 'status'
& ".\tools\board-usb.ps1" -Action enter-app
& ".\tools\board-usb.ps1" -Action app-command -Command 'help'
```

This gives the repo one shared USB control layer instead of script-local COM-port logic. That is the expected direction if the bootloader shell keeps growing into a broader recovery and maintenance surface.

## Test

Host tests:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_host_tests.ps1"
```

Full local CI bundle:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_ci_checks.ps1"
```

Local GitHub Actions validation with `act`:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
act -j validate-linux
```

The repository now uses a Linux CI job for cross-platform host/build validation, which makes `act` practical locally, and a separate Windows self-hosted hardware workflow for board-connected USB and ST-Link checks.

ST-Link-driven hardware validation:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_stlink_validation.ps1"
```

GitHub Actions hardware validation is defined in `.github/workflows/hardware-validation.yml` and expects a self-hosted runner labeled `windows` and `keyswitch-hil` with the SKR2 board, ST-Link, and USB CDC path connected.

The host test runner compiles and executes:

- bootloader protocol unit tests
- protocol unit tests
- domain unit tests
- integration tests covering parser-to-domain end-to-end behavior for homing, move aliases, stop handling, hold/enable semantics, cycle routines, and motion fault paths

Live-board serial integration tests:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_live_serial_tests.ps1"
```

Optional motion-command coverage:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_live_serial_tests.ps1" -IncludeMotion
```

Optional persistence-command coverage:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_live_serial_tests.ps1" -IncludePersistence
```

The live serial runner now adapts to both runtime safety modes:

- if TMC verification succeeds, it expects enable, hold, and motion commands to execute normally
- if TMC verification fails, it expects the firmware to block energizing and motion commands with `reason=tmc_unverified`

The live suite also exercises command aliases, config-source reporting, driver verification telemetry, simulation toggles, stop commands, and post-command heartbeat continuity.

## Browser Dashboard

A static browser operator console is available under `tools/arm-dashboard/`.

Run the local server:

```powershell
Set-Location "c:\git\BTT SKR2 Testing\tools\arm-dashboard"
& .\serve.ps1
```

Then open `http://localhost:8765/` in Chrome or Edge and connect with Web Serial.

The dashboard provides:

- live state and driver telemetry
- force/position charting
- motion and cycle controls
- driver current tuning controls
- simulation controls for pre-load-cell workflows
- a raw command console and macro runner

## Manual End-to-End Checks

Hardware E2E validation is now split between scripted serial checks and manual motion observation:

1. Run `test\run_live_serial_tests.ps1` to verify heartbeat plus safe command/response behavior.
2. Run `test\run_live_serial_tests.ps1 -IncludeMotion` when the axis is safe to move.
3. Run `test\run_bench_readiness.ps1` to summarize the current TMC verification state and, when requested, replay the calibrated away-and-back motion check.
4. Observe that `HOME` or `G28` triggers seek, switch hit, backoff, and stop.
5. Observe that `HOME` seeks in the configured home direction, and that `G0 X<n>` moves in millimeters while `JOG <n>` and `CYCLE <n>` behave as expected after homing.
6. Observe that `BACKOFF` runs reverse motion as expected.

Example calibrated bench check on the current reference setup:

```powershell
& ".\test\run_bench_readiness.ps1" -IncludeMotion -AllowUnverifiedMotion -TravelTargetSteps -20126 -TravelUmPerRotation 15900 -TravelMinUm -100000 -TravelMaxUm 100000 -HomeFeedrateMmPerMin 600 -MoveFeedrateMmPerMin 1500 -HoldAtTargetMs 15000
```

Use `-HoldAtTargetMs` when you need a stable pause at the outbound position for ruler or caliper checks before the automatic return move. The script now also accepts `-HomeFeedrateMmPerMin` and `-MoveFeedrateMmPerMin` so the reference bench profile can be replayed explicitly. On shutdown it explicitly sends `HOLD OFF` and `DISABLE`, so a completed bench pass should not leave the driver energized at idle.

Pre-load-cell validation flow:

1. Send `SIMTHRESH 1000` to define the simulated load threshold.
2. Send `SIMLOAD 1200` and confirm `SAFETY` reports `load=1`, `source` remains unchanged until motion uses it, and `force=1200`.
3. Send `SIMMECH ON` or `SIMSTALL ON` to validate the fallback telemetry paths.
4. Send `SIMCLEAR` to return the simulated inputs to a neutral state.

## Actuator Control Model

The firmware now maintains a tracked arm position once homed, and it can be orchestrated through scripts or direct commands without immediately dropping motor state:

1. `HOME` establishes the zero reference.
2. `HOLD ON` keeps the driver enabled at idle so the tracked position stays physically meaningful.
3. `G0 X<n>` moves to an absolute millimeter position derived from the configured axis mechanics, while `MOVEABS <n>` remains the explicit absolute step-position command.
4. `JOG <n>` or `MOVEREL <n>` performs a relative move in steps.
5. `PRESSPOS <n>` sets the press depth used by `CYCLE <count>`.
6. `CYCLE <count>` performs repeated press-and-return routines from the current home reference.

The runtime motion model is still step-native internally, but the trimmed G-code surface now maps `G0/G1 X...` millimeter positions through the configured axis mechanics. The explicit bring-up commands `MOVEABS`, `MOVEREL`, `JOG`, `SETPOS`, and `PRESSPOS` remain step-native so scripts and low-level diagnostics can still work directly in step units.

For attached load-cell bring-up, the repo now includes `test/run_probe_cycle.ps1`. That script configures `LOADCELL.SOURCE`, `LOADCELL.CONNECTOR`, `LOADCELL.THRESHOLD`, then runs `HOME`, `PRESSPOS`, and `CYCLE` over the live CDC shell so the full operator probe path is repeatable from one command.

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_probe_cycle.ps1" -AllowUnverifiedMotion -LoadCellConnector SKR2_DET -LoadCellSource HX711 -LoadCellThreshold 1000 -PressTargetSteps 40 -CycleCount 5
```

That script is intentionally honest about the current firmware boundary: the motion/probe command path is ready, but real HX711 or ADC threshold trips still depend on the future acquisition implementation in `src/load_cell.cpp`.

For no-hardware prep, run:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_simulated_probe_workflow.ps1"
```

Then serve `tools/arm-dashboard` and click **Load exported curve** to inspect the simulated force-versus-position response locally before the real board and load cell are hooked up.

The runtime scheduler and domain model are still single-axis today, but the base firmware now carries a generalized indexed channel array with labels and transport metadata. That keeps the keyswitch tester as one application profile over a broader motion/control stack that can grow toward printer, CNC, robotics, multi-MCU, and bus-backed workloads without rewriting the software-layer channel model.

The firmware targets the SKR2 X-slot TMC UART pin on `PE0`, and the command/config surface for TMC2209 current and stall-threshold tuning is in place. On the current reference bench, live verification still reports `driver verify=0` / `ifcnt_valid=0`, so driver-backed motion remains a constrained bring-up path and still depends on `TMC.ALLOW_UNVERIFIED_MOTION=1` for physical travel testing.

Current default startup tuning is intentionally conservative to keep the driver cool during bench work:

- `IRUN 5`
- `IHOLD 0`
- `IHOLDDELAY 4`
- `TPOWERDOWN 4`

That target was chosen after observing roughly `0.8 A` at a hotter configuration; these defaults are meant to land much closer to a light-duty `~0.2 A` starting point. Increase them only as needed for reliable motion under load.

Useful first-pass driver commands:

1. `DRIVER` or `TMC` reports the currently applied UART config.
2. `IRUN <0-31>` sets the TMC run-current scale.
3. `IHOLD <0-31>` sets the TMC hold-current scale.
4. `IHOLDDELAY <0-15>` sets the hold-current transition delay.
5. `SGTHRS <0-255>` sets the TMC2209 StallGuard threshold register.

Current bench notes:

- On the present mechanics setup, home is on the positive side, so travel away from home is the negative coordinate direction.
- That home-side convention is now configurable per channel through `LOGIC.HOME_TOWARDS_POSITIVE`, so other machines can home toward either coordinate direction without changing the electrical dir inversion setting.
- The observed rotation-distance calibration was approximately `2x` high at `AXIS.TRAVEL_UM_PER_ROTATION=8000`; the current temporary bench fit is `AXIS.TRAVEL_UM_PER_ROTATION=15900`, which put `MOVEABS -20126` within about `2 mm` of a `100 mm` reference marker on this setup.
- The current nicest motion tradeoff on this reference bench was `MOTION.HOME_FEEDRATE_MM_PER_MIN=600` and `MOTION.MOVE_FEEDRATE_MM_PER_MIN=1500`; the axis remained smooth, stable, and nearly silent there.
- `MOTION.MOVE_FEEDRATE_MM_PER_MIN=2400` completed repeated away-and-back passes without fault, but it was noticeably noisier and more vibration-prone, so treat that range as exploratory until the TMC chopping and quiet-step behavior are tuned properly.
- The current runtime now uses timer-driven step emission, incremental display flushing, and deferred runtime TMC sync; use the heartbeat `loop_*`, `steps_*`, and `tmc_sync` fields to watch whether later changes reintroduce loop starvation or queued driver work.
- HX711 sampling now uses a falling-edge ready interrupt on the data pin to gate the blocking 24-bit read path, which keeps the main loop from continuously polling the load-cell ready line between samples.
- Do not treat the load-cell phase as ready until the TMC UART path verifies cleanly without the unverified-motion override.

## Design Direction

The current command layer is intentionally small, but it is structured to evolve toward a trimmed-down G-code style surface. The preferred path is:

1. keep plain macro commands for bring-up simplicity
2. add a narrow, documented G-code alias set for motion and diagnostics
3. avoid importing a large printer-oriented G-code stack until the tester domain actually needs it

## Roadmap

Near-term cleanup targets:

1. reduce or eliminate main-loop HX711 polling so sensing does not compete with runtime control work
2. wire in the load-cell frontend calibration flow behind the existing placeholder hooks
3. finish TMC2209 DIAG/UART verification on the real bench without the unverified-motion override
4. split hardware pin maps and board config from firmware startup
5. add richer host-side monitoring/dashboard views on top of the existing heartbeat telemetry

Keyswitch tester targets:

1. load cell acquisition and calibration flow
2. actuator force / travel sampling
3. buttons / joystick / local UI controls
4. richer serial protocol for scripted test sessions
5. structured test results export

## Next Hardware Hooks

The firmware shell now has explicit placeholders for the next two hardware integrations:

1. `src/load_cell.cpp` plus `LOADCELL.SOURCE`, `LOADCELL.THRESHOLD`, `PIN.LOADCELL_DATA`, and `PIN.LOADCELL_CLOCK` for HX711 or ADC-based force acquisition.
2. `read_stallguard_triggered()` in `src/main.cpp` for TMC2209 DIAG or UART-driven stall reporting.
3. `read_mechanical_fallback_triggered()` in `src/main.cpp` for combining the configured `x_stop` signal with simulated or future alternate fallback sources.

The next driver-facing expansion is explicit TMC2209 current configuration so `HOLD ON` can eventually map to a real reduced hold current instead of only keeping the driver enabled.

Once those are connected, the domain already prioritizes stops in this order during seek:

1. load-cell threshold hit
2. StallGuard stall detection
3. mechanical fallback stop

On the SKR2 reference configuration, the default `x_stop` mapping still points at `PC1`, but the runtime now treats that as a configurable role instead of a fixed architectural assumption.