@echo off
cd /d "%~dp0"
if not exist "tessera\build_host\tessera.exe" powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" host
if errorlevel 1 exit /b 1
"tessera\build_host\tessera.exe" %*
