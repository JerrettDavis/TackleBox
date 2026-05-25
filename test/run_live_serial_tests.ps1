param(
    [string]$PortName,
    [switch]$IncludeMotion,
    [switch]$IncludePersistence
)

$ErrorActionPreference = 'Stop'

function Get-Stm32Port {
    $device = Get-CimInstance Win32_SerialPort |
        Where-Object { $_.PNPDeviceID -like 'USB\VID_0483&PID_5740*' } |
        Select-Object -First 1

    if (-not $device) {
        throw 'STM32 CDC device not found. Make sure the SKR2 is powered and enumerated over USB.'
    }

    return $device.DeviceID
}

function Open-SerialPort {
    param([string]$ResolvedPort)

    $port = New-Object System.IO.Ports.SerialPort $ResolvedPort,115200,None,8,one
    $port.ReadTimeout = 500
    $port.WriteTimeout = 1000
    $port.NewLine = "`r`n"
    $port.DtrEnable = $true
    $port.RtsEnable = $true
    $port.Open()
    return $port
}

function Read-LinesFor {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [int]$Milliseconds,
        [int]$MaxLines = 200
    )

    $lines = New-Object System.Collections.Generic.List[string]
    $deadline = [Environment]::TickCount + $Milliseconds

    while (($lines.Count -lt $MaxLines) -and ([Environment]::TickCount -lt $deadline)) {
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

function Assert-ContainsLine {
    param(
        [string[]]$Lines,
        [string]$Pattern,
        [string]$FailureMessage
    )

    if (-not ($Lines | Where-Object { $_ -match $Pattern })) {
        throw $FailureMessage + "`nObserved lines:`n" + ($Lines -join "`n")
    }
}

function Assert-NotContainsLine {
    param(
        [string[]]$Lines,
        [string]$Pattern,
        [string]$FailureMessage
    )

    if ($Lines | Where-Object { $_ -match $Pattern }) {
        throw $FailureMessage + "`nObserved lines:`n" + ($Lines -join "`n")
    }
}

function Get-DriverSnapshot {
    param([string[]]$Lines)

    $driverLine = $Lines | Where-Object { $_ -match '^driver uart=\d+ irun=\d+ ihold=\d+ iholddelay=\d+ tpowerdown=\d+ sgthrs=\d+$' } | Select-Object -Last 1
    $verifyLine = $Lines | Where-Object { $_ -match '^driver verify=\d+ ifcnt_valid=\d+ ifcnt=\d+ gconf=0x[0-9A-Fa-f]{8} ihold_irun=0x[0-9A-Fa-f]{8} tpowerdown_reg=0x[0-9A-Fa-f]{8} sgthrs_reg=0x[0-9A-Fa-f]{8}$' } | Select-Object -Last 1

    if (-not $driverLine) {
        throw "Driver summary line missing.`nObserved lines:`n" + ($Lines -join "`n")
    }
    if (-not $verifyLine) {
        throw "Driver verification line missing.`nObserved lines:`n" + ($Lines -join "`n")
    }

    $driverMatch = [regex]::Match($driverLine, '^driver uart=(\d+) irun=(\d+) ihold=(\d+) iholddelay=(\d+) tpowerdown=(\d+) sgthrs=(\d+)$')
    $verifyMatch = [regex]::Match($verifyLine, '^driver verify=(\d+) ifcnt_valid=(\d+) ifcnt=(\d+) gconf=0x([0-9A-Fa-f]{8}) ihold_irun=0x([0-9A-Fa-f]{8}) tpowerdown_reg=0x([0-9A-Fa-f]{8}) sgthrs_reg=0x([0-9A-Fa-f]{8})$')

    return @{
        Uart = [int]$driverMatch.Groups[1].Value
        Irun = [int]$driverMatch.Groups[2].Value
        Ihold = [int]$driverMatch.Groups[3].Value
        IholdDelay = [int]$driverMatch.Groups[4].Value
        Tpowerdown = [int]$driverMatch.Groups[5].Value
        Sgthrs = [int]$driverMatch.Groups[6].Value
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
        [string]$ExpectedDataPin,
        [string]$ExpectedClockPin,
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

    if ($match.Groups[1].Value -ne $ExpectedSource) {
        throw "Unexpected load-cell source. Expected '$ExpectedSource', observed '$($match.Groups[1].Value)'."
    }
    if ($match.Groups[2].Value -ne $ExpectedConnector) {
        throw "Unexpected load-cell connector. Expected '$ExpectedConnector', observed '$($match.Groups[2].Value)'."
    }
    if ([int]$match.Groups[3].Value -ne $ExpectedThreshold) {
        throw "Unexpected load-cell threshold. Expected '$ExpectedThreshold', observed '$($match.Groups[3].Value)'."
    }
    if ($match.Groups[4].Value -ne $ExpectedDataPin) {
        throw "Unexpected load-cell data pin. Expected '$ExpectedDataPin', observed '$($match.Groups[4].Value)'."
    }
    if ($match.Groups[5].Value -ne $ExpectedClockPin) {
        throw "Unexpected load-cell clock pin. Expected '$ExpectedClockPin', observed '$($match.Groups[5].Value)'."
    }
}

function Invoke-CommandCheck {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [string]$Command,
        [int]$ReadMs,
        [string[]]$ExpectedPatterns
    )

    $Port.WriteLine($Command)
    $lines = Read-LinesFor -Port $Port -Milliseconds $ReadMs

    foreach ($pattern in $ExpectedPatterns) {
        Assert-ContainsLine -Lines $lines -Pattern $pattern -FailureMessage "Command '$Command' did not produce expected pattern '$pattern'."
    }

    return $lines
}

$resolvedPort = if ($PortName) { $PortName } else { Get-Stm32Port }
Write-Host "Using port $resolvedPort"

$port = $null

try {
    $port = Open-SerialPort -ResolvedPort $resolvedPort

    $startupLines = Read-LinesFor -Port $port -Milliseconds 2500 -MaxLines 50
    if (-not ($startupLines | Where-Object { $_ -match 'heartbeat count=' })) {
        $startupLines += Read-LinesFor -Port $port -Milliseconds 2500 -MaxLines 50
    }

    $startupIdentified = $startupLines | Where-Object { $_ -match '^boot: (microsd missing|tmc verify failed|source=(usb|microsd|flash|defaults)|motion override enabled without tmc verify)$|^SKR2 X endstop homing test start$|^baseline diag0=\d+ xstop=\d+ diag2=\d+$|^homing:' }
    if ((-not ($startupLines | Where-Object { $_ -match 'heartbeat count=' })) -and (-not $startupIdentified)) {
        throw 'Did not observe heartbeat telemetry or recognized startup lines.' + "`nObserved lines:`n" + ($startupLines -join "`n")
    }

    if (-not ($startupLines | Where-Object { $_ -match '^boot: source=(usb|microsd|flash|defaults)$|^SKR2 X endstop homing test start$' })) {
        Write-Host 'Startup identification lines were not observed; continuing with steady-state heartbeat attachment.'
    }

    $helpLines = Invoke-CommandCheck -Port $port -Command 'help' -ReadMs 1500 -ExpectedPatterns @(
        ('cmds: .*SAFETY/M122.*CONFIG/CFG.*SET KEY VALUE.*SAVE/SAVECFG.*RESETCFG.*REBOOT.*DRIVER/TMC.*IRUN.*IHOLD.*IHOLDDELAY.*SGTHRS.*ENABLE/M17.*DISABLE/M18/M84.*HOLD.*MOVEABS.*MOVEREL.*SETPOS.*CYCLE.*PRESSPOS.*SIMLOAD.*SIMTHRESH.*SIMMECH.*SIMSTALL.*SIMCLEAR.*HOME/G28.*STOP/M112.*BACKOFF.*HELP/' + [char]3 + '?').Replace([char]3, '?')
    )

    $configLines = Invoke-CommandCheck -Port $port -Command 'config' -ReadMs 1500 -ExpectedPatterns @(
        'cmd: config',
        '^config loaded=\d+ dirty=\d+ reboot=\d+$',
        '^config pin.x_step=P[A-H]\d+ pin.x_dir=P[A-H]\d+ pin.x_enable=P[A-H]\d+ pin.x_uart=P[A-H]\d+ pin.x_stop=P[A-H]\d+$'
    )

    $configSourcesLines = Invoke-CommandCheck -Port $port -Command 'config sources' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: config$',
        '^config sources selected=(usb|microsd|flash|defaults) usb=1 microsd_card=\d+ microsd_cfg=\d+ flash=\d+ defaults=1$'
    )

    $bootRuntimeLines = Invoke-CommandCheck -Port $port -Command 'boot' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: boot ok=0 stage=runtime$'
    )

    $statusLines = Invoke-CommandCheck -Port $port -Command 'status' -ReadMs 1500 -ExpectedPatterns @(
        'cmd: status',
        '^diag0=\d+ xstop=\d+ diag2=\d+ pressed=\d+ conf=\d+ load=\d+ mech=\d+ stall=\d+ source=\d+ force=\d+ state=\d+ homed=\d+ hold=\d+ pos=-?\d+ target=-?\d+ press=-?\d+ cycles=\d+ done=\d+ backoff=\d+ seek=\d+ fault=\d+$'
    )

    $statusAliasLines = Invoke-CommandCheck -Port $port -Command 'm114' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: status$',
        '^diag0=\d+ xstop=\d+ diag2=\d+ pressed=\d+ conf=\d+ load=\d+ mech=\d+ stall=\d+ source=\d+ force=\d+ state=\d+ homed=\d+ hold=\d+ pos=-?\d+ target=-?\d+ press=-?\d+ cycles=\d+ done=\d+ backoff=\d+ seek=\d+ fault=\d+$'
    )

    $safetyLines = Invoke-CommandCheck -Port $port -Command 'safety' -ReadMs 1500 -ExpectedPatterns @(
        'cmd: safety',
        '^diag0=\d+ xstop=\d+ diag2=\d+ pressed=\d+ conf=\d+ load=\d+ mech=\d+ stall=\d+ source=\d+ force=\d+ state=\d+ homed=\d+ hold=\d+ pos=-?\d+ target=-?\d+ press=-?\d+ cycles=\d+ done=\d+ backoff=\d+ seek=\d+ fault=\d+$',
        '^sim raw=\d+ thresh=\d+ load=\d+ mech=\d+ stall=\d+$'
    )

    $safetyAliasLines = Invoke-CommandCheck -Port $port -Command 'm122' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: safety$',
        '^sim raw=\d+ thresh=\d+ load=\d+ mech=\d+ stall=\d+$'
    )

    $holdOffLines = Invoke-CommandCheck -Port $port -Command 'hold off' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: hold value=0$'
    )

    $driverLines = Invoke-CommandCheck -Port $port -Command 'driver' -ReadMs 1500 -ExpectedPatterns @(
        'cmd: driver',
        '^driver uart=\d+ irun=\d+ ihold=\d+ iholddelay=\d+ tpowerdown=\d+ sgthrs=\d+$',
        '^driver verify=\d+ ifcnt_valid=\d+ ifcnt=\d+ gconf=0x[0-9A-Fa-f]{8} ihold_irun=0x[0-9A-Fa-f]{8} tpowerdown_reg=0x[0-9A-Fa-f]{8} sgthrs_reg=0x[0-9A-Fa-f]{8}$'
    )
    $driverSnapshot = Get-DriverSnapshot -Lines $driverLines
    $allowUnverifiedMotion = Get-AllowUnverifiedMotion -Lines $configLines
    $motionAllowed = ($driverSnapshot.Verify -ne 0) -or ($allowUnverifiedMotion -ne 0)

    $irunLines = Invoke-CommandCheck -Port $port -Command 'irun 5' -ReadMs 1500 -ExpectedPatterns @(
        'cmd: irun value=5',
        '^driver uart=\d+ irun=5 ihold=\d+ iholddelay=\d+ tpowerdown=\d+ sgthrs=\d+$'
    )

    $iholdLines = Invoke-CommandCheck -Port $port -Command 'ihold 0' -ReadMs 1500 -ExpectedPatterns @(
        'cmd: ihold value=0',
        '^driver uart=\d+ irun=5 ihold=0 iholddelay=\d+ tpowerdown=\d+ sgthrs=\d+$'
    )

    $iholdDelayLines = Invoke-CommandCheck -Port $port -Command 'iholddelay 4' -ReadMs 1500 -ExpectedPatterns @(
        'cmd: iholddelay value=4',
        '^driver uart=\d+ irun=5 ihold=0 iholddelay=4 tpowerdown=\d+ sgthrs=\d+$'
    )

    $sgthrsLines = Invoke-CommandCheck -Port $port -Command 'sgthrs 4' -ReadMs 1500 -ExpectedPatterns @(
        'cmd: sgthrs value=4',
        '^driver uart=\d+ irun=5 ihold=0 iholddelay=4 tpowerdown=\d+ sgthrs=4$'
    )

    $disableLines = Invoke-CommandCheck -Port $port -Command 'disable' -ReadMs 1500 -ExpectedPatterns @(
        'cmd: disable'
    )

    $disableAliasLines = Invoke-CommandCheck -Port $port -Command 'm18' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: disable$'
    )

    if (-not $motionAllowed) {
        $holdLines = Invoke-CommandCheck -Port $port -Command 'hold on' -ReadMs 1500 -ExpectedPatterns @(
            '^cmd: hold ok=0 reason=tmc_unverified$'
        )

        $enableLines = Invoke-CommandCheck -Port $port -Command 'enable' -ReadMs 1500 -ExpectedPatterns @(
            '^cmd: enable ok=0 reason=tmc_unverified$'
        )

        $enableAliasLines = Invoke-CommandCheck -Port $port -Command 'm17' -ReadMs 1500 -ExpectedPatterns @(
            '^cmd: enable ok=0 reason=tmc_unverified$'
        )
    }
    else {
        $holdLines = Invoke-CommandCheck -Port $port -Command 'hold on' -ReadMs 1500 -ExpectedPatterns @(
            '^cmd: hold value=1$'
        )

        $enableLines = Invoke-CommandCheck -Port $port -Command 'enable' -ReadMs 1500 -ExpectedPatterns @(
            '^cmd: enable$'
        )

        $enableAliasLines = Invoke-CommandCheck -Port $port -Command 'm17' -ReadMs 1500 -ExpectedPatterns @(
            '^cmd: enable$'
        )
    }

    $setThresholdLines = Invoke-CommandCheck -Port $port -Command 'set sim.load_threshold 1000' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: set key=SIM.LOAD_THRESHOLD value=1000 ok=1 reboot=0$'
    )

    $setLoadCellDetLines = Invoke-CommandCheck -Port $port -Command 'set loadcell.connector skr2_det' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: set key=LOADCELL.CONNECTOR value=SKR2_DET ok=1 reboot=1$'
    )
    $configLoadCellDetLines = Invoke-CommandCheck -Port $port -Command 'config' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: config$',
        '^config loadcell\.source=hx711 loadcell\.connector=skr2_det loadcell\.threshold=1000 pin\.loadcell_data=PC2 pin\.loadcell_clock=PA0$'
    )
    Assert-LoadCellConfig -Lines $configLoadCellDetLines -ExpectedSource 'hx711' -ExpectedConnector 'skr2_det' -ExpectedDataPin 'PC2' -ExpectedClockPin 'PA0' -ExpectedThreshold 1000

    $setLoadCellTh1Lines = Invoke-CommandCheck -Port $port -Command 'set loadcell.connector skr2_th1' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: set key=LOADCELL.CONNECTOR value=SKR2_TH1 ok=1 reboot=1$'
    )
    $configLoadCellTh1Lines = Invoke-CommandCheck -Port $port -Command 'config' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: config$',
        '^config loadcell\.source=adc loadcell\.connector=skr2_th1 loadcell\.threshold=1000 pin\.loadcell_data=PA3 pin\.loadcell_clock=P\?0$'
    )
    Assert-LoadCellConfig -Lines $configLoadCellTh1Lines -ExpectedSource 'adc' -ExpectedConnector 'skr2_th1' -ExpectedDataPin 'PA3' -ExpectedClockPin 'P?0' -ExpectedThreshold 1000

    $setLoadCellSourceLines = Invoke-CommandCheck -Port $port -Command 'set loadcell.source hx711' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: set key=LOADCELL.SOURCE value=HX711 ok=1 reboot=1$'
    )
    $setLoadCellDataPinLines = Invoke-CommandCheck -Port $port -Command 'set pin.loadcell_data pc2' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: set key=PIN.LOADCELL_DATA value=PC2 ok=1 reboot=1$'
    )
    $setLoadCellClockPinLines = Invoke-CommandCheck -Port $port -Command 'set pin.loadcell_clock pa0' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: set key=PIN.LOADCELL_CLOCK value=PA0 ok=1 reboot=1$'
    )
    $configLoadCellCustomLines = Invoke-CommandCheck -Port $port -Command 'config' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: config$',
        '^config loadcell\.source=hx711 loadcell\.connector=custom loadcell\.threshold=1000 pin\.loadcell_data=PC2 pin\.loadcell_clock=PA0$'
    )
    Assert-LoadCellConfig -Lines $configLoadCellCustomLines -ExpectedSource 'hx711' -ExpectedConnector 'custom' -ExpectedDataPin 'PC2' -ExpectedClockPin 'PA0' -ExpectedThreshold 1000

    $simThreshLines = Invoke-CommandCheck -Port $port -Command 'simthresh 1000' -ReadMs 1500 -ExpectedPatterns @(
        'cmd: simthresh thresh=1000',
        '^sim raw=\d+ thresh=1000 load=\d+ mech=\d+ stall=\d+$'
    )

    $simLoadLines = Invoke-CommandCheck -Port $port -Command 'simload 1200' -ReadMs 1500 -ExpectedPatterns @(
        'cmd: simload raw=1200',
        '^sim raw=1200 thresh=1000 load=1 mech=\d+ stall=\d+$'
    )

    $simSafetyLines = Invoke-CommandCheck -Port $port -Command 'safety' -ReadMs 1500 -ExpectedPatterns @(
        'cmd: safety',
        '^diag0=\d+ xstop=\d+ diag2=\d+ pressed=\d+ conf=\d+ load=1 mech=\d+ stall=\d+ source=\d+ force=1200 state=\d+ homed=\d+ hold=\d+ pos=-?\d+ target=-?\d+ press=-?\d+ cycles=\d+ done=\d+ backoff=\d+ seek=\d+ fault=\d+$',
        '^sim raw=1200 thresh=1000 load=1 mech=\d+ stall=\d+$'
    )

    $simMechLines = Invoke-CommandCheck -Port $port -Command 'simmech on' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: simmech value=1$',
        '^sim raw=1200 thresh=1000 load=1 mech=1 stall=\d+$'
    )

    $simStallLines = Invoke-CommandCheck -Port $port -Command 'simstall on' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: simstall value=1$',
        '^sim raw=1200 thresh=1000 load=1 mech=1 stall=1$'
    )

    $simSafetyLatchedLines = Invoke-CommandCheck -Port $port -Command 'safety' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: safety$',
        '^sim raw=1200 thresh=1000 load=1 mech=1 stall=1$'
    )

    $simClearLines = Invoke-CommandCheck -Port $port -Command 'simclear' -ReadMs 1500 -ExpectedPatterns @(
        'cmd: simclear',
        '^sim raw=0 thresh=1000 load=0 mech=0 stall=0$'
    )

    $stopLines = Invoke-CommandCheck -Port $port -Command 'stop' -ReadMs 1500 -ExpectedPatterns @(
        'cmd: stop'
    )

    $stopAliasLines = Invoke-CommandCheck -Port $port -Command 'm112' -ReadMs 1500 -ExpectedPatterns @(
        '^cmd: stop$'
    )

    if ($IncludePersistence) {
        $saveLines = Invoke-CommandCheck -Port $port -Command 'save' -ReadMs 1500 -ExpectedPatterns @(
            '^cmd: save ok=\d+ reboot=\d+$'
        )
        $null = $saveLines
    }

    if ($IncludeMotion -and $motionAllowed) {
        $setPosLines = Invoke-CommandCheck -Port $port -Command 'setpos 0' -ReadMs 1500 -ExpectedPatterns @(
            'cmd: setpos pos=0'
        )

        $pressPosLines = Invoke-CommandCheck -Port $port -Command 'presspos 40' -ReadMs 1500 -ExpectedPatterns @(
            'cmd: presspos pos=40 ok=1'
        )

        $moveAbsLines = Invoke-CommandCheck -Port $port -Command 'moveabs 20' -ReadMs 2500 -ExpectedPatterns @(
            'cmd: moveabs target=20 ok=1',
            'move: target reached|heartbeat count='
        )

        $moveRelLines = Invoke-CommandCheck -Port $port -Command 'jog -10' -ReadMs 2500 -ExpectedPatterns @(
            'cmd: moverel delta=-10 ok=1',
            'move: target reached|heartbeat count='
        )

        $cycleLines = Invoke-CommandCheck -Port $port -Command 'cycle 2' -ReadMs 4000 -ExpectedPatterns @(
            'cmd: cycle count=2 ok=1',
            'cycle: complete|heartbeat count='
        )

        $homeLines = Invoke-CommandCheck -Port $port -Command 'home' -ReadMs 4000 -ExpectedPatterns @(
            'cmd: home',
            'homing:'
        )

        $backoffLines = Invoke-CommandCheck -Port $port -Command 'backoff' -ReadMs 3000 -ExpectedPatterns @(
            'cmd: backoff'
        )

        $null = $setPosLines
        $null = $pressPosLines
        $null = $moveAbsLines
        $null = $moveRelLines
        $null = $cycleLines
        $null = $homeLines
        $null = $backoffLines
    }
    elseif (-not $motionAllowed) {
        $blockedMoveLines = Invoke-CommandCheck -Port $port -Command 'moveabs 20' -ReadMs 1500 -ExpectedPatterns @(
            '^cmd: moveabs ok=0 reason=tmc_unverified$'
        )

        $blockedJogLines = Invoke-CommandCheck -Port $port -Command 'jog -10' -ReadMs 1500 -ExpectedPatterns @(
            '^cmd: moverel ok=0 reason=tmc_unverified$'
        )

        $blockedCycleLines = Invoke-CommandCheck -Port $port -Command 'cycle 2' -ReadMs 1500 -ExpectedPatterns @(
            '^cmd: cycle ok=0 reason=tmc_unverified$'
        )

        $blockedHomeLines = Invoke-CommandCheck -Port $port -Command 'home' -ReadMs 1500 -ExpectedPatterns @(
            '^cmd: home ok=0 reason=tmc_unverified$'
        )

        $blockedHomeAliasLines = Invoke-CommandCheck -Port $port -Command 'g28' -ReadMs 1500 -ExpectedPatterns @(
            '^cmd: home ok=0 reason=tmc_unverified$'
        )

        $blockedBackoffLines = Invoke-CommandCheck -Port $port -Command 'backoff' -ReadMs 1500 -ExpectedPatterns @(
            '^cmd: backoff ok=0 reason=tmc_unverified$'
        )

        $null = $blockedMoveLines
        $null = $blockedJogLines
        $null = $blockedCycleLines
        $null = $blockedHomeLines
        $null = $blockedHomeAliasLines
        $null = $blockedBackoffLines
    }

    $null = $helpLines
    $null = $statusLines
    $null = $statusAliasLines
    $null = $safetyLines
    $null = $safetyAliasLines
    $null = $configLines
    $null = $configSourcesLines
    $null = $bootRuntimeLines
    $null = $holdOffLines
    $null = $holdLines
    $null = $driverLines
    $null = $irunLines
    $null = $iholdLines
    $null = $iholdDelayLines
    $null = $sgthrsLines
    $null = $disableLines
    $null = $disableAliasLines
    $null = $enableLines
    $null = $enableAliasLines
    $null = $setThresholdLines
    $null = $setLoadCellDetLines
    $null = $configLoadCellDetLines
    $null = $setLoadCellTh1Lines
    $null = $configLoadCellTh1Lines
    $null = $setLoadCellSourceLines
    $null = $setLoadCellDataPinLines
    $null = $setLoadCellClockPinLines
    $null = $configLoadCellCustomLines
    $null = $simThreshLines
    $null = $simLoadLines
    $null = $simSafetyLines
    $null = $simMechLines
    $null = $simStallLines
    $null = $simSafetyLatchedLines
    $null = $simClearLines
    $null = $stopLines
    $null = $stopAliasLines

    $postLines = Read-LinesFor -Port $port -Milliseconds 2000 -MaxLines 50
    Assert-ContainsLine -Lines $postLines -Pattern 'heartbeat count=' -FailureMessage 'Heartbeat stopped after command checks.'

    Write-Host 'PASS live serial tests'
}
finally {
    if ($port) {
        if ($port.IsOpen) {
            try {
                $null = Invoke-CommandCheck -Port $port -Command 'hold off' -ReadMs 1000 -ExpectedPatterns @(
                    '^cmd: hold value=0$'
                )
                $null = Invoke-CommandCheck -Port $port -Command 'disable' -ReadMs 1000 -ExpectedPatterns @(
                    '^cmd: disable$|^cmd: disable'
                )
            }
            catch {
                Write-Warning ('Failed to leave the driver disabled at script shutdown: {0}' -f $_.Exception.Message)
            }

            $port.Close()
        }
        $port.Dispose()
    }
}