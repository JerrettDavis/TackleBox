$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$legacyTool = Join-Path $repoRoot 'tools\flash-over-cdc.ps1'

& $legacyTool @args
exit $LASTEXITCODE