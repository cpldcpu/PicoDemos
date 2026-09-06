@echo off
if not exist "%~dp0vesper\build_host\vesper.exe" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" -Target host
    if errorlevel 1 exit /b 1
)
"%~dp0vesper\build_host\vesper.exe" %*
