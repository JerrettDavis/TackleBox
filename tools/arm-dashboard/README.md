# SKR2 Arm Dashboard

This is a static browser dashboard for the SKR2 arm firmware. It uses the Web Serial API to connect directly to the board over USB CDC and provides:

- live telemetry cards for state, position, force, driver config, and safety inputs
- a simple force/position chart
- buttons for common commands (`HOME`, `STOP`, `STATUS`, `SAFETY`, `DRIVER`)
- forms for target motion, cycle routines, driver current tuning, and simulation controls
- a macro editor for scripted command sequences

## Run It

Web Serial requires a secure context, so do not open `index.html` directly from `file://`.

Serve the folder on localhost:

```powershell
Set-Location "c:\git\BTT SKR2 Testing\tools\arm-dashboard"
& .\serve.ps1
```

Then open:

```text
http://localhost:8765/
```

Use Chrome or Edge and click **Connect Serial**.

## Notes

- The dashboard expects the current line-oriented firmware protocol documented in the repo root README.
- `driver`/`irun`/`ihold` use the raw TMC2209 register scales currently exposed by the firmware.
- The macro runner understands one command per line and `DELAY <ms>` entries.