# BootAnchors

`BootAnchors` owns optional bootloaders and recovery environments.

Current mapped implementation:

- `src/bootloader_cdc_main.cpp`
- `src/bootloader_main.cpp`
- `src/bootloader_flash.cpp`
- `src/bootloader_flash.h`
- `src/bootloader_protocol.cpp`
- `src/bootloader_protocol.h`
- `src/bootloader_dfu_media.c`

Immediate rule: keep the working SKR 2 bootloader sources in `src/` until compatibility wrappers and build metadata are ready.

This directory is the landing zone for the post-bring-up package split described in `docs/REPOSITORY_LAYOUT.md`.