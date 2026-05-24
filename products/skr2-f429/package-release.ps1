param(
    [string]$OutputDirectory = '.\artifacts\releases\skr2-f429',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $repoRoot

function Resolve-PlatformIoCommand {
    if ($env:KEYSWITCH_PLATFORMIO -and (Test-Path $env:KEYSWITCH_PLATFORMIO)) {
        return @{ Executable = $env:KEYSWITCH_PLATFORMIO; Arguments = @() }
    }

    $localPlatformIo = 'C:\Users\jd\.platformio\penv\Scripts\platformio.exe'
    if (Test-Path $localPlatformIo) {
        return @{ Executable = $localPlatformIo; Arguments = @() }
    }

    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if ($null -ne $pythonCommand) {
        return @{ Executable = $pythonCommand.Source; Arguments = @('-m', 'platformio') }
    }

    throw 'Unable to locate PlatformIO. Set KEYSWITCH_PLATFORMIO or install platformio.'
}

if (-not $SkipBuild) {
    $platformIo = Resolve-PlatformIoCommand
    & $platformIo.Executable @($platformIo.Arguments) 'run' '-e' 'skr2_f429_usb'
    if ($LASTEXITCODE -ne 0) {
        throw 'Application firmware build failed.'
    }

    & $platformIo.Executable @($platformIo.Arguments) 'run' '-e' 'skr2_f429_bootloader_cdc'
    if ($LASTEXITCODE -ne 0) {
        throw 'BootAnchor firmware build failed.'
    }
}

$resolvedOutputDirectory = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
[System.IO.Directory]::CreateDirectory($resolvedOutputDirectory) | Out-Null

$manifestSource = Join-Path $PSScriptRoot 'manifest.json'
$manifestTarget = Join-Path $resolvedOutputDirectory 'manifest.json'
$appSource = Join-Path $repoRoot '.pio\build\skr2_f429_usb\firmware.bin'
$bootSource = Join-Path $repoRoot '.pio\build\skr2_f429_bootloader_cdc\firmware.bin'
$appTarget = Join-Path $resolvedOutputDirectory 'tacklebox-keelware-skr2-f429-app.bin'
$bootTarget = Join-Path $resolvedOutputDirectory 'tacklebox-bootanchor-skr2-f429-cdc.bin'

if (-not (Test-Path $manifestSource -PathType Leaf)) {
    throw "Release manifest not found: $manifestSource"
}

if (-not (Test-Path $appSource -PathType Leaf)) {
    if ($SkipBuild) {
        throw "Application build output not found at $appSource. Re-run without -SkipBuild or build environment skr2_f429_usb first."
    }

    throw "Application build output not found after build: $appSource"
}

if (-not (Test-Path $bootSource -PathType Leaf)) {
    if ($SkipBuild) {
        throw "BootAnchor build output not found at $bootSource. Re-run without -SkipBuild or build environment skr2_f429_bootloader_cdc first."
    }

    throw "BootAnchor build output not found after build: $bootSource"
}

Copy-Item $manifestSource $manifestTarget -Force
Copy-Item $appSource $appTarget -Force
Copy-Item $bootSource $bootTarget -Force

Write-Host "Packaged release artifacts in $resolvedOutputDirectory"
Write-Host " - $appTarget"
Write-Host " - $bootTarget"
Write-Host " - $manifestTarget"