# Release Readiness

## Goal

This document defines the minimum standard for publishing TackleBox for external consumption.

Shipping means more than pushing code to GitHub. The repository is only ready when naming, architecture, validation, board procedures, and release evidence are coherent for a new consumer.

## Readiness Gates

### Product identity

- repository name, README, docs, and release artifacts all use `TackleBox` consistently
- the current slice of `BootAnchors`, `KeelWare`, `Boatswain`, `Semaphorio`, and `SpyGlass` is described explicitly
- known bring-up-specific names are documented as transitional rather than presented as final product boundaries

### Board readiness

- at least one reference board can be flashed from a clean checkout using documented steps
- bootloader offsets, flash layout, and recovery paths are documented
- board smoke tests cover boot, enumeration, movement, stop inputs, homing, and storage/config flows
- a known-good image is left on the reference board after validation

### Validation readiness

- host tests are green
- local CI-equivalent validation is green
- GitHub Actions CI is green
- hardware validation scripts are runnable and documented
- failures produce actionable logs or artifacts

### Documentation readiness

- architecture docs explain the product boundary and layering
- operator docs explain build, flash, recovery, and validation workflows
- portability docs explain how additional boards should be added
- the repo states what is implemented today versus what is planned

### Packaging readiness

- release artifacts are named predictably
- firmware environments and produced binaries are documented
- host tools required for bring-up and validation are versioned in-repo
- future splitting into subpackages or subprojects has a documented path

Current reference naming for `products/skr2-f429`:

- application artifact: `tacklebox-keelware-skr2-f429-app.bin`
- BootAnchor artifact: `tacklebox-bootanchor-skr2-f429-cdc.bin`
- release manifest: `products/skr2-f429/manifest.json`

Current flashless Boatswain update contract:

- package spec: `boatswain/PACKAGE_FORMAT_V1.md`
- reference installer: `semaphorio/boatswain/install-package.ps1`
- reference product wrapper: `products/skr2-f429/install-boatswain-package.ps1`

## Pre-Push Checklist

1. Run `test/run_ci_checks.ps1` locally.
2. Run `act -j validate-linux` locally.
3. Run the live serial and bootloader validation scripts on the reference board.
4. Confirm the board ends in application mode with a known-good image.
5. Review `README.md`, `docs/ARCHITECTURE.md`, `docs/PORTABILITY.md`, and this file for consistency.
6. Confirm no critical TODOs remain in the current implementation plan.

Supporting release docs:

- `docs/FIRST_PUBLIC_PUSH_CHECKLIST.md`
- `.github/templates/RELEASE_NOTES_TEMPLATE.md`

## Current Gaps Before First Public Push

- repository folder and package naming still reflect the SKR 2 bring-up history
- `lib/keyswitch_core` is still branded around the current bring-up domain instead of the final `Boatswain` package name
- multi-board support is still architectural intent rather than a demonstrated second-board implementation
- release packaging exists for the SKR 2 reference product, but GitHub release automation is not yet defined
- TMC UART verification on the current reference bench still reports `verify=0` / `ifcnt_valid=0` after the firmware-side sync-byte path was corrected and re-tested live, so the remaining blocker should be treated as board-side wiring, jumper, address, or module-state investigation rather than as a resolved motion subsystem

## Release Decision Rule

Do not publish the first public TackleBox release until the current reference board can be cloned, built, flashed, validated, and understood by someone who did not participate in the bring-up.