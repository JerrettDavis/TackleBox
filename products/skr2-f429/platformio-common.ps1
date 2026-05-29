function Resolve-ProductPlatformIoCommand {
    if ($env:KEYSWITCH_PLATFORMIO -and (Test-Path $env:KEYSWITCH_PLATFORMIO)) {
        return @{
            Executable = $env:KEYSWITCH_PLATFORMIO
            Arguments = @()
        }
    }

    $localPlatformIo = 'C:\Users\jd\.platformio\penv\Scripts\platformio.exe'
    if (Test-Path $localPlatformIo) {
        return @{
            Executable = $localPlatformIo
            Arguments = @()
        }
    }

    $platformIoCommand = Get-Command platformio.exe -ErrorAction SilentlyContinue
    if ($null -eq $platformIoCommand) {
        $platformIoCommand = Get-Command platformio -ErrorAction SilentlyContinue
    }
    if ($null -ne $platformIoCommand) {
        return @{
            Executable = $platformIoCommand.Source
            Arguments = @()
        }
    }

    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if ($null -ne $pythonCommand) {
        return @{
            Executable = $pythonCommand.Source
            Arguments = @('-m', 'platformio')
        }
    }

    throw 'Unable to locate PlatformIO. Set KEYSWITCH_PLATFORMIO or install platformio.'
}