param(
    [switch]$SkipBuild,
    [switch]$SkipFlash,
    [int]$UsbWaitMs = 10000
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'usb_serial_common.ps1')
. (Join-Path $PSScriptRoot 'openocd_common.ps1')

function Write-Step {
    param([string]$Message)
    Write-Host "==> $Message"
}

function Require-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Get-StartupDiagnostics {
    $state = Get-BoardUsbState
    Write-Host ("DIAG usb application={0} bootloader={1}" -f $(if ($state.ApplicationPort) { $state.ApplicationPort } else { 'none' }), $(if ($state.BootloaderPort) { $state.BootloaderPort } else { 'none' }))

    try {
        $openOcdOutput = Get-OpenOcdCommandOutput -Command 'init; reset halt; mdw 0x08008000 2; shutdown'
        Write-Host 'DIAG stlink'
        Write-Host $openOcdOutput.TrimEnd()
    }
    catch {
        Write-Host ("DIAG stlink unavailable: {0}" -f $_.Exception.Message)
    }
}

function Get-RepoRoot {
    return (Split-Path -Parent $PSScriptRoot)
}

function Build-Env {
    param([string]$EnvironmentName)

    $platformIo = Resolve-PlatformIoCommand
    & $platformIo.Executable @($platformIo.Arguments) 'run' '-e' $EnvironmentName
    if ($LASTEXITCODE -ne 0) {
        throw "PlatformIO build failed for environment '$EnvironmentName'."
    }
}

function Invoke-AppCommandChecked {
    param([string]$PortName, [string]$Command)

    $serial = $null
    try {
        $serial = Open-UsbSerialPort -ResolvedPort $PortName
        $null = Read-SerialLinesUntilIdle -Port $serial -MaxMs 2500
        return Invoke-SerialCommand -Port $serial -Command $Command -ExpectedPatterns @()
    }
    finally {
        if ($serial -and $serial.IsOpen) {
            $serial.Close()
        }
    }
}

function Invoke-AppCommandWithRetry {
    param(
        [string]$PortName,
        [string]$Command,
        [int]$Attempts = 3,
        [int]$DelayMs = 500
    )

    $lastLines = @()
    for ($attempt = 1; $attempt -le $Attempts; ++$attempt) {
        $lastLines = @(Invoke-AppCommandChecked -PortName $PortName -Command $Command)
        if (($lastLines | Where-Object { $_ -match '^diag0=' -or $_ -match '^heartbeat ' } | Select-Object -First 1) -ne $null) {
            return $lastLines
        }

        if ($attempt -lt $Attempts) {
            Start-Sleep -Milliseconds $DelayMs
        }
    }

    return $lastLines
}

function Invoke-BootloaderCommandChecked {
    param(
        [string]$PortName,
        [string]$Command,
        [switch]$AllowDisconnect
    )

    $serial = $null
    try {
        $serial = Open-UsbSerialPort -ResolvedPort $PortName
        $null = Read-SerialLinesUntilIdle -Port $serial -MaxMs 2500
        return Invoke-SerialCommand -Port $serial -Command $Command -ExpectedPatterns @() -AllowDisconnect:$AllowDisconnect
    }
    finally {
        if ($serial -and $serial.IsOpen) {
            $serial.Close()
        }
    }
}

$repoRoot = Get-RepoRoot
Set-Location $repoRoot

$bootloaderBin = '.\.pio\build\skr2_f429_bootloader_cdc\firmware.bin'
$appBin = '.\.pio\build\skr2_f429_usb\firmware.bin'

if (-not $SkipBuild) {
    Write-Step 'Building bootloader and application'
    Build-Env -EnvironmentName 'skr2_f429_bootloader_cdc'
    Build-Env -EnvironmentName 'skr2_f429_usb'
}

if (-not $SkipFlash) {
    Write-Step 'Flashing bootloader over ST-Link'
    Flash-FirmwareWithStLink -FirmwarePath $bootloaderBin -FlashAddress '0x08000000'

    Write-Step 'Flashing application over ST-Link'
    Flash-FirmwareWithStLink -FirmwarePath $appBin -FlashAddress '0x08008000'
}

Write-Step 'Resetting board and waiting for application USB'
Reset-BoardWithStLink
$state = Wait-BoardUsbState -Mode application -WaitMs $UsbWaitMs
if ($null -eq $state.ApplicationPort) {
    Get-StartupDiagnostics
    throw 'Application CDC port did not appear after reset.'
}
Write-Host "PASS app-enumerated port=$($state.ApplicationPort)"

Write-Step 'Verifying application status command'
$statusLines = Invoke-AppCommandWithRetry -PortName $state.ApplicationPort -Command 'status'
if (($statusLines | Where-Object { $_ -match '^diag0=' -or $_ -match '^heartbeat ' } | Select-Object -First 1) -eq $null) {
    throw ("Application did not return expected status output. Observed: {0}" -f ($statusLines -join '; '))
}
Write-Host 'PASS app-status'

Write-Step 'Verifying app-to-bootloader transition'
$bootPort = Enter-BootloaderMode -AppPortName $state.ApplicationPort -BootloaderWaitMs $UsbWaitMs
Require-True (-not [string]::IsNullOrWhiteSpace($bootPort)) 'Bootloader CDC port did not appear after BOOTLOADER request.'
Write-Host "PASS bootloader-enumerated port=$bootPort"

Write-Step 'Verifying bootloader reports an application image'
$infoLines = Invoke-BootloaderCommandChecked -PortName $bootPort -Command 'INFO'
Require-True (($infoLines | Where-Object { $_ -match '^INFO app_base=0x[0-9A-FA-F]{8} present=1$' } | Select-Object -First 1) -ne $null) 'Bootloader INFO did not confirm a present application image.'
Write-Host 'PASS bootloader-info'

Write-Step 'Verifying bootloader BOOT returns to the application'
$appPort = Enter-ApplicationMode -BootloaderPortName $bootPort -AppWaitMs $UsbWaitMs
Require-True (-not [string]::IsNullOrWhiteSpace($appPort)) 'Application CDC port did not appear after BOOT.'
Write-Host "PASS boot-command app_port=$appPort"

Write-Step 'Verifying bootloader RESET auto-boots to the application'
$bootPort = Enter-BootloaderMode -AppPortName $appPort -BootloaderWaitMs $UsbWaitMs
Require-True (-not [string]::IsNullOrWhiteSpace($bootPort)) 'Bootloader CDC port did not reappear for RESET auto-boot validation.'
$null = Invoke-BootloaderCommandChecked -PortName $bootPort -Command 'RESET' -AllowDisconnect
$state = Wait-BoardUsbState -Mode application -WaitMs $UsbWaitMs
if ($null -eq $state.ApplicationPort) {
    Get-StartupDiagnostics
    throw 'Application CDC port did not appear after bootloader RESET auto-boot validation.'
}
Write-Host "PASS reset-auto-boot app_port=$($state.ApplicationPort)"

Write-Host 'PASS validate-boot-handoff'