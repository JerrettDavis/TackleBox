# Products

`products/` owns composed, supported deliverables.

The current first-class reference product is the SKR 2 STM32F429 target backed by the existing PlatformIO environments in `platformio.ini`.

Current explicit manifest:

- `products/skr2-f429/README.md`
- `products/skr2-f429/manifest.json`

Expected contents over time:

- board-specific firmware compositions
- release manifests
- smoke-test bundles
- onboarding material for supported boards

Until that split is complete, the current product composition remains anchored in `src/main.cpp`, `platformio.ini`, `boards/`, and the live validation scripts under `test/`.