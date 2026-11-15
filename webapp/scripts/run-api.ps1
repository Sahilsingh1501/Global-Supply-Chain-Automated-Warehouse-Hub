$ErrorActionPreference = "Stop"
$webappRoot = Split-Path -Parent $PSScriptRoot
$projectRoot = Split-Path -Parent $webappRoot
$apiBinary = Join-Path $webappRoot "server\gsc-server.exe"

if (-not (Test-Path $apiBinary -PathType Leaf)) {
    throw "C++ API executable not found. Run RUN_PROJECT.bat to build the API first."
}

$env:GSC_WEB_ROOT = ""
Set-Location (Join-Path $projectRoot "VER2")
try {
    & $apiBinary
} catch {
    if ($_.Exception.Message -match "Application Control policy has blocked this file") {
        [Console]::Error.WriteLine("ERROR: Windows application control blocked the locally built API executable.")
        [Console]::Error.WriteLine("Use an organization-approved signed build or ask your administrator to allow '$apiBinary'. Do not disable application control.")
        exit 1
    }
    throw
}
exit $LASTEXITCODE
