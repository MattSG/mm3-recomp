@echo off
setlocal
cd /d "%~dp0"
set "MM3_LOG_SHELL=%~dp0tools\bin\powershell\pwsh.exe"
if not exist "%MM3_LOG_SHELL%" set "MM3_LOG_SHELL=powershell.exe"
if exist "%~dp0out\building-diagnostics\RelWithDebInfo\mm3_recomp.exe" (
    "%MM3_LOG_SHELL%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\play.ps1" -TraceBuildings -Exe "%~dp0out\building-diagnostics\RelWithDebInfo\mm3_recomp.exe" %*
) else (
    "%MM3_LOG_SHELL%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\play.ps1" -TraceBuildings %*
)
set "MM3_LAUNCH_EXIT=%ERRORLEVEL%"
if not "%MM3_LAUNCH_EXIT%"=="0" pause
exit /b %MM3_LAUNCH_EXIT%
