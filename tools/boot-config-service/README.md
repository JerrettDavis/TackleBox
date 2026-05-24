# Boot Config Service

This PowerShell script acts as a small host-side companion for the firmware boot window. It waits for the STM32 CDC device to enumerate, opens the USB serial port, pushes config commands from a file, and then sends `BOOT` so the board continues startup using that session config.

## Run

```powershell
Set-Location "c:\git\BTT SKR2 Testing\tools\boot-config-service"
Copy-Item .\device.cfg.example .\device.cfg
& .\boot_config_service.ps1 -OneShot
```

Continuous mode:

```powershell
Set-Location "c:\git\BTT SKR2 Testing\tools\boot-config-service"
& .\boot_config_service.ps1
```

## Config File Format

- blank lines are ignored
- lines starting with `#` or `;` are ignored
- `KEY=VALUE` lines are converted to `SET KEY VALUE`
- explicit `SET KEY VALUE` lines are also accepted

The service does not call `SAVE` automatically. That is deliberate so you can use boot-time configs repeatedly without rewriting internal flash on every startup.