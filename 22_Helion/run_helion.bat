@echo off
cd /d "%~dp0"
if not exist "helion\build_host\helion.exe" powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" host
if errorlevel 1 exit /b 1
"helion\build_host\helion.exe" %*
