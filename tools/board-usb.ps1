param(
    [ValidateSet('discover', 'enter-bootloader', 'enter-app', 'app-command', 'bootloader-command')]
    [string]$Action = 'discover',
    [string]$Command,
    [string]$AppPortName,
    [string]$BootloaderPortName,
    [int]$WaitMs = 8000
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'usb_serial_common.ps1')

switch ($Action) {
    'discover' {
        $state = Get-BoardUsbState
        Write-Host ("ApplicationPort={0}" -f ($(if ($state.ApplicationPort) { $state.ApplicationPort } else { 'none' })))
        Write-Host ("BootloaderPort={0}" -f ($(if ($state.BootloaderPort) { $state.BootloaderPort } else { 'none' })))
        break
    }

    'enter-bootloader' {
        $bootPort = Enter-BootloaderMode -AppPortName $AppPortName -BootloaderWaitMs $WaitMs
        Write-Host "BootloaderPort=$bootPort"
        break
    }

    'enter-app' {
        $appPort = Enter-ApplicationMode -BootloaderPortName $BootloaderPortName -AppWaitMs $WaitMs
        Write-Host "ApplicationPort=$appPort"
        break
    }

    'app-command' {
        if (-not $Command) {
            throw 'Action app-command requires -Command.'
        }

        $appPort = Resolve-AppPort -ExplicitPort $AppPortName -WaitMs $WaitMs
        $serial = $null
        try {
            $serial = Open-UsbSerialPort -ResolvedPort $appPort
            $null = Read-SerialLinesUntilIdle -Port $serial -MaxMs 2500
            $lines = Invoke-SerialCommand -Port $serial -Command $Command -ExpectedPatterns @()
            if ($lines.Count -gt 0) {
                Write-Host ($lines -join [Environment]::NewLine)
            }
        }
        finally {
            if ($serial -and $serial.IsOpen) {
                $serial.Close()
            }
        }
        break
    }

    'bootloader-command' {
        if (-not $Command) {
            throw 'Action bootloader-command requires -Command.'
        }

        $bootPort = Resolve-BootloaderPort -ExplicitPort $BootloaderPortName -WaitMs $WaitMs
        $serial = $null
        try {
            $serial = Open-UsbSerialPort -ResolvedPort $bootPort
            $null = Read-SerialLinesUntilIdle -Port $serial -MaxMs 2500
            $lines = Invoke-SerialCommand -Port $serial -Command $Command -ExpectedPatterns @() -AllowDisconnect:($Command -match '^(BOOT|RESET)$')
            if ($lines.Count -gt 0) {
                Write-Host ($lines -join [Environment]::NewLine)
            }
        }
        finally {
            if ($serial -and $serial.IsOpen) {
                $serial.Close()
            }
        }
        break
    }
}