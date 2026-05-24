# SpyGlass

`SpyGlass` owns observational tooling and operator-facing interfaces.

Current mapped implementation:

- `tools/board-usb.ps1`
- `tools/arm-dashboard/`

Stable wrapper entrypoints now exist at:

- `spyglass/operator/board-usb.ps1`

Immediate rule: keep operator entrypoints stable while the package split is being introduced.

This directory is the landing zone for the observational tooling split described in `docs/REPOSITORY_LAYOUT.md`.