$ErrorActionPreference = "Stop"
$webappRoot = Split-Path -Parent $PSScriptRoot
$projectRoot = Split-Path -Parent $webappRoot
$apiBinary = Join-Path $webappRoot "server\gsc-server.exe"
$frontendRoot = Join-Path $webappRoot "dist"

if (-not (Test-Path $apiBinary -PathType Leaf)) {
    throw "Native API executable not found. Run 'npm run build' first."
}
if (-not (Test-Path (Join-Path $frontendRoot "index.html") -PathType Leaf)) {
    throw "Built frontend not found. Run 'npm run build' first."
}

$env:GSC_WEB_ROOT = $frontendRoot
Set-Location (Join-Path $projectRoot "VER2")
& $apiBinary
exit $LASTEXITCODE
