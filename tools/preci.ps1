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
#   pwsh -NoProfile -File tools\preci.ps1 -Selftest
#                                     proves the untracked-source guard
#                                     and the ctest-count floor against
#                                     disposable fixtures (throwaway git
#                                     repos under $env:TEMP, synthetic
#                                     `ctest -N` text) - never against
#                                     $rootDir or a real build. Never
#                                     runs any other stage of this file.
#                                     See the "PRECI-WIN-SELFTEST" block
#                                     right before Invoke-SelfTest below
#                                     for exactly what this covers and
#                                     what it declares out of scope,
#                                     GODS_LAWS.md L-36/L-40.
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
    [string]$Modo = "both",

    # -Selftest short-circuits Main entirely (see the bottom of this
    # file) - it never touches $Modo, $rootDir, or any build directory.
    [switch]$Selftest
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
# Split out of Test-UntrackedGuard below (PRECI-WIN-SELFTEST, same
# enumerate-vs-decide-vs-fail-loud split as preci.sh's enumerate_
# untracked_cpp_hpp / require_no_untracked_source / stage_untracked_
# guard trio) so -Selftest can exercise the git listing in isolation,
# against a disposable directory, without the process-ending `Fail`
# call below ever firing mid-selftest. A real failure of the underlying
# `git` command (not a repo, git missing) THROWS - it is never allowed
# to collapse into "found nothing, so pass" (GODS_LAWS.md L-40); Test-
# UntrackedGuard turns that into the same loud Fail() it already used
# before this split, unchanged.
function Get-UntrackedCppHpp([string]$rootDir) {
    $listing = git -C $rootDir ls-files --others --exclude-standard -- '*.cpp' '*.hpp' ':!:tests/preci_fixtures/*'
    if ($LASTEXITCODE -ne 0) {
        throw "'git ls-files --others' falhou em '$rootDir' - varredura recusada, nunca presumida vazia (GODS_LAWS.md L-40)"
    }
    return @($listing | Where-Object { $_ -ne "" })
}

function Test-UntrackedGuard([string]$rootDir) {
    Write-Stage "estagio 0: guarda de arquivo novo nao rastreado"
    try {
        $files = Get-UntrackedCppHpp $rootDir
    } catch {
        Fail $_.Exception.Message
    }
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
# Split out of Assert-NonEmptyTests below (PRECI-WIN-SELFTEST) so
# -Selftest can prove the "Total Tests: N" parse against synthetic text
# - same downgrade tools/ci/check-dep-zero-win.ps1's own -SelfTest
# already declares and uses (parser proven against documented-shape
# text, never against a real ctest/dumpbin run off the real server).
function Get-CTestTotalCount([string[]]$listOutput) {
    $totalLine = $listOutput | Select-String -Pattern 'Total Tests:\s*(\d+)'
    if ($totalLine) { return [int]$totalLine.Matches[0].Groups[1].Value }
    return 0
}

function Assert-NonEmptyTests([string]$stageName, [string]$buildDir, [string[]]$extraArgs) {
    $listOutput = & ctest --test-dir $buildDir -N @extraArgs
    $total = Get-CTestTotalCount $listOutput
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

# --- PRECI-WIN-SELFTEST (03/09/2026, GODS_LAWS.md L-36/L-40 - "um
# portao so conta depois de PROVADO vermelho", "zero e sinal de
# varredura quebrada"): this file existed since its own header's dated
# comments with no selftest and no CI step calling it at all - measured,
# not assumed (`grep -c 'preci.ps1' .github/workflows/ci.yml` returned
# 0 before this fatia). -Selftest closes that: it is wired into the
# `windows` job below, and proves the two pisos this file enforces
# BEFORE any push relies on them, the same discipline tools/preci.sh's
# own --selftest already applies to its Linux twin.
#
# COVERED, against disposable fixtures only - never $rootDir, never a
# real build:
#   - the untracked-source guard (Get-UntrackedCppHpp): positive (a
#     clean throwaway repo passes), negative (one loose *.cpp is
#     flagged), ignored (a *.cpp under a .gitignore'd build/ never
#     blocks), git-failure (a non-repo directory throws loud, never
#     "found nothing, so pass"). This is the minimum the task asked
#     for: proving the guard actually bites.
#   - the ctest-count floor (Get-CTestTotalCount): positive (a "Total
#     Tests: N" line with N>0 parses to N), empty-scan (text with no
#     such line, or N=0, parses to 0 and Assert-NonEmptyTests's own
#     caller would then refuse it).
#
# NOT COVERED, WITH THE MEASURED REASON (GODS_LAWS.md L-40: a mirror
# that claims more than it proves is worse than one that admits a gap):
#
#   - clang-format / clang-tidy / cppcheck / gitleaks / GATE-DEBUG
#     fixture controls (preci.sh's run_selftest_positive_control /
#     _negative_control and its assert-count controls). This file's own
#     header ("NOT COVERED, ON PURPOSE") already declares these stages
#     entirely out of scope for the Windows mirror - there is no stage
#     here to selftest.
#
#   - a ROOT_DIR cd-failure control (preci.sh's run_selftest_rootdir_
#     cd_failure_control, which regression-tests a hand-rolled
#     `readonly ROOT_DIR=$(cd ... )` masking bug, GODS_LAWS.md SC2155
#     lesson). Get-RootDir here has no equivalent hand-rolled construct
#     to regress: $PSScriptRoot is set by the PowerShell host itself
#     when a script runs via -File, and Resolve-Path already throws
#     under $ErrorActionPreference = "Stop" if the path does not exist -
#     there is no silent-mask shape possible in this idiom to prove
#     against.
#
#   - accented-name / embedded-newline hostile filename controls
#     (preci.sh's run_selftest_untracked_guard_accented_name_control /
#     _newline_name_control, closed on Linux by GATE-DEPZERO-NOFORK's
#     `git ls-files -z`, 01/09/2026). Get-UntrackedCppHpp above still
#     calls `git ls-files --others` WITHOUT `-z`, unchanged from before
#     this fatia - it inherits the SAME quoting/newline-splitting
#     exposure the Linux script had before that fix. Closing it here is
#     a real behavioral change to Test-UntrackedGuard, outside this
#     fatia's declared scope (autoteste only, "nenhum trabalho
#     existente pode mudar de comportamento") and unverifiable without a
#     real Windows/pwsh run (this machine has neither) - flagged here as
#     a KNOWN GAP for a dedicated follow-up fatia, not silently absorbed
#     into a "TUDO VERDE" selftest banner.
#
#   - the CTEST_UNIT_LABEL_FILTER substring-match defect preci.sh's own
#     run_selftest_ctest_count_substring_control regression-tests (an
#     unanchored `-L unit` matching a `nonunit` label by substring).
#     This file never filters ctest by LABEL - Test-Win32RunnerProbe
#     uses `-R win32_runner_probe_test` (an exact test NAME, not a
#     label), and the general Assert-NonEmptyTests call in Invoke-
#     MatrixEntry passes no filter at all - so that specific defect
#     shape does not exist here to regress. The shared risk that DOES
#     apply identically (parsing "Total Tests: N" itself) is exactly
#     what the two Get-CTestTotalCount controls above cover.
#
# Every control below builds its OWN throwaway resource (a git repo
# under $env:TEMP, or a plain string array) and prints PASS/FAIL as it
# goes; Invoke-SelfTest aggregates and Fail()s once at the end if any
# control failed - same shape as tools/ci/check-dep-zero-win.ps1's own
# Invoke-SelfTest, the proven convention for a -SelfTest switch in this
# tree's Windows scripts.

function New-UntrackedGuardSelftestRepo() {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("glintfx-preci-untracked-" + [System.Guid]::NewGuid())
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    # Every git call below is Out-Null'd on purpose, not just the ones
    # known to print: a function's own unpiped output becomes part of
    # ITS return value in PowerShell, and this function's only intended
    # return value is $dir at the bottom - any stray stdout line from
    # git (a version-specific banner, a hint) would otherwise silently
    # ride along.
    git -C $dir init -q | Out-Null
    git -C $dir config user.email "preci-selftest@glintfx.invalid" | Out-Null
    git -C $dir config user.name "preci selftest" | Out-Null
    Set-Content -Path (Join-Path $dir "tracked.cpp") -Value "int tracked_probe() { return 0; }"
    git -C $dir add tracked.cpp | Out-Null
    git -C $dir commit -q -m "selftest: tracked.cpp" | Out-Null
    return $dir
}

function Invoke-SelfTestUntrackedGuardPositiveControl() {
    $dir = New-UntrackedGuardSelftestRepo
    try {
        $files = Get-UntrackedCppHpp $dir
        if ($files.Count -ne 0) {
            Write-Error "selftest: guarda de untracked - controle POSITIVO FALHOU: repositorio so com tracked.cpp committed deveria dar 0 arquivos, obteve $($files.Count): $($files -join ', ')"
            return $false
        }
        Write-Host "selftest: guarda de untracked - controle positivo OK (repositorio limpo aprovado)"
        return $true
    } finally {
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
    }
}

function Invoke-SelfTestUntrackedGuardNegativeControl() {
    $dir = New-UntrackedGuardSelftestRepo
    try {
        Set-Content -Path (Join-Path $dir "solto.cpp") -Value "int solto() { return 1; }"
        $files = Get-UntrackedCppHpp $dir
        if ($files.Count -ne 1 -or $files[0] -ne "solto.cpp") {
            Write-Error "selftest: guarda de untracked - controle NEGATIVO FALHOU: esperava exatamente [solto.cpp], obteve: $($files -join ', ')"
            return $false
        }
        Write-Host "selftest: guarda de untracked - controle negativo OK (solto.cpp, nao rastreado, foi pego)"
        return $true
    } finally {
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
    }
}

# The "nao deve bloquear demais" half: a *.cpp covered by .gitignore
# (the shape of every build/ directory in this tree) must never block,
# or the guard gets disabled within a week - same reasoning as preci.sh's
# own run_selftest_untracked_guard_ignored_control.
function Invoke-SelfTestUntrackedGuardIgnoredControl() {
    $dir = New-UntrackedGuardSelftestRepo
    try {
        Set-Content -Path (Join-Path $dir ".gitignore") -Value "build/"
        New-Item -ItemType Directory -Path (Join-Path $dir "build") -Force | Out-Null
        Set-Content -Path (Join-Path $dir "build/gerado.cpp") -Value "int gerado() { return 2; }"
        $files = Get-UntrackedCppHpp $dir
        if ($files.Count -ne 0) {
            Write-Error "selftest: guarda de untracked - controle de ARQUIVO IGNORADO FALHOU: build/gerado.cpp esta coberto por .gitignore e nao deveria bloquear, obteve $($files.Count): $($files -join ', ')"
            return $false
        }
        Write-Host "selftest: guarda de untracked - controle de arquivo ignorado OK (.gitignore respeitado, nao bloqueou)"
        return $true
    } finally {
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
    }
}

# A REAL failure of the underlying git command (not a repository, git
# missing) must never collapse into "found nothing, so pass"
# (GODS_LAWS.md L-40) - it has to throw, which Test-UntrackedGuard turns
# into a loud Fail(). This control asserts the throw itself, isolated
# from Test-UntrackedGuard's process-ending Fail() call.
function Invoke-SelfTestUntrackedGuardGitFailureControl() {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("glintfx-preci-untracked-nogit-" + [System.Guid]::NewGuid())
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    try {
        $threw = $false
        try {
            Get-UntrackedCppHpp $dir | Out-Null
        } catch {
            $threw = $true
        }
        if (-not $threw) {
            Write-Error "selftest: guarda de untracked - controle de FALHA REAL DE GIT FALHOU: diretorio sem repositorio deveria abortar a varredura (excecao), mas nao lancou nada - isso seria 'nao achei nada, entao passa' (GODS_LAWS.md L-40)"
            return $false
        }
        Write-Host "selftest: guarda de untracked - controle de falha real de git OK (nunca vira aprovacao silenciosa)"
        return $true
    } finally {
        Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue
    }
}

function Invoke-SelfTestCtestCountPositiveControl() {
    $n = Get-CTestTotalCount @("Test project C:/fake", "    Start 1: dummy", "", "Total Tests: 1")
    if ($n -ne 1) {
        Write-Error "selftest: piso de contagem de testes - controle POSITIVO FALHOU: esperava 1, Get-CTestTotalCount devolveu $n"
        return $false
    }
    Write-Host "selftest: piso de contagem de testes - controle positivo OK ('Total Tests: 1' parseado como 1)"
    return $true
}

# Two shapes of "empty", both of which a real caller (Assert-
# NonEmptyTests) must refuse: the line "Total Tests: 0" (a build with
# zero registered tests still prints this line, ctest itself exits 0),
# and text with NO such line at all (ctest's "No tests were found!!!"
# shape). Neither may parse to anything but 0.
function Invoke-SelfTestCtestCountEmptyScanControl() {
    $nZero = Get-CTestTotalCount @("Test project C:/fake", "Total Tests: 0")
    $nMissing = Get-CTestTotalCount @("No tests were found!!!")
    if ($nZero -ne 0 -or $nMissing -ne 0) {
        Write-Error "selftest: piso de contagem de testes - controle de VARREDURA VAZIA FALHOU: esperava 0/0, obteve $nZero/$nMissing"
        return $false
    }
    Write-Host "selftest: piso de contagem de testes - controle de varredura vazia OK (0 testes, com e sem a linha 'Total Tests:', parseado como 0 - Assert-NonEmptyTests recusaria os dois)"
    return $true
}

# Same shape as tools/ci/check-dep-zero-win.ps1's own Invoke-SelfTest:
# each control called by name (not via a scriptblock array - this file
# has never run under a real pwsh, so it stays with the ONE pattern
# already proven in this tree rather than a second, novel one), $ok
# accumulates, one Fail() at the end names every control that failed by
# having already printed its own Write-Error above.
function Invoke-SelfTest() {
    $ok = $true
    if (-not (Invoke-SelfTestUntrackedGuardPositiveControl)) { $ok = $false }
    if (-not (Invoke-SelfTestUntrackedGuardNegativeControl)) { $ok = $false }
    if (-not (Invoke-SelfTestUntrackedGuardIgnoredControl)) { $ok = $false }
    if (-not (Invoke-SelfTestUntrackedGuardGitFailureControl)) { $ok = $false }
    if (-not (Invoke-SelfTestCtestCountPositiveControl)) { $ok = $false }
    if (-not (Invoke-SelfTestCtestCountEmptyScanControl)) { $ok = $false }
    if (-not $ok) {
        Fail "preci.ps1 -Selftest: FALHOU (ver acima) - GODS_LAWS.md L-36/L-40"
    }
    Write-Host "preci.ps1 -Selftest: TODOS OS CONTROLES PASSARAM (escopo declarado no bloco PRECI-WIN-SELFTEST acima deste comentario, no arquivo-fonte)"
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

if ($Selftest) {
    Invoke-SelfTest
} else {
    Main
}
