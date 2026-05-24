# BTT SKR2 Firmware Diagnostics Guide

## What Changed
The diagnostic firmware provides **visual startup indicators** on PE5 to show:
1. **Code is executing** (runs early, before complex clock setup)
2. **Clock configuration status** (success vs. fallback to HSI)
3. **Ready to run** (all systems initialized)

## LED Pattern Meanings (on PE5 with LED+resistor)

| Pattern | Meaning | Next Action |
|---------|---------|------------|
| 3 quick pulses | ✅ Code reached GPIO_Init_Early() | Normal - wait for next pattern |
| 1 long pulse (500ms on) | ⚠️ HSE clock setup failed, using HSI | System running slow, but functional |
| 2 quick pulses | ✅ Ready to run - main loop starting | Watch for heartbeat |
| Slow blink (1 Hz) | ✅ Firmware running normally | Stepper test runs every 5 sec |

## How to Test

### Setup
1. **Copy firmware to microSD**: Copy `firmware_diagnostic.bin` to SKR2 microSD card (may need to rename to `firmware.bin`)
2. **Connect LED diagnostics** (optional but recommended):
   - PE5 (pin E5) → 220Ω resistor → LED cathode → GND
3. **Or measure with multimeter**: Set to DC voltage, probe PE5 to GND, watch for toggling

### Expected Behavior (with LED connected)

**First 2 seconds after power-up:**
```
Time 0s:     3 quick flashes   (GPIO initialized, HSI running)
Time ~0.5s:  1 long pulse      (IF HSE failed) OR nothing (if HSE OK)
Time ~1s:    2 quick flashes   (Ready to run)
Time 1-5s:   No activity
Time ~5s:    Continuous slow blink at 1 Hz (main loop heartbeat)
```

**If you see something different:**
- **No LED activity at all** → Code not running, likely bootloader/VTOR issue
- **Only 3 pulses, then nothing** → Crash during clock setup (HSE failed and HSI fallback had issue)
- **Only 3 pulses + long pulse, then nothing** → HSE failed, HSI OK, but crash during GPIO_Init()

## What Each Phase Tests

### Phase 1: GPIO_Init_Early() (3 pulses)
- Enables GPIOE and GPIOC clocks
- Configures PE5 as output (heartbeat indicator)
- Configures stepper/endstop pins
- **If this fails**: VTOR or bootloader problem

### Phase 2: Clock Config (long pulse = failure)
- Tries HSE (8 MHz crystal) with PLL to 168 MHz
- If HSE fails, falls back to HSI (16 MHz internal oscillator)
- **If HSE fails**: Crystal not detected or timing capacitors issue
- **If HSI fallback fails**: Something seriously wrong with chip/HAL

### Phase 3: GPIO_Init() (2 pulses)
- Full GPIO initialization for all ports
- **If this fails**: GPIO setup issue

### Phase 4: Main Loop (1 Hz blink)
- PE5 toggles every 500ms (visible 1 Hz blink)
- Every 5 seconds: stepper moves 50 steps
- Every 1 second: endstop state checked

## Troubleshooting

### Scenario 1: No LED activity at all
**Diagnosis**: Code is not running
**Causes**:
- Bootloader not working
- VTOR offset (0x8000) is wrong
- Firmware flashed to wrong location
- Firmware binary corrupt

**Fix**:
- Verify bootloader is functional (device shows up in bootloader mode)
- Check SD card filesystem is clean
- Try erasing flash and reflashing

### Scenario 2: 3 pulses, then long pause, then crash
**Diagnosis**: HSE setup failed, but fallback also had issue
**Causes**:
- 8 MHz crystal missing or damaged
- HSE capacitors misplaced or wrong value
- Internal HSI oscillator not working
- Flash latency misconfigured for HSI speeds

**Fix**:
- Check crystal and capacitors with multimeter (measure crystal voltage between oscillator pins)
- Try lower flash latency in HSI config
- Look at schematic to verify crystal frequency

### Scenario 3: 3 pulses + 1 long pulse (HSE error), then 2 pulses + normal heartbeat
**Diagnosis**: ✅ HSE failed but HSI fallback works
**This is OK for testing!** 
- Firmware will run at 16 MHz instead of 168 MHz
- Stepper will move slower
- Everything should still work functionally

### Scenario 4: Heartbeat visible but stepper not moving
**Diagnosis**: GPIO working, but stepper not connected or driver disabled
**Check**:
- Is stepper motor connected to correct JST header?
- Is stepper power connected?
- Can you measure PE1/PE2/PE3 toggling with multimeter or oscilloscope?
- Is PE3 going low (to enable driver)?

### Scenario 4b: Motion works only with `TMC.ALLOW_UNVERIFIED_MOTION=1`
**Diagnosis**: Motion control is usable, but the TMC UART path is still not verifying on the board.
**Observed bench signature**:
- `driver verify=0 ifcnt_valid=0 ifcnt=0`
- verified registers remain `0x00000000`
- `HOME`, `ENABLE`, and motion commands are blocked with `reason=tmc_unverified` until the override is enabled
**Check**:
- Confirm the X slot actually has a UART-capable TMC2209 module installed.
- Confirm the SKR2 X-slot UART jumper configuration is set for UART mode rather than standalone mode.
- Confirm the X-slot driver address/jumper state matches the firmware expectation of serial address `0`.
- Confirm the PDN/UART path from the X driver to `PE0` is actually populated and not isolated by jumper placement.
- Re-run `DRIVER` after each physical change; do not continue to the load-cell phase until `verify=1` and `ifcnt_valid=1`.
- Use `test\run_bench_readiness.ps1` after each physical change to capture the current `DRIVER` state and, if desired, rerun the calibrated motion check with the same host-side procedure each time.
- If the driver address is unknown, use `SET CHANNEL.ADDRESS <0..3>` followed by `DRIVER` to probe the address live before changing firmware again.

### Scenario 5: Heartbeat visible, stepper moving, endstop not responding
**Diagnosis**: Endstop wiring or initialization issue
**Check**:
- Is the endstop wired to the configured `x_stop` input? On the SKR2 reference setup that defaults to `PC1`.
- Try shorting the configured `x_stop` input to GND with a jumper. On the SKR2 reference setup that means `PC1` - does board detect it?
- Check if pullup is present on board

## Next Steps After Diagnostic Success

Once you see:
- 3 pulses + possibly 1 long pulse + 2 pulses + heartbeat

We can:
1. Verify stepper movement with a meter or listening for clicks
2. Verify endstop detection by shorting the configured `x_stop` input. On the SKR2 reference setup that means `PC1`
3. Add serial USB output to see firmware messages
4. Verify `DRIVER` reports `verify=1` / `ifcnt_valid=1` without `TMC.ALLOW_UNVERIFIED_MOTION`
5. Integrate load cell ADC for phase 2

## Firmware Versions

- `firmware.bin` - Original (non-diagnostic)
- `firmware_diagnostic.bin` - Current with startup indicators

To use diagnostic version: Copy to microSD as `firmware.bin`
