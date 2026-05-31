# SKR2 F429 Reference Product

This directory defines the first explicit TackleBox reference product package.

The current target is the BigTreeTech SKR 2 board using the STM32F429VGT6 MCU. This is the reference implementation used to validate the current `BootAnchors`, `KeelWare`, `Boatswain`, `Semaphorio`, and `SpyGlass` slices.

## Product Identity

- product id: `skr2-f429`
- board vendor: `BigTreeTech`
- board name: `SKR 2`
- mcu: `STM32F429VGT6`
- framework: `stm32cube`
- primary firmware environment: `skr2_f429_usb`
- primary bootloader environment: `skr2_f429_bootloader_cdc`

## Current Deliverables

### Board image

- environment: `skr2_f429_usb`
- board definition: `boards/btt_skr2_f429_usb.json`
- linker script: `STM32F429VGTx_FLASH_SKR2.ld`
- flash offset: `0x08008000`
- build output: `.pio/build/skr2_f429_usb/firmware.bin`
- release artifact name: `tacklebox-keelware-skr2-f429-app.bin`

### Alternate board images

- `skr2_f429`: ST-Link/SWD-oriented application build using `boards/btt_skr2_f429.json`
- `skr2_f429_usb_64k`: app base at `0x08010000`
- `skr2_f429_usb_0`: no bootloader offset app image

### BootAnchor image

- environment: `skr2_f429_bootloader_cdc`
- linker script: `STM32F429VGTx_FLASH_BOOTLOADER.ld`
- flash range: `0x08000000` to `0x08007FFF`
- application base: `0x08008000`
- build output: `.pio/build/skr2_f429_bootloader_cdc/firmware.bin`
- release artifact name: `tacklebox-bootanchor-skr2-f429-cdc.bin`

## Product-Owned Entry Points

- discover board state: `products/skr2-f429/discover.ps1`
- build application: `products/skr2-f429/build-app.ps1`
- flash application over USB CDC: `products/skr2-f429/flash-app.ps1`
- recover application over ST-Link: `products/skr2-f429/recover-app.ps1`
- build BootAnchor: `products/skr2-f429/build-bootanchor.ps1`
- flash BootAnchor: `products/skr2-f429/flash-bootanchor.ps1`
- install Boatswain config package: `products/skr2-f429/install-boatswain-package.ps1`
- validate runtime USB surface: `products/skr2-f429/validate-runtime.ps1`
- validate BootAnchor and transitions: `products/skr2-f429/validate-bootanchor.ps1`
- validate the product wrapper workflow: `products/skr2-f429/validate-all.ps1`
- package release artifacts: `products/skr2-f429/package-release.ps1`

## USB Identity

- application CDC VID/PID: `0483:5740`
- bootloader CDC VID/PID: `0483:5741`
- last validated local ports: application `COM16`, bootloader `COM19`

These COM port numbers are not stable identifiers. Scripts should resolve devices by VID/PID and only use COM names as resolved runtime values.

## Required Tooling

- PowerShell 7+
- PlatformIO
- host `g++` toolchain for host tests
- ST-Link V2 for direct SWD recovery or first-time bootloader deployment

## Primary Workflows

Policy:
Use the product-owned scripts as the normal operator surface. For the application image, prefer USB CDC flashing via `flash-app.ps1`. Reserve ST-Link app flashing for diagnostics, recovery, or when the USB bootloader path is unavailable. Use ST-Link for first-time BootAnchor deployment.

### Local validation

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_ci_checks.ps1"
```

### Linux CI simulation via `act`

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
act -j validate-linux
```

### Build application

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\products\skr2-f429\build-app.ps1"
```

### Build BootAnchor

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\products\skr2-f429\build-bootanchor.ps1"
```

### Flash BootAnchor via ST-Link

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\products\skr2-f429\flash-bootanchor.ps1"
```

### Flash application via USB CDC

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\products\skr2-f429\flash-app.ps1" -EnterBootloader -FirmwarePath ".\.pio\build\skr2_f429_usb\firmware.bin" -SkipBuild
```

### Recover application via ST-Link

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\products\skr2-f429\recover-app.ps1"
```

### Operator control via SpyGlass wrapper

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\spyglass\operator\board-usb.ps1" -Action discover
```

### Package a release bundle

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\products\skr2-f429\package-release.ps1"
```

### Install a Boatswain V1 config package

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\products\skr2-f429\install-boatswain-package.ps1" -PackagePath ".\boatswain\packages\examples\skr2-safe-low-current-v1" -Persist
```

## Validation Spine

The current release-quality validation for this reference product is:

1. `test/run_host_tests.ps1`
2. `test/run_ci_checks.ps1`
3. `act -j validate-linux`
4. `test/run_bootloader_serial_tests.ps1`
5. `test/run_live_serial_tests.ps1`

Use `products/skr2-f429/validate-all.ps1` to exercise the operator-facing wrapper flow in the preferred order: app build, BootAnchor build, USB app flash, BootAnchor transition validation, and runtime validation. Add `-IncludeRecovery` only when you explicitly want an ST-Link recovery pass.

## Reference Docs

- `docs/STLINK_HOOKUP.md`
- `docs/RELEASE_READINESS.md`
- `docs/REPOSITORY_LAYOUT.md`
- `keelware/contracts/skr2-f429.md`
- `products/skr2-f429/manifest.json`
- `boatswain/PACKAGE_FORMAT_V1.md`

## Current Product Boundary

This product package is explicit now, but the implementation remains transitional:

- product composition still terminates in `src/main.cpp`
- PlatformIO environments still live in `platformio.ini`
- live validation scripts still live in `test/`

That is acceptable for the first public release as long as this manifest remains accurate and the validation spine stays green.