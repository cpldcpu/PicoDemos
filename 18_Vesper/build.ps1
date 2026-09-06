param(
    [ValidateSet('host','pico','check','capture','all')][string]$Target = 'host',
    [string]$SdkPath = $env:PICO_SDK_PATH,
    [string]$ExtrasPath = $env:PICO_EXTRAS_PATH
)
$ErrorActionPreference = 'Stop'
$vesperSource = Join-Path $PSScriptRoot 'vesper'
$vesperHost = Join-Path $vesperSource 'build_host'
$vesperPico = Join-Path $vesperSource 'build_rp2350'
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program exited with $LASTEXITCODE" }
}
$vesperCompiler = (Get-Command gcc -ErrorAction Stop).Source
$vesperToolBin = Split-Path $vesperCompiler
$vesperPrefix = Split-Path $vesperToolBin
$vesperMake = (Get-Command mingw32-make -ErrorAction Stop).Source
if ($Target -in @('host','check','capture','all')) {
    Invoke-Checked 'cmake' @('-S',$vesperSource,'-B',$vesperHost,'-G','MinGW Makefiles','-DVESPER_HOST=ON',"-DCMAKE_PREFIX_PATH=$vesperPrefix","-DCMAKE_MAKE_PROGRAM=$vesperMake")
    Invoke-Checked 'cmake' @('--build',$vesperHost,'-j','8')
    # Include the runtime DLLs so launching from Explorer does not depend on PATH.
    foreach ($vesperDll in @('SDL2.dll','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
        $vesperDllPath = Join-Path $vesperToolBin $vesperDll
        if (Test-Path -LiteralPath $vesperDllPath) { Copy-Item -LiteralPath $vesperDllPath -Destination $vesperHost }
    }
}
if ($Target -in @('check','all')) {
    Push-Location $vesperHost
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
    Invoke-Checked 'cmake' @('-S',$vesperSource,'-B',$vesperPico,'-G','MinGW Makefiles',"-DPICO_SDK_PATH=$SdkPath","-DPICO_EXTRAS_PATH=$ExtrasPath","-DCMAKE_MAKE_PROGRAM=$vesperMake")
    Invoke-Checked 'cmake' @('--build',$vesperPico,'-j','8')
    Copy-Item -LiteralPath (Join-Path $vesperPico 'vesper.uf2') -Destination (Join-Path $PSScriptRoot 'vesper_vga_rp2350.uf2')
}
if ($Target -in @('capture','all')) {
    Invoke-Checked 'python' @((Join-Path $vesperSource 'tools/capture.py'),'--exe',(Join-Path $vesperHost 'vesper.exe'))
}
