# SPDX-License-Identifier: AGPL-3.0-or-later
# check-output-name-win.ps1 - Windows twin of tests/tools/
# check_output_name.sh (FIX-CONSUMO-3, achado QA-2-2), registered as
# output_name_win_test (tests/CMakeLists.txt, if(WIN32)) and paired
# with output_name_test in tests/parity_aliases.txt (PARITY-GATE,
# GODS_LAWS.md L-04).
#
# WHAT THE LINUX SCRIPT PROVES, read for its INTENT: the compiled
# artifact's OWN basename honors cmake/GlintfxLibrary.cmake's
# OUTPUT_NAME (glintfx), never the internal CMake target name
# (glintfx_library) - proven by linking against the INSTALLED artifact
# with NO CMake and NO find_package at all, the one path none of the
# other consumption gates (consume_test/embed_test/install_packager_
# layout_test/no_target_collision_test) exercise, because they all
# resolve the library through the glintfx::glintfx imported CMake
# target.
#
# THE MECHANISM GENUINELY DIFFERS HERE (GODS_LAWS.md L-04: "mecanismo
# pode diferir; comportamento observavel e cobertura, nao"): MSVC has
# no `-l`/`-L`/`-Wl,-rpath` - the installed artifact is glintfx.dll +
# glintfx.lib (import library) in shared mode, or a lone glintfx.lib
# in static mode (never libglintfx.* - that prefix is a GNU/Unix ar/ld
# convention), and there is no rpath equivalent (a Windows executable
# resolves a DLL from its own directory or PATH, not an embedded
# search path). Rather than hand-typing cl.exe flags from memory (this
# project's own established precedent for anything MSVC-specific -
# see the "Padrao C++23 aceito" step in .github/workflows/ci.yml,
# which generates a throwaway CMake project instead of guessing a raw
# `cl` invocation), this script does the same: a tiny generated
# CMakeLists.txt whose ONLY consumer-facing action is
# target_link_libraries(... PRIVATE glintfx) - the BARE name, with no
# find_package(glintfx) and no glintfx::glintfx imported target - so
# the linker resolves glintfx.lib purely by name search against
# -L-equivalent link directories, the exact raw-link spirit `-lglintfx`
# has on the Unix side. The DLL is then copied next to the produced
# .exe before running it, mirroring how an end user actually deploys a
# raw-linked Windows consumer (no rpath to fall back on).
#
# Usage: check-output-name-win.ps1 -BuildDir <path> -ConsumerSrcFile <path>
#
# Each function below does one thing (GODS_LAWS.md L-17).

param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [Parameter(Mandatory = $true)][string]$ConsumerSrcFile
)

$ErrorActionPreference = "Stop"

function New-ScratchWorkdir() {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("glintfx-outputname-win-" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    return $dir
}

function Install-IntoPrefix([string]$buildDir, [string]$prefix) {
    cmake --install $buildDir --prefix $prefix
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# Finds the installed import/static library (glintfx.lib) and, when it
# exists, the runtime DLL (glintfx.dll) alongside it. Fails on
# libglintfx.* (the GNU/Unix name OUTPUT_NAME must never regress to
# here either - CE- lesson applied both ways) or on no glintfx.lib at
# all.
function Find-InstalledArtifact([string]$prefix) {
    $badLibNameHit = Get-ChildItem -Recurse -Filter "libglintfx.*" $prefix -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($badLibNameHit) {
        throw "check-output-name-win.ps1: found $($badLibNameHit.FullName) - OUTPUT_NAME regressed to the GNU/Unix 'lib' prefix on a Windows install"
    }

    $importLib = Get-ChildItem -Recurse -Filter "glintfx.lib" $prefix -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $importLib) {
        throw "check-output-name-win.ps1: no glintfx.lib found under $prefix - OUTPUT_NAME regressed (installed under some OTHER basename, e.g. glintfx_library.lib)"
    }
    $dll = Get-ChildItem -Recurse -Filter "glintfx.dll" $prefix -ErrorAction SilentlyContinue | Select-Object -First 1

    return [PSCustomObject]@{
        LibDir = $importLib.DirectoryName
        Dll    = $dll
    }
}

# Throwaway CMake project, same precedent as ci.yml's own "Padrao
# C++23 aceito" step: raw_link_consumer links PRIVATE glintfx (bare
# target NAME, not glintfx::glintfx and no find_package) purely by
# name search against link_directories - the linker resolves
# glintfx.lib exactly the way -lglintfx resolves libglintfx.so/.a on
# Unix, with zero CMake package machinery for glintfx itself involved.
function New-RawLinkProject([string]$projectDir, [string]$consumerSrcFile, [string]$includeDir, [string]$libDir) {
    New-Item -ItemType Directory -Force -Path $projectDir | Out-Null
    Copy-Item -LiteralPath $consumerSrcFile -Destination (Join-Path $projectDir "main.cpp")

    $includeDirCmake = $includeDir -replace '\\', '/'
    $libDirCmake = $libDir -replace '\\', '/'

    @"
cmake_minimum_required(VERSION 3.25)
project(raw_link_consumer CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(raw_link_consumer main.cpp)
target_include_directories(raw_link_consumer PRIVATE "$includeDirCmake")
target_link_directories(raw_link_consumer PRIVATE "$libDirCmake")
target_link_libraries(raw_link_consumer PRIVATE glintfx)
"@ | Set-Content (Join-Path $projectDir "CMakeLists.txt")
}

function Invoke-ConfigureAndBuildRawLinkProject([string]$projectDir, [string]$projectBuild) {
    cmake -S $projectDir -B $projectBuild -G Ninja -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    cmake --build $projectBuild
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-RunRawLinkConsumer([string]$projectBuild, [System.IO.FileSystemInfo]$dll) {
    $binary = (Get-ChildItem -Recurse -Filter raw_link_consumer.exe $projectBuild | Select-Object -First 1).FullName
    if (-not $binary) {
        Write-Error "check-output-name-win.ps1: raw_link_consumer.exe not found after build"
        exit 1
    }

    if ($dll) {
        # No rpath on Windows: a raw-linked consumer resolves its DLL
        # from its own directory or PATH, never from an embedded
        # search path baked in at link time. Copying next to the .exe
        # is the direct Windows analogue of -Wl,-rpath on the Unix
        # side - both make the SAME claim: "this exact installed
        # artifact resolves at RUN time, not just at link time."
        Copy-Item -LiteralPath $dll.FullName -Destination (Split-Path $binary -Parent) -Force
    }

    Write-Host "check-output-name-win.ps1: running $binary"
    & $binary
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$scratch = New-ScratchWorkdir
try {
    $prefix = Join-Path $scratch "prefix"
    Install-IntoPrefix $BuildDir $prefix

    $artifact = Find-InstalledArtifact $prefix
    Write-Host "check-output-name-win.ps1: installed import/static library at $($artifact.LibDir)\glintfx.lib"

    $includeDir = Join-Path $prefix "include"
    $projectDir = Join-Path $scratch "raw-link-project"
    $projectBuild = Join-Path $scratch "raw-link-build"

    New-RawLinkProject $projectDir $ConsumerSrcFile $includeDir $artifact.LibDir
    Invoke-ConfigureAndBuildRawLinkProject $projectDir $projectBuild
    Invoke-RunRawLinkConsumer $projectBuild $artifact.Dll

    Write-Host "ok: OUTPUT_NAME contract holds - a bare glintfx.lib raw link (no find_package, no glintfx::glintfx target) compiles, links and runs against the installed artifact."
}
finally {
    Remove-Item -Recurse -Force $scratch -ErrorAction SilentlyContinue
}
