# First Public Push Checklist

## Goal

This checklist exists to prevent an empty-remote push from becoming a public debugging session.

The repository should only be pushed publicly when a new user can understand what TackleBox is, reproduce the reference product flow, and recover the board if something goes wrong.

## Repository Identity

- [x] `README.md` presents the repository as `TackleBox`, not as a one-off SKR 2 bring-up dump
- [x] named stack layers are documented: `BootAnchors`, `KeelWare`, `Boatswain`, `Semaphorio`, `SpyGlass`
- [x] transitional names are documented as transitional rather than final product boundaries

## Reference Product

- [x] `products/skr2-f429/README.md` is current
- [x] `products/skr2-f429/manifest.json` matches the actual build outputs and flash layout
- [x] release artifacts package correctly through `products/skr2-f429/package-release.ps1`
- [ ] `keelware/contracts/skr2-f429.md` still matches the active board contract

## Validation

- [x] `test/run_host_tests.ps1` passes
- [x] `test/run_ci_checks.ps1` passes
- [x] `act -j validate-linux` passes
- [x] `products/skr2-f429/validate-bootanchor.ps1` passes on hardware
- [x] `products/skr2-f429/validate-runtime.ps1` passes on hardware
- [x] the board is left in application mode after validation

## Flashless Update Path

- [x] `boatswain/PACKAGE_FORMAT_V1.md` matches the supported installer behavior
- [x] `products/skr2-f429/install-boatswain-package.ps1` succeeds with the example safe package
- [x] post-reboot config source reports `selected=flash` when the example package is persisted

## Recovery Path

- [ ] ST-Link hookup and restore docs are current in `docs/STLINK_HOOKUP.md`
- [ ] bootloader backup/restore scripts still match the documented flash region
- [ ] the BootAnchor image can still be deployed over ST-Link and the app can still be deployed over USB CDC

## Documentation

- [ ] `docs/ARCHITECTURE.md` is consistent with the actual repo layout
- [ ] `docs/REPOSITORY_LAYOUT.md` matches the package surfaces in the repo
- [ ] `docs/RELEASE_READINESS.md` still reflects the real blockers
- [ ] release notes are drafted from `.github/templates/RELEASE_NOTES_TEMPLATE.md`

## Remaining Honest Blockers

Do not check this file off dishonestly. If any of these remain true, note them explicitly in the first public push summary:

- the current workspace folder is not attached to a local `.git` directory, so this exact checkout cannot be committed or pushed until the real Git clone/worktree is restored
- self-hosted hardware workflow exists but has not yet run in GitHub
- multi-board support is still not demonstrated
- Boatswain code-bearing updates still do not exist beyond V1 config-profile installs

## Session Notes

- local `validate-all.ps1` passed end to end on hardware
- local `act -j validate-linux` passed, but emitted non-fatal warnings because this workspace is not inside a Git repository
- TMC UART verification remains an explicit documented bring-up limitation on the current reference bench

## Final Decision Rule

Push publicly only when the remaining gaps are clearly documented and none of them invalidate safe use of the SKR 2 reference product.