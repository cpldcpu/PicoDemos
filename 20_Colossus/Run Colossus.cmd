@echo off
rem COLOSSUS / LATENT / 2026 -- the desktop player.
rem
rem Builds the host player if it is not already there, then launches it. The
rem player runs the same renderer and the same synth as the RP2350 firmware;
rem it is not a preview of the demo, it is the demo with a different scanout.
rem
rem   space        pause
rem   left/right   seek one phrase (8 bars, 15.36 s)
rem   , / .        seek one bar
rem   0..9         jump to phrase 1..10, shift for 11..20
rem   home         back to the start
rem   s            screenshot, PNG, native size
rem   n / g        native size window / 3x
rem   f            fullscreen
rem   esc          quit
rem
rem The firmware is colossus_vga_rp2350.uf2 beside this file: hold BOOTSEL,
rem plug the Pico 2 in, and copy it across.

setlocal
cd /d "%~dp0"

if not exist "colossus\build_host\player.exe" (
    echo Building the host player...
    powershell -NoProfile -ExecutionPolicy Bypass -File "build.ps1" host
    if errorlevel 1 (
        echo.
        echo Build failed. It needs gcc, cmake and SDL2 on PATH.
        pause
        exit /b 1
    )
)

start "" "colossus\build_host\player.exe" %*
endlocal
