# COLOSSUS build, in VESPER's style.
#
#   .\build.ps1 host              the SDL player and the capture tool
#   .\build.ps1 pico              colossus_vga_rp2350.uf2, 300 MHz at 1.20 V
#   .\build.ps1 all               both, then the WAV and the phrase stills
#   .\build.ps1 flash             reboot the board into BOOTSEL and load it
#   .\build.ps1 run               flash, then read telemetry for -Seconds
#   .\build.ps1 capture           WAV, per-second hashes, one still per phrase
#   .\build.ps1 video             the whole run to media/colossus.mp4
#
# Switches:
#   -Stub          ignore render.cmake; build the platform's stub renderer
#   -Scanout640    VESPER's 480-copies-a-frame scanout, for the comparison
#   -MaterialTest  draw Phase's render_material_test(), PLANNING section 8's
#                  worst case, instead of the demo
#   -Ballast N     link N dead bytes of .bss (the heap floor measurement)
#   -Port COM10    the board
#   -Seconds N     how long `run` listens
#
# gcc does not run under the Bash tool in this environment; everything that
# compiles goes through here.

param(
    [ValidateSet('host','pico','all','flash','run','capture','video','floor','check','ship','clean')]
    [string]$Target = 'host',
    [switch]$Stub,
    [switch]$Scanout640,
    [switch]$MaterialTest,
    [int]$Ballast = 0,
    [string]$Port = 'COM10',
    [double]$Seconds = 12,
    [string]$SdkPath = $env:PICO_SDK_PATH,
    [string]$ExtrasPath = $env:PICO_EXTRAS_PATH
)

$ErrorActionPreference = 'Stop'

$cvRoot      = $PSScriptRoot
$cvSource    = Join-Path $cvRoot 'colossus'
$cvMedia     = Join-Path $cvRoot 'media'
# The stub and the renderer get their own build trees and their own uf2. They
# are two different programs and reconfiguring one cmake cache back and forth
# between them wastes minutes and eventually lies about what was built.
$cvVariant = ''
if ($Stub)       { $cvVariant = '_stub' }
if ($Scanout640)  { $cvVariant = $cvVariant + '640' }
if ($MaterialTest) { $cvVariant = $cvVariant + '_mat' }
if ($Stub) { $cvHostBuild = Join-Path $cvSource 'build_host_stub' }
else       { $cvHostBuild = Join-Path $cvSource 'build_host' }
$cvPicoBuild = Join-Path $cvSource "build_rp2350$cvVariant"
if ($cvVariant) { $cvUf2 = Join-Path $cvRoot "colossus$($cvVariant)_rp2350.uf2" }
else            { $cvUf2 = Join-Path $cvRoot 'colossus_vga_rp2350.uf2' }

# gcc and cmake write warnings to stderr, and with $ErrorActionPreference =
# 'Stop' PowerShell turns a native command's stderr into a terminating
# NativeCommandError -- so a warning would fail the build and a real failure
# would be reported as whatever the first warning happened to be. The exit
# code is the only thing that decides here.
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & $Program @Arguments } finally { $ErrorActionPreference = $previous }
    if ($LASTEXITCODE -ne 0) { throw "$Program exited with $LASTEXITCODE" }
}

$cvGcc    = (Get-Command gcc -ErrorAction Stop).Source
$cvBin    = Split-Path $cvGcc
$cvPrefix = Split-Path $cvBin
$cvMake   = (Get-Command mingw32-make -ErrorAction Stop).Source

$cvStub = if ($Stub) { 'ON' } else { 'OFF' }
$cvS640 = if ($Scanout640) { 'ON' } else { 'OFF' }
$cvMat  = if ($MaterialTest) { 'ON' } else { 'OFF' }

if ($Target -eq 'clean') {
    foreach ($d in (Get-ChildItem -LiteralPath $cvSource -Directory -Filter 'build_*' |
                    ForEach-Object { $_.FullName })) {
        if (Test-Path -LiteralPath $d) { Remove-Item -LiteralPath $d -Recurse -Force }
    }
    'cleaned'
    return
}

if ($Target -in @('host','all','capture','video','check','ship')) {
    Invoke-Checked 'cmake' @('-S',$cvSource,'-B',$cvHostBuild,'-G','MinGW Makefiles',
        '-DCOLOSSUS_HOST=ON',"-DCOLOSSUS_STUB=$cvStub",
        "-DCMAKE_PREFIX_PATH=$cvPrefix","-DCMAKE_MAKE_PROGRAM=$cvMake")
    Invoke-Checked 'cmake' @('--build',$cvHostBuild,'-j','8')
    # So the player runs from Explorer without the toolchain on PATH.
    foreach ($dll in @('SDL2.dll','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
        $p = Join-Path $cvBin $dll
        if (Test-Path -LiteralPath $p) { Copy-Item -LiteralPath $p -Destination $cvHostBuild -Force }
    }
}

if ($Target -in @('pico','all','flash','run','floor')) {
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
    Invoke-Checked 'cmake' @('-S',$cvSource,'-B',$cvPicoBuild,'-G','MinGW Makefiles',
        "-DPICO_SDK_PATH=$SdkPath","-DPICO_EXTRAS_PATH=$ExtrasPath","-DCMAKE_MAKE_PROGRAM=$cvMake",
        "-DCOLOSSUS_STUB=$cvStub","-DCOLOSSUS_SCANOUT_640=$cvS640",
        "-DCOLOSSUS_MATERIAL_TEST=$cvMat","-DCOLOSSUS_BALLAST=$Ballast")
    Invoke-Checked 'cmake' @('--build',$cvPicoBuild,'-j','8')
    Copy-Item -LiteralPath (Join-Path $cvPicoBuild 'colossus.uf2') -Destination $cvUf2 -Force
    $elf = Join-Path $cvPicoBuild 'colossus.elf'
    Invoke-Checked 'python' @((Join-Path $cvSource 'tools/ledger_check.py'),'--map',"$elf.map",'--warn-only')
    "uf2: $cvUf2"
}

if ($Target -in @('flash','run','floor')) {
    # The board is usually running the previous demo, so force BOOTSEL first.
    $ErrorActionPreference = 'Continue'
    & picotool reboot -f -u
    $ErrorActionPreference = 'Stop'
    Start-Sleep -Milliseconds 1500
    Invoke-Checked 'picotool' @('load','-x',$cvUf2)
}

if ($Target -eq 'run') {
    Invoke-Checked 'python' @((Join-Path $cvSource 'tools/serial_read.py'),'--port',$Port,'--seconds',"$Seconds")
}

if ($Target -eq 'floor') {
    # The heap floor, measured rather than assumed. Link N bytes of dead .bss,
    # flash, and see whether the firmware still gets past video_init(), where
    # pico_scanvideo mallocs its scanline buffers. The largest N that boots
    # gives the smallest heap_free the board has been seen to survive.
    #
    # This is only safe because cv_panic() (main.c) keeps USB alive: a failed
    # arm leaves the board readable and reflashable instead of needing a hand.
    # The pico + flash blocks above already built and loaded the -Ballast N
    # image; all that is left is to see how far it gets.
    "floor: ballast $Ballast -- watching for BOOT / PANIC"
    Invoke-Checked 'python' @((Join-Path $cvSource 'tools/serial_read.py'),
        '--port',$Port,'--seconds','25')
}

if ($Target -eq 'check') {
    # PLANNING section 10's four referees, plus the memory ledger. One
    # command, one verdict: if this passes the production is valid, and if it
    # does not it says which referee and why.
    #
    #   1  sync        every chapter boundary is a phrase boundary in the score
    #   2  audio       song_check: block-size independent, no clipping, ends silent
    #   -  renderer    483 seek comparisons, clipping, depth order, ceiling frames
    #   4  film        no black or flat frame outside the permitted bars
    #   -  ledger      every SRAM allocation declared, heap above the measured floor
    $cvFailures = @()
    function Referee([string]$Name, [scriptblock]$Body) {
        Write-Host ""
        Write-Host "=== $Name ==="
        & $Body
        if ($LASTEXITCODE -ne 0) { $script:cvFailures += $Name }
    }
    $ErrorActionPreference = 'Continue'
    Referee 'referee 1  sync'     { python (Join-Path $cvSource 'tools/sync_check.py') }
    Referee 'referee 2  audio'    { python (Join-Path $cvSource 'tools/song_check.py') }
    Referee 'renderer'            { & (Join-Path $cvHostBuild 'checks.exe') }
    Referee 'referee 4  film'     { python (Join-Path $cvSource 'tools/film_check.py') }
    Referee 'film self-test'      { python (Join-Path $cvSource 'tools/film_check.py') --selftest }
    Referee 'ledger'              { python (Join-Path $cvSource 'tools/ledger_check.py') }
    # A report, not a gate: PLANNING allows the glow alone where no real
    # matched shape exists, so this prints which boundaries have one and
    # exits zero either way.
    Referee 'transitions (report)' { python (Join-Path $cvSource 'tools/transition_check.py') }
    Write-Host ""
    if ($cvFailures.Count) {
        Write-Host ("FAILED: " + ($cvFailures -join ', '))
        exit 1
    }
    Write-Host "ALL REFEREES PASS"
}

if ($Target -eq 'ship') {
    New-Item -ItemType Directory -Force -Path $cvMedia | Out-Null
    Copy-Item -LiteralPath (Join-Path $cvPicoBuild 'colossus.uf2') -Destination $cvUf2 -Force -ErrorAction SilentlyContinue
    $cap = Join-Path $cvHostBuild 'capture.exe'
    $wav = Join-Path $cvMedia 'colossus.wav'
    if (-not (Test-Path $wav)) { Invoke-Checked $cap @('--wav',$wav) }
    $mp4 = Join-Path $cvMedia 'colossus.mp4'
    $raw = Join-Path $env:TEMP 'colossus_ship_raw.bin'
    $line = "`"$cap`" --raw --fps 30 | ffmpeg -y -v error -f rawvideo -pixel_format rgb24 " +
            "-video_size 320x240 -framerate 30 -i pipe:0 -i `"$wav`" -vf scale=960:720:flags=neighbor " +
            "-c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p -c:a aac -b:a 192k " +
            "-movflags +faststart -shortest `"$mp4`""
    cmd /c $line | Out-Null
    "ship: $cvUf2"
    "ship: $mp4"
}

if ($Target -in @('capture','all','video')) {
    New-Item -ItemType Directory -Force -Path $cvMedia | Out-Null
    $cap = Join-Path $cvHostBuild 'capture.exe'
    Invoke-Checked $cap @('--wav',(Join-Path $cvMedia 'colossus.wav'))
    Invoke-Checked $cap @('--hashes',(Join-Path $cvMedia 'hashes.txt'))
    Invoke-Checked $cap @('--out',(Join-Path $cvMedia 'phrases'),'--phrases','--quiet')
    "media: $cvMedia"
}

if ($Target -eq 'video') {
    $cap = Join-Path $cvHostBuild 'capture.exe'
    $wav = Join-Path $cvMedia 'colossus.wav'
    $mp4 = Join-Path $cvMedia 'colossus.mp4'
    $render = Start-Process -FilePath $cap -ArgumentList @('--raw','--fps','60') -NoNewWindow -PassThru -RedirectStandardOutput (Join-Path $env:TEMP 'colossus_raw.bin')
    $render.WaitForExit()
    # PowerShell 5.1 can report $null for ExitCode after WaitForExit unless the
    # handle was touched first; read it through the cached handle.
    $null = $render.Handle
    $code = $render.ExitCode
    if ($null -ne $code -and $code -ne 0) { throw "capture --raw exited with $code" }
    Invoke-Checked 'ffmpeg' @('-y','-v','warning','-f','rawvideo','-pixel_format','rgb24','-video_size','320x240',
        '-framerate','60','-i',(Join-Path $env:TEMP 'colossus_raw.bin'),'-i',$wav,
        '-vf','scale=960:720:flags=neighbor','-c:v','libx264','-preset','slow','-crf','18',
        '-pix_fmt','yuv420p','-c:a','aac','-b:a','192k','-movflags','+faststart','-shortest',$mp4)
    Remove-Item -LiteralPath (Join-Path $env:TEMP 'colossus_raw.bin') -Force
    "video: $mp4"
}
