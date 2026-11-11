@echo off
setlocal
title Northstar Supply Chain Hub

set "WEBAPP_DIR=%~dp0webapp"
if not exist "%WEBAPP_DIR%\package.json" (
    echo ERROR: Could not find the webapp folder next to this run file.
    echo Keep RUN_PROJECT.bat inside the Global-Supply-Chain-Automated-Warehouse-Hub folder.
    pause
    exit /b 1
)

where node >nul 2>nul
if errorlevel 1 (
    echo ERROR: Node.js was not found. Install Node.js 20 or later, then try again.
    pause
    exit /b 1
)

where npm >nul 2>nul
if errorlevel 1 (
    echo ERROR: npm was not found. Reinstall Node.js with npm included, then try again.
    pause
    exit /b 1
)

if not exist "%WEBAPP_DIR%\node_modules\.bin\vite.cmd" (
    echo First run: installing website packages...
    pushd "%WEBAPP_DIR%"
    call npm.cmd install
    if errorlevel 1 (
        popd
        echo ERROR: Package installation failed. Check your internet connection and try again.
        pause
        exit /b 1
    )
    popd
)

for /f %%P in ('powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$listener = $null; foreach ($port in 5173..5199) { try { $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $port); $listener.Start(); $listener.Stop(); Write-Output $port; exit 0 } catch { if ($listener) { $listener.Stop() } } }; exit 1"') do set "VITE_PORT=%%P"
if not defined VITE_PORT (
    echo ERROR: Could not find a free website port between 5173 and 5199.
    pause
    exit /b 1
)

echo Checking that the C++ backend can be built...
pushd "%WEBAPP_DIR%"
call npm.cmd run build:api
set "BUILD_EXIT_CODE=%ERRORLEVEL%"
popd
if not "%BUILD_EXIT_CODE%"=="0" (
    echo.
    echo The C++ backend did not build, so the website was not started.
    echo Open Visual Studio Installer, choose Modify, and install:
    echo Desktop development with C++ including MSVC x64/x86 tools and a Windows SDK.
    echo Then double-click RUN_PROJECT.bat again.
    pause
    exit /b %BUILD_EXIT_CODE%
)

set "GSC_API_BUILT=1"
echo Starting the C++ backend and website...
echo A second window will show backend and website startup messages.
echo Please wait while the website starts...
echo Website port: %VITE_PORT%
start "Northstar Supply Chain Hub - Server" /D "%WEBAPP_DIR%" "%WEBAPP_DIR%\scripts\run-dev.bat"

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$port = [int]$env:VITE_PORT; $deadline = (Get-Date).AddSeconds(40); while ((Get-Date) -lt $deadline) { $client = New-Object System.Net.Sockets.TcpClient; try { $client.Connect('127.0.0.1', $port); $client.Close(); exit 0 } catch { Start-Sleep -Milliseconds 500 } }; exit 1"
if errorlevel 1 (
    echo.
    echo The website did not start at http://localhost:%VITE_PORT% within 40 seconds.
    echo Check the second window for startup errors.
    pause
    exit /b 1
)

echo Website is ready. Opening http://localhost:%VITE_PORT%
start "" http://localhost:%VITE_PORT%
echo.
echo Keep the second window open while using the app.
echo If the API reports a C++ compiler error, install MSVC or MinGW-w64 POSIX threads.
pause
