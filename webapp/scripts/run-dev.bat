@echo off
setlocal
if not defined VITE_PORT set "VITE_PORT=5173"
pushd "%~dp0.."
call npm.cmd run dev
set "RUN_EXIT_CODE=%ERRORLEVEL%"
popd
if not "%RUN_EXIT_CODE%"=="0" pause
exit /b %RUN_EXIT_CODE%
