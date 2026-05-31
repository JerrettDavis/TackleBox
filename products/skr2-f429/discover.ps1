$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tool = Join-Path $repoRoot 'spyglass\operator\board-usb.ps1'

try {
	& $tool -Action discover @args
	exit 0
}
catch {
	throw
}