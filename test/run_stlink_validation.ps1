param(
    [string]$Environment = 'skr2_f429_usb',
    [string]$PortName,
    [switch]$IncludeMotion,
    [switch]$IncludePersistence,
    [switch]$SkipBuild,
    [switch]$SkipUpload,
    [switch]$SkipLiveTests,
    [int]$UsbReenumerationDelayMs = 4000
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

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$platformIo = Resolve-PlatformIoCommand

if (-not $SkipBuild) {
    Write-Host "Building firmware environment: $Environment"
    & $platformIo.Executable @($platformIo.Arguments) 'run' '-e' $Environment
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for environment $Environment"
    }
}

if (-not $SkipUpload) {
    Write-Host "Uploading via ST-Link: $Environment"
    & $platformIo.Executable @($platformIo.Arguments) 'run' '-e' $Environment '-t' 'upload'
    if ($LASTEXITCODE -ne 0) {
        throw "ST-Link upload failed for environment $Environment"
    }
}

if (-not $SkipLiveTests) {
    Write-Host "Waiting $UsbReenumerationDelayMs ms for USB CDC re-enumeration"
    Start-Sleep -Milliseconds $UsbReenumerationDelayMs

    $liveTestArgs = @()
    if ($PortName) {
        $liveTestArgs += @('-PortName', $PortName)
    }
    if ($IncludeMotion) {
        $liveTestArgs += '-IncludeMotion'
    }
    if ($IncludePersistence) {
        $liveTestArgs += '-IncludePersistence'
    }

    & (Join-Path $PSScriptRoot 'run_live_serial_tests.ps1') @liveTestArgs
    if ($LASTEXITCODE -ne 0) {
        throw 'Live serial tests failed after ST-Link upload.'
    }
}

Write-Host 'PASS stlink validation'