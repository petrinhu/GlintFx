# SPDX-License-Identifier: AGPL-3.0-or-later
# check-dep-zero-win.ps1 - Windows counterpart of tests/tools/check_dep_
# zero.sh sub-check (c) (the built artifact's own truth). Read
# check_dep_zero.sh's own header comment in full before touching this
# file - it is the specification this script ports, including the
# GODS_LAWS.md L-11 lesson ("no process per item scanned") it already
# paid for once.
#
# GAP THIS CLOSES (DEPZERO-PARITY-WIN, GODS_LAWS.md L-04 reabertura por
# paridade, 03/09/2026): check_dep_zero.sh's sub-check (c) runs `readelf
# -d` on the Linux .so and never touches glintfx.dll - src/platform/
# win32/ (WIN-DISPLAY) links user32 (target_link_libraries(glintfx_
# library PRIVATE user32), src/platform/win32/CMakeLists.txt), and until
# that directory existed there was no Windows import table to inspect.
# There is one now, and it has never been read by anyone.
#
# TOOL: dumpbin.exe, part of the VC.Tools.x86.x64 MSVC component -
# MEASURED against actions/runner-images' own Windows2025-Readme.md
# (fetched 03/09/2026): "Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
# 17.14.36510.44 is installed on windows-latest (= windows-2025). This is
# the EXACT component the workflow's own "Preparar ambiente do compilador
# (MSVC x64)" step already requires via `vswhere -requires
# Microsoft.VisualStudio.Component.VC.Tools.x86.x64` before it runs
# vcvarsall.bat x64 - dumpbin.exe lives in that same component's
# VC\Tools\MSVC\<ver>\bin\Hostx64\x64\ directory, which vcvarsall.bat
# prepends to PATH (exported to every later step via $env:GITHUB_ENV).
# No new environment preparation is added by this script - it relies
# entirely on the step that already runs earlier in the `windows` job.
#
# WHAT THIS PROVES: the DLL's import table (`dumpbin /imports`) names
# only DLLs on the closed IMPORT_ALLOWLIST below. An entry outside it
# means the binary links a third party - GODS_LAWS.md L-07, dependency
# zero - and it does not matter whether check-dep-zero.sh's sibling
# scripts on the other four platforms would ever see it; this is the
# only place the Windows artifact's own truth is read.
#
# DECLARED LIMITATION (this fatia cannot close it - report to the
# orchestrator, not silently worked around): this script was written and
# reviewed for syntax without ever running dumpbin.exe, because this
# machine has no Windows and no MSVC. The parsing logic (Get-
# DumpbinImportedDlls below) is exercised by -SelfTest against SYNTHETIC
# text shaped exactly like documented `dumpbin /imports` output (docs.
# microsoft.com/cpp/build/reference/dumpbin-reference), never against a
# real dumpbin.exe run - that proves the PARSER, not the ALLOWLIST. The
# IMPORT_ALLOWLIST itself is derived from documentation (Microsoft's
# Universal CRT deployment article names the api-ms-win-crt-*.dll
# forwarder family; VCRUNTIME140.dll/VCRUNTIME140_1.dll/MSVCP140.dll are
# the default dynamic C++ runtime redistributables for this toolset) and
# from this tree's own win32/CMakeLists.txt (user32) and KERNEL32.dll
# (every PE image links it) - it has NEVER been measured against the
# real glintfx.dll the way NEEDED_ALLOWLIST in check_dep_zero.sh was
# measured live on 28/08/2026. THE FIRST REAL RED PROOF OF THIS GATE CAN
# ONLY HAPPEN ON THE SERVER: the first CI run of the `windows` job after
# this lands is the debut, and per GODS_LAWS.md D3 (no environment-
# variable escape hatch; the only legitimate fix is a conscious, reviewed
# edit of the allowlist below, cited in the commit) a legitimate CRT DLL
# this list does not yet name is expected to be the FIRST failure mode,
# not a design defect - exactly the same correction cycle
# SO_HEADER_ALLOWLIST in check_dep_zero.sh already went through twice
# (GODS_LAWS.md L-43) before it matched the real tree.
#
# Usage:
#   check-dep-zero-win.ps1 -Modo <compartilhado|estatico> -BuildDir <path>
#   check-dep-zero-win.ps1 -SelfTest
#
# Each function below does one thing (GODS_LAWS.md L-17).

param(
    [Parameter(ParameterSetName = "Real", Mandatory = $true)][string]$Modo,
    [Parameter(ParameterSetName = "Real", Mandatory = $true)][string]$BuildDir,
    [Parameter(ParameterSetName = "SelfTest", Mandatory = $true)][switch]$SelfTest
)

$ErrorActionPreference = "Stop"

# Exact names: KERNEL32.dll (every PE image; unconditional), USER32.dll
# (src/platform/win32/CMakeLists.txt's own comment: "carries
# RegisterClassExW/CreateWindowExW/DestroyWindow/UnregisterClassW/
# DefWindowProcW/GetModuleHandleW - Win32 counts as system API for the
# dependency-zero rule, GODS_LAWS.md L-07, same category as
# libwayland-client on Linux"), and the default MSVC dynamic C++ runtime
# redistributable DLLs for this toolset (VCRUNTIME140.dll,
# VCRUNTIME140_1.dll for the x64 SEH thunk, MSVCP140.dll, ucrtbase.dll -
# the last one covers a build where the Universal CRT resolves directly
# instead of through the api-ms-win-crt-*.dll forwarder family below).
$IMPORT_ALLOWLIST_EXACT = @(
    "KERNEL32.dll", "USER32.dll",
    "VCRUNTIME140.dll", "VCRUNTIME140_1.dll", "MSVCP140.dll", "ucrtbase.dll"
)

# Prefix, case-insensitive: the Universal CRT API-set forwarder family
# (api-ms-win-crt-runtime-l1-1-0.dll, api-ms-win-crt-stdio-l1-1-0.dll,
# api-ms-win-crt-heap-l1-1-0.dll, ...) - documented at
# learn.microsoft.com/cpp/windows/universal-crt-deployment, the standard
# shape a dynamically-linked UCRT program imports on a default MSVC
# toolset instead of (or alongside) ucrtbase.dll directly.
$IMPORT_ALLOWLIST_PREFIX = "api-ms-win-crt-"

$IMPORT_ADVICE = "The DLL imports this library. Find the link flag or the source that pulled it in and REMOVE it. There is no allowlist fix for third-party linkage without the leader's order (GODS_LAWS.md L-07) - if it is a legitimate CRT/OS DLL missing from this script's allowlist, that is a conscious, reviewed edit of tools/ci/check-dep-zero-win.ps1, cited in the commit, never an environment-variable escape hatch."

function Test-ImportedDllAllowed([string]$dllName) {
    foreach ($known in $IMPORT_ALLOWLIST_EXACT) {
        if ($dllName -ieq $known) { return $true }
    }
    return $dllName.ToLowerInvariant().StartsWith($IMPORT_ALLOWLIST_PREFIX)
}

# Parses `dumpbin /imports` text. Documented shape (learn.microsoft.com/
# cpp/build/reference/dumpbin-reference and every real sample this
# project's author has seen): each imported DLL introduces a line
# indented by exactly four spaces, holding ONLY the DLL name, followed by
# further-indented detail lines (hex addresses, ordinal/name entries) and
# a blank line before the next DLL block. A four-space-indented line
# ending in ".dll" (case-insensitive) and containing no other token IS a
# DLL-name header line under this shape; nothing else in the dump matches
# that exact indentation with that exact trailing token, because every
# deeper line carries additional content (a hex address, an ordinal, a
# name) after its own indent.
function Get-DumpbinImportedDlls([string[]]$dumpbinOutput) {
    $names = @()
    foreach ($line in $dumpbinOutput) {
        if ($line -match '^ {4}(\S+\.[Dd][Ll][Ll])\s*$') {
            $names += $matches[1]
        }
    }
    return $names
}

function Invoke-CheckDepZeroWin([string]$libraryPath) {
    if (-not (Test-Path $libraryPath)) {
        Write-Error "check-dep-zero-win.ps1: DLL not found: $libraryPath"
        exit 1
    }
    Write-Host "check-dep-zero-win.ps1: dumpbin /imports $libraryPath"
    $output = & dumpbin /imports $libraryPath
    if ($LASTEXITCODE -ne 0) {
        Write-Error "check-dep-zero-win.ps1: dumpbin /imports failed with code $LASTEXITCODE"
        exit 1
    }

    $imported = Get-DumpbinImportedDlls $output
    if ($imported.Count -eq 0) {
        # GODS_LAWS.md L-40 (piso de varredura nao-vazia): every real PE
        # image imports at least KERNEL32.dll - zero parsed here means
        # the parser broke or dumpbin's output shape changed, never "a
        # clean binary".
        Write-Error "check-dep-zero-win.ps1: varredura vazia (0 imported DLLs parsed from $libraryPath) - GODS_LAWS.md L-40. Either dumpbin's output shape changed or the parser in this file is broken; this is never a legitimate pass."
        exit 1
    }

    $violations = @($imported | Where-Object { -not (Test-ImportedDllAllowed $_) })
    if ($violations.Count -gt 0) {
        Write-Error "check-dep-zero-win.ps1: PROHIBITED (GODS_LAWS.md L-07 zero dependency):"
        foreach ($v in $violations) {
            Write-Error "  ${libraryPath}: $v"
            Write-Error "    -> $IMPORT_ADVICE"
        }
        exit 1
    }
    Write-Host "check-dep-zero-win.ps1: $($imported.Count) imported DLL(s) scanned in $libraryPath, all allowed: $($imported -join ', ')"
}

function Invoke-RealMode([string]$modo, [string]$buildDir) {
    if ($modo -eq "estatico") {
        # Same declared skip shape as check_dep_zero.sh's own sub-check
        # (c) under BUILD_SHARED_LIBS=OFF: a static .lib has no import
        # table to inspect - never a silent skip, always a printed
        # reason (GODS_LAWS.md L-40).
        Write-Host "check-dep-zero-win.ps1: skipped - modo 'estatico' produces a static .lib, which has no dumpbin /imports table to inspect"
        return
    }
    $dll = Get-ChildItem -Recurse -Filter "glintfx.dll" -Path $buildDir -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $dll) {
        Write-Error "check-dep-zero-win.ps1: glintfx.dll not found under $buildDir (modo '$modo' should have built one)"
        exit 1
    }
    Invoke-CheckDepZeroWin $dll.FullName
}

# --- -SelfTest: proves the PARSER and the ALLOWLIST LOGIC against
# synthetic, documented-shape dumpbin text - never against a real
# dumpbin.exe run (this machine has none). Same declared downgrade this
# whole file's header names: this is the only red proof possible off the
# real Windows CI server. ---

function Get-SyntheticCleanDumpbinOutput() {
    return @(
        "Dump of file glintfx.dll",
        "",
        "File Type: DLL",
        "",
        "  Section contains the following imports:",
        "",
        "    KERNEL32.dll",
        "              140003000 Import Address Table",
        "              140005000 Import Name Table",
        "                      0 time date stamp",
        "                      0 Index of first forwarder reference",
        "",
        "                       89 GetModuleHandleW",
        "",
        "    USER32.dll",
        "              140004000 Import Address Table",
        "                      142 CreateWindowExW",
        "",
        "    api-ms-win-crt-runtime-l1-1-0.dll",
        "              140006000 Import Address Table",
        "                       12 exit",
        "",
        "    VCRUNTIME140.dll",
        "              140007000 Import Address Table",
        "                        7 memset",
        "",
        "  Summary"
    )
}

function Invoke-SelfTestPositiveControl() {
    $imported = Get-DumpbinImportedDlls (Get-SyntheticCleanDumpbinOutput)
    $expected = @("KERNEL32.dll", "USER32.dll", "api-ms-win-crt-runtime-l1-1-0.dll", "VCRUNTIME140.dll")
    if (@(Compare-Object $imported $expected).Count -ne 0) {
        Write-Error "selftest: controle POSITIVO FALHOU (parser nao extraiu exatamente as 4 DLLs sinteticas) - obtido: $($imported -join ', ')"
        return $false
    }
    $violations = @($imported | Where-Object { -not (Test-ImportedDllAllowed $_) })
    if ($violations.Count -ne 0) {
        Write-Error "selftest: controle POSITIVO FALHOU (fixture limpa reprovada): $($violations -join ', ')"
        return $false
    }
    Write-Host "selftest: controle POSITIVO OK (4 DLLs sinteticas, todas permitidas)"
    return $true
}

function Invoke-SelfTestNegativeControl() {
    $poisoned = Get-SyntheticCleanDumpbinOutput
    $poisoned += "    evil3rdparty.dll"
    $poisoned += "              140008000 Import Address Table"
    $poisoned += "                        1 do_evil"
    $poisoned += ""
    $imported = Get-DumpbinImportedDlls $poisoned
    $violations = @($imported | Where-Object { -not (Test-ImportedDllAllowed $_) })
    if ($violations.Count -ne 1 -or $violations[0] -ne "evil3rdparty.dll") {
        Write-Error "selftest: controle NEGATIVO FALHOU (evil3rdparty.dll deveria ter sido a UNICA violacao) - obtido: $($violations -join ', ')"
        return $false
    }
    Write-Host "selftest: controle NEGATIVO OK (evil3rdparty.dll pego e citado)"
    return $true
}

function Invoke-SelfTestEmptyScanControl() {
    $imported = Get-DumpbinImportedDlls @("this text has no dll import table at all")
    if ($imported.Count -ne 0) {
        Write-Error "selftest: controle de VARREDURA VAZIA FALHOU (parser deveria ter extraido 0 nomes de um texto sem DLLs)"
        return $false
    }
    Write-Host "selftest: controle de VARREDURA VAZIA OK (0 DLLs extraidas de texto sem tabela de importacao - a chamada real trataria isto como reprovacao, GODS_LAWS.md L-40)"
    return $true
}

function Invoke-SelfTest() {
    $ok = $true
    if (-not (Invoke-SelfTestPositiveControl)) { $ok = $false }
    if (-not (Invoke-SelfTestNegativeControl)) { $ok = $false }
    if (-not (Invoke-SelfTestEmptyScanControl)) { $ok = $false }
    if (-not $ok) {
        Write-Error "check-dep-zero-win.ps1 -SelfTest: FALHOU (ver acima)"
        exit 1
    }
    Write-Host "check-dep-zero-win.ps1 -SelfTest: os tres controles OK"
}

if ($SelfTest) {
    Invoke-SelfTest
} else {
    Invoke-RealMode $Modo $BuildDir
}
