param(
    [ValidateSet('host','pico','check','capture','all')][string]$Target = 'host',
    [string]$SdkPath = $env:PICO_SDK_PATH,
    [string]$ExtrasPath = $env:PICO_EXTRAS_PATH,
    [switch]$Smooth
)
$ErrorActionPreference = 'Stop'
$pelagicSource = Join-Path $PSScriptRoot 'pelagic'
$pelagicHost = Join-Path $pelagicSource 'build_host'
$pelagicPico = Join-Path $pelagicSource 'build_rp2350'
$pelagicVariant = ''
$pelagicQuality = 'OFF'
if ($Smooth) {
    $pelagicVariant = '_smooth'
    $pelagicQuality = 'ON'
    $pelagicHost += $pelagicVariant
    $pelagicPico += $pelagicVariant
}
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
$pelagicCompiler = (Get-Command gcc -ErrorAction Stop).Source
$pelagicToolBin = Split-Path $pelagicCompiler
$pelagicPrefix = Split-Path $pelagicToolBin
$pelagicMake = (Get-Command mingw32-make -ErrorAction Stop).Source
if ($Target -in @('host','check','capture','all')) {
    Invoke-Checked 'cmake' @('-S',$pelagicSource,'-B',$pelagicHost,'-G','MinGW Makefiles','-DPELAGIC_HOST=ON',"-DPELAGIC_SMOOTH=$pelagicQuality","-DPELAGIC_INTERP=$pelagicQuality","-DCMAKE_PREFIX_PATH=$pelagicPrefix","-DCMAKE_MAKE_PROGRAM=$pelagicMake")
    Invoke-Checked 'cmake' @('--build',$pelagicHost,'-j','8')
    # Include the runtime DLLs so launching from Explorer does not depend on PATH.
    foreach ($pelagicDll in @('SDL2.dll','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
        $pelagicDllPath = Join-Path $pelagicToolBin $pelagicDll
        if (Test-Path -LiteralPath $pelagicDllPath) { Copy-Item -LiteralPath $pelagicDllPath -Destination $pelagicHost }
    }
}
if ($Target -in @('check','all')) {
    Push-Location $pelagicHost
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
    Invoke-Checked 'cmake' @('-S',$pelagicSource,'-B',$pelagicPico,'-G','MinGW Makefiles',"-DPELAGIC_SMOOTH=$pelagicQuality","-DPELAGIC_INTERP=$pelagicQuality","-DPICO_SDK_PATH=$SdkPath","-DPICO_EXTRAS_PATH=$ExtrasPath","-DCMAKE_MAKE_PROGRAM=$pelagicMake")
    Invoke-Checked 'cmake' @('--build',$pelagicPico,'-j','8')
    Copy-Item -LiteralPath (Join-Path $pelagicPico 'pelagic.uf2') -Destination (Join-Path $PSScriptRoot "pelagic${pelagicVariant}_vga_rp2350.uf2") -Force
}
if ($Target -in @('capture','all')) {
    Invoke-Checked 'python' @((Join-Path $pelagicSource 'tools/capture.py'),'--exe',(Join-Path $pelagicHost 'pelagic.exe'),'--out',(Join-Path $PSScriptRoot "media/pelagic${pelagicVariant}.mp4"))
}
