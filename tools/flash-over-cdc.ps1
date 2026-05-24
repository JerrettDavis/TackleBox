param(
    [string]$PortName,
    [string]$AppPortName,
    [string]$FirmwarePath = '.\.pio\build\skr2_f429_usb\firmware.bin',
    [int]$ChunkBytes = 32,
    [switch]$EnterBootloader,
    [switch]$SkipBuild,
    [switch]$SkipBoot
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'usb_serial_common.ps1')

function Get-Crc32 {
    param([byte[]]$Bytes)

    [uint64]$crc = 4294967295
    foreach ($byte in $Bytes) {
        $crc = ($crc -bxor [uint64]$byte) -band 4294967295
        for ($bit = 0; $bit -lt 8; $bit++) {
            if (($crc -band 1) -ne 0) {
                $crc = ((($crc -shr 1) -bxor 3988292384) -band 4294967295)
            }
            else {
                $crc = (($crc -shr 1) -band 4294967295)
            }
        }
    }
    return [uint32]((4294967295 -bxor $crc) -band 4294967295)
}


$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (-not $SkipBuild) {
    $platformIo = Resolve-PlatformIoCommand
    & $platformIo.Executable @($platformIo.Arguments) 'run' '-e' 'skr2_f429_usb'
    if ($LASTEXITCODE -ne 0) {
        throw 'Application firmware build failed.'
    }
}

$resolvedFirmwarePath = (Resolve-Path $FirmwarePath).Path
[byte[]]$firmwareBytes = [System.IO.File]::ReadAllBytes($resolvedFirmwarePath)
$expectedCrc = Get-Crc32 -Bytes $firmwareBytes

$resolvedBootloaderPort = $PortName
if (-not $resolvedBootloaderPort) {
    $resolvedBootloaderPort = Get-UsbSerialPortByVidPid -VidPidPattern $script:KeyswitchUsbIds.Bootloader
}

if ((-not $resolvedBootloaderPort) -and $EnterBootloader) {
    $resolvedBootloaderPort = Enter-BootloaderMode -AppPortName $AppPortName
}

if (-not $resolvedBootloaderPort) {
    throw 'Bootloader CDC port not found. Pass -PortName or use -EnterBootloader to switch from the running app.'
}

$port = Open-UsbSerialPort -ResolvedPort $resolvedBootloaderPort -ReadTimeoutMs 200 -WriteTimeoutMs 2000

try {
    $port.DiscardInBuffer()
    $port.DiscardOutBuffer()

    $info = Invoke-SerialCommand -Port $port -Command 'INFO'
    if (-not ($info -match 'INFO app_base=')) {
        throw "Bootloader did not return INFO. Observed: $($info -join '; ')"
    }

    $erase = Invoke-SerialCommand -Port $port -Command ("ERASE {0}" -f $firmwareBytes.Length)
    if (-not ($erase -match 'OK ERASE')) {
        throw "Erase failed. Observed: $($erase -join '; ')"
    }

    for ($offset = 0; $offset -lt $firmwareBytes.Length; $offset += $ChunkBytes) {
        $count = [Math]::Min($ChunkBytes, $firmwareBytes.Length - $offset)
        $chunk = $firmwareBytes[$offset..($offset + $count - 1)]
        $hex = ($chunk | ForEach-Object { $_.ToString('X2') }) -join ''
        $response = Invoke-SerialCommand -Port $port -Command ("WRITE {0} {1}" -f $offset, $hex)
        if (-not ($response -match 'OK WRITE')) {
            throw "Write failed at offset $offset. Observed: $($response -join '; ')"
        }
    }

    $crcLines = Invoke-SerialCommand -Port $port -Command ("CRC {0}" -f $firmwareBytes.Length)
    $crcLine = $crcLines | Where-Object { $_ -match '^CRC 0x[0-9A-Fa-f]{8} size=' } | Select-Object -First 1
    if (-not $crcLine) {
        throw "CRC response missing. Observed: $($crcLines -join '; ')"
    }

    $deviceCrc = [uint32]::Parse(($crcLine -replace '^CRC 0x', '' -replace ' size=.*$', ''), [System.Globalization.NumberStyles]::HexNumber)
    if ($deviceCrc -ne $expectedCrc) {
        throw ("CRC mismatch. Device=0x{0:X8} Host=0x{1:X8}" -f $deviceCrc, $expectedCrc)
    }

    Write-Host ("PASS CDC flash CRC=0x{0:X8} bytes={1}" -f $deviceCrc, $firmwareBytes.Length)

    if (-not $SkipBoot) {
        $bootLines = Invoke-SerialCommand -Port $port -Command 'BOOT' -AllowDisconnect
        if ($bootLines.Count -gt 0) {
            Write-Host ($bootLines -join [Environment]::NewLine)
        }

        $applicationPort = Resolve-AppPort -WaitMs 8000
        Write-Host "Application enumerated on $applicationPort"
    }
}
finally {
    if ($port.IsOpen) {
        $port.Close()
    }
}