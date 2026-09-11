@echo off
REM Watch PERSISTENCE on the desktop, with sound.
REM
REM The host player is not a preview of the demo -- it is the demo's own
REM kernels, called the way core 1 calls them: beam_frame(f) once, then
REM beam_line(f, px, y) for y = 0..479, in order.
REM
REM Keys: ESC quit, SPACE pause, LEFT/RIGHT seek 5s, R restart, S screenshot,
REM       F fullscreen, L toggle the half-resolution mode.

setlocal
set "HERE=%~dp0"
set "PATH=D:\msys64\ucrt64\bin;%PATH%"

if not exist "%HERE%persistence\host\persistence.exe" (
  echo Building the host player...
  pushd "%HERE%persistence\host" && make -j8 && popd
)

if not exist "%HERE%persistence\host\persistence.exe" (
  echo.
  echo Could not build the host player. It needs MSYS2 UCRT64 with SDL2:
  echo   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2
  echo and D:\msys64\ucrt64\bin on PATH ^(edit this file if yours is elsewhere^).
  pause
  exit /b 1
)

pushd "%HERE%persistence\host"
persistence.exe %*
popd
endlocal
