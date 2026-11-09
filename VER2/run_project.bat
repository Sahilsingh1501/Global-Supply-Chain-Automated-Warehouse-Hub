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
echo 2. Run automated test suite
echo Q. Quit
choice /c 12Q /n /m "Choose an option: "
if errorlevel 3 goto done
if errorlevel 2 goto test_suite
if errorlevel 1 goto integrated

:test_suite
echo.
echo Building and running test suite...
g++ -std=c++11 -Wall -Wextra -pedantic run_tests.cpp -o run_tests.exe
if errorlevel 1 goto build_failed
run_tests.exe
pause
goto menu

:integrated
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

:done
endlocal
