param(
    [string[]]$Environments = @('skr2_f429_usb', 'skr2_f429', 'skr2_f429_usb_64k', 'skr2_f429_usb_0'),
    [switch]$SkipHostTests,
    [switch]$SkipFirmwareBuilds
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

if (-not $SkipHostTests) {
    & (Join-Path $PSScriptRoot 'run_host_tests.ps1')
    if ($LASTEXITCODE -ne 0) {
        throw 'Host tests failed.'
    }
}

if (-not $SkipFirmwareBuilds) {
    $platformIo = Resolve-PlatformIoCommand
    foreach ($environment in $Environments) {
        Write-Host "Building firmware environment: $environment"
        & $platformIo.Executable @($platformIo.Arguments) 'run' '-e' $environment
        if ($LASTEXITCODE -ne 0) {
            throw "Firmware build failed for $environment"
        }
    }
}

Write-Host 'PASS CI checks'