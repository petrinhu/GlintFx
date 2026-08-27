# SPDX-License-Identifier: AGPL-3.0-or-later
# diagnose-win-pkgconfig.ps1 - PKG-WIN-SCOPE.
#
# Permanent, judgment-free diagnostic step for the `windows` job. It
# COLLECTS data and never JUDGES it - always exits 0, explicitly (never
# relying on the absence of a nonzero $LASTEXITCODE, which would mask a
# genuine syntax error in this script itself as a silent pass).
#
# WHY THIS STAYS PERMANENT NOW THAT glintfx.pc IS INSTALLED ON WINDOWS
# AGAIN (PKG-WIN-SCOPE, revertido por ordem do lider, 27/08/2026): this
# is the script that turned a first, reasoned-only guess (PKG-VALIDATE's
# PKG_CONFIG_PATH separator theory, cmake_path(CONVERT ...
# TO_NATIVE_PATH_LIST ...) applied with no Windows machine anywhere in
# this repository's own development environment to test it against)
# into a MEASURED fact: run 33050708122, job "Windows - compartilhado",
# found exactly one pkg-config on that runner's PATH (Strawberry Perl's
# Pure-Perl `pkg-config.bat`), and its closed five-cell matrix showed
# that a bare, unseparated PKG_CONFIG_PATH value fails there while the
# same value with a trailing native ';' succeeds -
# cmake/GlintfxPkgConfigValidateInstalled.cmake.in's own PKG_CONFIG_PATH
# construction now applies exactly that fix. This script keeps running
# on every push, unconditionally, for the same reason it was born
# judgment-free: the NEXT pkg-config-on-Windows question - a different
# GHA image swapping which pkg-config ships by default, a packager
# reporting an odd MSYS2 interaction, or simple curiosity about what
# windows-latest carries next year - stays a log read, not a fifth
# guess.
#
# Usage: diagnose-win-pkgconfig.ps1
#
# Each function below does one thing (GODS_LAWS.md L-17).

$ErrorActionPreference = "Continue"

function Write-Section([string]$title) {
    Write-Host ""
    Write-Host "=== diagnose-win-pkgconfig.ps1: $title ==="
}

# Item 1: Get-Command pkg-config, pkgconf - full path, version, and the
# count of binaries found. Zero is a printed FACT, not a failure: this
# script never decides pass/fail.
function Show-PkgConfigBinaries() {
    Write-Section "binarios pkg-config/pkgconf no PATH deste executor"
    $found = @(Get-Command -Name pkg-config, pkgconf -ErrorAction SilentlyContinue)
    Write-Host "contagem de binarios achados: $($found.Count)"
    if ($found.Count -eq 0) {
        Write-Host "nenhum pkg-config/pkgconf no PATH - fato impresso, nao falha."
        return
    }
    foreach ($cmd in $found) {
        Write-Host "--- $($cmd.Name) ---"
        Write-Host "caminho completo: $($cmd.Source)"
        $versionOutput = & $cmd.Source --version 2>&1
        Write-Host "versao (exit $LASTEXITCODE): $versionOutput"
    }
}

# Item 2: PKG_CONFIG_PATH and PKG_CONFIG_LIBDIR, verbatim, as this
# executor already had them BEFORE this script touches anything.
function Show-PreexistingEnvironment() {
    Write-Section "PKG_CONFIG_PATH e PKG_CONFIG_LIBDIR pre-existentes do executor, verbatim"
    Write-Host "PKG_CONFIG_PATH=[$($env:PKG_CONFIG_PATH)]"
    Write-Host "PKG_CONFIG_LIBDIR=[$($env:PKG_CONFIG_LIBDIR)]"
}

# One glintfx.pc-shaped fixture, planted once, read by all five calls
# in the matrix below. Built from an array of single-quoted lines, on
# purpose, not a here-string: the pkg-config `${var}` tokens below have
# to reach the file as LITERAL text, for pkg-config's own parser to
# expand at query time - single-quoted strings need no backtick-escaping
# of the `$` to avoid PowerShell's own variable interpolation doing that
# expansion (wrongly, to empty) before the file is even written.
function New-PkgConfigFixture() {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("glintfx-diagnose-pkgconfig-" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $prefixLine = "prefix=" + ($dir -replace '\\', '/')
    $lines = @(
        $prefixLine,
        'exec_prefix=${prefix}',
        'libdir=${exec_prefix}/lib',
        'includedir=${prefix}/include',
        '',
        'Name: glintfx',
        'Description: diagnostic fixture, not the real glintfx.pc',
        'Version: 0.1.0.0',
        'Cflags: -I${includedir}',
        'Libs: -L${libdir} -lglintfx'
    )
    Set-Content -Path (Join-Path $dir "glintfx.pc") -Value $lines
    return $dir
}

# One cell of the matrix: sets PKG_CONFIG_PATH to the given value for
# the duration of one query, runs it, restores the previous value, and
# prints exit code and stderr/stdout verbatim - never throws, never
# judges pass/fail.
function Invoke-PkgConfigMatrixCell([string]$pkgConfigExe, [string]$cellName, [string]$pkgConfigPathValue, [string[]]$extraArgs) {
    Write-Host ""
    Write-Host "--- celula: $cellName ---"
    Write-Host "PKG_CONFIG_PATH usado: [$pkgConfigPathValue]"
    $oldValue = $env:PKG_CONFIG_PATH
    $env:PKG_CONFIG_PATH = $pkgConfigPathValue
    try {
        # $extraArgs can arrive as an empty array (a known PowerShell
        # parameter-binding quirk sometimes collapses that to $null) -
        # guarded explicitly instead of trusting `+ $extraArgs` to be a
        # no-op, so an empty cell never smuggles a stray $null argument
        # into the native pkg-config/pkgconf invocation below.
        $allArgs = @("--print-errors", "--exists", "glintfx")
        if ($extraArgs) {
            $allArgs += $extraArgs
        }
        $output = & $pkgConfigExe @allArgs 2>&1
        Write-Host "codigo de saida: $LASTEXITCODE"
        Write-Host "stderr/stdout combinado: $output"
    }
    finally {
        $env:PKG_CONFIG_PATH = $oldValue
    }
}

# Item 3: the closed, four-cell matrix - {barra normal, barra invertida}
# x {separador ; , separador :} - plus a fifth call with --define-prefix,
# all against the SAME fixture, all for ONE pkg-config/pkgconf binary.
# A trailing separator is appended in each of the four cells on purpose:
# it is what turned a genuinely correct install into a reported failure
# in PKG-VALIDATE (a bare drive letter, "C:/...", already contains a
# ':' before any list-separator logic runs - see this script's own file
# header) - a bare path with no trailing separator at all would never
# have exercised that failure shape.
function Invoke-PkgConfigMatrix([string]$pkgConfigExe) {
    Write-Section "matriz fechada contra fixture glintfx.pc ($pkgConfigExe)"
    $fixtureDir = New-PkgConfigFixture
    try {
        $forwardSlashDir = $fixtureDir -replace '\\', '/'
        $backslashDir = $fixtureDir

        Invoke-PkgConfigMatrixCell $pkgConfigExe "barra normal, separador ;" "${forwardSlashDir};" @()
        Invoke-PkgConfigMatrixCell $pkgConfigExe "barra normal, separador :" "${forwardSlashDir}:" @()
        Invoke-PkgConfigMatrixCell $pkgConfigExe "barra invertida, separador ;" "${backslashDir};" @()
        Invoke-PkgConfigMatrixCell $pkgConfigExe "barra invertida, separador :" "${backslashDir}:" @()
        Invoke-PkgConfigMatrixCell $pkgConfigExe "quinta chamada: --define-prefix" "$forwardSlashDir" @("--define-prefix")
    }
    finally {
        Remove-Item -Recurse -Force $fixtureDir -ErrorAction SilentlyContinue
    }
}

function Invoke-Main() {
    Show-PkgConfigBinaries
    Show-PreexistingEnvironment

    $found = @(Get-Command -Name pkg-config, pkgconf -ErrorAction SilentlyContinue)
    if ($found.Count -eq 0) {
        Write-Section "matriz fechada contra fixture glintfx.pc"
        Write-Host "pulada - nenhum pkg-config/pkgconf no PATH deste executor para rodar a matriz contra."
        return
    }

    foreach ($cmd in $found) {
        Invoke-PkgConfigMatrix $cmd.Source
    }
}

Invoke-Main

# Sempre 0, explicito (GODS_LAWS.md, cabecalho deste arquivo): este
# script colhe dado, nunca julga.
exit 0
