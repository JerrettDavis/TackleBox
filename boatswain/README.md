# Boatswain

`Boatswain` owns board-agnostic on-device supervisor behavior.

Current mapped implementation:

- `lib/keyswitch_core/include/keyswitch_domain.h`
- `lib/keyswitch_core/src/keyswitch_domain.cpp`
- `lib/keyswitch_core/include/keyswitch_protocol.h`
- `lib/keyswitch_core/src/keyswitch_protocol.cpp`
- `lib/keyswitch_core/include/keyswitch_tmc2209.h`
- `lib/keyswitch_core/src/keyswitch_tmc2209.cpp`

Immediate rule: `lib/keyswitch_core/` remains the build source of truth until compatibility headers are added under this package.

Compatibility headers now exist under `boatswain/include/` and forward to the current `lib/keyswitch_core/include/` files. That gives new code a stable landing zone while preserving the validated build layout.

This directory is the landing zone for the runtime package split described in `docs/REPOSITORY_LAYOUT.md`.

The first concrete flashless update contract is now documented in `boatswain/PACKAGE_FORMAT_V1.md` and exercised through the `config-profile` package installer at `semaphorio/boatswain/install-package.ps1`.