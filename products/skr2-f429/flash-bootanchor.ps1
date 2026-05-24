$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $repoRoot

$platformIo = Get-Command python -ErrorAction SilentlyContinue
if ($null -eq $platformIo) {
    throw 'Python is required to run PlatformIO for bootanchor flashing.'
}

& $platformIo.Source '-m' 'platformio' 'run' '-e' 'skr2_f429_bootloader_cdc' '-t' 'upload' @args
exit $LASTEXITCODE