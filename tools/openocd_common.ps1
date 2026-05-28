$ErrorActionPreference = 'Stop'

$script:OpenOcdRetrySpeedsKhz = @(2000, 1000, 400)
$script:OpenOcdRetryDelayMs = 750

function Test-OpenOcdRetryableFailure {
    param([string]$Output)

    if ([string]::IsNullOrWhiteSpace($Output)) {
        return $false
    }

    return ($Output -match 'Error:\s+open failed') -or
           ($Output -match 'unable to open') -or
           ($Output -match 'LIBUSB_ERROR') -or
           ($Output -match 'No device found')
}

function Resolve-OpenOcdCommand {
    $openOcd = Join-Path $env:USERPROFILE '.platformio\packages\tool-openocd\bin\openocd.exe'
    if (-not (Test-Path $openOcd)) {
        throw 'Unable to locate OpenOCD in the PlatformIO packages directory.'
    }

    return $openOcd
}

function Invoke-OpenOcdRaw {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [Parameter(Mandatory = $true)]
        [int]$AdapterSpeedKhz
    )

    $openOcd = Resolve-OpenOcdCommand
    $interfaceCfg = Join-Path $env:USERPROFILE '.platformio\packages\tool-openocd\openocd\scripts\interface\stlink.cfg'
    $targetCfg = Join-Path $env:USERPROFILE '.platformio\packages\tool-openocd\openocd\scripts\target\stm32f4x.cfg'

    $output = & $openOcd -f $interfaceCfg -f $targetCfg -c "transport select hla_swd; adapter speed $AdapterSpeedKhz; $Command" 2>&1 | Out-String
    return @{
        Output = $output
        ExitCode = $LASTEXITCODE
        AdapterSpeedKhz = $AdapterSpeedKhz
    }
}

function Invoke-OpenOcdWithRetry {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [switch]$ReturnOutput
    )

    $attempt = 0
    $totalAttempts = $script:OpenOcdRetrySpeedsKhz.Count
    $lastResult = $null
    foreach ($adapterSpeedKhz in $script:OpenOcdRetrySpeedsKhz) {
        $attempt += 1
        $result = Invoke-OpenOcdRaw -Command $Command -AdapterSpeedKhz $adapterSpeedKhz
        $lastResult = $result
        if ($result.ExitCode -eq 0) {
            if (-not [string]::IsNullOrWhiteSpace($result.Output)) {
                Write-Host $result.Output.TrimEnd()
            }

            if ($ReturnOutput) {
                return $result.Output
            }

            return
        }

        if (($attempt -lt $totalAttempts) -and (Test-OpenOcdRetryableFailure -Output $result.Output)) {
            Write-Warning ("OpenOCD transport attempt {0}/{1} failed at {2} kHz; retrying after {3} ms." -f $attempt, $totalAttempts, $adapterSpeedKhz, $script:OpenOcdRetryDelayMs)
            Start-Sleep -Milliseconds $script:OpenOcdRetryDelayMs
            continue
        }

        break
    }

    $message = "OpenOCD command failed: $Command"
    if ($lastResult -ne $null) {
        $message += "`nAdapter speed: $($lastResult.AdapterSpeedKhz) kHz"
        if (-not [string]::IsNullOrWhiteSpace($lastResult.Output)) {
            $message += "`n$($lastResult.Output.TrimEnd())"
        }
    }

    throw $message
}

function Invoke-OpenOcdCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command
    )

    Invoke-OpenOcdWithRetry -Command $Command
}

function Get-OpenOcdCommandOutput {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command
    )

    return Invoke-OpenOcdWithRetry -Command $Command -ReturnOutput
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