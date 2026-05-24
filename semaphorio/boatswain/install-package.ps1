param(
    [Parameter(Mandatory = $true)]
    [string]$PackagePath,
    [string]$AppPortName,
    [switch]$Persist,
    [switch]$Reboot,
    [switch]$DryRun,
    [int]$WaitMs = 8000
)

$ErrorActionPreference = 'Stop'

. (Join-Path (Split-Path -Parent $PSScriptRoot) 'serial\usb_serial_common.ps1')

function Convert-CommandFileLineToRuntimeCommand {
    param([string]$Line)

    if ($null -eq $Line) {
        return $null
    }

    $trimmed = $Line.Trim()
    if ([string]::IsNullOrWhiteSpace($trimmed)) {
        return $null
    }
    if ($trimmed.StartsWith('#') -or $trimmed.StartsWith(';')) {
        return $null
    }

    if ($trimmed -match '^(SET\s+.+)$') {
        $runtimeCommand = $matches[1].Trim().ToUpperInvariant()
    }
    elseif ($trimmed -match '^([A-Za-z0-9\._]+)\s*=\s*(.+)$') {
        $runtimeCommand = ('SET {0} {1}' -f $matches[1].Trim().ToUpperInvariant(), $matches[2].Trim().ToUpperInvariant())
    }
    else {
        throw "Unsupported Boatswain package command line: $trimmed"
    }

    if ($runtimeCommand -notmatch '^SET\s+[A-Z0-9\._]+\s+.+$') {
        throw "Boatswain package V1 only supports SET operations. Rejected line: $trimmed"
    }

    return $runtimeCommand
}

function Read-BoatswainPackage {
    param([string]$ResolvedPackagePath)

    $manifestPath = Join-Path $ResolvedPackagePath 'manifest.json'
    if (-not (Test-Path $manifestPath -PathType Leaf)) {
        throw "Package manifest not found: $manifestPath"
    }

    $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.packageFormat -ne 'boatswain-package-v1') {
        throw "Unsupported package format '$($manifest.packageFormat)'."
    }
    if ($manifest.packageType -ne 'config-profile') {
        throw "Unsupported package type '$($manifest.packageType)'."
    }

    $commandsPath = Join-Path $ResolvedPackagePath $manifest.commandsFile
    if (-not (Test-Path $commandsPath -PathType Leaf)) {
        throw "Package commands file not found: $commandsPath"
    }

    $commands = New-Object System.Collections.Generic.List[string]
    foreach ($line in Get-Content $commandsPath) {
        $runtimeCommand = Convert-CommandFileLineToRuntimeCommand -Line $line
        if ($null -ne $runtimeCommand) {
            [void]$commands.Add($runtimeCommand)
        }
    }

    if ($commands.Count -eq 0) {
        throw 'Package contained no valid runtime SET commands.'
    }

    return @{
        Manifest = $manifest
        ManifestPath = $manifestPath
        CommandsPath = $commandsPath
        Commands = $commands.ToArray()
    }
}

$resolvedPackagePath = (Resolve-Path $PackagePath).Path
$package = Read-BoatswainPackage -ResolvedPackagePath $resolvedPackagePath

if (($package.Manifest.targetProduct -ne 'generic') -and ($package.Manifest.targetProduct -ne 'skr2-f429')) {
    throw "Package targetProduct '$($package.Manifest.targetProduct)' is not compatible with the current reference product wrapper."
}

if (($package.Manifest.targetKeelwareContract -ne 'generic') -and ($package.Manifest.targetKeelwareContract -ne 'keelware.stm32.skr2-f429.v1')) {
    throw "Package targetKeelwareContract '$($package.Manifest.targetKeelwareContract)' is not compatible with the current KeelWare contract."
}

Write-Host "Installing Boatswain package: $($package.Manifest.packageId)@$($package.Manifest.packageVersion)"
Write-Host "Package path: $resolvedPackagePath"
Write-Host "Commands file: $($package.CommandsPath)"
Write-Host "Command count: $($package.Commands.Length)"

if ($DryRun) {
    $package.Commands | ForEach-Object { Write-Host "DRYRUN $_" }
    if ($Persist -or $package.Manifest.persistRecommended) {
        Write-Host 'DRYRUN SAVE'
    }
    if ($Reboot -or $package.Manifest.rebootRecommended) {
        Write-Host 'DRYRUN REBOOT'
    }
    exit 0
}

$shouldPersist = $Persist.IsPresent -or [bool]$package.Manifest.persistRecommended
$shouldReboot = $Reboot.IsPresent -or [bool]$package.Manifest.rebootRecommended

$appPort = Resolve-AppPort -ExplicitPort $AppPortName -WaitMs $WaitMs
$serial = $null

try {
    $serial = Open-UsbSerialPort -ResolvedPort $appPort
    $null = Read-SerialLinesUntilIdle -Port $serial -MaxMs 2500

    foreach ($command in $package.Commands) {
        Write-Host ">> $command"
        $lines = Invoke-SerialCommand -Port $serial -Command $command -ExpectedPatterns @('^cmd: set key=.* ok=1 reboot=[01]$')
        if ($lines.Count -gt 0) {
            Write-Host ($lines -join [Environment]::NewLine)
        }
    }

    $configLines = Invoke-SerialCommand -Port $serial -Command 'CONFIG' -ExpectedPatterns @('^cmd: config$', '^config loaded=\d+ dirty=\d+ reboot=\d+$')
    if ($configLines.Count -gt 0) {
        Write-Host ($configLines -join [Environment]::NewLine)
    }

    $driverLines = Invoke-SerialCommand -Port $serial -Command 'DRIVER' -ExpectedPatterns @('^cmd: driver$', '^driver uart=\d+ irun=\d+ ihold=\d+ iholddelay=\d+ tpowerdown=\d+ sgthrs=\d+$')
    if ($driverLines.Count -gt 0) {
        Write-Host ($driverLines -join [Environment]::NewLine)
    }

    if ($shouldPersist) {
        $saveLines = Invoke-SerialCommand -Port $serial -Command 'SAVE' -ExpectedPatterns @('^cmd: save ok=1 reboot=0$')
        if ($saveLines.Count -gt 0) {
            Write-Host ($saveLines -join [Environment]::NewLine)
        }
    }

    if ($shouldReboot) {
        $rebootLines = Invoke-SerialCommand -Port $serial -Command 'REBOOT' -ExpectedPatterns @() -AllowDisconnect
        if ($rebootLines.Count -gt 0) {
            Write-Host ($rebootLines -join [Environment]::NewLine)
        }
    }
}
finally {
    if ($serial -and $serial.IsOpen) {
        $serial.Close()
    }
}