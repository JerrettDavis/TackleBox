$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tool = Join-Path $repoRoot 'semaphorio\boot\flash-over-cdc.ps1'

& $tool @args
exit $LASTEXITCODE