# Boatswain Package Format V1

## Goal

This document defines the first concrete flashless Boatswain update contract for TackleBox.

V1 is intentionally narrow. It supports runtime configuration profile updates over the existing USB CDC command surface without reflashing `KeelWare` or `BootAnchors`.

It does not claim code-bearing on-device module updates yet. That requires a future loader/runtime format that does not exist in the current firmware.

## Scope

Boatswain package format V1 supports:

- applying `SET <key> <value>` configuration commands to a running device
- optionally persisting those changes with `SAVE`
- optionally rebooting the device after install if the package or applied keys require it
- packaging those commands with a machine-readable manifest

Boatswain package format V1 does not support:

- shipping arbitrary executable code to the board
- replacing `KeelWare`
- replacing `BootAnchors`
- introducing new on-device commands not already supported by the firmware

## Package Layout

Each package is a directory containing at least:

```text
manifest.json
commands.cfg
```

Optional additional files may exist for docs or future tooling, but V1 installers only require the manifest and command file.

## Manifest Schema

Required manifest fields:

- `packageFormat`: must equal `boatswain-package-v1`
- `packageType`: must equal `config-profile`
- `packageId`: stable package identifier
- `packageVersion`: package revision string
- `targetProduct`: target product id such as `skr2-f429` or `generic`
- `targetKeelwareContract`: expected board contract id or `generic`
- `commandsFile`: relative path to the command file

Recommended manifest fields:

- `name`
- `description`
- `persistRecommended`
- `rebootRecommended`
- `validation` object describing post-install checks

## Command File Rules

The command file uses the same syntax already accepted by the boot config service and runtime config loader:

- blank lines are ignored
- lines beginning with `#` or `;` are ignored
- `KEY=VALUE` is converted to `SET KEY VALUE`
- explicit `SET KEY VALUE` lines are accepted

V1 command files must only contain configuration operations that normalize into `SET` commands.

Disallowed in V1 packages:

- `SAVE`
- `REBOOT`
- `BOOTLOADER`
- movement commands
- enable/disable/hold commands
- any non-config command

The installer owns persistence and reboot behavior so package contents remain declarative.

## Install Modes

### Runtime apply

The default V1 mode applies commands to the running application over USB CDC.

Recommended flow:

1. resolve the application CDC port
2. apply package commands sequentially
3. inspect command responses for `ok=1`
4. optionally send `SAVE`
5. optionally send `REBOOT`

### Boot-window compatibility

Because `commands.cfg` is intentionally aligned with the existing boot config syntax, a V1 package can also be adapted for the boot window or microSD `device.cfg` flows.

That is a transport compatibility benefit, not a separate package format.

## Safety Rules

- V1 packages should prefer conservative runtime config changes
- packages should not depend on hidden board assumptions beyond the declared product or KeelWare contract
- installers must fail fast if the target product or contract does not match
- installers must reject non-config commands in V1 packages

## Current Reference Installer

The first installer surface is:

- `semaphorio/boatswain/install-package.ps1`

Product wrapper for the current reference board:

- `products/skr2-f429/install-boatswain-package.ps1`

## Example

See:

- `boatswain/packages/examples/skr2-safe-low-current-v1/manifest.json`
- `boatswain/packages/examples/skr2-safe-low-current-v1/commands.cfg`