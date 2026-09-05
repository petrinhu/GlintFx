# SPDX-License-Identifier: AGPL-3.0-or-later
# check-install-includedir-win.ps1 - Windows twin of tests/tools/
# check_install_includedir.sh (FIX-CONSUMO-2, achado QA-1), registered
# as install_includedir_win_test (tests/CMakeLists.txt, if(WIN32)) and
# paired with install_includedir_test in tests/parity_aliases.txt
# (PARITY-GATE, GODS_LAWS.md L-04).
#
# WHAT THE LINUX SCRIPT PROVES, read for its INTENT and not its
# mechanism (the lider's own instruction for closing this gap): a
# non-default CMAKE_INSTALL_INCLUDEDIR actually propagates into the
# EXPORTED CMake target set (glintfxTargets.cmake), not only onto disk
# - cmake/GlintfxLibrary.cmake's $<INSTALL_INTERFACE:...> generator
# expression is what makes that true, and it is pure CMake machinery,
# identical on every generator/compiler this project supports. Nothing
# here is Unix-specific: no readelf, no nm, no rpath, no raw linker
# flag - just cmake --install and a text search inside a generated
# .cmake file. The MECHANISM is therefore the SAME as the sh version,
# not a reinvention - only the shell it runs in differs.
#
# Usage: check-install-includedir-win.ps1 -GlintfxSourceDir <path>
#
# Each function below does one thing (GODS_LAWS.md L-17).

param(
    [Parameter(Mandatory = $true)][string]$GlintfxSourceDir
)

$ErrorActionPreference = "Stop"

$script:NonDefaultIncludeDir = "include/glintfx-nondefault-includedir"

function New-ScratchWorkdir() {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("glintfx-includedir-win-" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    return $dir
}

# -G Ninja explicito (CI-WIN-GEN, mesmo motivo de check-consume.ps1: o
# ambiente MSVC ja foi preparado por vcvarsall num passo anterior do
# job `windows`; sem -G, o default de linha de comando do CMake no
# Windows cai para NMake, que nao acha cl.exe sozinho).
function Invoke-ConfigureWithNonDefaultIncludeDir([string]$glintfxSrc, [string]$buildDir) {
    cmake -S $glintfxSrc -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_INSTALL_INCLUDEDIR=$($script:NonDefaultIncludeDir)" -DGLINTFX_BUILD_TESTS=OFF
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-BuildAndInstall([string]$buildDir, [string]$prefix) {
    cmake --build $buildDir
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    cmake --install $buildDir --prefix $prefix
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Assert-ExportedIncludeDirIsNonDefault([string]$prefix) {
    $targetsFile = Get-ChildItem -Recurse -Filter glintfxTargets.cmake $prefix -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $targetsFile) {
        throw "check-install-includedir-win.ps1: glintfxTargets.cmake not found under $prefix"
    }

    $expected = "`${_IMPORT_PREFIX}/$($script:NonDefaultIncludeDir)"
    $content = Get-Content -LiteralPath $targetsFile.FullName -Raw
    if ($content -notlike "*$expected*") {
        throw "check-install-includedir-win.ps1: INTERFACE_INCLUDE_DIRECTORIES in $($targetsFile.FullName) does not contain the non-default include dir ($expected): the dynamic INSTALL_INTERFACE regressed"
    }
    Write-Host "check-install-includedir-win.ps1: exported INTERFACE_INCLUDE_DIRECTORIES honors CMAKE_INSTALL_INCLUDEDIR=$($script:NonDefaultIncludeDir)"
}

$scratch = New-ScratchWorkdir
try {
    $buildDir = Join-Path $scratch "build"
    $prefix = Join-Path $scratch "prefix"

    Invoke-ConfigureWithNonDefaultIncludeDir $GlintfxSourceDir $buildDir
    Invoke-BuildAndInstall $buildDir $prefix
    Assert-ExportedIncludeDirIsNonDefault $prefix

    Write-Host "ok: CMAKE_INSTALL_INCLUDEDIR override propagates to the exported CMake package."
}
finally {
    Remove-Item -Recurse -Force $scratch -ErrorAction SilentlyContinue
}
