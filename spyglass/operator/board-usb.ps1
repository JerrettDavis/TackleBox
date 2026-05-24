$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$legacyTool = Join-Path $repoRoot 'tools\board-usb.ps1'

& $legacyTool @args
exit $LASTEXITCODE