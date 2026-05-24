param(
    [string]$OutputPath,
    [uint32]$Address = 0x08000000,
    [uint32]$LengthBytes = 0x8000,
    [int]$AdapterSpeedKhz = 4000,
    [switch]$SkipToolInstall,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

function Resolve-PlatformIoCommand {
    if ($env:KEYSWITCH_PLATFORMIO -and (Test-Path $env:KEYSWITCH_PLATFORMIO)) {
        return @{
            Executable = $env:KEYSWITCH_PLATFORMIO
            Arguments = @()
        }
    }

    $localPlatformIo = 'C:\Users\jd\.platformio\penv\Scripts\platformio.exe'
    if (Test-Path $localPlatformIo) {
        return @{
            Executable = $localPlatformIo
            Arguments = @()
        }
    }

    $platformIoCommand = Get-Command platformio.exe -ErrorAction SilentlyContinue
    if ($null -eq $platformIoCommand) {
        $platformIoCommand = Get-Command platformio -ErrorAction SilentlyContinue
    }
    if ($null -ne $platformIoCommand) {
        return @{
            Executable = $platformIoCommand.Source
            Arguments = @()
        }
    }

    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if ($null -ne $pythonCommand) {
        return @{
            Executable = $pythonCommand.Source
            Arguments = @('-m', 'platformio')
        }
    }

    throw 'Unable to locate PlatformIO. Set KEYSWITCH_PLATFORMIO or install platformio.'
}

function Install-OpenOcdPackage {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$PlatformIo
    )

    Write-Host 'Installing PlatformIO OpenOCD tooling'
    & $PlatformIo.Executable @($PlatformIo.Arguments) 'pkg' 'install' '-g' '--tool' 'tool-openocd'
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to install PlatformIO tool-openocd package.'
    }
}

function Resolve-OpenOcdExecutable {
    $explicitPath = $env:KEYSWITCH_OPENOCD
    if ($explicitPath -and (Test-Path $explicitPath)) {
        return $explicitPath
    }

    $packageRoots = @(
        'C:\Users\jd\.platformio\packages',
        (Join-Path $env:USERPROFILE '.platformio\packages')
    ) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique

    foreach ($packageRoot in $packageRoots) {
        $match = Get-ChildItem -Path $packageRoot -Filter 'openocd.exe' -Recurse -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($null -ne $match) {
            return $match.FullName
        }
    }

    return $null
}

function Get-StLinkPnpDevice {
    $devices = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
        Where-Object {
            ($_.InstanceId -match '^USB\\VID_0483&PID_3748') -or
            ($_.FriendlyName -match 'ST-?Link|STM32 STLink')
        }

    return $devices | Select-Object -First 1
}

function Assert-StLinkDriverReady {
    $device = Get-StLinkPnpDevice
    if ($null -eq $device) {
        throw 'No ST-Link probe was detected. Check the SWD wiring and USB cable.'
    }

    if ($device.Status -eq 'OK') {
        return
    }

    $problemCode = $null
    try {
        $problemCode = (Get-PnpDeviceProperty -InstanceId $device.InstanceId -KeyName 'DEVPKEY_Device_ProblemCode' -ErrorAction Stop).Data
    }
    catch {
        $problemCode = $null
    }

    if ($problemCode -eq 28) {
        throw @"
The ST-Link probe is connected but Windows has no working driver for it.
Device: $($device.FriendlyName)
InstanceId: $($device.InstanceId)
ProblemCode: 28 (driver not installed)

For the current OpenOCD-based scripts, bind the probe to a libusb-compatible driver such as WinUSB or libusbK.
Typical fix path on Windows:
1. Open Zadig as Administrator.
2. Select the device named STM32 STLink.
3. Install WinUSB or libusbK for that device.
4. Re-run test\backup_bootloader.ps1.
"@
    }

    throw "The ST-Link probe is present but not usable. Status=$($device.Status) ProblemCode=$problemCode InstanceId=$($device.InstanceId)"
}

function Format-Hex32 {
    param(
        [uint32]$Value
    )

    return ('0x{0:X8}' -f $Value)
}

function Convert-ToOpenOcdPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    return $resolvedPath.Replace('\', '/')
}

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (-not $OutputPath) {
    $backupDirectory = Join-Path $repoRoot 'artifacts\bootloader-backups'
    New-Item -ItemType Directory -Force -Path $backupDirectory | Out-Null
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $OutputPath = Join-Path $backupDirectory ("skr2-bootloader-$timestamp.bin")
}
else {
    $outputDirectory = Split-Path -Parent $OutputPath
    if ($outputDirectory) {
        New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
    }
}

$platformIo = Resolve-PlatformIoCommand
$openOcdExecutable = Resolve-OpenOcdExecutable
if (($null -eq $openOcdExecutable) -and (-not $SkipToolInstall) -and (-not $DryRun)) {
    Install-OpenOcdPackage -PlatformIo $platformIo
    $openOcdExecutable = Resolve-OpenOcdExecutable
}

if (($null -eq $openOcdExecutable) -and (-not $DryRun)) {
    throw 'Unable to locate openocd.exe. Set KEYSWITCH_OPENOCD or install PlatformIO tool-openocd.'
}

if (-not $DryRun) {
    Assert-StLinkDriverReady
}

$formattedAddress = Format-Hex32 -Value $Address
$formattedLength = Format-Hex32 -Value $LengthBytes
$openOcdOutputPath = if ($DryRun) { $OutputPath.Replace('\', '/') } else { Convert-ToOpenOcdPath -Path $OutputPath }
$dumpCommand = "init; reset halt; dump_image `"$openOcdOutputPath`" $formattedAddress $formattedLength; shutdown"
$openOcdArguments = @(
    '-f', 'interface/stlink.cfg',
    '-f', 'target/stm32f4x.cfg',
    '-c', "adapter speed $AdapterSpeedKhz",
    '-c', $dumpCommand
)

Write-Host "Bootloader backup region: $formattedAddress length $formattedLength"
Write-Host "Backup output: $OutputPath"

if ($DryRun) {
    $openOcdPathText = if ($openOcdExecutable) { $openOcdExecutable } else { '<openocd.exe not resolved>' }
    Write-Host 'Dry run only. Command preview:'
    Write-Host "$openOcdPathText $($openOcdArguments -join ' ')"
    exit 0
}

& $openOcdExecutable @openOcdArguments
if ($LASTEXITCODE -ne 0) {
    throw 'OpenOCD bootloader dump failed.'
}

if (-not (Test-Path $OutputPath)) {
    throw 'Expected bootloader dump file was not created.'
}

$fileInfo = Get-Item $OutputPath
if ($fileInfo.Length -ne [int64]$LengthBytes) {
    throw "Bootloader dump size mismatch. Expected $LengthBytes bytes, got $($fileInfo.Length)."
}

$hash = Get-FileHash -Path $OutputPath -Algorithm SHA256
$hashPath = "$OutputPath.sha256.txt"
@(
    "File: $OutputPath",
    "Address: $formattedAddress",
    "Length: $formattedLength",
    "SHA256: $($hash.Hash)"
) | Set-Content -Path $hashPath

Write-Host "PASS bootloader backup: $OutputPath"
Write-Host "SHA256 manifest: $hashPath"