param(
    [int]$Threshold = 1000,
    [int]$PressTargetSteps = 40,
    [int]$CycleCount = 3,
    [string]$ExportPath = ''
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ExportPath)) {
    $ExportPath = Join-Path $root 'tools\arm-dashboard\simulated-probe-curve.json'
}

Set-Location $root

& (Join-Path $PSScriptRoot 'run_host_tests.ps1')
if ($LASTEXITCODE -ne 0) {
    throw 'Host tests failed before simulated probe export.'
}

$exePath = Join-Path $root '.pio\host-tests\test_probe_workflow.exe'
if (-not (Test-Path $exePath)) {
    throw "Probe workflow host executable was not found at '$exePath'."
}

& $exePath --export $ExportPath --threshold $Threshold --press-target $PressTargetSteps --cycle-count $CycleCount
if ($LASTEXITCODE -ne 0) {
    throw 'Simulated probe workflow export failed.'
}

Write-Host ("PASS simulated probe workflow export -> {0}" -f $ExportPath)