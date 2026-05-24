$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tool = Join-Path $repoRoot 'test\run_ci_checks.ps1'

& $tool @args
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$bootTool = Join-Path $PSScriptRoot 'validate-bootanchor.ps1'
& $bootTool
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$runtimeTool = Join-Path $PSScriptRoot 'validate-runtime.ps1'
& $runtimeTool
exit $LASTEXITCODE