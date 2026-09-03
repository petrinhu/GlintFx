# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/preci.ps1 - Windows mirror of tools/preci.sh (GODS_LAWS.md L-04,
# item reopened as FUND-4 pela lei de paridade). Read tools/preci.sh's
# own header comment first - it is the specification this file mirrors,
# and its decisions (piso de varredura nao-vazia, no cega
# -ErrorAction SilentlyContinue, one process per real command rather
# than per scanned item) apply here identically, GODS_LAWS.md L-40.
#
# LANGUAGE DECISION (documented once, so it is not re-litigated per
# fatia): PowerShell, not Python, even though two gates in this tree
# were just ported shell -> python3 for the OPPOSITE reason (tests/
# tools/check_spdx.py, check_hygiene_coverage.py, check_dep_zero_
# trace.py carry real cross-platform GATE LOGIC that has to run
# identically on five platforms, so one python3 implementation beats
# five per-shell copies). This script carries NO gate logic of its own -
# every real check it runs is either `cmake`/`ctest` directly, or one of
# the FIVE .ps1 scripts already living in tools/ci/ (check-consume.ps1,
# check-embed.ps1, check-pkgconfig-installed.ps1, diagnose-win-*.ps1),
# which are themselves already PowerShell because that is what the
# `windows` job of .github/workflows/ci.yml runs (`shell: pwsh` on every
# step). Writing this orchestrator in Python would not remove a
# duplicate implementation (there is none to remove - it calls the real
# scripts, it does not reimplement them); it would only add a SECOND
# scripting language to a Windows lane that is 100% PowerShell today,
# for zero gain in the one place a Windows contributor needs the tool to
# feel native (double-clickable, debuggable in the PowerShell ISE/VS
# Code PowerShell extension, no separate interpreter to explain).
#
# SCOPE, DECLARED (GODS_LAWS.md L-40: a mirror that claims to cover more
# than it does is worse than one that admits a gap):
#
#   COVERED - everything the `windows` job of .github/workflows/ci.yml
#   actually runs, for both matrix entries (shared and static):
#   untracked-source guard (mirrors stage_untracked_guard in
#   preci.sh), configure + build with -DGLINTFX_WERROR=ON, the full
#   ctest suite (which ALREADY exercises check_spdx.py/check_hygiene_
#   coverage.py/check_dep_zero_trace.py - tests/CMakeLists.txt registers
#   all three WITHOUT an if(UNIX) guard, so this script does not need to
#   call them a second time), the win32_runner_probe_test non-empty
#   verbose check (mirrors the ci.yml step commented X-0-LOG), and the
#   three windows-only consumption gates (check-consume.ps1,
#   check-pkgconfig-installed.ps1, check-embed.ps1) via the exact same
#   scripts the server calls.
#
#   NOT COVERED, ON PURPOSE, WITH THE MEASURED REASON:
#
#   - clang-format / clang-tidy / cppcheck / NOLINT-justification /
#     gitleaks / the GATE-DEBUG stage (preci.sh's stage_tidy,
#     stage_cppcheck, stage_nolint_justification, stage_gitleaks,
#     stage_debug). These are NOT skipped for lack of a Windows build of
#     the tools (several have one); they are out of scope because the
#     SERVER itself never runs them on Windows - CLAUDE.md's own "Estado
#     atual do repositorio" section states the `lint`, `sanitizer`,
#     `gitleaks` and (per TODO.md GATE-DEBUG) `debug` jobs run "so no
#     alvo primario" (Fedora), regardless of which OS authored the push.
#     A Windows preci mirror exists to run BEFORE PUSH what the server
#     WILL run; adding a stage the server never runs on this OS would
#     not be paridade, it would be scope inflation past what L-04 asks
#     for.
#
#   - ASan/UBSan (preci.sh's stage_sanitizer, GLINTFX_SANITIZE). MSVC
#     has had /fsanitize=address since Visual Studio 2019 16.9 - but
#     TWO separate gaps stop this script from offering it today, and
#     both are declared here rather than papered over with a stage that
#     would silently do nothing:
#       1. cmake/GlintfxCompileOptions.cmake wires -fsanitize=... behind
#          `$<CXX_COMPILER_ID:GNU,Clang>` only (see GLINTFX_SANITIZE's
#          own handling there) - MSVC is not in that generator
#          expression at all, so passing -DGLINTFX_SANITIZE=address on
#          this toolchain today is a SILENT NO-OP: the option is
#          accepted, the flag never reaches the compiler, and a stage
#          built around it would print green having sanitized nothing -
#          exactly the defect GODS_LAWS.md L-40 exists to forbid.
#          Wiring MSVC's own /fsanitize=address flag is a cmake/ change,
#          outside this file's declared scope (tools/ only per this
#          fatia's briefing); it is real follow-up work, not done here.
#       2. Undefined-behavior sanitization has NO MSVC equivalent at
#          all, at any Visual Studio version, measured against current
#          Microsoft documentation, not assumed - so even after gap 1 is
#          closed, only the ASan half of preci.sh's sanitizer stage
#          could ever exist on Windows. The other half (UBSan) stays
#          permanently absent here, not a temporary gap to backfill.
#     No Windows machine exists anywhere in this repository's own
#     development environment (same standing fact CI-WIN-* comments
#     elsewhere in this tree already note) to build and prove an MSVC
#     ASan flag against even if gap 1 were closed today - this script is
#     written by reading tools/preci.sh, cmake/GlintfxCompileOptions.cmake
#     and .github/workflows/ci.yml, never run here, and this header says
#     so instead of implying otherwise.
#
# Usage:
#   pwsh -NoProfile -File tools\preci.ps1
#                                     full mirror, both matrix entries
#                                     (shared then static).
#   pwsh -NoProfile -File tools\preci.ps1 -Modo shared
#                                     only the shared-library entry.
#   pwsh -NoProfile -File tools\preci.ps1 -Modo static
#                                     only the static-library entry.
#
# Invoke it as `pwsh -File`, not by dot-sourcing or typing `.\preci.ps1`
# straight into an interactive session: PowerShell's `exit` terminates
# the WHOLE host process when run non-interactively, which is exactly
# the property Invoke-ChildScript below relies on to keep this script's
# own process alive while a called tools/ci/*.ps1 helper's own internal
# `exit` runs (see that function's comment) - typing this script
# directly into an already-open interactive pwsh window instead can, on
# some hosts, close that window's whole session the same way.
#
# Must run in a shell where the MSVC x64 toolchain is already on PATH
# (a "Developer PowerShell for VS 2022", or a plain PowerShell after
# running VC\Auxiliary\Build\vcvarsall.bat x64) - Test-CompilerEnvironment
# below checks for this and fails with that exact guidance rather than
# falling through to a confusing CMake/Ninja error later (same failure
# mode ci.yml's own CI-WIN-GEN comment documents live against the
# server's own runner, before that job carried its own dedicated
# "Preparar ambiente do compilador" step to avoid it).
#
# Each function below does one thing (GODS_LAWS.md L-17).

param(
    [ValidateSet("shared", "static", "both")]
    [string]$Modo = "both"
)

$ErrorActionPreference = "Stop"

function Write-Stage([string]$name) {
    Write-Host "== $name =="
}

function Fail([string]$message) {
    Write-Error "preci.ps1: $message"
    exit 1
}

# Same resolution shape as preci.sh's resolve_root_dir: the script's own
# directory, one level up (tools/.. -> repo root).
function Get-RootDir {
    return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

# Fails loud on a missing tool BEFORE any stage runs, instead of letting
# an unrelated-looking "not recognized as a cmdlet" error surface deep
# inside some later stage. `pwsh` itself is required here even though
# this script already runs under it: Invoke-ChildScript below spawns
# every tools/ci/*.ps1 helper as its OWN child `pwsh` process (see that
# function's own comment for why), so `pwsh` has to be resolvable on
# PATH exactly like git/cmake/ninja do.
function Test-Prerequisites {
    foreach ($tool in @("git", "cmake", "ninja", "pwsh")) {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
            Fail "'$tool' nao esta no PATH - obrigatorio para este mirror (mesmas ferramentas que .github/workflows/ci.yml usa no job 'windows')."
        }
    }
}

# Compiler environment check (see header comment above). Get-Command is
# used instead of a bare `cl.exe --version` invocation because cl.exe
# with NO arguments still prints its banner and exits 0 vs argument
# errors depending on version - Get-Command only asks "is it resolvable
# on PATH", which is the actual precondition Ninja needs (CI-WIN-GEN).
function Test-CompilerEnvironment {
    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        Fail "cl.exe nao esta no PATH. Rode este script de um 'Developer PowerShell for VS 2022', ou rode VC\Auxiliary\Build\vcvarsall.bat x64 nesta janela antes (mesma preparacao que .github/workflows/ci.yml faz no passo 'Preparar ambiente do compilador (MSVC x64)')."
    }
}

# CMake minimum version is read from CMakeLists.txt itself, never
# hardcoded here - this project's own CLAUDE.md documents, by name, a
# real defect (item DOC-ESTADO) caused by a number copied into a comment
# instead of measured: it rotted twice before the rule "read the
# command, never the value" replaced it. cmake_minimum_required's own
# VERSION argument is the single source of truth for the floor.
function Get-CMakeMinimumVersion([string]$rootDir) {
    $listFile = Join-Path $rootDir "CMakeLists.txt"
    $match = Select-String -Path $listFile -Pattern 'cmake_minimum_required\(VERSION\s+([0-9]+(?:\.[0-9]+)*)\)' | Select-Object -First 1
    if (-not $match) {
        Fail "nao foi possivel ler cmake_minimum_required de $listFile"
    }
    return $match.Matches[0].Groups[1].Value
}

function Test-CMakeVersionFloor([string]$rootDir) {
    $floor = Get-CMakeMinimumVersion $rootDir
    $raw = (cmake --version | Select-Object -First 1) -replace '^cmake version\s+', ''
    if ([version]$raw -lt [version]$floor) {
        Fail "cmake $raw esta abaixo do piso $floor que $rootDir\CMakeLists.txt declara (cmake_minimum_required). Instale um cmake mais novo antes de continuar."
    }
    Write-Host "preci.ps1: cmake $raw (piso $floor de CMakeLists.txt) OK"
}

# Mirrors preci.sh's enumerate_untracked_cpp_hpp / require_no_untracked_
# source (stage_untracked_guard), GODS_LAWS.md L-40 PRECI-UNTRACKED: a
# brand-new *.cpp/*.hpp never `git add`ed is invisible to every gate
# that only walks `git ls-files`. `--exclude-standard` respects
# .gitignore exactly like `git status` (build directories never block).
# tests/preci_fixtures is excluded for the same reason preci.sh excludes
# it: it exists on purpose in a state that would otherwise trip this
# guard, and neither preci.sh's nor this script's real pipeline is
# supposed to be gated by fixture content.
function Test-UntrackedGuard([string]$rootDir) {
    Write-Stage "estagio 0: guarda de arquivo novo nao rastreado"
    $listing = git -C $rootDir ls-files --others --exclude-standard -- '*.cpp' '*.hpp' ':!:tests/preci_fixtures/*'
    if ($LASTEXITCODE -ne 0) {
        Fail "'git ls-files --others' falhou em '$rootDir' - varredura recusada, nunca presumida vazia (GODS_LAWS.md L-40)"
    }
    $files = @($listing | Where-Object { $_ -ne "" })
    if ($files.Count -gt 0) {
        Write-Host "untracked-guard: $($files.Count) arquivo(s) *.cpp/*.hpp novo(s), fora do controle de versao:" -ForegroundColor Red
        $files | ForEach-Object { Write-Host "  $_" }
        Fail "estagio untracked-guard recusado - rode 'git add' antes de preci.ps1 (GODS_LAWS.md L-40)"
    }
    Write-Host "untracked-guard: 0 arquivo(s) *.cpp/*.hpp novo(s) fora do controle de versao"
}

# ctest -N reports the registered test count WITHOUT running anything -
# same piso as preci.sh's require_nonempty_tests: a build with zero
# registered tests exits 0 and prints "No tests were found!!!", which a
# bare exit-code check would never catch (this is the exact gap that
# reproved preci.sh's first revision under adversarial review, FUND-4).
function Assert-NonEmptyTests([string]$stageName, [string]$buildDir, [string[]]$extraArgs) {
    $listOutput = & ctest --test-dir $buildDir -N @extraArgs
    $totalLine = $listOutput | Select-String -Pattern 'Total Tests:\s*(\d+)'
    $total = if ($totalLine) { [int]$totalLine.Matches[0].Groups[1].Value } else { 0 }
    if ($total -eq 0) {
        Fail "$stageName recusado (varredura vazia de testes, GODS_LAWS.md L-40)"
    }
    Write-Host "$stageName: $total teste(s) varrido(s)"
}

function Invoke-ConfigureAndBuild([string]$rootDir, [string]$buildDir, [string]$sharedFlag) {
    Write-Host "preci.ps1: cmake -S $rootDir -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Release -DGLINTFX_WERROR=ON $sharedFlag"
    $configureArgs = @("-S", $rootDir, "-B", $buildDir, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DGLINTFX_WERROR=ON", "-DGLINTFX_BUILD_TESTS=ON")
    if ($sharedFlag) { $configureArgs += $sharedFlag }
    & cmake @configureArgs
    if ($LASTEXITCODE -ne 0) { Fail "configure falhou ($buildDir)" }
    & cmake --build $buildDir
    if ($LASTEXITCODE -ne 0) { Fail "build falhou ($buildDir)" }
}

# Mirrors the ci.yml windows job step commented X-0-LOG: --output-on-
# failure (used by the ctest run right below) only prints output for a
# test that FAILS, so win32_runner_probe_test PASSING never reaches any
# log by itself - measured live on the server (run 33642256048) before
# this step existed there. Zero tests matching this filter is NEVER
# legitimate inside a real Windows build (the probe is registered under
# if(WIN32) with no other guard), so this is a hard gate, not a
# diagnostic.
function Test-Win32RunnerProbe([string]$buildDir) {
    Assert-NonEmptyTests "win32_runner_probe_test" $buildDir @("-R", "win32_runner_probe_test")
    & ctest --test-dir $buildDir -V -R win32_runner_probe_test
    if ($LASTEXITCODE -ne 0) { Fail "win32_runner_probe_test reprovou" }
}

function Invoke-Ctest([string]$buildDir) {
    Assert-NonEmptyTests "ctest" $buildDir @()
    & ctest --test-dir $buildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) { Fail "ctest reprovou ($buildDir)" }
}

# GODS_LAWS.md L-17 sibling hardening, own to this file: check-consume.
# ps1/check-pkgconfig-installed.ps1/check-embed.ps1 (and diagnose-win-
# runtime.ps1) each finish with `exit $LASTEXITCODE`/`exit 0` at their
# OWN top level, because .github/workflows/ci.yml runs each of them as
# the ONLY script inside its own `shell: pwsh` step - one process each,
# `exit` there correctly ends just that one process. Calling one of
# those scripts in-process via the `&` call operator from INSIDE this
# orchestrator would not have that isolation: PowerShell's `exit`, run
# non-interactively (the only way preci.ps1 itself is meant to be
# invoked - see Usage in the header), terminates the WHOLE running
# pwsh.exe process, not just the called script's own scope - so the
# very first in-process `exit 0` inside diagnose-win-runtime.ps1 would
# silently kill this ENTIRE mirror right after the first matrix entry's
# build, with exit code 0, having never run ctest, the win32 probe, or
# either consumption gate. That is a varredura-vazia defect of exactly
# the shape GODS_LAWS.md L-40 exists to forbid, just self-inflicted by
# this file instead of by a scanned-zero stage. Spawning each helper as
# its OWN child `pwsh` process sidesteps this entirely: a child
# process's exit code lands in $LASTEXITCODE like any other external
# command (cmake, ctest, git above), and never touches this
# orchestrator's own process. Never proven on a real Windows machine
# (no compiler or pwsh runtime anywhere in this repository's own
# development environment) - this is the conservative, documented-
# behavior choice, not a guess dressed as a measurement.
function Invoke-ChildScript([string]$scriptPath, [string[]]$scriptArgs) {
    & pwsh -NoProfile -File $scriptPath @scriptArgs
    return $LASTEXITCODE
}

# Reuses check-consume.ps1's own GITHUB_OUTPUT-shaped extension point
# (its Write-PrefixOutput only writes when $env:GITHUB_OUTPUT is set)
# instead of touching that script at all: pointing $env:GITHUB_OUTPUT at
# a throwaway local file for the duration of this one call lets this
# script read back the SAME installed prefix check-pkgconfig-
# installed.ps1 needs next, with zero modification to either CI script -
# the two stay byte-identical to what .github/workflows/ci.yml itself
# runs, which is the whole point of a mirror. The env var is inherited
# by the child pwsh process spawned via Invoke-ChildScript above (child
# processes inherit their parent's environment by default), so the
# same file-based handoff CI itself uses (GITHUB_ACTIONS sets this
# exact variable) works unmodified here too.
function Invoke-ConsumeAndPkgconfig([string]$ciDir, [string]$buildDir) {
    $outFile = New-TemporaryFile
    $previousOutput = $env:GITHUB_OUTPUT
    try {
        $env:GITHUB_OUTPUT = $outFile.FullName
        $rc = Invoke-ChildScript (Join-Path $ciDir "check-consume.ps1") @("-BuildDir", $buildDir, "-PackageSrcDir", "tests/package")
        if ($rc -ne 0) { Fail "check-consume.ps1 reprovou ($buildDir)" }
        $prefixLine = Get-Content $outFile.FullName | Select-String '^prefix='
        if (-not $prefixLine) {
            Fail "check-consume.ps1 nao escreveu 'prefix=' em GITHUB_OUTPUT - nao ha prefixo instalado para check-pkgconfig-installed.ps1 ler"
        }
        $prefix = $prefixLine.ToString() -replace '^prefix=', ''
        $rc = Invoke-ChildScript (Join-Path $ciDir "check-pkgconfig-installed.ps1") @("-InstalledPrefix", $prefix)
        if ($rc -ne 0) { Fail "check-pkgconfig-installed.ps1 reprovou ($prefix)" }
    }
    finally {
        $env:GITHUB_OUTPUT = $previousOutput
        Remove-Item -Force $outFile.FullName -ErrorAction SilentlyContinue
    }
}

function Invoke-Embed([string]$ciDir, [string]$rootDir) {
    $rc = Invoke-ChildScript (Join-Path $ciDir "check-embed.ps1") @("-GlintfxSourceDir", $rootDir, "-EmbedSrcDir", (Join-Path $rootDir "tests\embed"))
    if ($rc -ne 0) { Fail "check-embed.ps1 reprovou" }
}

# One full matrix entry (shared or static), same command shape as the
# `windows` job of .github/workflows/ci.yml, same order of steps.
function Invoke-MatrixEntry([string]$rootDir, [string]$ciDir, [string]$label, [string]$buildDir, [string]$sharedFlag) {
    Write-Stage "modo: $label"

    Write-Stage "build ($label)"
    Invoke-ConfigureAndBuild $rootDir $buildDir $sharedFlag

    Write-Stage "diagnostico de runtime ($label, informativo, nunca reprova)"
    Invoke-ChildScript (Join-Path $ciDir "diagnose-win-runtime.ps1") @("-BuildDir", $buildDir) | Out-Null

    Write-Stage "testes ($label)"
    Invoke-Ctest $buildDir

    Write-Stage "sonda win32 ($label, verbosa)"
    Test-Win32RunnerProbe $buildDir

    Write-Stage "consumo instalado + pkg-config ($label)"
    Invoke-ConsumeAndPkgconfig $ciDir $buildDir

    Write-Stage "consumo embutido ($label)"
    Invoke-Embed $ciDir $rootDir

    Write-Host "preci.ps1: modo $label VERDE"
}

function Main {
    $rootDir = Get-RootDir
    $ciDir = Join-Path $rootDir "tools\ci"

    Test-Prerequisites
    Test-CompilerEnvironment
    Test-CMakeVersionFloor $rootDir
    Test-UntrackedGuard $rootDir

    if ($Modo -eq "shared" -or $Modo -eq "both") {
        Invoke-MatrixEntry $rootDir $ciDir "compartilhado" (Join-Path $rootDir "build-shared") ""
    }
    if ($Modo -eq "static" -or $Modo -eq "both") {
        Invoke-MatrixEntry $rootDir $ciDir "estatico" (Join-Path $rootDir "build-static") "-DBUILD_SHARED_LIBS=OFF"
    }

    Write-Host "preci.ps1: TUDO VERDE (escopo declarado no cabecalho deste arquivo - clang-tidy/cppcheck/gitleaks/GATE-DEBUG/sanitizer NAO cobertos, ver comentario)"
}

Main
