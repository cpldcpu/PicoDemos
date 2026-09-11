param(
    [ValidateSet('host','pico','check','capture','all')][string]$Target = 'host',
    [string]$SdkPath = $env:PICO_SDK_PATH,
    [string]$ExtrasPath = $env:PICO_EXTRAS_PATH,
    [switch]$Reference
)
$ErrorActionPreference = 'Stop'
$tesseraSource = Join-Path $PSScriptRoot 'tessera'
$tesseraHost = Join-Path $tesseraSource 'build_host'
$tesseraPico = Join-Path $tesseraSource 'build_rp2350'
$tesseraVariant = ''
$tesseraQuality = 'ON'
if ($Reference) {
    $tesseraVariant = '_reference'
    $tesseraQuality = 'OFF'
    $tesseraHost += $tesseraVariant
    $tesseraPico += $tesseraVariant
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
$tesseraCompiler = (Get-Command gcc -ErrorAction Stop).Source
$tesseraToolBin = Split-Path $tesseraCompiler
$tesseraPrefix = Split-Path $tesseraToolBin
$tesseraMake = (Get-Command mingw32-make -ErrorAction Stop).Source
if ($Target -in @('host','check','capture','all')) {
    Invoke-Checked 'cmake' @('-S',$tesseraSource,'-B',$tesseraHost,'-G','MinGW Makefiles','-DTESSERA_HOST=ON',"-DTESSERA_DMA=$tesseraQuality","-DTESSERA_INTERP=$tesseraQuality","-DCMAKE_PREFIX_PATH=$tesseraPrefix","-DCMAKE_MAKE_PROGRAM=$tesseraMake")
    Invoke-Checked 'cmake' @('--build',$tesseraHost,'-j','8')
    # Include the runtime DLLs so launching from Explorer does not depend on PATH.
    foreach ($tesseraDll in @('SDL2.dll','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
        $tesseraDllPath = Join-Path $tesseraToolBin $tesseraDll
        if (Test-Path -LiteralPath $tesseraDllPath) { Copy-Item -LiteralPath $tesseraDllPath -Destination $tesseraHost }
    }
}
if ($Target -in @('check','all')) {
    Push-Location $tesseraHost
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
    Invoke-Checked 'cmake' @('-S',$tesseraSource,'-B',$tesseraPico,'-G','MinGW Makefiles',"-DTESSERA_DMA=$tesseraQuality","-DTESSERA_INTERP=$tesseraQuality","-DPICO_SDK_PATH=$SdkPath","-DPICO_EXTRAS_PATH=$ExtrasPath","-DCMAKE_MAKE_PROGRAM=$tesseraMake")
    Invoke-Checked 'cmake' @('--build',$tesseraPico,'-j','8')
    Copy-Item -LiteralPath (Join-Path $tesseraPico 'tessera.uf2') -Destination (Join-Path $PSScriptRoot "tessera${tesseraVariant}_vga_rp2350.uf2") -Force
}
if ($Target -in @('capture','all')) {
    Invoke-Checked 'python' @((Join-Path $tesseraSource 'tools/capture.py'),'--exe',(Join-Path $tesseraHost 'tessera.exe'),'--out',(Join-Path $PSScriptRoot "media/tessera${tesseraVariant}.mp4"))
}
