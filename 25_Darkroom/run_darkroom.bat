@echo off
cd /d "%~dp0"
if not exist darkroom\build_host\darkroom.exe powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 host
start "Darkroom" darkroom\build_host\darkroom.exe
