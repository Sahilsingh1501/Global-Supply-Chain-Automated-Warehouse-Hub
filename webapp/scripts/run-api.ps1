$ErrorActionPreference = "Stop"
$webappRoot = Split-Path -Parent $PSScriptRoot
$projectRoot = Split-Path -Parent $webappRoot
$apiBinary = Join-Path $webappRoot "server\gsc-server.exe"

if (-not (Test-Path $apiBinary -PathType Leaf)) {
    throw "C++ API executable not found. Run RUN_PROJECT.bat to build the API first."
}

$env:GSC_WEB_ROOT = ""
Set-Location (Join-Path $projectRoot "VER2")
& $apiBinary
exit $LASTEXITCODE
