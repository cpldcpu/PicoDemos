@echo off
cd /d "%~dp0"
if not exist "pelagic\build_host\pelagic.exe" powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" host
if errorlevel 1 exit /b 1
"pelagic\build_host\pelagic.exe" %*
