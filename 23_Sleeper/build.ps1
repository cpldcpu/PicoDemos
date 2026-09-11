param(
    [ValidateSet('host','pico','check','capture','all')][string]$Target = 'host',
    [string]$SdkPath = $env:PICO_SDK_PATH,
    [string]$ExtrasPath = $env:PICO_EXTRAS_PATH
)
$ErrorActionPreference = 'Stop'
$sleeperSource = Join-Path $PSScriptRoot 'sleeper'
$sleeperHost = Join-Path $sleeperSource 'build_host'
$sleeperPico = Join-Path $sleeperSource 'build_rp2350'
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
$sleeperCompiler = (Get-Command gcc -ErrorAction Stop).Source
$sleeperToolBin = Split-Path $sleeperCompiler
$sleeperPrefix = Split-Path $sleeperToolBin
$sleeperMake = (Get-Command mingw32-make -ErrorAction Stop).Source
if ($Target -in @('host','check','capture','all')) {
    Invoke-Checked 'cmake' @('-S',$sleeperSource,'-B',$sleeperHost,'-G','MinGW Makefiles','-DSLEEPER_HOST=ON',"-DCMAKE_PREFIX_PATH=$sleeperPrefix","-DCMAKE_MAKE_PROGRAM=$sleeperMake")
    Invoke-Checked 'cmake' @('--build',$sleeperHost,'-j','8')
    # Include the runtime DLLs so launching from Explorer does not depend on PATH.
    foreach ($sleeperDll in @('SDL2.dll','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
        $sleeperDllPath = Join-Path $sleeperToolBin $sleeperDll
        if (Test-Path -LiteralPath $sleeperDllPath) { Copy-Item -LiteralPath $sleeperDllPath -Destination $sleeperHost }
    }
}
if ($Target -in @('check','all')) {
    Push-Location $sleeperHost
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
    Invoke-Checked 'cmake' @('-S',$sleeperSource,'-B',$sleeperPico,'-G','MinGW Makefiles',"-DPICO_SDK_PATH=$SdkPath","-DPICO_EXTRAS_PATH=$ExtrasPath","-DCMAKE_MAKE_PROGRAM=$sleeperMake")
    Invoke-Checked 'cmake' @('--build',$sleeperPico,'-j','8')
    Copy-Item -LiteralPath (Join-Path $sleeperPico 'sleeper.uf2') -Destination (Join-Path $PSScriptRoot 'sleeper_vga_rp2350.uf2') -Force
}
if ($Target -in @('capture','all')) {
    Invoke-Checked 'python' @((Join-Path $sleeperSource 'tools/capture.py'),'--exe',(Join-Path $sleeperHost 'sleeper.exe'),'--out',(Join-Path $PSScriptRoot 'media/sleeper.mp4'))
}
