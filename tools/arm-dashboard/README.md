# SKR2 Arm Dashboard

This is a static browser dashboard for the SKR2 arm firmware. It uses the Web Serial API to connect directly to the board over USB CDC and provides:

- a Fluidd/Mainsail-style shell with a persistent sidebar, view switching, and common command actions
- live telemetry cards for state, position, force, driver config, and safety inputs
- force/position and live HX711 scope charts on the overview page
- a controls view for setup, persistence, motion, driver, and simulation tuning
- a simulated response-curve view loaded from the local host-side probe workflow exporter
- system-aware theme selection with manual light/dark override
- forms for target motion, cycle routines, driver current tuning, and simulation controls
- a macro editor plus raw command console for scripted and manual command sequences

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

Generate and load the local simulated probe curve with:

```powershell
Set-Location "c:\git\BTT SKR2 Testing"
& ".\test\run_simulated_probe_workflow.ps1"
```

That writes `tools\arm-dashboard\simulated-probe-curve.json`. In the dashboard, click **Load exported curve** to preview the simulated switch/load-cell force-versus-position response without hardware attached.

## Notes

- The dashboard expects the current line-oriented firmware protocol documented in the repo root README.
- The simulated response curve comes from the host-side `test_probe_workflow` executable, which exercises the same parser plus domain motion/load-cell flow used by the local tests.
- `driver`/`irun`/`ihold` use the raw TMC2209 register scales currently exposed by the firmware.
- The macro runner understands one command per line and `DELAY <ms>` entries.