param(
    [string]$AppPortName,
    [string]$BootloaderPortName,
    [switch]$SkipTransitionToBootloader,
    [switch]$SkipTransitionBackToApp,
    [int]$BootloaderWaitMs = 8000,
    [int]$AppWaitMs = 8000
)

$ErrorActionPreference = 'Stop'

. (Join-Path (Split-Path -Parent $PSScriptRoot) 'tools\usb_serial_common.ps1')

function Assert-ContainsLine {
    param(
        [string[]]$Lines,
        [string]$Pattern,
        [string]$FailureMessage
    )

    if (-not ($Lines | Where-Object { $_ -match $Pattern })) {
        throw $FailureMessage + "`nObserved lines:`n" + ($Lines -join "`n")
    }
}

function Open-UsbSerialPortWithRetry {
    param(
        [string]$ResolvedPort,
        [int]$WaitMs = 4000,
        [int]$RetryMs = 250
    )

    $deadline = [Environment]::TickCount + $WaitMs
    do {
        try {
            return Open-UsbSerialPort -ResolvedPort $ResolvedPort
        }
        catch {
            if ([Environment]::TickCount -ge $deadline) {
                throw
            }

            Start-Sleep -Milliseconds $RetryMs
        }
    }
    while ($true)
}

function Invoke-SerialCommandWithRetry {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [string]$Command,
        [string[]]$ExpectedPatterns,
        [int]$WaitMs = 4000,
        [int]$RetryMs = 250,
        [switch]$AllowDisconnect
    )

    $deadline = [Environment]::TickCount + $WaitMs
    do {
        try {
            return Invoke-SerialCommand -Port $Port -Command $Command -ExpectedPatterns $ExpectedPatterns -AllowDisconnect:$AllowDisconnect
        }
        catch {
            if ([Environment]::TickCount -ge $deadline) {
                throw
            }

            Start-Sleep -Milliseconds $RetryMs
        }
    }
    while ($true)
}

$bootPort = $null
$appPort = $null
$bootloaderSerial = $null
$appSerial = $null

try {
    if (-not $SkipTransitionToBootloader) {
        $initialState = Get-BoardUsbState
        if ((-not $AppPortName) -and (-not $initialState.ApplicationPort) -and $initialState.BootloaderPort) {
            Write-Host "Board already in bootloader mode on $($initialState.BootloaderPort)"
            $bootPort = $initialState.BootloaderPort
        }
        else {
            $appPort = Resolve-AppPort -ExplicitPort $AppPortName
            Write-Host "Using application port $appPort"
            $appSerial = Open-UsbSerialPort -ResolvedPort $appPort
            $null = Read-SerialLinesUntilIdle -Port $appSerial -MaxMs 2500
            $null = Invoke-SerialCommand -Port $appSerial -Command 'bootloader' -ExpectedPatterns @() -AllowDisconnect
            $appSerial.Close()
            $appSerial = $null
        }
    }

    if (-not $bootPort) {
        $bootPort = Resolve-BootloaderPort -ExplicitPort $BootloaderPortName -WaitMs $BootloaderWaitMs
    }
    Write-Host "Using bootloader port $bootPort"
    $bootloaderSerial = Open-UsbSerialPort -ResolvedPort $bootPort

    $null = Read-SerialLinesUntilIdle -Port $bootloaderSerial -MaxMs 2500

    $null = Invoke-SerialCommand -Port $bootloaderSerial -Command 'help' -ExpectedPatterns @(
        '^BOOTLOADER HELP: .*STATUS/DIAG.*FLASH/MAP.*READ <offset> <size>.*ERASE <size>.*WRITE <offset> <hex>.*CRC <size>.*BOOT RESET ESTOP ESTOPCLEAR$'
    )

    $null = Invoke-SerialCommand -Port $bootloaderSerial -Command 'info' -ExpectedPatterns @(
        '^INFO app_base=0x[0-9A-F]{8} present=[01]$'
    )

    $null = Invoke-SerialCommand -Port $bootloaderSerial -Command 'status' -ExpectedPatterns @(
        '^STATUS mode=BOOTLOADER uptime_ms=\d+ usb_configured=[01] app_present=[01] estop=[01]$'
    )

    $null = Invoke-SerialCommand -Port $bootloaderSerial -Command 'estop' -ExpectedPatterns @(
        '^ESTOP value=1$'
    )

    $null = Invoke-SerialCommand -Port $bootloaderSerial -Command 'status' -ExpectedPatterns @(
        '^STATUS mode=BOOTLOADER uptime_ms=\d+ usb_configured=[01] app_present=[01] estop=1$'
    )

    $null = Invoke-SerialCommand -Port $bootloaderSerial -Command 'flash' -ExpectedPatterns @(
        '^FLASH boot_base=0x08000000 boot_size=32768 app_base=0x08008000 flash_end=0x08100000$'
    )

    $null = Invoke-SerialCommand -Port $bootloaderSerial -Command 'read 0 16' -ExpectedPatterns @(
        '^DATA offset=0 size=16 hex=[0-9A-F]{32}$'
    )

    if (-not $SkipTransitionBackToApp) {
        $null = Invoke-SerialCommand -Port $bootloaderSerial -Command 'boot' -ExpectedPatterns @('^BOOTING$') -AllowDisconnect
        $bootloaderSerial.Close()
        $bootloaderSerial = $null

        if ($AppPortName) {
            $appPort = $AppPortName
        }
        else {
            $appPort = Resolve-AppPort -WaitMs $AppWaitMs
        }
        if (-not $appPort) {
            throw 'Application CDC port did not return after BOOT.'
        }

        Write-Host "Returned to application port $appPort"
        $appSerial = Open-UsbSerialPortWithRetry -ResolvedPort $appPort -WaitMs $AppWaitMs
        $null = Read-SerialLinesUntilIdle -Port $appSerial -MaxMs 2500
        $null = Invoke-SerialCommandWithRetry -Port $appSerial -Command 'status' -ExpectedPatterns @(
            '^cmd: status$',
            'estop=1'
        ) -WaitMs $AppWaitMs
        $null = Invoke-SerialCommandWithRetry -Port $appSerial -Command 'estopclear' -ExpectedPatterns @(
            '^cmd: estop value=0$'
        ) -WaitMs $AppWaitMs
        $null = Invoke-SerialCommandWithRetry -Port $appSerial -Command 'help' -ExpectedPatterns @(
            '^cmds: .*BOOTLOADER/RECOVERY.*BOOT/START.*HELP/\?$'
        ) -WaitMs $AppWaitMs
        $null = Invoke-SerialCommand -Port $appSerial -Command 'reboot' -ExpectedPatterns @() -AllowDisconnect
        $appSerial.Close()
        $appSerial = $null

        $appPort = Get-UsbSerialPortByVidPid -VidPidPattern 'USB\VID_0483&PID_5740*' -WaitMs $AppWaitMs
        if (-not $appPort) {
            throw 'Application CDC port did not return after REBOOT.'
        }

        Write-Host "Application reboot returned on $appPort"
    }

    Write-Host 'PASS bootloader serial tests'
}
finally {
    if ($bootloaderSerial -and $bootloaderSerial.IsOpen) {
        $bootloaderSerial.Close()
    }
    if ($appSerial -and $appSerial.IsOpen) {
        $appSerial.Close()
    }
}