$ErrorActionPreference = "Stop"
$webappRoot = Split-Path -Parent $PSScriptRoot

Push-Location $webappRoot
try {
    if ($env:GSC_API_BUILT -ne "1") {
        & npm.cmd run build:api
        if ($LASTEXITCODE -ne 0) {
            throw "The C++ API could not be built. Install MSVC or MinGW-w64 with POSIX threads, then run RUN_PROJECT.bat again."
        }
    }

    & npm.cmd run dev:services
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
} finally {
    Pop-Location
}
