param(
    [ValidateSet('host','pico','check','capture','all')][string]$Target = 'host',
    [string]$SdkPath = $env:PICO_SDK_PATH,
    [string]$ExtrasPath = $env:PICO_EXTRAS_PATH
)
$ErrorActionPreference = 'Stop'
$darkroomSource = Join-Path $PSScriptRoot 'darkroom'
$darkroomHost = Join-Path $darkroomSource 'build_host'
$darkroomPico = Join-Path $darkroomSource 'build_rp2350'
# cmake, gcc and ctest write progress and warnings to stderr, and with
# $ErrorActionPreference = 'Stop' PowerShell turns a native command's stderr
# into a terminating NativeCommandError -- so cmake's own "PICO_SDK_PATH is
# ..." banner fails the build, and a real failure gets reported as whatever
# was written to stderr first. The exit code is the only thing that decides.
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & $Program @Arguments } finally { $ErrorActionPreference = $previous }
    if ($LASTEXITCODE -ne 0) { throw "$Program exited with $LASTEXITCODE" }
}
$darkroomCompiler = (Get-Command gcc -ErrorAction Stop).Source
$darkroomToolBin = Split-Path $darkroomCompiler
$darkroomPrefix = Split-Path $darkroomToolBin
$darkroomMake = (Get-Command mingw32-make -ErrorAction Stop).Source
if ($Target -in @('host','check','capture','all')) {
    Invoke-Checked 'cmake' @('-S',$darkroomSource,'-B',$darkroomHost,'-G','MinGW Makefiles','-DDARKROOM_HOST=ON',"-DCMAKE_PREFIX_PATH=$darkroomPrefix","-DCMAKE_MAKE_PROGRAM=$darkroomMake")
    Invoke-Checked 'cmake' @('--build',$darkroomHost,'-j','8')
    # Include the runtime DLLs so launching from Explorer does not depend on PATH.
    foreach ($darkroomDll in @('SDL2.dll','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
        $darkroomDllPath = Join-Path $darkroomToolBin $darkroomDll
        if (Test-Path -LiteralPath $darkroomDllPath) { Copy-Item -LiteralPath $darkroomDllPath -Destination $darkroomHost }
    }
}
if ($Target -in @('check','all')) {
    Push-Location $darkroomHost
    try { Invoke-Checked 'ctest' @('--output-on-failure') } finally { Pop-Location }
}
if ($Target -in @('pico','all')) {
    if (-not $SdkPath -or -not (Test-Path -LiteralPath $SdkPath)) {
        if (Test-Path -LiteralPath 'D:/Pico/pico-sdk') { $SdkPath = 'D:/Pico/pico-sdk' }
        else { throw 'Pass -SdkPath pointing to pico-sdk.' }
    }
    if (-not $ExtrasPath -or -not (Test-Path -LiteralPath $ExtrasPath)) {
        if (Test-Path -LiteralPath 'D:/Pico/pico-extras') { $ExtrasPath = 'D:/Pico/pico-extras' }
        else { throw 'Pass -ExtrasPath pointing to pico-extras.' }
    }
    $SdkPath = (Resolve-Path -LiteralPath $SdkPath).Path
    $ExtrasPath = (Resolve-Path -LiteralPath $ExtrasPath).Path
    Invoke-Checked 'cmake' @('-S',$darkroomSource,'-B',$darkroomPico,'-G','MinGW Makefiles',"-DPICO_SDK_PATH=$SdkPath","-DPICO_EXTRAS_PATH=$ExtrasPath","-DCMAKE_MAKE_PROGRAM=$darkroomMake")
    Invoke-Checked 'cmake' @('--build',$darkroomPico,'-j','8')
    Copy-Item -LiteralPath (Join-Path $darkroomPico 'darkroom.uf2') -Destination (Join-Path $PSScriptRoot 'darkroom_vga_rp2350.uf2') -Force
}
if ($Target -in @('capture','all')) {
    Invoke-Checked 'python' @((Join-Path $darkroomSource 'tools/capture.py'),
        '--exe',(Join-Path $darkroomHost 'darkroom.exe'),
        '--out',(Join-Path $PSScriptRoot 'media/darkroom.mp4'))
}
