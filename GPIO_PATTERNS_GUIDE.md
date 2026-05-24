# GPIO Pattern Debugging Guide

## Quick Reference: What PE5 Patterns Mean

When you flash the firmware and power on, watch **PE5 (pin E5)** for these patterns. You can measure with a multimeter on DC voltage mode (0V = low/off, 3.3V = high/on).

### Startup Patterns (watch first 10 seconds):

| Pattern | Meaning | Action |
|---------|---------|--------|
| **3 short pulses** (100ms on/off each) | ✅ Code is running - GPIO initialized | System is alive, board works! |
| **Nothing after 3 pulses** | ❌ Code crashed during clock setup | Debug clock configuration |
| **1 long pulse** (500ms on) | ⚠️ HSE crystal failed - using backup HSI | OK - system runs slower but functional |
| **2 short pulses** (100ms on/off each) | ✅ Ready to start main loop | System initialization complete |
| **Continuous ~1 Hz blink** | ✅ Main loop running - heartbeat | Firmware working normally |

### Runtime Patterns (after startup):

| Pattern | Meaning | When |
|---------|---------|------|
| **1 Hz slow blink** (0.5s on, 0.5s off) | ✅ System alive, main loop running | Continuous during operation |
| **N pulses together** (N = test number) | Stepper test #N running (50 steps) | Every 5 seconds |
| **5 rapid pulses** (50ms on/off) | Endstop switch just pressed | When you press the endstop |

## How to Measure

### Option 1: Multimeter (DC Voltage)
1. Set multimeter to **DC Voltage** (V or DCV)
2. Connect **Red probe** to **PE5** (pin E5 on GPIOE)
3. Connect **Black probe** to **GND** (any GND pin)
4. Watch the voltage oscillate between 0V and 3.3V
5. Count the blinks as you watch

### Option 2: Oscilloscope / Logic Analyzer
1. Connect probe to PE5
2. Trigger on rising or falling edge
3. Watch the waveform and count pulses

### Option 3: LED (if you have 220Ω resistor)
1. Connect: PE5 → 220Ω resistor → LED anode
2. Connect: LED cathode → GND
3. Watch LED blink according to patterns above

## Troubleshooting

### Scenario 1: No signal on PE5
**Diagnosis**: Code not running at all
**Likely causes**:
- Bootloader not working
- VTOR offset (0x8000) wrong
- Firmware binary corrupt or not flashing

**Fix**:
- Verify bootloader boots to USB mass storage
- Try erasing flash and reflashing
- Check SD card filesystem

### Scenario 2: 3 pulses, then nothing
**Diagnosis**: Code crashed after GPIO init, probably during clock setup
**Likely causes**:
- HSE crystal startup failing
- HSI fallback also failing
- Flash latency wrong for the CPU speed

**Fix**:
- Check if you see the 1-long-pulse pattern (HSE error) before crash
- Verify crystal with multimeter (should see oscillation on crystal pins if working)
- Try adjusting clock dividers in code

### Scenario 3: 3 pulses + 1 long pulse + 2 pulses + heartbeat = ✅ SUCCESS!
- HSE startup failed (crystal may be missing or not working)
- But HSI fallback worked - system running at 16 MHz
- This is functional for testing even if not at full speed
- Motor will move slower (good for testing without vibration issues)

### Scenario 4: 3 pulses + 2 pulses + heartbeat (no long pulse) = ✅ PERFECT!
- HSE crystal working perfectly
- Running at full 168 MHz
- Motor will move fast

### Scenario 5: Heartbeat visible but stepper not moving
**Check**:
- Is stepper physically connected? (Listen for clicks/buzzing)
- Is stepper getting power to the connector?
- Can you measure PE2 (STEP) toggling with meter?
- Can you measure PE3 (ENABLE) going LOW during movement?

### Scenario 6: Heartbeat visible but endstop not triggering
**Check**:
- Is endstop switch wired to PC1?
- Short PC1 to GND directly - does pattern change?
- Try pressing switch while watching - should get 5 rapid pulses

## Next Steps After Getting Startup Patterns

Once you see the startup pattern sequence (3 pulses → possibly 1 long pulse → 2 pulses → heartbeat):

1. **Verify stepper movement**:
   - Every 5 seconds you should get N pulses then stepper moves
   - Test 1: forward direction, Test 2: backward, etc.
   - Listen for motor sounds or measure PE2 with oscilloscope

2. **Verify endstop**:
   - Press the X endstop switch
   - You should see 5 rapid pulses on PE5
   - Try pressing multiple times

3. **Future phases**:
   - Add serial output (UART or USB CDC) for detailed logging
   - Integrate load cell ADC
   - Build USB command protocol for PC control

## Firmware Versions

- `firmware.bin` - Latest production build
- `firmware_gpio_patterns.bin` - Current (debug via PE5 patterns)
- `firmware_diagnostic.bin` - Earlier version

To test: Copy desired .bin to microSD as `firmware.bin`, then power cycle board.

## PE5 Pin Location

- **Connector**: GPIOE (upper right area of board)
- **Pin**: E5 (5th pin on GPIOE header)
- **Silk screen**: Usually labeled "E5" or near motor connectors
- **Function**: Unused/safe general purpose IO - good for testing without interfering with any hardware
