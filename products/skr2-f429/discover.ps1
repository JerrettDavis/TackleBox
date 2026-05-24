$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tool = Join-Path $repoRoot 'spyglass\operator\board-usb.ps1'

& $tool -Action discover @args
exit $LASTEXITCODE