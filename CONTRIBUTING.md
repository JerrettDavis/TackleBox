# Contributing

## Project Direction

TackleBox is being built as a dual-use firmware stack:

- fully functional standalone on embedded hardware
- able to pair with a host-side coprocessor for richer planning, tuning, and fleet workflows
- portable across boards by isolating hardware bindings from reusable domain logic

Contributions should move the codebase toward the named TackleBox slices instead of baking more product behavior directly into one board-specific entrypoint.

Current naming model:

- `BootAnchors`: optional bootloader and recovery layer
- `KeelWare`: board package and hardware abstraction layer
- `Boatswain`: on-device supervisor/runtime
- `Semaphorio`: host and coprocessor API/orchestration layer
- `SpyGlass`: observational tooling and operator interfaces

## Local Setup

1. Install PowerShell 7 or newer.
2. Install PlatformIO.
3. Install a `g++` toolchain for host tests.
4. Clone the repository and work from the repo root.

On the current Windows setup, the known-good local PlatformIO path is `C:\Users\jd\.platformio\penv\Scripts\platformio.exe`.

## Validation Commands

Run the full local validation bundle:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_ci_checks.ps1"
```

Run just host tests:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_host_tests.ps1"
```

## Engineering Expectations

- Keep reusable logic in `lib/keyswitch_core/` whenever it does not need direct register or HAL access.
- Move the repository toward explicit `BootAnchors`, `KeelWare`, `Boatswain`, `Semaphorio`, and `SpyGlass` boundaries as files are touched.
- Treat firmware safety paths as testable behavior, not incidental implementation details.
- Prefer adding one narrow validation step per change over broad manual inspection.
- Avoid introducing board assumptions into protocol, motion, or runtime-config layers.
- Preserve standalone operation even when adding host-assisted workflows.

## Pull Request Checklist

- Explain the behavior change and the hardware or host workflow it affects.
- List validation performed.
- Update docs when commands, configuration, boot flow, or supported boards change.
- Add or adjust tests when modifying protocol, motion domain behavior, or safety rules.

## Portability Standard

New hardware support should follow these boundaries:

- board-specific startup, clocks, GPIO, buses, and transport live under `src/` or future board packages
- transport-agnostic and board-agnostic logic lives under `lib/keyswitch_core/`
- runtime configuration should describe capabilities and mappings instead of assuming one board layout

See `docs/ARCHITECTURE.md` and `docs/PORTABILITY.md` before changing hardware interfaces.