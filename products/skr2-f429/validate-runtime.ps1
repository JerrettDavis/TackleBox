$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tool = Join-Path $repoRoot 'test\run_live_serial_tests.ps1'

& $tool @args
exit $LASTEXITCODE