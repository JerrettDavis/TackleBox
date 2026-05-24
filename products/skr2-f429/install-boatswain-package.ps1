$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tool = Join-Path $repoRoot 'semaphorio\boatswain\install-package.ps1'

& $tool @args
exit $LASTEXITCODE