$ErrorActionPreference = "Continue"
$webappRoot = Split-Path -Parent $PSScriptRoot
$output = Join-Path $webappRoot "server\gsc-server.exe"
$compiler = $env:CXX

function Import-MsvcEnvironment([string]$devCommand) {
    $setOutput = & $env:ComSpec /d /c "call `"$devCommand`" -arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "Could not initialize the Visual Studio C++ environment."
    }
    foreach ($line in $setOutput) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2]
        }
    }
}

if (-not $compiler -and (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $compiler = "cl.exe"
}

$visualStudioInstalled = $false
$visualStudioIncomplete = $false
if (-not $compiler) {
    $vswhere = Get-Command vswhere.exe -ErrorAction SilentlyContinue
    if (-not $vswhere) {
        $vswhereCandidates = @(
            "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
            "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe"
        )
        foreach ($candidate in $vswhereCandidates) {
            if (Test-Path $candidate -PathType Leaf) {
                $vswhere = $candidate
                break
            }
        }
    }
    if ($vswhere) {
        $vswherePath = if ($vswhere.Source) { $vswhere.Source } else { $vswhere }
        $installPath = & $vswherePath -latest -products "*" -property installationPath
        if (-not $installPath) {
            $instances = & $vswherePath -all -products "*" -format json | ConvertFrom-Json
            $partialInstallation = $instances |
                Where-Object { $_.installationPath } |
                Select-Object -First 1
            if ($partialInstallation) {
                $installPath = $partialInstallation.installationPath
                $visualStudioIncomplete = -not $partialInstallation.isComplete
            }
        }
    }
    if ($installPath) {
        $visualStudioInstalled = $true
        $devCommand = Join-Path $installPath "Common7\Tools\VsDevCmd.bat"
        if (-not $visualStudioIncomplete -and (Test-Path $devCommand -PathType Leaf)) {
            Import-MsvcEnvironment $devCommand
            if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
                $compiler = "cl.exe"
            }
        }
    }
}

if (-not $compiler -and $visualStudioInstalled) {
    if ($visualStudioIncomplete) {
        throw "Visual Studio Build Tools installation is incomplete or was cancelled. Open Visual Studio Installer, choose Visual Studio Build Tools 2026 > Resume or Modify, select 'Desktop development with C++' (including MSVC x64/x86 build tools and a Windows SDK), and let installation finish. Then run RUN_PROJECT.bat again."
    }
    throw "Visual Studio is installed, but its C++ compiler is missing. Open Visual Studio Installer, choose Modify, select 'Desktop development with C++' (including MSVC x64/x86 build tools and a Windows SDK), install it, then run RUN_PROJECT.bat again."
}

if (-not $compiler) {
    if (Get-Command g++ -ErrorAction SilentlyContinue) {
        $compiler = "g++"
    } else {
        throw "Install the Visual Studio C++ build tools or MinGW-w64 with POSIX threads."
    }
}

Push-Location $webappRoot
try {
    $compilerName = Split-Path $compiler -Leaf
    if ($compilerName -match "^cl(\.exe)?$") {
        & $compiler /nologo /EHsc /std:c++14 /W4 "/Fe:$output" server\main.cpp ws2_32.lib
    } else {
        $compilerInfo = & $compiler -v 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Could not run C++ compiler '$compiler'. Check the CXX setting and compiler installation."
        }
        if ($compilerInfo | Select-String -Pattern "Thread model:\s*win32") {
            throw "This GCC uses the win32 thread model, which cannot build the API. Install MinGW-w64 with POSIX threads, or run from a Visual Studio Developer PowerShell with MSVC available."
        }
        & $compiler -std=c++11 -pthread -Wall -Wextra -pedantic server\main.cpp -o $output -lws2_32
    }
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
} finally {
    Pop-Location
}
