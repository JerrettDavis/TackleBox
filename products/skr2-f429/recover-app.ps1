$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $repoRoot

. (Join-Path $PSScriptRoot 'platformio-common.ps1')

$platformIo = Resolve-ProductPlatformIoCommand

& $platformIo.Executable @($platformIo.Arguments) 'run' '-e' 'skr2_f429_usb' '-t' 'upload' @args
exit $LASTEXITCODE