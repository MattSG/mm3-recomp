@echo off
setlocal
cd /d "%~dp0"
if exist "%~dp0out\building-diagnostics\RelWithDebInfo\mm3_recomp.exe" (
    python "%~dp0scripts\play.py" --trace-buildings --exe "%~dp0out\building-diagnostics\RelWithDebInfo\mm3_recomp.exe" %*
) else (
    python "%~dp0scripts\play.py" --trace-buildings %*
)
set "MM3_LAUNCH_EXIT=%ERRORLEVEL%"
if not "%MM3_LAUNCH_EXIT%"=="0" pause
exit /b %MM3_LAUNCH_EXIT%
