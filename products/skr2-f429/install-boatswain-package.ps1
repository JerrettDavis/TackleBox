$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tool = Join-Path $repoRoot 'semaphorio\boatswain\install-package.ps1'

try {
	& $tool @args
	exit 0
}
catch {
	throw
}