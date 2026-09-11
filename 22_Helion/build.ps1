param(
    [ValidateSet('host','pico','check','capture','all')][string]$Target = 'host',
    [string]$SdkPath = $env:PICO_SDK_PATH,
    [string]$ExtrasPath = $env:PICO_EXTRAS_PATH,
    [switch]$Reference
)
$ErrorActionPreference = 'Stop'
$helionSource = Join-Path $PSScriptRoot 'helion'
$helionHost = Join-Path $helionSource 'build_host'
$helionPico = Join-Path $helionSource 'build_rp2350'
$helionVariant = ''
$helionQuality = 'ON'
if ($Reference) {
    $helionVariant = '_reference'
    $helionQuality = 'OFF'
    $helionHost += $helionVariant
    $helionPico += $helionVariant
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
$helionCompiler = (Get-Command gcc -ErrorAction Stop).Source
$helionToolBin = Split-Path $helionCompiler
$helionPrefix = Split-Path $helionToolBin
$helionMake = (Get-Command mingw32-make -ErrorAction Stop).Source
if ($Target -in @('host','check','capture','all')) {
    Invoke-Checked 'cmake' @('-S',$helionSource,'-B',$helionHost,'-G','MinGW Makefiles','-DHELION_HOST=ON',"-DHELION_DMA=$helionQuality","-DHELION_INTERP=$helionQuality","-DCMAKE_PREFIX_PATH=$helionPrefix","-DCMAKE_MAKE_PROGRAM=$helionMake")
    Invoke-Checked 'cmake' @('--build',$helionHost,'-j','8')
    # Include the runtime DLLs so launching from Explorer does not depend on PATH.
    foreach ($helionDll in @('SDL2.dll','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
        $helionDllPath = Join-Path $helionToolBin $helionDll
        if (Test-Path -LiteralPath $helionDllPath) { Copy-Item -LiteralPath $helionDllPath -Destination $helionHost }
    }
}
if ($Target -in @('check','all')) {
    Push-Location $helionHost
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
    Invoke-Checked 'cmake' @('-S',$helionSource,'-B',$helionPico,'-G','MinGW Makefiles',"-DHELION_DMA=$helionQuality","-DHELION_INTERP=$helionQuality","-DPICO_SDK_PATH=$SdkPath","-DPICO_EXTRAS_PATH=$ExtrasPath","-DCMAKE_MAKE_PROGRAM=$helionMake")
    Invoke-Checked 'cmake' @('--build',$helionPico,'-j','8')
    Copy-Item -LiteralPath (Join-Path $helionPico 'helion.uf2') -Destination (Join-Path $PSScriptRoot "helion${helionVariant}_vga_rp2350.uf2") -Force
}
if ($Target -in @('capture','all')) {
    Invoke-Checked 'python' @((Join-Path $helionSource 'tools/capture.py'),'--exe',(Join-Path $helionHost 'helion.exe'),'--out',(Join-Path $PSScriptRoot "media/helion${helionVariant}.mp4"))
}
