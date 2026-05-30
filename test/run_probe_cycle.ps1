param(
    [string]$PortName,
    [switch]$AllowUnverifiedMotion,
    [switch]$RequireTmcVerified,
    [ValidateSet('SIMULATION', 'HX711', 'ADC')]
    [string]$LoadCellSource = 'HX711',
    [ValidateSet('CUSTOM', 'SKR2_BLTOUCH', 'SKR2_DET', 'SKR2_TH1', 'SKR2_TH0', 'SKR2_TB')]
    [string]$LoadCellConnector = 'SKR2_DET',
    [string]$LoadCellDataPin = 'PC2',
    [string]$LoadCellClockPin = 'PA0',
    [int]$LoadCellThreshold = 1000,
    [int]$PressTargetSteps = 40,
    [int]$CycleCount = 1,
    [switch]$SaveConfig
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $root 'tools\usb_serial_common.ps1')

function Invoke-ProbeCommand {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [string]$Command,
        [string[]]$ExpectedPatterns = @(),
        [int]$MaxMs = 4000,
        [int]$IdleMs = 300
    )

    Write-Host (">>> {0}" -f $Command)
    $lines = Invoke-SerialCommand -Port $Port -Command $Command -ExpectedPatterns $ExpectedPatterns -MaxMs $MaxMs -IdleMs $IdleMs
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

function Assert-LoadCellConfig {
    param(
        [string[]]$Lines,
        [string]$ExpectedSource,
        [string]$ExpectedConnector,
        [int]$ExpectedThreshold
    )

    $configLine = $Lines | Where-Object { $_ -match '^config loadcell\.source=' } | Select-Object -Last 1
    if (-not $configLine) {
        throw "Load-cell config line missing.`nObserved lines:`n" + ($Lines -join "`n")
    }

    $pattern = '^config loadcell\.source=([^ ]+) loadcell\.connector=([^ ]+) loadcell\.threshold=(\d+) pin\.loadcell_data=([^ ]+) pin\.loadcell_clock=([^ ]+)$'
    $match = [regex]::Match($configLine, $pattern)
    if (-not $match.Success) {
        throw "Load-cell config line was not parseable.`nObserved line:`n$configLine"
    }

    if ($match.Groups[1].Value -ne $ExpectedSource.ToLowerInvariant()) {
        throw "Unexpected load-cell source. Expected '$ExpectedSource', observed '$($match.Groups[1].Value)'."
    }
    if ($match.Groups[2].Value -ne $ExpectedConnector.ToLowerInvariant()) {
        throw "Unexpected load-cell connector. Expected '$ExpectedConnector', observed '$($match.Groups[2].Value)'."
    }
    if ([int]$match.Groups[3].Value -ne $ExpectedThreshold) {
        throw "Unexpected load-cell threshold. Expected '$ExpectedThreshold', observed '$($match.Groups[3].Value)'."
    }
}

if ($LoadCellThreshold -le 0) {
    throw 'LoadCellThreshold must be greater than zero.'
}
if ($PressTargetSteps -lt 0) {
    throw 'PressTargetSteps must be zero or greater.'
}
if ($CycleCount -le 0) {
    throw 'CycleCount must be greater than zero.'
}

$resolvedPort = Resolve-AppPort -ExplicitPort $PortName -WaitMs 8000
Write-Host ("Using port {0}" -f $resolvedPort)

$serial = $null

try {
    $serial = Open-UsbSerialPort -ResolvedPort $resolvedPort
    $startupLines = Read-SerialLinesUntilIdle -Port $serial -MaxMs 2500
    if (-not ($startupLines | Where-Object { $_ -match 'heartbeat count=' })) {
        $startupLines += Read-SerialLinesUntilIdle -Port $serial -MaxMs 2500
    }

    $configLines = Invoke-ProbeCommand -Port $serial -Command 'config' -ExpectedPatterns @('^cmd: config$')
    $driverLines = Invoke-ProbeCommand -Port $serial -Command 'driver' -ExpectedPatterns @('^cmd: driver$')
    $driver = Get-DriverSnapshot -Lines $driverLines
    $allowUnverified = Get-AllowUnverifiedMotion -Lines $configLines

    if ($RequireTmcVerified -and (($driver.Verify -eq 0) -or ($driver.IfcntValid -eq 0))) {
        throw ('TMC verification is still failing. verify={0} ifcnt_valid={1} ifcnt={2}' -f $driver.Verify, $driver.IfcntValid, $driver.Ifcnt)
    }

    if ($AllowUnverifiedMotion) {
        $overrideLines = Invoke-ProbeCommand -Port $serial -Command 'set tmc.allow_unverified_motion 1' -ExpectedPatterns @('^cmd: set key=TMC.ALLOW_UNVERIFIED_MOTION value=1 ok=1 reboot=0$')
        $allowUnverified = 1
        $null = $overrideLines
    }

    if (($driver.Verify -eq 0) -and ($allowUnverified -eq 0)) {
        throw 'Probe cycling requires verified TMC motion or TMC.ALLOW_UNVERIFIED_MOTION=1.'
    }

    if ($LoadCellConnector -ne 'CUSTOM') {
        $connectorLines = Invoke-ProbeCommand -Port $serial -Command ("set loadcell.connector {0}" -f $LoadCellConnector.ToLowerInvariant()) -ExpectedPatterns @(("^cmd: set key=LOADCELL.CONNECTOR value={0} ok=1 reboot=1$" -f $LoadCellConnector))
        $null = $connectorLines
    }
    else {
        $sourceLines = Invoke-ProbeCommand -Port $serial -Command ("set loadcell.source {0}" -f $LoadCellSource.ToLowerInvariant()) -ExpectedPatterns @(("^cmd: set key=LOADCELL.SOURCE value={0} ok=1 reboot=1$" -f $LoadCellSource))
        $dataLines = Invoke-ProbeCommand -Port $serial -Command ("set pin.loadcell_data {0}" -f $LoadCellDataPin.ToLowerInvariant()) -ExpectedPatterns @(("^cmd: set key=PIN.LOADCELL_DATA value={0} ok=1 reboot=1$" -f $LoadCellDataPin.ToUpperInvariant()))
        if ($LoadCellSource -eq 'HX711') {
            $clockLines = Invoke-ProbeCommand -Port $serial -Command ("set pin.loadcell_clock {0}" -f $LoadCellClockPin.ToLowerInvariant()) -ExpectedPatterns @(("^cmd: set key=PIN.LOADCELL_CLOCK value={0} ok=1 reboot=1$" -f $LoadCellClockPin.ToUpperInvariant()))
            $null = $clockLines
        }
        $null = $sourceLines
        $null = $dataLines
    }

    $thresholdLines = Invoke-ProbeCommand -Port $serial -Command ("set loadcell.threshold {0}" -f $LoadCellThreshold) -ExpectedPatterns @(("^cmd: set key=LOADCELL.THRESHOLD value={0} ok=1 reboot=0$" -f $LoadCellThreshold))
    $configuredLines = Invoke-ProbeCommand -Port $serial -Command 'config' -ExpectedPatterns @('^cmd: config$')
    Assert-LoadCellConfig -Lines $configuredLines -ExpectedSource $LoadCellSource -ExpectedConnector $LoadCellConnector -ExpectedThreshold $LoadCellThreshold

    if ($SaveConfig) {
        $saveLines = Invoke-ProbeCommand -Port $serial -Command 'save' -ExpectedPatterns @('^cmd: save ok=\d+ reboot=\d+$')
        $null = $saveLines
    }

    $homeLines = Invoke-ProbeCommand -Port $serial -Command 'home' -ExpectedPatterns @('^cmd: home$|^cmd: home ok=0 reason=tmc_unverified$', 'homing:') -MaxMs 20000 -IdleMs 1200
    Assert-ContainsPattern -Lines $homeLines -Pattern '^cmd: home$' -FailureMessage 'Home command did not start cleanly.'

    $holdLines = Invoke-ProbeCommand -Port $serial -Command 'hold on' -ExpectedPatterns @('^cmd: hold value=1$')
    $pressLines = Invoke-ProbeCommand -Port $serial -Command ("presspos {0}" -f $PressTargetSteps) -ExpectedPatterns @(("^cmd: presspos pos={0} ok=1$" -f $PressTargetSteps))
    $cycleLines = Invoke-ProbeCommand -Port $serial -Command ("cycle {0}" -f $CycleCount) -ExpectedPatterns @(("^cmd: cycle count={0} ok=1$" -f $CycleCount), 'cycle: complete|heartbeat count=') -MaxMs 20000 -IdleMs 1200
    $statusLines = Invoke-ProbeCommand -Port $serial -Command 'status' -ExpectedPatterns @('^cmd: status$', '^diag0=\d+ xstop=\d+ diag2=\d+ pressed=\d+ conf=\d+ load=\d+ mech=\d+ stall=\d+ source=\d+ force=\d+ state=\d+ homed=\d+ hold=\d+ pos=-?\d+ target=-?\d+ press=-?\d+ contact_pos=-?\d+ cycles=\d+ done=\d+ probe=\d+ backoff=\d+ seek=\d+ fault=\d+ estop=\d+ ui_click=\d+ ui_a=\d+ ui_b=\d+ loop_last_us=\d+ loop_max_us=\d+ steps_total=\d+ steps_hb=\d+ steps_burst=\d+ tmc_sync=\d+$')
    $safetyLines = Invoke-ProbeCommand -Port $serial -Command 'safety' -ExpectedPatterns @('^cmd: safety$', '^sim source=[^ ]+ raw=\d+ thresh=\d+ load=\d+ mech=\d+ stall=\d+$')

    $null = $thresholdLines
    $null = $configuredLines
    $null = $holdLines
    $null = $pressLines
    $null = $cycleLines
    $null = $statusLines
    $null = $safetyLines

    Write-Host ('PASS probe cycle source={0} connector={1} press={2} count={3} verify={4} ifcnt_valid={5}' -f $LoadCellSource, $LoadCellConnector, $PressTargetSteps, $CycleCount, $driver.Verify, $driver.IfcntValid)
    Write-Warning 'Real HX711/ADC threshold trips still depend on future acquisition code. This script validates the operator command/config path and the motion cycle itself.'
}
finally {
    if ($serial) {
        if ($serial.IsOpen) {
            try {
                $null = Invoke-ProbeCommand -Port $serial -Command 'hold off' -ExpectedPatterns @('^cmd: hold value=0$')
                $null = Invoke-ProbeCommand -Port $serial -Command 'disable' -ExpectedPatterns @('^cmd: disable$|^cmd: disable')
            }
            catch {
                Write-Warning ('Failed to leave the driver disabled at script shutdown: {0}' -f $_.Exception.Message)
            }

            $serial.Close()
        }
        $serial.Dispose()
    }
}