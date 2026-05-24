---
name: Bug report
about: Report a firmware, tooling, CI, or hardware integration problem
title: '[bug] '
labels: bug
assignees: ''
---

## Summary

Describe the failure clearly.

## Affected Area

- [ ] Firmware behavior
- [ ] Driver or power safety
- [ ] Motion or homing
- [ ] USB or host companion flow
- [ ] microSD or persistence
- [ ] CI or build tooling
- [ ] Documentation

## Target Hardware

- Board:
- MCU:
- Driver:
- Sensors attached:

## Reproduction

1.
2.
3.

## Expected Behavior

What should have happened?

## Actual Behavior

What happened instead?

## Logs or Telemetry

Paste serial output, CI logs, or screenshots.

## Validation Performed

- [ ] `test/run_host_tests.ps1`
- [ ] `test/run_ci_checks.ps1`
- [ ] Live hardware check

## Safety Impact

- [ ] No safety impact known
- [ ] Could energize hardware unexpectedly
- [ ] Could move unexpectedly
- [ ] Could mask fault or stop conditions
- [ ] Could corrupt config or storage