param(
    [string]$AppPortName,
    [switch]$SkipUsbFlash,
    [switch]$IncludeRecovery,
    [switch]$SkipRecovery
)

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $repoRoot

$appBuildTool = Join-Path $PSScriptRoot 'build-app.ps1'
& $appBuildTool
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$bootBuildTool = Join-Path $PSScriptRoot 'build-bootanchor.ps1'
& $bootBuildTool
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not $SkipUsbFlash) {
    $firmwarePath = Join-Path $repoRoot '.pio\build\skr2_f429_usb\firmware.bin'
    $appFlashTool = Join-Path $PSScriptRoot 'flash-app.ps1'
    & $appFlashTool -EnterBootloader -FirmwarePath $firmwarePath -SkipBuild -AppPortName $AppPortName
        if ($LASTEXITCODE -ne 0) {
		exit $LASTEXITCODE
    }
}

$bootValidateTool = Join-Path $PSScriptRoot 'validate-bootanchor.ps1'
& $bootValidateTool -AppPortName $AppPortName
if ($LASTEXITCODE -ne 0) {
	exit $LASTEXITCODE
}

$runtimeValidateTool = Join-Path $PSScriptRoot 'validate-runtime.ps1'
if ($AppPortName) {
	& $runtimeValidateTool -PortName $AppPortName
}
else {
	& $runtimeValidateTool
}
if ($LASTEXITCODE -ne 0) {
	exit $LASTEXITCODE
}

if ($IncludeRecovery -and (-not $SkipRecovery)) {
    $recoverTool = Join-Path $PSScriptRoot 'recover-app.ps1'
    & $recoverTool
        if ($LASTEXITCODE -ne 0) {
		exit $LASTEXITCODE
    }
}

exit 0