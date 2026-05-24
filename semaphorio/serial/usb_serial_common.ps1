$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$legacyTool = Join-Path $repoRoot 'tools\usb_serial_common.ps1'

. $legacyTool