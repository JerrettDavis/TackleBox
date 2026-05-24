# Semaphorio

`Semaphorio` owns host and coprocessor API/orchestration behavior.

Current mapped implementation:

- `tools/usb_serial_common.ps1`
- `tools/flash-over-cdc.ps1`
- `tools/boot-config-service/boot_config_service.ps1`
- `tools/boot-config-service/device.cfg.example`

Stable wrapper entrypoints now exist at:

- `semaphorio/serial/usb_serial_common.ps1`
- `semaphorio/boot/flash-over-cdc.ps1`
- `semaphorio/boot/boot_config_service.ps1`
- `semaphorio/boatswain/install-package.ps1`

Immediate rule: preserve the current PowerShell entrypoints in `tools/` until wrappers and docs are updated together.

This directory is the landing zone for the host orchestration split described in `docs/REPOSITORY_LAYOUT.md`.