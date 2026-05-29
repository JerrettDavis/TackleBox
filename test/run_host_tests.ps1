$ErrorActionPreference = 'Stop'

function Resolve-GxxCompiler {
    if ($env:KEYSWITCH_GXX -and (Test-Path $env:KEYSWITCH_GXX)) {
        return $env:KEYSWITCH_GXX
    }

    $defaultCompiler = 'C:\msys64\mingw64\bin\g++.exe'
    if (Test-Path $defaultCompiler) {
        return $defaultCompiler
    }

    foreach ($candidate in @('g++.exe', 'g++')) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($null -ne $command) {
            return $command.Source
        }
    }

    throw 'Unable to locate g++. Set KEYSWITCH_GXX or install MSYS2/MinGW.'
}

$compiler = Resolve-GxxCompiler
$root = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $root '.pio\host-tests'

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$commonArgs = @(
    '-std=gnu++17',
    '-I', (Join-Path $root 'include'),
    '-I', (Join-Path $root 'lib\keyswitch_core\include'),
    '-I', (Join-Path $root 'src')
)

$supportInclude = Join-Path $PSScriptRoot 'support\include'

$domainSources = @(
    (Join-Path $root 'lib\keyswitch_core\src\keyswitch_domain.cpp')
)

$protocolSources = @(
    (Join-Path $root 'lib\keyswitch_core\src\keyswitch_protocol.cpp')
)

$tests = @(
    @{
        Name = 'test_bootloader_protocol';
        Sources = @(
            (Join-Path $root 'src\bootloader_protocol.cpp'),
            (Join-Path $PSScriptRoot 'test_bootloader_protocol\test_main.cpp')
        );
    },
    @{
        Name = 'test_protocol';
        Sources = $protocolSources + @(Join-Path $PSScriptRoot 'test_protocol\test_main.cpp');
    },
    @{
        Name = 'test_tmc';
        Sources = $protocolSources + @(
            (Join-Path $root 'lib\keyswitch_core\src\keyswitch_tmc2209.cpp'),
            (Join-Path $PSScriptRoot 'test_tmc\test_main.cpp')
        );
    },
    @{
        Name = 'test_load_cell';
        Sources = @(
            (Join-Path $root 'src\load_cell.cpp'),
            (Join-Path $PSScriptRoot 'test_load_cell\test_main.cpp')
        );
    },
    @{
        Name = 'test_app_runtime_config';
        Sources = $protocolSources + @(
            (Join-Path $root 'src\load_cell.cpp'),
            (Join-Path $root 'src\app_runtime_config.cpp'),
            (Join-Path $PSScriptRoot 'support\app_runtime_config_host_stubs.cpp'),
            (Join-Path $PSScriptRoot 'test_app_runtime_config\test_main.cpp')
        );
        ExtraArgs = @(
            '-DKEYSWITCH_HOST_TEST',
            '-I', $supportInclude
        );
    },
    @{
        Name = 'test_domain';
        Sources = $domainSources + @(Join-Path $PSScriptRoot 'test_domain\test_main.cpp');
    },
    @{
        Name = 'test_integration';
        Sources = $domainSources + $protocolSources + @(Join-Path $PSScriptRoot 'test_integration\test_main.cpp');
    },
    @{
        Name = 'test_probe_workflow';
        Sources = $domainSources + $protocolSources + @(
            (Join-Path $root 'src\load_cell.cpp'),
            (Join-Path $PSScriptRoot 'test_probe_workflow\test_main.cpp')
        );
    }
)

foreach ($test in $tests) {
    $exePath = Join-Path $outDir ($test.Name + '.exe')
    $args = @($commonArgs)
    if ($test.ContainsKey('ExtraArgs')) {
        $args += $test.ExtraArgs
    }

    & $compiler @args @($test.Sources) '-o' $exePath
    if ($LASTEXITCODE -ne 0) {
        throw "Compile failed for $($test.Name)"
    }

    & $exePath
    if ($LASTEXITCODE -ne 0) {
        throw "Execution failed for $($test.Name)"
    }
}

Write-Host 'PASS host tests'