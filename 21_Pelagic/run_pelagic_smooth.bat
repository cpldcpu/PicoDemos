@echo off
cd /d "%~dp0"
if not exist "pelagic\build_host_smooth\pelagic.exe" powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" host -Smooth
if errorlevel 1 exit /b 1
"pelagic\build_host_smooth\pelagic.exe" %*
