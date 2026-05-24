---
goal: Establish TackleBox product architecture, repository boundaries, and first-public-release readiness for the current SKR 2 reference implementation
version: 1.0
date_created: 2026-05-23
last_updated: 2026-05-23
owner: Jerrett Davis / GitHub Copilot
status: Planned
tags: [architecture, migration, release, firmware, host-tooling]
---

# Introduction

![Status: Planned](https://img.shields.io/badge/status-Planned-blue)

This plan defines the first deterministic path from the current SKR 2 bring-up repository to a publishable TackleBox repository. It formalizes the named stack layers, identifies the file boundaries that already exist, and sequences the work required before the first public push.

## 1. Requirements & Constraints

- **REQ-001**: The product shall support standalone on-device operation without requiring a host or coprocessor for safe execution.
- **REQ-002**: The product shall support optional host-assisted orchestration over CDC/serial on Windows, Linux, and macOS.
- **REQ-003**: The current SKR 2 reference implementation shall remain buildable and field-recoverable during the migration.
- **REQ-004**: Bootloader support shall remain optional and board-specific under the `BootAnchors` concept.
- **REQ-005**: The hardware abstraction layer shall become the narrow board-specific flash-resident boundary under the `KeelWare` concept.
- **REQ-006**: The on-device runtime shall evolve toward an independently updateable supervisor contract under the `Boatswain` concept.
- **REQ-007**: Host tools shall be separated into API/orchestration (`Semaphorio`) and observational/operator (`SpyGlass`) surfaces.
- **REQ-008**: The repository shall include deterministic validation and board bring-up documentation before first public release.
- **CON-001**: The existing PlatformIO-based firmware environments in `platformio.ini` must keep working while boundaries are being refactored.
- **CON-002**: The existing SKR 2 CDC bootloader and USB-only flashing path must remain available as the primary recovery path.
- **CON-003**: Safety-critical behavior must remain executable on-device and must not become host-dependent.
- **CON-004**: The remote GitHub repository is empty, so the first push should happen only after the minimum publishability bar is met.
- **PAT-001**: Prefer additive package boundaries and compatibility shims over big-bang renames.
- **PAT-002**: Prefer host-testable modules for protocol, state, and safety semantics.
- **GUD-001**: Update user-facing docs in the same phase as public naming or workflow changes.

## 2. Implementation Steps

### Implementation Phase 1

- **GOAL-001**: Define the TackleBox product model and make the repository self-describing for new consumers.

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-001 | Update `README.md` to describe TackleBox, `BootAnchors`, `KeelWare`, `Boatswain`, `Semaphorio`, and `SpyGlass` using the current SKR 2 implementation as the first reference slice. | ✅ | 2026-05-23 |
| TASK-002 | Update `docs/ARCHITECTURE.md` to formalize the named stack layers, target boundaries, and update model. | ✅ | 2026-05-23 |
| TASK-003 | Update `CONTRIBUTING.md` so new work follows the TackleBox naming and layering model. | ✅ | 2026-05-23 |
| TASK-004 | Add `docs/RELEASE_READINESS.md` defining the minimum publishability standard for the first public release. | ✅ | 2026-05-23 |

### Implementation Phase 2

- **GOAL-002**: Separate the current implementation into explicit package and directory boundaries without breaking the working SKR 2 stack.

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-005 | Create a target repository layout for `bootanchors/`, `keelware/`, `boatswain/`, `semaphorio/`, `spyglass/`, and `products/` and map current files into that structure in a design document. | ✅ | 2026-05-23 |
| TASK-006 | Introduce compatibility wrappers or directory aliases so `lib/keyswitch_core/` can evolve toward the `Boatswain` package name without a destabilizing rename. | ✅ | 2026-05-23 |
| TASK-007 | Split shared host tooling in `tools/` into deliberate `Semaphorio` and `SpyGlass` modules with a stable command contract. | ✅ | 2026-05-23 |
| TASK-008 | Define the KeelWare board contract for pins, transports, storage, capabilities, and boot offsets, using the SKR 2 as the first concrete implementation. | ✅ | 2026-05-23 |

### Implementation Phase 3

- **GOAL-003**: Raise validation from bring-up quality to release quality.

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-009 | Expand host tests to cover configuration source precedence, telemetry contract stability, and invalid runtime mappings. |  |  |
| TASK-010 | Add documented board smoke-test scripts covering boot, toggle, configure, move, switch readback, home, and recovery paths. | ✅ | 2026-05-23 |
| TASK-011 | Add release artifact naming and packaging rules for bootloader, board image, and host tooling outputs. | ✅ | 2026-05-23 |
| TASK-012 | Execute the self-hosted hardware validation workflow and capture the runner prerequisites in docs. |  |  |

### Implementation Phase 4

- **GOAL-004**: Prove that TackleBox is portable and consumable beyond one bring-up session.

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-013 | Add a second board plan or prototype implementation to prove the KeelWare contract is additive instead of SKR 2 specific. |  |  |
| TASK-014 | Document the flashless-update model for Boatswain and Semaphorio, including what can change without reflashing KeelWare. | ✅ | 2026-05-23 |
| TASK-015 | Prepare the first public repository push checklist and release notes template. | ✅ | 2026-05-23 |
| TASK-016 | Verify the full clone-to-board onboarding flow using only repository docs and scripts. |  |  |

## 3. Alternatives

- **ALT-001**: Keep the repository branded around the SKR 2 bring-up target until multiple boards exist. Rejected because it obscures the intended product boundary and makes early architectural decisions look incidental.
- **ALT-002**: Perform a full directory/package rename immediately. Rejected because it would mix branding work with functional refactors and risk destabilizing the validated firmware stack.
- **ALT-003**: Publish the empty GitHub repository now and refactor in public. Rejected because the repository is not yet coherent enough for outside consumers.

## 4. Dependencies

- **DEP-001**: `platformio.ini` firmware environments for `skr2_f429_usb`, `skr2_f429`, `skr2_f429_bootloader_cdc`, and related SKR 2 targets.
- **DEP-002**: Existing host validation entrypoints in `test/run_ci_checks.ps1`, `test/run_host_tests.ps1`, `test/run_live_serial_tests.ps1`, and `test/run_bootloader_serial_tests.ps1`.
- **DEP-003**: Existing host tooling in `tools/flash-over-cdc.ps1`, `tools/board-usb.ps1`, and `tools/usb_serial_common.ps1`.
- **DEP-004**: Existing architecture and quality docs in `docs/ARCHITECTURE.md`, `docs/PORTABILITY.md`, and `docs/QUALITY.md`.

## 5. Files

- **FILE-001**: `README.md` — top-level product identity and scope.
- **FILE-002**: `docs/ARCHITECTURE.md` — named stack layers and target boundaries.
- **FILE-003**: `docs/RELEASE_READINESS.md` — first-public-release quality gate.
- **FILE-004**: `CONTRIBUTING.md` — contribution boundary rules.
- **FILE-005**: `platformio.ini` — board and bootloader build matrix that must remain stable during migration.
- **FILE-006**: `tools/` — early Semaphorio and SpyGlass implementation surface.
- **FILE-007**: `src/` — current BootAnchors and KeelWare implementation surface.
- **FILE-008**: `lib/keyswitch_core/` — current Boatswain implementation surface.

## 6. Testing

- **TEST-001**: Run `test/run_host_tests.ps1` after any refactor affecting protocol or domain logic.
- **TEST-002**: Run `test/run_ci_checks.ps1` before first public push.
- **TEST-003**: Run `act -j validate-linux` before first public push.
- **TEST-004**: Run `test/run_live_serial_tests.ps1` on the reference board before first public push.
- **TEST-005**: Run `test/run_bootloader_serial_tests.ps1` on the reference board before first public push.

## 7. Risks & Assumptions

- **RISK-001**: The current code and directory names may bias future work toward the original bring-up shape unless the architectural naming is made explicit early.
- **RISK-002**: A premature big-bang rename could break build scripts, tests, and hardware workflows that are already validated.
- **RISK-003**: The flashless-update story for Boatswain is still conceptual and requires a concrete package/update mechanism before it can be advertised as complete.
- **ASSUMPTION-001**: The SKR 2 remains the reference validation board for the first public release.
- **ASSUMPTION-002**: CDC/serial remains the first transport for Semaphorio and SpyGlass.
- **ASSUMPTION-003**: The first public release should optimize for clarity and reliability over breadth of supported boards.

## 8. Related Specifications / Further Reading

- `docs/ARCHITECTURE.md`
- `docs/PORTABILITY.md`
- `docs/QUALITY.md`
- `docs/RELEASE_READINESS.md`
- `README.md`