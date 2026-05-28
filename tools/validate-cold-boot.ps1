param(
    [int]$DisconnectWaitMs = 15000,
    [int]$ReconnectWaitMs = 20000,
    [switch]$CheckStatus
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'usb_serial_common.ps1')

function Write-Step {
    param([string]$Message)
    Write-Host "==> $Message"
}

function Format-UsbState {
    param($State)

    return ("application={0} bootloader={1}" -f $(if ($State.ApplicationPort) { $State.ApplicationPort } else { 'none' }), $(if ($State.BootloaderPort) { $State.BootloaderPort } else { 'none' }))
}

function Invoke-AppStatusCheck {
    param([string]$PortName)

    $serial = $null
    try {
        $serial = Open-UsbSerialPort -ResolvedPort $PortName
        $null = Read-SerialLinesUntilIdle -Port $serial -MaxMs 2500
        return @(Invoke-SerialCommand -Port $serial -Command 'status' -ExpectedPatterns @())
    }
    finally {
        if ($serial -and $serial.IsOpen) {
            $serial.Close()
        }
    }
}

$initialState = Get-BoardUsbState
Write-Host ("START usb {0}" -f (Format-UsbState -State $initialState))
Write-Host 'ACTION remove all board power now: disconnect ST-Link and also remove the board USB/data power path or any external PSU that keeps the MCU alive, then restore power while this script is waiting.'

Write-Step 'Waiting for the board USB device to disappear'
$absentState = Wait-BoardUsbAbsent -WaitMs $DisconnectWaitMs
if ($absentState.ApplicationPort -or $absentState.BootloaderPort) {
    throw (("Board USB did not disappear within the timeout. Current state: {0}. " +
        'This means the board never lost host-visible USB power; unplug the board USB cable or otherwise remove all MCU power, then retry.') -f (Format-UsbState -State $absentState))
}
Write-Host 'PASS usb-disappeared'

Write-Step 'Waiting for the board USB device to return'
$returnState = Wait-BoardUsbState -Mode any -WaitMs $ReconnectWaitMs
if ((-not $returnState.ApplicationPort) -and (-not $returnState.BootloaderPort)) {
    throw 'Board USB did not return within the timeout.'
}

Write-Host ("RESULT usb {0}" -f (Format-UsbState -State $returnState))

if ($returnState.BootloaderPort) {
    throw ("Cold boot failed: board returned as bootloader on {0}." -f $returnState.BootloaderPort)
}

if (-not $returnState.ApplicationPort) {
    throw 'Cold boot failed: board returned without an application CDC port.'
}

Write-Host ("PASS cold-boot app_port={0}" -f $returnState.ApplicationPort)

if ($CheckStatus) {
    Write-Step 'Verifying application status after cold boot'
    $statusLines = Invoke-AppStatusCheck -PortName $returnState.ApplicationPort
    if (($statusLines | Where-Object { $_ -match '^diag0=' -or $_ -match '^heartbeat ' } | Select-Object -First 1) -eq $null) {
        throw ("Application status check failed after cold boot. Observed: {0}" -f ($statusLines -join '; '))
    }
    Write-Host 'PASS cold-boot-status'
}