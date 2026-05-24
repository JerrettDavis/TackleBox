param(
    [string]$ConfigPath = (Join-Path $PSScriptRoot 'device.cfg'),
    [string]$PortName,
    [int]$BaudRate = 115200,
    [int]$PollIntervalMs = 750,
    [switch]$OneShot
)

$ErrorActionPreference = 'Stop'

function Get-Stm32Port {
    $device = Get-CimInstance Win32_SerialPort |
        Where-Object { $_.PNPDeviceID -like 'USB\VID_0483&PID_5740*' } |
        Select-Object -First 1

    if (-not $device) {
        return $null
    }

    return $device.DeviceID
}

function Get-ConfigCommands {
    param([string]$Path)

    if (-not (Test-Path $Path -PathType Leaf)) {
        throw "Config file not found: $Path"
    }

    $commands = New-Object System.Collections.Generic.List[string]
    foreach ($rawLine in Get-Content $Path) {
        $line = $rawLine.Trim()
        if ([string]::IsNullOrWhiteSpace($line)) {
            continue
        }
        if ($line.StartsWith('#') -or $line.StartsWith(';')) {
            continue
        }
        if ($line -match '^(SET\s+.+)$') {
            $commands.Add($matches[1].Trim())
            continue
        }
        if ($line -match '^([A-Za-z0-9\._]+)\s*=\s*(.+)$') {
            $commands.Add(("SET {0} {1}" -f $matches[1].Trim().ToUpperInvariant(), $matches[2].Trim().ToUpperInvariant()))
            continue
        }
        throw "Unsupported config line: $line"
    }

    return $commands
}

function Read-LinesFor {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [int]$Milliseconds
    )

    $deadline = [Environment]::TickCount + $Milliseconds
    $lines = New-Object System.Collections.Generic.List[string]

    while ([Environment]::TickCount -lt $deadline) {
        try {
            $line = $Port.ReadLine()
            if ($line) {
                $lines.Add($line)
            }
        }
        catch [System.TimeoutException] {
        }
    }

    return $lines
}

function Send-Commands {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [string[]]$Commands
    )

    $Port.WriteLine('config')
    Start-Sleep -Milliseconds 200

    foreach ($command in $Commands) {
        Write-Host ">> $command"
        $Port.WriteLine($command)
        Start-Sleep -Milliseconds 120
    }

    $Port.WriteLine('boot')
    Start-Sleep -Milliseconds 150
}

function Invoke-BootConfigSession {
    param(
        [string]$ResolvedPort,
        [string[]]$Commands
    )

    $port = New-Object System.IO.Ports.SerialPort $ResolvedPort,$BaudRate,None,8,one
    $port.ReadTimeout = 250
    $port.WriteTimeout = 1000
    $port.NewLine = "`r`n"
    $port.DtrEnable = $true
    $port.RtsEnable = $true
    $port.Open()

    try {
        $startupLines = Read-LinesFor -Port $port -Milliseconds 1800
        $startupLines | ForEach-Object { Write-Host "<< $_" }

        $bootWindowSeen = $startupLines | Where-Object { $_ -match '^boot: wait_usb ms=' }
        if (-not $bootWindowSeen) {
            Write-Host 'Boot wait banner not observed; attempting config push anyway.'
        }

        Send-Commands -Port $port -Commands $Commands

        $resultLines = Read-LinesFor -Port $port -Milliseconds 2000
        $resultLines | ForEach-Object { Write-Host "<< $_" }
        return ($resultLines | Where-Object { $_ -match '^cmd: boot|^boot: source=' }) -ne $null
    }
    finally {
        if ($port.IsOpen) {
            $port.Close()
        }
        $port.Dispose()
    }
}

$commands = Get-ConfigCommands -Path $ConfigPath
Write-Host "Loaded $($commands.Count) config commands from $ConfigPath"

do {
    $resolvedPort = if ($PortName) { $PortName } else { Get-Stm32Port }
    if (-not $resolvedPort) {
        Start-Sleep -Milliseconds $PollIntervalMs
        continue
    }

    Write-Host "Connecting to $resolvedPort"
    try {
        $success = Invoke-BootConfigSession -ResolvedPort $resolvedPort -Commands $commands
        if ($success) {
            Write-Host 'Boot configuration session completed.'
        }
    }
    catch {
        Write-Warning $_
    }

    if ($OneShot) {
        break
    }

    Start-Sleep -Milliseconds $PollIntervalMs
}
while ($true)