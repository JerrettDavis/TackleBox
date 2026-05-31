$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tool = Join-Path $repoRoot 'test\run_bootloader_serial_tests.ps1'

try {
	& $tool @args
	exit 0
}
catch {
	throw
}