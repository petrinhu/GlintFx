# SPDX-License-Identifier: AGPL-3.0-or-later
# check-embed.ps1 - Windows equivalent of tests/tools/check_embed.sh
# (FIX-CONSUMO, achado A7; FIX-CONSUMO-2, achado QA-2). Proves glintfx
# works consumed via add_subdirectory (the shape FetchContent takes
# after populating the tree), the SAME two things check_embed.sh proves
# on Linux, closing the gap CI-CONSUME found: embed_test was registered
# under if(UNIX) in tests/CMakeLists.txt with no Windows counterpart, so
# "the five targets" was true for the installed-package path
# (check-consume.ps1) but false for the embedded path until this file
# existed.
#
#   1. Consuming glintfx via add_subdirectory configures, builds and
#      runs (glintfx's generated headers land scoped under the embed
#      build's own subdirectory, not spilled into the consumer's own
#      top-level build dir - that is the collision FIX-CONSUMO fixed).
#   2. GLINTFX_INSTALL default is PROJECT_IS_TOP_LEVEL: an embedding
#      consumer's own `install` target must NOT leak glintfx's headers
#      by surprise.
#   3. A consumer that opts IN (GLINTFX_INSTALL=ON) genuinely gets
#      glintfx's headers and CMake package installed alongside its own.
#
# Usage: check-embed.ps1 -GlintfxSourceDir <path> -EmbedSrcDir <path>
#
# Each function below does one thing (GODS_LAWS.md L-17).

param(
    [Parameter(Mandatory = $true)][string]$GlintfxSourceDir,
    [Parameter(Mandatory = $true)][string]$EmbedSrcDir
)

$ErrorActionPreference = "Stop"

# GLINTFX_SOURCE_DIR is forwarded as a raw string into
# add_subdirectory("${GLINTFX_SOURCE_DIR}" glintfx-build) inside
# tests/embed/CMakeLists.txt - it is a plain CMake CACHE VARIABLE, not a
# -S/-B argument that cmake itself would resolve to absolute. CMake
# resolves a RELATIVE add_subdirectory() source argument against
# CMAKE_CURRENT_SOURCE_DIR of the CALLING CMakeLists.txt (tests/embed
# itself, mid-configure), not against the process's original working
# directory. A relative value here (CI-CONSUME passed "." for the
# repository root) makes tests/embed add ITSELF as its own subdirectory,
# which adds itself again, forever, until CMake's recursion-depth guard
# trips - measured live: "Maximum recursion depth of 1000 exceeded" at
# CMakeLists.txt:11 (tests/embed/CMakeLists.txt's own
# cmake_minimum_required line), reproduced with plain cmake, no pwsh and
# no Windows involved - this is generic CMake path-resolution semantics,
# not an MSVC-generator quirk (CI-CONSUME-2). EmbedSrcDir does not have
# this failure mode (it is passed straight to cmake's own -S argument,
# which cmake resolves to absolute internally before add_subdirectory
# ever runs), but is resolved here too: a caller should never have to
# know which of the two parameters is the fragile one.
function Resolve-AbsolutePath([string]$path) {
    return (Resolve-Path -LiteralPath $path).ProviderPath
}

$GlintfxSourceDir = Resolve-AbsolutePath $GlintfxSourceDir
$EmbedSrcDir = Resolve-AbsolutePath $EmbedSrcDir

# Generator is explicit (CI-WIN-GEN): Ninja is a single-config generator,
# so the configuration is chosen here, at configure time, never at build
# or install time. The compiler environment (cl.exe on PATH) is prepared
# by a dedicated step earlier in the workflow file, not by this script.
function Invoke-ConfigureEmbed([string]$embedSrc, [string]$embedBuild, [string]$glintfxSrc) {
    cmake -S $embedSrc -B $embedBuild -G Ninja -DCMAKE_BUILD_TYPE=Release -DGLINTFX_SOURCE_DIR="$glintfxSrc"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# Same configure as above, plus the opt-in flag a consumer sets to get
# glintfx installed alongside its own artifacts (GlintfxOptions.cmake).
# Kept as its own function, not a parameterized variant of
# Invoke-ConfigureEmbed(), because the two configure different consumer
# intent, not the same intent with a knob (GODS_LAWS.md L-17: a name that
# needs "and"/a conditional to stay honest is two functions) - mirrors
# check_embed.sh's configure_embed()/configure_embed_with_install_opt_in()
# split exactly.
# Same generator fix as Invoke-ConfigureEmbed() above (CI-WIN-GEN):
# Ninja is single-config, so this variant also chooses its configuration
# here, at configure time, not via a later --config flag.
function Invoke-ConfigureEmbedWithInstallOptIn([string]$embedSrc, [string]$embedBuild, [string]$glintfxSrc) {
    cmake -S $embedSrc -B $embedBuild -G Ninja -DCMAKE_BUILD_TYPE=Release -DGLINTFX_SOURCE_DIR="$glintfxSrc" -DGLINTFX_INSTALL=ON
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-BuildEmbed([string]$embedBuild) {
    cmake --build $embedBuild
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# Reports where the produced .dll(s) landed relative to the executable
# that needs to load them - BEFORE the run, not after, so the log
# explains a loader failure instead of only reporting its symptom.
# Windows has no build-tree equivalent of the ELF RPATH the Linux side
# relies on (embed_test never needed this function - CMake auto-embeds
# a build-tree RPATH into the Linux executable, GODS_LAWS.md L-27:
# confirmed live, not assumed, by reading the DLL search order Microsoft
# publishes: the executable's OWN folder is searched, the rest of the
# build tree is not - learn.microsoft.com/windows/win32/dlls/
# dynamic-link-library-search-order). Mirrors diagnose-win-runtime.ps1's
# Test-DllBesideExe (WIN-HANG) - same shape, generalized to a list of
# DLLs instead of exactly one.
function Show-RuntimeLayout([string]$exePath, [string[]]$dllPaths) {
    $exeDir = Split-Path -Parent $exePath
    Write-Host "check-embed.ps1: executable directory: $exeDir"
    if (-not $dllPaths -or $dllPaths.Count -eq 0) {
        Write-Host "check-embed.ps1: WARNING - no .dll found anywhere under the embed build tree"
        return
    }
    foreach ($dll in $dllPaths) {
        $dllDir = Split-Path -Parent $dll
        if ($dllDir -eq $exeDir) {
            Write-Host "check-embed.ps1: $dll is BESIDE the executable (the default Windows DLL search order will find it)"
        } else {
            Write-Host "check-embed.ps1: $dll is NOT beside the executable - the default Windows DLL search order will NOT find it (it searches the executable's own folder, not the rest of the build tree)"
            Write-Host "check-embed.ps1:   exe dir: $exeDir"
            Write-Host "check-embed.ps1:   dll dir: $dllDir"
        }
    }
}

# Runs the built executable with its exit code AND its stdout/stderr
# captured and printed unconditionally, bounded so a hang cannot stall
# the CI step forever - same shape as diagnose-win-runtime.ps1's
# Invoke-BoundedRun (WIN-HANG precedent), reused here because a script
# that runs a binary and can only say "exit code 1" is not a diagnosis
# (CI-CONSUME, achado do lider: "um script que roda um binario e so
# sabe dizer 'deu 1' e diagnostico cego"). Returns the real exit code so
# the caller decides pass/fail; a caught launch failure or a timeout
# both exit this script directly, since neither leaves a real exit code
# to return.
function Invoke-BoundedRun([string]$exePath, [int]$timeoutSeconds) {
    $stdout = Join-Path ([System.IO.Path]::GetTempPath()) "check-embed-run-stdout.txt"
    $stderr = Join-Path ([System.IO.Path]::GetTempPath()) "check-embed-run-stderr.txt"
    Write-Host "check-embed.ps1: running $exePath directly, bounded to ${timeoutSeconds}s, stdout/stderr captured"
    try {
        $proc = Start-Process -FilePath $exePath -NoNewWindow -PassThru `
            -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    } catch {
        Write-Error "check-embed.ps1: the OS could not even start the process - $($_.Exception.Message)"
        exit 1
    }
    $finished = $proc.WaitForExit($timeoutSeconds * 1000)
    if (-not $finished) {
        Write-Error "check-embed.ps1: TIMED OUT after ${timeoutSeconds}s - process did not exit on its own, killing it now"
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        exit 1
    }
    $exitCode = $proc.ExitCode
    Write-Host ("check-embed.ps1: process exited with code {0} (0x{0:X8})" -f $exitCode)
    # Piped through Write-Host, not left as bare pipeline output: this
    # function is called in an ASSIGNMENT context ($exitCode = ...) by
    # its caller, and PowerShell aggregates every uncaptured pipeline
    # object produced during that call into the assigned variable - Get-
    # Content's own output would silently ride along with the intended
    # return value and corrupt it into an array (caught live: $exitCode
    # became @(<stdout lines>, 0), and "$exitCode -ne 0" on that array
    # evaluated true even for a clean exit, because the array held a
    # non-zero-looking string element). Write-Host writes directly to
    # the host and is immune to this capture, same reason the "process
    # exited with code" line above already used it.
    Write-Host "--- check-embed.ps1: stdout ---"
    Get-Content $stdout -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ }
    Write-Host "--- check-embed.ps1: stderr ---"
    Get-Content $stderr -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ }
    return $exitCode
}

function Invoke-RunEmbed([string]$embedBuild) {
    $binary = (Get-ChildItem -Recurse -Filter embed_consumer.exe $embedBuild | Select-Object -First 1).FullName
    if (-not $binary) {
        Write-Error "check-embed.ps1: embed_consumer.exe not found under $embedBuild"
        exit 1
    }
    $dllPaths = Get-ChildItem -Recurse -Filter *.dll -Path $embedBuild -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty FullName
    Show-RuntimeLayout $binary $dllPaths

    $exitCode = Invoke-BoundedRun $binary 20
    if ($exitCode -ne 0) {
        Write-Error ("check-embed.ps1: embed_consumer.exe exited with code {0} - see the runtime layout and the captured stdout/stderr above for why" -f $exitCode)
        exit 1
    }
}

function Assert-GeneratedHeadersScoped([string]$embedBuild) {
    $scopedHeader = Join-Path $embedBuild "glintfx-build/generated/include/glintfx/version_macros.hpp"
    $leakedHeader = Join-Path $embedBuild "generated/include/glintfx/version_macros.hpp"

    if (-not (Test-Path $scopedHeader)) {
        Write-Error "check-embed.ps1: generated header missing where the embedded subdirectory should have put it: $scopedHeader"
        exit 1
    }
    if (Test-Path $leakedHeader) {
        Write-Error "check-embed.ps1: generated header leaked into the consumer's own top-level build dir: $leakedHeader"
        exit 1
    }
    Write-Host "check-embed.ps1: generated headers scoped correctly under glintfx-build/"
}

function Assert-InstallDoesNotLeakHeaders([string]$embedBuild, [string]$scratchPrefix) {
    cmake --install $embedBuild --prefix $scratchPrefix *> $null
    $leaked = Join-Path $scratchPrefix "include/glintfx"
    if (Test-Path $leaked) {
        Write-Error "check-embed.ps1: glintfx headers were installed by an embedded consumer's install target (GLINTFX_INSTALL guard not honored)"
        exit 1
    }
    Write-Host "check-embed.ps1: embedded install did not leak glintfx headers (GLINTFX_INSTALL default respected)"
}

# Opposite assertion of Assert-InstallDoesNotLeakHeaders() above: here the
# consumer explicitly opted in (GLINTFX_INSTALL=ON), so the headers and
# the CMake package MUST appear. This is the only path that exercises
# glintfx's install() rules under this script (FIX-CONSUMO-2, achado
# QA-2), same as check_embed.sh's assert_install_opt_in_installs_glintfx().
function Assert-InstallOptInInstallsGlintfx([string]$embedBuild, [string]$scratchPrefix) {
    cmake --install $embedBuild --prefix $scratchPrefix
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $header = Join-Path $scratchPrefix "include/glintfx/core/version.hpp"
    if (-not (Test-Path $header)) {
        Write-Error "check-embed.ps1: glintfx header missing after GLINTFX_INSTALL=ON opt-in install: $header"
        exit 1
    }

    $configFile = Get-ChildItem -Recurse -Filter glintfxConfig.cmake -Path $scratchPrefix -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $configFile) {
        Write-Error "check-embed.ps1: glintfx CMake package (glintfxConfig.cmake) missing after GLINTFX_INSTALL=ON opt-in install under $scratchPrefix"
        exit 1
    }
    Write-Host "check-embed.ps1: embedded install with GLINTFX_INSTALL=ON opt-in installed glintfx headers and CMake package"
}

# Orchestrates the opt-in pass in its own scratch build/prefix, separate
# from the default pass above (GLINTFX_INSTALL is a cache variable fixed
# at configure time, so it needs its own build directory).
function Invoke-CheckInstallOptIn([string]$embedSrc, [string]$glintfxSrc, [string]$scratch) {
    $optInBuild = Join-Path $scratch "embed-build-install-opt-in"
    $optInPrefix = Join-Path $scratch "prefix-install-opt-in"

    Invoke-ConfigureEmbedWithInstallOptIn $embedSrc $optInBuild $glintfxSrc
    Invoke-BuildEmbed $optInBuild
    Assert-InstallOptInInstallsGlintfx $optInBuild $optInPrefix
}

function New-ScratchWorkdir() {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("glintfx-embed-" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    return $dir
}

$scratch = New-ScratchWorkdir
try {
    $embedBuild = Join-Path $scratch "embed-build"
    $scratchPrefix = Join-Path $scratch "prefix"

    Invoke-ConfigureEmbed $EmbedSrcDir $embedBuild $GlintfxSourceDir
    Invoke-BuildEmbed $embedBuild
    Invoke-RunEmbed $embedBuild
    Assert-GeneratedHeadersScoped $embedBuild
    Assert-InstallDoesNotLeakHeaders $embedBuild $scratchPrefix

    Invoke-CheckInstallOptIn $EmbedSrcDir $GlintfxSourceDir $scratch

    Write-Host "ok: glintfx consumed successfully via add_subdirectory (embedding)."
}
finally {
    Remove-Item -Recurse -Force $scratch -ErrorAction SilentlyContinue
}
