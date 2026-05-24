$ErrorActionPreference = 'Stop'

$script:KeyswitchUsbIds = @{
    Application = 'USB\VID_0483&PID_5740*'
    Bootloader  = 'USB\VID_0483&PID_5741*'
}

function Resolve-PlatformIoCommand {
    if ($env:KEYSWITCH_PLATFORMIO -and (Test-Path $env:KEYSWITCH_PLATFORMIO)) {
        return @{ Executable = $env:KEYSWITCH_PLATFORMIO; Arguments = @() }
    }

    $localPlatformIo = 'C:\Users\jd\.platformio\penv\Scripts\platformio.exe'
    if (Test-Path $localPlatformIo) {
        return @{ Executable = $localPlatformIo; Arguments = @() }
    }

    throw 'Unable to locate PlatformIO. Set KEYSWITCH_PLATFORMIO or install platformio.'
}

function Get-UsbSerialPortByVidPid {
    param(
        [Parameter(Mandatory = $true)]
        [string]$VidPidPattern,
        [int]$WaitMs = 0
    )

    $deadline = [Environment]::TickCount + $WaitMs
    do {
        $device = Get-CimInstance Win32_SerialPort |
            Where-Object { $_.PNPDeviceID -like $VidPidPattern } |
            Select-Object -First 1

        if ($device) {
            return $device.DeviceID
        }

        if ($WaitMs -le 0) {
            break
        }

        Start-Sleep -Milliseconds 250
    }
    while ([Environment]::TickCount -lt $deadline)

    return $null
}

function Resolve-AppPort {
    param(
        [string]$ExplicitPort,
        [int]$WaitMs = 0
    )

    if ($ExplicitPort) {
        return $ExplicitPort
    }

    $port = Get-UsbSerialPortByVidPid -VidPidPattern $script:KeyswitchUsbIds.Application -WaitMs $WaitMs
    if (-not $port) {
        throw 'Application CDC port not found.'
    }

    return $port
}

function Resolve-BootloaderPort {
    param(
        [string]$ExplicitPort,
        [int]$WaitMs = 0
    )

    if ($ExplicitPort) {
        return $ExplicitPort
    }

    $port = Get-UsbSerialPortByVidPid -VidPidPattern $script:KeyswitchUsbIds.Bootloader -WaitMs $WaitMs
    if (-not $port) {
        throw 'Bootloader CDC port not found.'
    }

    return $port
}

function Get-BoardUsbState {
    [pscustomobject]@{
        ApplicationPort = Get-UsbSerialPortByVidPid -VidPidPattern $script:KeyswitchUsbIds.Application
        BootloaderPort  = Get-UsbSerialPortByVidPid -VidPidPattern $script:KeyswitchUsbIds.Bootloader
    }
}

function Open-UsbSerialPort {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ResolvedPort,
        [int]$BaudRate = 115200,
        [int]$ReadTimeoutMs = 500,
        [int]$WriteTimeoutMs = 1000
    )

    $port = New-Object System.IO.Ports.SerialPort $ResolvedPort,$BaudRate,None,8,one
    $port.ReadTimeout = $ReadTimeoutMs
    $port.WriteTimeout = $WriteTimeoutMs
    $port.NewLine = "`r`n"
    $port.DtrEnable = $true
    $port.RtsEnable = $true
    $port.Open()
    return $port
}

function Read-SerialLinesUntilIdle {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$Port,
        [int]$IdleMs = 300,
        [int]$MaxMs = 3000,
        [switch]$AllowDisconnect
    )

    $lines = New-Object System.Collections.Generic.List[string]
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $lastRead = [System.Diagnostics.Stopwatch]::StartNew()

    while ($stopwatch.ElapsedMilliseconds -lt $MaxMs) {
        try {
            $line = $Port.ReadLine()
            if ($line) {
                [void]$lines.Add($line.TrimEnd())
                $lastRead.Restart()
            }
        }
        catch [System.TimeoutException] {
            if (($lines.Count -gt 0) -and ($lastRead.ElapsedMilliseconds -ge $IdleMs)) {
                break
            }
        }
        catch {
            if ($AllowDisconnect -and ((-not $Port.IsOpen) -or ($_.Exception.Message -like '*operation was canceled*'))) {
                break
            }

            throw
        }
    }

    return ,$lines.ToArray()
}

function Invoke-SerialCommand {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$Port,
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [string[]]$ExpectedPatterns = @(),
        [switch]$AllowDisconnect,
        [int]$IdleMs = 300,
        [int]$MaxMs = 3000
    )

    $Port.WriteLine($Command)
    $lines = Read-SerialLinesUntilIdle -Port $Port -IdleMs $IdleMs -MaxMs $MaxMs -AllowDisconnect:$AllowDisconnect

    foreach ($pattern in $ExpectedPatterns) {
        if (-not ($lines | Where-Object { $_ -match $pattern })) {
            throw "Command '$Command' did not match '$pattern'.`nObserved lines:`n$($lines -join "`n")"
        }
    }

    return $lines
}

function Enter-BootloaderMode {
    param(
        [string]$AppPortName,
        [int]$BootloaderWaitMs = 8000
    )

    $existingBootPort = Get-UsbSerialPortByVidPid -VidPidPattern $script:KeyswitchUsbIds.Bootloader
    if ($existingBootPort) {
        return $existingBootPort
    }

    $appPort = Resolve-AppPort -ExplicitPort $AppPortName
    $serial = $null
    try {
        $serial = Open-UsbSerialPort -ResolvedPort $appPort
        $null = Read-SerialLinesUntilIdle -Port $serial -MaxMs 2500
        $null = Invoke-SerialCommand -Port $serial -Command 'BOOTLOADER' -ExpectedPatterns @() -AllowDisconnect
    }
    finally {
        if ($serial -and $serial.IsOpen) {
            $serial.Close()
        }
    }

    return Resolve-BootloaderPort -WaitMs $BootloaderWaitMs
}

function Enter-ApplicationMode {
    param(
        [string]$BootloaderPortName,
        [int]$AppWaitMs = 8000
    )

    $existingAppPort = Get-UsbSerialPortByVidPid -VidPidPattern $script:KeyswitchUsbIds.Application
    if ($existingAppPort) {
        return $existingAppPort
    }

    $bootPort = Resolve-BootloaderPort -ExplicitPort $BootloaderPortName
    $serial = $null
    try {
        $serial = Open-UsbSerialPort -ResolvedPort $bootPort
        $null = Read-SerialLinesUntilIdle -Port $serial -MaxMs 2500
        $null = Invoke-SerialCommand -Port $serial -Command 'BOOT' -ExpectedPatterns @('^BOOTING$') -AllowDisconnect
    }
    finally {
        if ($serial -and $serial.IsOpen) {
            $serial.Close()
        }
    }

    return Resolve-AppPort -WaitMs $AppWaitMs
}