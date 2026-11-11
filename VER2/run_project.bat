@echo off
setlocal
cd /d "%~dp0"

where g++ >nul 2>nul
if errorlevel 1 goto missing_compiler

:menu
cls
echo =========================================
echo   Global Supply Chain System
echo =========================================
echo 1. Start system
echo Q. Quit
choice /c 1Q /n /m "Choose an option: "
if errorlevel 2 goto done
if errorlevel 1 goto integrated

:integrated
tasklist /FI "IMAGENAME eq supply_chain.exe" /NH | find /I "supply_chain.exe" >nul
if not errorlevel 1 goto app_running
echo.
echo Building Global Supply Chain System...
g++ -std=c++11 -Wall -Wextra -pedantic integrated_main.cpp -o supply_chain.exe
if errorlevel 1 goto build_failed
echo Starting Global Supply Chain System...
supply_chain.exe
pause
goto menu

:missing_compiler
echo A C++ compiler was not found. Install MinGW g++ and add it to PATH.
pause
goto done

:build_failed
echo Build failed. Check the compiler messages above.
pause
goto menu

:app_running
echo The system is already running. Close the existing system window before rebuilding.
pause
goto menu

:done
endlocal
