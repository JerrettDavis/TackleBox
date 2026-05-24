# Quality Strategy

## Release Standard

Production readiness here means more than a successful flash:

- deterministic local and CI validation
- explicit safety gates
- board bring-up repeatability
- documented operator workflows
- portable architecture boundaries

## Validation Pyramid

### Host tests

Host tests are the fastest path for:

- protocol parsing
- motion-domain behavior
- safety state transitions
- TMC frame encoding and decoding
- integration of commands into domain state

These should remain the default guardrail for most logic changes.

### Firmware builds

Every PR should at minimum build all declared firmware environments so linker and integration regressions surface early.

### Live hardware tests

Hardware validation remains required for:

- boot and enumeration
- driver enable and current safety
- motion correctness
- sensor readback
- homing behavior
- removable media and host companion interactions

## Required Test Growth Areas

1. Config source precedence and persistence integrity.
2. TMC verification lockout behavior.
3. Telemetry contract stability.
4. Sensor fault handling and fallback source selection.
5. Board capability mismatches and invalid runtime config.

## CI Policy

The repository should keep one CI entrypoint that developers can run locally. That entrypoint is now `test/run_ci_checks.ps1`.

CI currently enforces:

- host tests
- firmware builds across declared environments

CI should later expand to include:

- artifact publishing
- generated docs checks
- release packaging
- optional hardware-in-the-loop jobs when infrastructure exists

## Release Gates

Before calling a build production-ready, require:

1. CI green on the target branch.
2. Updated docs for user-visible behavior changes.
3. Host tests covering new behavior or an explicit gap note.
4. Board smoke test evidence for affected hardware paths.
5. Safety-sensitive changes reviewed with failure mode discussion.