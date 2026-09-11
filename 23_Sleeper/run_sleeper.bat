@echo off
rem SLEEPER / LATENT / 2026 -- the desktop player.
rem
rem The renderer and the score are the same C the RP2350 runs; SDL2 supplies a
rem window and an audio device and nothing else. 320x240, doubled, 60 fps.
rem
rem   space  pause      f  fullscreen      left/right  seek 15 s
rem   r      restart    esc  quit
rem
setlocal
cd /d "%~dp0"
if not exist "sleeper\build_host\sleeper.exe" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" host
    if errorlevel 1 exit /b 1
)
start "" "sleeper\build_host\sleeper.exe" %*
