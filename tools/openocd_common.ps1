$ErrorActionPreference = 'Stop'

function Resolve-OpenOcdCommand {
    $openOcd = Join-Path $env:USERPROFILE '.platformio\packages\tool-openocd\bin\openocd.exe'
    if (-not (Test-Path $openOcd)) {
        throw 'Unable to locate OpenOCD in the PlatformIO packages directory.'
    }

    return $openOcd
}

function Invoke-OpenOcdCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command
    )

    $openOcd = Resolve-OpenOcdCommand
    $interfaceCfg = Join-Path $env:USERPROFILE '.platformio\packages\tool-openocd\openocd\scripts\interface\stlink.cfg'
    $targetCfg = Join-Path $env:USERPROFILE '.platformio\packages\tool-openocd\openocd\scripts\target\stm32f4x.cfg'

    & $openOcd -f $interfaceCfg -f $targetCfg -c "transport select hla_swd; $Command"
    if ($LASTEXITCODE -ne 0) {
        throw "OpenOCD command failed: $Command"
    }
}

function Get-OpenOcdCommandOutput {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command
    )

    $openOcd = Resolve-OpenOcdCommand
    $interfaceCfg = Join-Path $env:USERPROFILE '.platformio\packages\tool-openocd\openocd\scripts\interface\stlink.cfg'
    $targetCfg = Join-Path $env:USERPROFILE '.platformio\packages\tool-openocd\openocd\scripts\target\stm32f4x.cfg'

    $output = & $openOcd -f $interfaceCfg -f $targetCfg -c "transport select hla_swd; $Command" 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "OpenOCD command failed: $Command`n$output"
    }

    return $output
}

function Flash-FirmwareWithStLink {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FirmwarePath,
        [Parameter(Mandatory = $true)]
        [string]$FlashAddress
    )

    $resolvedPath = (Resolve-Path $FirmwarePath).Path
    Invoke-OpenOcdCommand -Command "init; reset halt; flash write_image erase {$resolvedPath} $FlashAddress bin; verify_image {$resolvedPath} $FlashAddress bin; reset run; shutdown"
}

function Reset-BoardWithStLink {
    Invoke-OpenOcdCommand -Command 'init; reset run; shutdown'
}