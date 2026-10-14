@echo off
setlocal
set "editor_python=%~dp0..\venv\Scripts\pythonw.exe"
if exist "%editor_python%" (
    "%editor_python%" "%~dp0editor.py" %*
    exit /b
)
set "editor_python=%~dp0..\venv\Scripts\python.exe"
if exist "%editor_python%" (
    "%editor_python%" "%~dp0editor.py" %*
    exit /b
)
py -3 "%~dp0editor.py" %*
if errorlevel 1 pause
