param([string]$Toolset = '', [int]$StressSeconds = 60)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild/**/Bin/MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'Visual Studio C++ build tools were not found' }
$properties = @('/p:Configuration=Release', '/p:Platform=Win32', '/p:WindowsTargetPlatformVersion=10.0', '/clp:ErrorsOnly;Summary', '/nologo')
if ($Toolset) { $properties += "/p:PlatformToolset=$Toolset" }
Push-Location $root
try {
    foreach ($project in @('client/src/RakSAMPClient_2008.vcxproj','client/tests/CommandTrackingTests.vcxproj','client/tests/ChatEncodingTests.vcxproj','client/tests/ClientSafetyTests.vcxproj','client/tests/RuntimeReplayTests.vcxproj','client/tests/RuntimeStressTests.vcxproj')) {
        & $msbuild $project @properties
        if ($LASTEXITCODE -ne 0) { throw "Build failed: $project" }
    }
    foreach ($test in @('client/tests/obj/CommandTrackingTests.exe','client/tests/obj/chat-encoding/ChatEncodingTests.exe','client/tests/obj/safety/ClientSafetyTests.exe','client/tests/obj/runtime-replay/RuntimeReplayTests.exe')) {
        & (Join-Path $root $test)
        if ($LASTEXITCODE -ne 0) { throw "Test failed: $test" }
    }
    foreach ($fixture in @('cold_backend.json','oasis_native.json')) {
        $path = 'client/tests/fixtures/' + $fixture
        if (Test-Path -LiteralPath $path) {
            & (Join-Path $root 'client/tests/obj/runtime-replay/RuntimeReplayTests.exe') $path
            if ($LASTEXITCODE -ne 0) { throw "Replay failed: $fixture" }
        }
    }
    if ($StressSeconds -gt 0) {
        & (Join-Path $root 'client/tests/obj/stress/RuntimeStressTests.exe') $StressSeconds
        if ($LASTEXITCODE -ne 0) { throw 'Runtime stress test failed' }
    }
    $artifacts = Join-Path $PSScriptRoot 'obj/ci-artifacts'
    [IO.Directory]::CreateDirectory($artifacts) | Out-Null
    [IO.File]::Copy((Join-Path $root 'client/bin/RakSAMPClient.exe'), (Join-Path $artifacts 'RakSAMPClient.exe'), $true)
    [IO.File]::Copy((Join-Path $root 'client/INSTALL-AND-ROLLBACK.md'), (Join-Path $artifacts 'INSTALL-AND-ROLLBACK.md'), $true)
    [IO.File]::Copy((Join-Path $root 'client/tests/Create-DiagnosticReport.ps1'), (Join-Path $artifacts 'Create-DiagnosticReport.ps1'), $true)
    $digest = (Get-FileHash -LiteralPath (Join-Path $artifacts 'RakSAMPClient.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText((Join-Path $artifacts 'SHA256SUMS.txt'), $digest + '  RakSAMPClient.exe' + "`n", (New-Object Text.UTF8Encoding($false)))
} finally { Pop-Location }
