$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$legacyTool = Join-Path $repoRoot 'tools\boot-config-service\boot_config_service.ps1'

& $legacyTool @args
exit $LASTEXITCODE