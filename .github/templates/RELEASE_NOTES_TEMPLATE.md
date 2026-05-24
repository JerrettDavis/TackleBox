# TackleBox Release Notes Template

## Release Identity

- release tag:
- release date:
- target product(s):
- release type: `reference`, `preview`, `stabilization`, or `production`

## Summary

Describe what changed in one short paragraph focused on user-visible impact.

## Included Artifacts

- application artifact:
- BootAnchor artifact:
- manifest:
- additional host tooling bundles:

## Validated Hardware

- reference board:
- bootloader version or environment:
- application environment:
- validation date:

## Validation Performed

- host tests:
- local CI bundle:
- `act` Linux CI:
- runtime serial validation:
- bootloader serial validation:
- hardware workflow status:

## Flashless Boatswain Update Status

- supported package format version:
- example package validated:
- limitations:

## Recovery Notes

- required recovery tooling:
- boot region backup status:
- known-safe restore path:

## Known Limitations

- limitation 1
- limitation 2
- limitation 3

## Upgrade Guidance

1. Install or update the BootAnchor if required.
2. Flash the application image.
3. Apply any Boatswain config package if the release requires it.
4. Run the documented product validation checks.

## Next Planned Work

- next item 1
- next item 2
- next item 3