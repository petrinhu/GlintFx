# SPDX-License-Identifier: AGPL-3.0-or-later
# check-no-target-collision-win.ps1 - Windows twin of tests/tools/
# check_no_target_collision.sh (FIX-CONSUMO-2, achado QA-3), registered
# as no_target_collision_win_test (tests/CMakeLists.txt, if(WIN32)) and
# paired with no_target_collision_test in tests/parity_aliases.txt
# (PARITY-GATE, GODS_LAWS.md L-04).
#
# WHAT THE LINUX SCRIPT PROVES: a consumer that embeds glintfx via
# add_subdirectory can name its OWN CMake target `glintfx` (the bare
# name glintfx's own internal library target used to have, before the
# FIX-CONSUMO-2 rename to glintfx_library) without hitting CMake Error
# ... policy CMP0002. This is pure CMake target-namespace machinery -
# not one line of it is Unix-specific. The only platform difference is
# the produced binary's own filename (collision_consumer.exe here,
# collision_consumer with no extension on the sh side).
#
# Usage: check-no-target-collision-win.ps1 -GlintfxSourceDir <path> -FixtureSrcDir <path>
#
# Each function below does one thing (GODS_LAWS.md L-17).

param(
    [Parameter(Mandatory = $true)][string]$GlintfxSourceDir,
    [Parameter(Mandatory = $true)][string]$FixtureSrcDir
)

$ErrorActionPreference = "Stop"

function New-ScratchWorkdir() {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("glintfx-collision-win-" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    return $dir
}

function Invoke-ConfigureFixture([string]$fixtureSrc, [string]$fixtureBuild, [string]$glintfxSrc) {
    cmake -S $fixtureSrc -B $fixtureBuild -G Ninja -DCMAKE_BUILD_TYPE=Release "-DGLINTFX_SOURCE_DIR=$glintfxSrc"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-BuildFixture([string]$fixtureBuild) {
    cmake --build $fixtureBuild
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-RunFixture([string]$fixtureBuild) {
    $binary = (Get-ChildItem -Recurse -Filter collision_consumer.exe $fixtureBuild | Select-Object -First 1).FullName
    if (-not $binary) {
        Write-Error "check-no-target-collision-win.ps1: collision_consumer.exe not found under $fixtureBuild"
        exit 1
    }
    Write-Host "check-no-target-collision-win.ps1: running $binary"
    & $binary
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$scratch = New-ScratchWorkdir
try {
    $fixtureBuild = Join-Path $scratch "fixture-build"

    Invoke-ConfigureFixture $FixtureSrcDir $fixtureBuild $GlintfxSourceDir
    Invoke-BuildFixture $fixtureBuild
    Invoke-RunFixture $fixtureBuild

    Write-Host "ok: a consumer target named glintfx does not collide with glintfx's own internal target."
}
finally {
    Remove-Item -Recurse -Force $scratch -ErrorAction SilentlyContinue
}
