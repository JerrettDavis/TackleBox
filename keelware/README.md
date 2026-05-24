# KeelWare

`KeelWare` owns the flash-resident board package and hardware abstraction layer.

Current mapped implementation:

- `src/app_board.cpp`
- `src/app_runtime_config.cpp`
- `src/app_tmc_link.cpp`
- `src/boot_mode.cpp`
- `src/boot_mode.h`
- `src/usb_cdc_bridge.c`
- `src/usb_cdc_bridge.h`
- `src/usbd_cdc_if.c`
- `src/usbd_cdc_if.h`
- `src/usbd_conf.c`
- `src/usbd_conf.h`
- `src/usbd_desc.c`
- `src/usbd_desc.h`
- `src/fatfs_core.c`
- `src/sdcard_fatfs.c`
- `src/usb_mw/`

Immediate rule: keep linker-sensitive board startup and transport code in `src/` until the package split is validated with the existing PlatformIO environments.

This directory is the landing zone for the board package contract described in `docs/REPOSITORY_LAYOUT.md`.

The first concrete board contract is now documented at `keelware/contracts/skr2-f429.md`.