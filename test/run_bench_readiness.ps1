param(
    [string]$PortName,
    [switch]$IncludeMotion,
    [switch]$AllowUnverifiedMotion,
    [switch]$RequireTmcVerified,
    [int]$TravelTargetSteps = -20000,
    [int]$TravelUmPerRotation = 15900,
    [int]$TravelMinUm = -100000,
    [int]$TravelMaxUm = 100000,
    [int]$HomeFeedrateMmPerMin = 600,
    [int]$MoveFeedrateMmPerMin = 1500,
    [int]$HoldAtTargetMs = 0
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $root 'tools\usb_serial_common.ps1')

function Read-StartupLines {
    param([System.IO.Ports.SerialPort]$Port)

    $lines = Read-SerialLinesUntilIdle -Port $Port -MaxMs 2500
    if (-not ($lines | Where-Object { $_ -match 'heartbeat count=' })) {
        $lines += Read-SerialLinesUntilIdle -Port $Port -MaxMs 2500
    }

    return ,$lines
}

function Invoke-BenchCommand {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [string]$Command,
        [int]$MaxMs = 4000,
        [int]$IdleMs = 300
    )

    Write-Host (">>> {0}" -f $Command)
    $lines = Invoke-SerialCommand -Port $Port -Command $Command -ExpectedPatterns @() -MaxMs $MaxMs -IdleMs $IdleMs
    if ($lines.Count -gt 0) {
        $lines | ForEach-Object { Write-Host $_ }
    }

    return ,$lines
}

function Assert-ContainsPattern {
    param(
        [string[]]$Lines,
        [string]$Pattern,
        [string]$FailureMessage
    )

    if (-not ($Lines | Where-Object { $_ -match $Pattern })) {
        throw $FailureMessage + "`nObserved lines:`n" + ($Lines -join "`n")
    }
}

function Get-DriverSnapshot {
    param([string[]]$Lines)

    $verifyLine = $Lines | Where-Object { $_ -match '^driver verify=\d+ ifcnt_valid=\d+ ifcnt=\d+ gconf=0x[0-9A-Fa-f]{8} ihold_irun=0x[0-9A-Fa-f]{8} tpowerdown_reg=0x[0-9A-Fa-f]{8} sgthrs_reg=0x[0-9A-Fa-f]{8}$' } | Select-Object -Last 1
    if (-not $verifyLine) {
        throw "Driver verification line missing.`nObserved lines:`n" + ($Lines -join "`n")
    }

    $verifyMatch = [regex]::Match($verifyLine, '^driver verify=(\d+) ifcnt_valid=(\d+) ifcnt=(\d+) gconf=0x([0-9A-Fa-f]{8}) ihold_irun=0x([0-9A-Fa-f]{8}) tpowerdown_reg=0x([0-9A-Fa-f]{8}) sgthrs_reg=0x([0-9A-Fa-f]{8})$')
    return @{
        Verify = [int]$verifyMatch.Groups[1].Value
        IfcntValid = [int]$verifyMatch.Groups[2].Value
        Ifcnt = [int]$verifyMatch.Groups[3].Value
        Gconf = $verifyMatch.Groups[4].Value
        IholdIrun = $verifyMatch.Groups[5].Value
        Tpowerdown = $verifyMatch.Groups[6].Value
        Sgthrs = $verifyMatch.Groups[7].Value
    }
}

function Get-AllowUnverifiedMotion {
    param([string[]]$Lines)

    $configLine = $Lines | Where-Object { $_ -match 'tmc\.allow_unverified_motion=\d+' } | Select-Object -Last 1
    if (-not $configLine) {
        throw "Config allow-unverified-motion field missing.`nObserved lines:`n" + ($Lines -join "`n")
    }

    $match = [regex]::Match($configLine, 'tmc\.allow_unverified_motion=(\d+)')
    return [int]$match.Groups[1].Value
}

function Assert-StatusPosition {
    param(
        [string[]]$Lines,
        [int]$ExpectedPosition,
        [string]$FailureMessage
    )

    $pattern = '^diag0=\d+ xstop=\d+ diag2=\d+ pressed=0 conf=\d+ load=\d+ mech=\d+ stall=\d+ source=\d+ force=\d+ state=2 homed=1 hold=\d+ pos=' + $ExpectedPosition + ' target=' + $ExpectedPosition + ' press=-?\d+ cycles=\d+ done=\d+ backoff=0 seek=\d+ fault=0$'
    Assert-ContainsPattern -Lines $Lines -Pattern $pattern -FailureMessage $FailureMessage
}

$resolvedPort = Resolve-AppPort -ExplicitPort $PortName -WaitMs 8000
Write-Host ("Using port {0}" -f $resolvedPort)

$serial = $null

try {
    $serial = Open-UsbSerialPort -ResolvedPort $resolvedPort
    $startupLines = Read-StartupLines -Port $serial
    if (-not (($startupLines | Where-Object { $_ -match 'heartbeat count=' }) -or ($startupLines | Where-Object { $_ -match '^boot:' }))) {
        throw 'Did not observe a recognized startup or heartbeat line.'
    }

    $configLines = Invoke-BenchCommand -Port $serial -Command 'config'
    Assert-ContainsPattern -Lines $configLines -Pattern '^cmd: config$' -FailureMessage 'Config command did not respond.'

    $driverLines = Invoke-BenchCommand -Port $serial -Command 'driver'
    Assert-ContainsPattern -Lines $driverLines -Pattern '^cmd: driver$' -FailureMessage 'Driver command did not respond.'
    $driver = Get-DriverSnapshot -Lines $driverLines
    $allowUnverified = Get-AllowUnverifiedMotion -Lines $configLines

    if ($RequireTmcVerified -and (($driver.Verify -eq 0) -or ($driver.IfcntValid -eq 0))) {
        throw ('TMC verification is still failing. verify={0} ifcnt_valid={1} ifcnt={2}' -f $driver.Verify, $driver.IfcntValid, $driver.Ifcnt)
    }

    if ($AllowUnverifiedMotion) {
        $overrideLines = Invoke-BenchCommand -Port $serial -Command 'set tmc.allow_unverified_motion 1'
        Assert-ContainsPattern -Lines $overrideLines -Pattern '^cmd: set key=TMC.ALLOW_UNVERIFIED_MOTION value=1 ok=1 reboot=0$' -FailureMessage 'Failed to enable temporary unverified-motion override.'
        $allowUnverified = 1
    }

    if ($IncludeMotion) {
        if (($driver.Verify -eq 0) -and ($allowUnverified -eq 0)) {
            throw 'Motion was requested but TMC verification is failing and the unverified-motion override is disabled.'
        }

        $travelLimitUm = $TravelMaxUm - $TravelMinUm
        if ($travelLimitUm -le 0) {
            throw 'TravelMaxUm must be greater than TravelMinUm.'
        }

        $lines = Invoke-BenchCommand -Port $serial -Command ("set axis.travel_um_per_rotation {0}" -f $TravelUmPerRotation)
        Assert-ContainsPattern -Lines $lines -Pattern ('^cmd: set key=AXIS.TRAVEL_UM_PER_ROTATION value={0} ok=1 reboot=0$' -f $TravelUmPerRotation) -FailureMessage 'Failed to set travel_um_per_rotation.'

        $lines = Invoke-BenchCommand -Port $serial -Command ("set axis.travel_limit_um {0}" -f $travelLimitUm)
        Assert-ContainsPattern -Lines $lines -Pattern ('^cmd: set key=AXIS.TRAVEL_LIMIT_UM value={0} ok=1 reboot=0$' -f $travelLimitUm) -FailureMessage 'Failed to set travel_limit_um.'

        $lines = Invoke-BenchCommand -Port $serial -Command ("set axis.travel_min_um {0}" -f $TravelMinUm)
        Assert-ContainsPattern -Lines $lines -Pattern ('^cmd: set key=AXIS.TRAVEL_MIN_UM value={0} ok=1 reboot=0$' -f $TravelMinUm) -FailureMessage 'Failed to set travel_min_um.'

        $lines = Invoke-BenchCommand -Port $serial -Command ("set motion.home_feedrate_mm_per_min {0}" -f $HomeFeedrateMmPerMin)
        Assert-ContainsPattern -Lines $lines -Pattern ('^cmd: set key=MOTION.HOME_FEEDRATE_MM_PER_MIN value={0} ok=1 reboot=0$' -f $HomeFeedrateMmPerMin) -FailureMessage 'Failed to set home_feedrate_mm_per_min.'

        $lines = Invoke-BenchCommand -Port $serial -Command ("set motion.move_feedrate_mm_per_min {0}" -f $MoveFeedrateMmPerMin)
        Assert-ContainsPattern -Lines $lines -Pattern ('^cmd: set key=MOTION.MOVE_FEEDRATE_MM_PER_MIN value={0} ok=1 reboot=0$' -f $MoveFeedrateMmPerMin) -FailureMessage 'Failed to set move_feedrate_mm_per_min.'

        $configLines = Invoke-BenchCommand -Port $serial -Command 'config'
        Assert-ContainsPattern -Lines $configLines -Pattern ('axis\.travel_um_per_rotation={0}' -f $TravelUmPerRotation) -FailureMessage 'Updated travel_um_per_rotation was not reflected in config.'
        Assert-ContainsPattern -Lines $configLines -Pattern ('axis\.travel_min_um={0}' -f $TravelMinUm) -FailureMessage 'Updated travel_min_um was not reflected in config.'
        Assert-ContainsPattern -Lines $configLines -Pattern ('axis\.travel_max_um={0}' -f $TravelMaxUm) -FailureMessage 'Updated travel_max_um was not reflected in config.'
        Assert-ContainsPattern -Lines $configLines -Pattern ('motion\.home_feedrate_mm_per_min={0}' -f $HomeFeedrateMmPerMin) -FailureMessage 'Updated home_feedrate_mm_per_min was not reflected in config.'
        Assert-ContainsPattern -Lines $configLines -Pattern ('motion\.move_feedrate_mm_per_min={0}' -f $MoveFeedrateMmPerMin) -FailureMessage 'Updated move_feedrate_mm_per_min was not reflected in config.'

        $homeLines = Invoke-BenchCommand -Port $serial -Command 'home' -MaxMs 20000 -IdleMs 1500
        Assert-ContainsPattern -Lines $homeLines -Pattern '^cmd: home$' -FailureMessage 'Home command was not accepted.'
        Assert-ContainsPattern -Lines $homeLines -Pattern 'homing:' -FailureMessage 'Home command did not emit homing telemetry.'

        $statusLines = Invoke-BenchCommand -Port $serial -Command 'status'
        Assert-StatusPosition -Lines $statusLines -ExpectedPosition 0 -FailureMessage 'Home status did not settle at zero without fault.'

        $moveLines = Invoke-BenchCommand -Port $serial -Command ("moveabs {0}" -f $TravelTargetSteps) -MaxMs 20000 -IdleMs 1500
        Assert-ContainsPattern -Lines $moveLines -Pattern ('^cmd: moveabs target={0} ok=1$' -f $TravelTargetSteps) -FailureMessage 'Outbound move was not accepted.'
        Assert-ContainsPattern -Lines $moveLines -Pattern 'move: target reached' -FailureMessage 'Outbound move did not complete cleanly.'

        $statusLines = Invoke-BenchCommand -Port $serial -Command 'status'
        Assert-StatusPosition -Lines $statusLines -ExpectedPosition $TravelTargetSteps -FailureMessage 'Outbound move did not settle at the expected target.'

        if ($HoldAtTargetMs -gt 0) {
            Write-Host ('Holding at target for {0} ms before return.' -f $HoldAtTargetMs)
            Start-Sleep -Milliseconds $HoldAtTargetMs

            $statusLines = Invoke-BenchCommand -Port $serial -Command 'status'
            Assert-StatusPosition -Lines $statusLines -ExpectedPosition $TravelTargetSteps -FailureMessage 'Axis did not remain at the expected target during the hold interval.'
        }

        $returnLines = Invoke-BenchCommand -Port $serial -Command 'moveabs 0' -MaxMs 20000 -IdleMs 1500
        Assert-ContainsPattern -Lines $returnLines -Pattern '^cmd: moveabs target=0 ok=1$' -FailureMessage 'Return move was not accepted.'
        Assert-ContainsPattern -Lines $returnLines -Pattern 'move: target reached' -FailureMessage 'Return move did not complete cleanly.'

        $statusLines = Invoke-BenchCommand -Port $serial -Command 'status'
        Assert-StatusPosition -Lines $statusLines -ExpectedPosition 0 -FailureMessage 'Final status did not return to zero without fault.'
    }

    Write-Host ('PASS bench readiness verify={0} ifcnt_valid={1} ifcnt={2}' -f $driver.Verify, $driver.IfcntValid, $driver.Ifcnt)
}
finally {
    if ($serial) {
        if ($serial.IsOpen) {
            try {
                $null = Invoke-BenchCommand -Port $serial -Command 'hold off' -MaxMs 1500 -IdleMs 300
                $null = Invoke-BenchCommand -Port $serial -Command 'disable' -MaxMs 1500 -IdleMs 300
            }
            catch {
                Write-Warning ('Failed to leave the driver disabled at script shutdown: {0}' -f $_.Exception.Message)
            }

            $serial.Close()
        }
        $serial.Dispose()
    }
}