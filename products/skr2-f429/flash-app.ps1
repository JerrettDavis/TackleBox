param(
	[string]$PortName,
	[string]$AppPortName,
	[string]$FirmwarePath = '.\.pio\build\skr2_f429_usb\firmware.bin',
	[int]$ChunkBytes = 32,
	[switch]$EnterBootloader,
	[switch]$SkipBuild,
	[switch]$SkipBoot
)

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tool = Join-Path $repoRoot 'semaphorio\boot\flash-over-cdc.ps1'

& $tool -PortName $PortName -AppPortName $AppPortName -FirmwarePath $FirmwarePath -ChunkBytes $ChunkBytes -EnterBootloader:$EnterBootloader -SkipBuild:$SkipBuild -SkipBoot:$SkipBoot
exit $LASTEXITCODE