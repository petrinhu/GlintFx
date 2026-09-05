# SPDX-License-Identifier: AGPL-3.0-or-later
# check-exports-win.ps1 - Windows counterpart of tests/tools/check_
# exports.sh (the ABI-STDLIB-LEAK gate). Read check_exports.sh's own
# header comment in full before touching this file - the allowlist
# rule below MUST express the SAME contract as that file's
# EXPECTED_MANGLED_NAMESPACE/ALLOWED_RUNTIME_SYMBOLS, translated to
# MSVC's decoration scheme, not a weaker or stricter one invented here.
#
# GAP THIS CLOSES (EXPORTS-PARITY-WIN, GODS_LAWS.md L-04 reabertura de
# ABI-STDLIB-LEAK por paridade, 05/09/2026): check_exports.sh runs `nm
# -D --defined-only` on the Linux .so and asserts every dynamic symbol
# either starts with the glintfx:: mangled prefix or is one of five
# ELF/glibc housekeeping symbols (_init, _fini, _edata, _end,
# __bss_start). It has never touched glintfx.dll. cmake/glintfx.version
# (the linker version script that closes ABI-STDLIB-LEAK on Linux) is,
# by its own header and cmake/GlintfxLibrary.cmake's
# glintfx_apply_export_map(), explicitly `if(BUILD_SHARED_LIBS AND
# UNIX)`-only and "has nothing to say for ... Windows, where
# generate_export_header's __declspec(dllexport/dllimport) already IS
# the export mechanism" - a claim that, like PORT-PRIVACY-WIN's before
# it, had never been checked against the real binary.
#
# TOOL: dumpbin.exe, same MSVC component (VC.Tools.x86.x64) and same
# PATH preparation (vcvarsall.bat x64, workflow step "Preparar
# ambiente do compilador (MSVC x64)") as check-dep-zero-win.ps1 and
# check-port-privacy-win.ps1 already use; see either script's own
# header for the actions/runner-images citation. No new environment
# preparation added here.
#
# RESEARCH DONE BEFORE WRITING THIS (GODS_LAWS.md L-22/L-43, cited so
# the next reader does not redo it):
#
# 1. learn.microsoft.com/cpp/build/exporting-from-a-dll-using-declspec-
#    dllexport (fetched 05/09/2026): a PE/COFF DLL's export table is
#    OPT-IN - a symbol lands in it only via an explicit
#    __declspec(dllexport) (or a .def file EXPORTS entry). This is the
#    inverse of ELF's default: ELF exports every global symbol from a
#    shared object's dynamic symbol table UNLESS something narrows
#    that (-fvisibility=hidden, a version script), which is exactly
#    the default ABI-STDLIB-LEAK exploited - libstdc++'s
#    _GLIBCXX_VISIBILITY macro puts an EXPLICIT
#    __attribute__((visibility("default"))) on class templates like
#    std::basic_string, and an explicit attribute beats a compiler-
#    flag default (cmake/glintfx.version's own header, confirmed live
#    28/08/2026). There is no PE/COFF analogue of "default visibility
#    that a header can force to exported" - nothing is exported on
#    Windows unless something asks for it by name.
#
# 2. jeffpar.github.io/kbarchive/kb/168/Q168958 (Microsoft KB Q168958,
#    "HOWTO: Exporting STL Components Inside & Outside of a Class") and
#    codesynthesis.com/~boris/blog/2010/01/18/dll-export-cxx-templates
#    (fetched 05/09/2026): exporting an STL template instantiation
#    from an MSVC DLL requires the DLL AUTHOR to write an explicit
#    instantiation declaration carrying __declspec(dllexport)
#    themselves (the EXPIMP_TEMPLATE/DECLSPECIFIER pattern both
#    sources document) - the Microsoft STL headers do not put
#    __declspec(dllexport) on std:: templates unprompted the way
#    libstdc++'s headers put a visibility attribute on theirs. This
#    codebase's own source confirms it never does this either: `grep
#    -rn "__declspec(dllexport)\|__declspec(dllimport)" src/ include/`
#    (run 05/09/2026) matches NOTHING - GLINTFX_API, expanded solely by
#    generate_export_header() (cmake/GlintfxLibrary.cmake's
#    glintfx_generate_export_header()), is the only source of
#    __declspec(dllexport) anywhere in this tree.
#
# VERDICT: the ABI-STDLIB-LEAK bug class is impossible BY CONSTRUCTION
# on the Windows side, not merely unencountered - the causal chain that
# produced it on Linux (a system header's explicit attribute overriding
# our hidden-by-default compiler preset) has no equivalent step on
# Windows, where nothing is exported unless GLINTFX_API asked for it,
# and nothing in the Microsoft STL asks for it. This script does not
# rely on that argument alone, though: it measures glintfx.dll's REAL
# export table and re-proves the same contract check_exports.sh already
# proves on the .so - only glintfx-namespaced symbols leave the binary
# - so a future regression (a stray __declspec(dllexport), a `.def`
# file, an explicit STL instantiation added by a later author) is still
# caught mechanically, not argued away.
#
# NAMESPACE MATCH IS SIMPLER THAN THE LINUX SIDE, AND WHY: Itanium
# mangling (check_exports.sh's EXPECTED_MANGLED_NAMESPACE) needs two
# glob patterns (_ZN7glintfx* and _ZNK7glintfx*) because a const member
# function's cv-qualifier infix ("K") sits BETWEEN the "_ZN" prefix and
# the namespace digits, changing the substring itself. MSVC's scheme
# (learn.microsoft.com/cpp/build/reference/decorated-names, fetched
# 05/09/2026, table entry `void __stdcall b::c(float)` ->
# `?c@b@@AAGXM@Z`) puts the enclosing-scope chain BEFORE the `@@`
# terminator and the calling-convention/qualifier code AFTER it - a
# const-qualified member changes only the code after `@@` (a different
# access/qualifier letter), never the scope chain itself. A namespace
# nested inside glintfx (glintfx::style, glintfx::gfui, glintfx::asset
# - `grep -rn "^namespace" include/glintfx/`, run 05/09/2026, confirms
# glintfx is the OUTERMOST scope in every one of them) still terminates
# its scope chain in "...glintfx@@", because the outermost enclosing
# scope is always the LAST name before the `@@`. One substring check
# ("@glintfx@@"), not two, covers every glintfx-namespaced symbol this
# tree can emit - free function, non-const member, const member, or a
# further-nested namespace member - regardless of constness.
#
# DECLARED LIMITATION (same shape as check-dep-zero-win.ps1 and check-
# port-privacy-win.ps1, read either header for the full statement):
# written and syntax-reviewed without ever running dumpbin.exe on this
# machine (no Windows, no MSVC here). -SelfTest exercises the parser
# and the substring-match logic against synthetic, documented-shape
# `dumpbin /exports` text - it proves the MECHANISM, never that the
# real glintfx.dll passes. The first real red/green proof of this gate
# can only happen on the Windows CI server.
#
# Usage:
#   check-exports-win.ps1 -Modo <compartilhado|estatico> -BuildDir <path>
#   check-exports-win.ps1 -SelfTest
#
# Each function below does one thing (GODS_LAWS.md L-17).

param(
    [Parameter(ParameterSetName = "Real", Mandatory = $true)][string]$Modo,
    [Parameter(ParameterSetName = "Real", Mandatory = $true)][string]$BuildDir,
    [Parameter(ParameterSetName = "SelfTest", Mandatory = $true)][switch]$SelfTest
)

$ErrorActionPreference = "Stop"

# MUST express the same contract as tests/tools/check_exports.sh's
# EXPECTED_MANGLED_NAMESPACE, translated to MSVC decoration (see this
# file's own header, "NAMESPACE MATCH IS SIMPLER..."). No runtime-
# housekeeping allowlist is added here (unlike the Linux side's _init/
# _fini/_edata/_end/__bss_start): those five are ELF/glibc crt symbols
# with no PE/COFF analogue, and a Windows DLL's entry point (DllMain)
# is invoked by the loader directly - it is never placed in the export
# table unless the DLL author explicitly exports it by name, which
# this project's source does not do (see header, point 2).
$GLINTFX_NAMESPACE_MARKER = "@glintfx@@"

function Test-SymbolIsGlintfxNamespaced([string]$decoratedName) {
    return $decoratedName.Contains($GLINTFX_NAMESPACE_MARKER)
}

# Parses `dumpbin /exports` text. Same documented shape and same
# parsing rule as check-port-privacy-win.ps1's own Get-
# DumpbinExportedNames (learn.microsoft.com/cpp/build/reference/
# dumpbin-reference): each exported symbol is one line under the
# "ordinal hint RVA name" header, of the form
#   <ordinal> <hint> <RVA-hex> <name>
# Duplicated here rather than shared, matching this tree's own
# precedent (check-dep-zero-win.ps1 and check-port-privacy-win.ps1
# each carry their own standalone parser; nothing in tools/ci/ dot-
# sources a common module today) - each CI step invokes one script
# file directly, with no module-import step to add.
function Get-DumpbinExportedNames([string[]]$dumpbinOutput) {
    $names = @()
    foreach ($line in $dumpbinOutput) {
        if ($line -match '^\s*\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)') {
            $names += $matches[1]
        }
    }
    return $names
}

function Invoke-CheckExportsWin([string]$libraryPath) {
    if (-not (Test-Path $libraryPath)) {
        Write-Error "check-exports-win.ps1: DLL not found: $libraryPath"
        exit 1
    }
    Write-Host "check-exports-win.ps1: dumpbin /exports $libraryPath"
    $output = & dumpbin /exports $libraryPath
    if ($LASTEXITCODE -ne 0) {
        Write-Error "check-exports-win.ps1: dumpbin /exports failed with code $LASTEXITCODE"
        exit 1
    }

    $exported = Get-DumpbinExportedNames $output
    if ($exported.Count -eq 0) {
        # GODS_LAWS.md L-40 (piso de varredura nao-vazia): glintfx.dll
        # exports its public API by design (GlintfxLibrary.cmake's
        # generate_export_header()) - zero parsed here means the
        # parser broke or the build genuinely exports nothing, and
        # neither is a legitimate pass (check_exports.sh's own
        # require_nonempty_scan applies the identical reasoning to
        # `nm -D`).
        Write-Error "check-exports-win.ps1: varredura vazia (0 exported symbols parsed from $libraryPath) - GODS_LAWS.md L-40."
        exit 1
    }

    $intruders = @($exported | Where-Object { -not (Test-SymbolIsGlintfxNamespaced $_) })
    if ($intruders.Count -gt 0) {
        Write-Error "check-exports-win.ps1: symbol(s) exported outside the glintfx:: namespace (GODS_LAWS.md L-26, ABI-STDLIB-LEAK contract):"
        foreach ($i in $intruders) { Write-Error "  $i" }
        exit 1
    }
    # L-40, literal, same as check_exports.sh's own closing line: the
    # count decides nothing here by itself (every symbol already
    # passed Test-SymbolIsGlintfxNamespaced above, or this would have
    # exited already) - it is printed so a reviewer sees the SIZE of
    # what was actually scanned on the passing run too, not just the
    # wall of names above.
    Write-Host "check-exports-win.ps1: ok - $($exported.Count) exported symbol(s) scanned in $libraryPath, all within the glintfx:: namespace: $($exported -join ', ')"
}

function Invoke-RealMode([string]$modo, [string]$buildDir) {
    if ($modo -eq "estatico") {
        # Same declared skip shape as check-dep-zero-win.ps1/check-
        # port-privacy-win.ps1 under BUILD_SHARED_LIBS=OFF: a static
        # .lib has no export table to inspect - never a silent skip,
        # always a printed reason (GODS_LAWS.md L-40).
        Write-Host "check-exports-win.ps1: skipped - modo 'estatico' produces a static .lib, which has no dumpbin /exports table to inspect"
        return
    }
    $dll = Get-ChildItem -Recurse -Filter "glintfx.dll" -Path $buildDir -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $dll) {
        Write-Error "check-exports-win.ps1: glintfx.dll not found under $buildDir (modo '$modo' should have built one)"
        exit 1
    }
    Invoke-CheckExportsWin $dll.FullName
}

# --- -SelfTest: synthetic, documented-shape dumpbin text - see this
# file's own header, "DECLARED LIMITATION", for why this is the only
# proof possible off the real Windows CI server. ---

function Get-SyntheticCleanDumpbinExports() {
    return @(
        "Dump of file glintfx.dll",
        "",
        "File Type: DLL",
        "",
        "  Section contains the following exports for glintfx.dll",
        "",
        "    00000000 characteristics",
        "    FFFFFFFF time date stamp",
        "        0.00 version",
        "           1 ordinal base",
        "           3 number of functions",
        "           3 number of names",
        "",
        "    ordinal hint RVA      name",
        "",
        # Free function directly in namespace glintfx.
        "          1    0 00001040 ?runtime_version@glintfx@@YA?AUversion@1@XZ",
        # Non-const member function of a class in namespace glintfx.
        "          2    1 00001080 ?with_path@gltfx_err@glintfx@@QEAAAEAV12@V?`$basic_string_view@DU?`$char_traits@D@std@@@std@@@Z",
        # Const member function of the same class - proves the single
        # "@glintfx@@" substring check catches both const and non-
        # const alike (this file's own header, "NAMESPACE MATCH IS
        # SIMPLER..."), unlike the two-pattern Linux Itanium case.
        "          3    2 000010C0 ?path@gltfx_err@glintfx@@QEBA?AV?`$basic_string_view@DU?`$char_traits@D@std@@@std@@XZ",
        "",
        "  Summary"
    )
}

function Invoke-SelfTestPositiveControl() {
    $exported = Get-DumpbinExportedNames (Get-SyntheticCleanDumpbinExports)
    if ($exported.Count -ne 3) {
        Write-Error "selftest: controle POSITIVO FALHOU (parser deveria ter extraido 3 nomes sinteticos, extraiu $($exported.Count)) - obtido: $($exported -join ', ')"
        return $false
    }
    $intruders = @($exported | Where-Object { -not (Test-SymbolIsGlintfxNamespaced $_) })
    if ($intruders.Count -ne 0) {
        Write-Error "selftest: controle POSITIVO FALHOU (fixture limpa reprovada): $($intruders -join ', ')"
        return $false
    }
    Write-Host "selftest: controle POSITIVO OK (3 exports sinteticos - free function, membro nao-const, membro const - todos reconhecidos como glintfx::)"
    return $true
}

function Invoke-SelfTestNegativeControlStdlibLeak() {
    # The exact bug class this gate exists to catch: an MSVC STL
    # internal symbol (std::_Xlength_error, a real, documented
    # Microsoft STL diagnostic helper) landing in the export table -
    # the Windows-decoration analogue of the Linux
    # std::basic_string<char>::_M_replace_cold intrusion ABI-STDLIB-
    # LEAK actually measured (cmake/glintfx.version's own header).
    $poisoned = Get-SyntheticCleanDumpbinExports
    $poisoned += "          4    3 00001100 ?_Xlength_error@std@@YAXPEBD@Z"
    $exported = Get-DumpbinExportedNames $poisoned
    $target = $exported | Where-Object { $_ -like "*_Xlength_error*" }
    if (-not $target) {
        Write-Error "selftest: controle NEGATIVO (stdlib leak) FALHOU (parser nao extraiu o nome decorado plantado)"
        return $false
    }
    if (Test-SymbolIsGlintfxNamespaced $target) {
        Write-Error "selftest: controle NEGATIVO (stdlib leak) FALHOU (std::_Xlength_error nao deveria casar como glintfx::)"
        return $false
    }
    Write-Host "selftest: controle NEGATIVO (stdlib leak) OK (std::_Xlength_error pego, nao reconhecido como glintfx::)"
    return $true
}

function Invoke-SelfTestNegativeControlBareCSymbol() {
    # A plain, undecorated C-linkage export (no namespace at all) -
    # proves the check does not depend on the presence of any "@" in
    # the name to reject an intruder; a symbol with zero "@" simply
    # never contains the "@glintfx@@" marker either.
    $poisoned = Get-SyntheticCleanDumpbinExports
    $poisoned += "          4    3 00001140 SomeStrayCFunction"
    $exported = Get-DumpbinExportedNames $poisoned
    $target = $exported | Where-Object { $_ -eq "SomeStrayCFunction" }
    if (-not $target) {
        Write-Error "selftest: controle NEGATIVO (simbolo C solto) FALHOU (parser nao extraiu o nome plantado)"
        return $false
    }
    if (Test-SymbolIsGlintfxNamespaced $target) {
        Write-Error "selftest: controle NEGATIVO (simbolo C solto) FALHOU (SomeStrayCFunction nao deveria casar como glintfx::)"
        return $false
    }
    Write-Host "selftest: controle NEGATIVO (simbolo C solto) OK (SomeStrayCFunction pego, nao reconhecido como glintfx::)"
    return $true
}

function Invoke-SelfTestEmptyScanControl() {
    $exported = Get-DumpbinExportedNames @("this text has no export table at all")
    if ($exported.Count -ne 0) {
        Write-Error "selftest: controle de VARREDURA VAZIA FALHOU (parser deveria ter extraido 0 nomes de um texto sem tabela de exportacao)"
        return $false
    }
    Write-Host "selftest: controle de VARREDURA VAZIA OK (0 exports extraidos de texto sem tabela - a chamada real trataria isto como reprovacao, GODS_LAWS.md L-40)"
    return $true
}

function Invoke-SelfTest() {
    $ok = $true
    if (-not (Invoke-SelfTestPositiveControl)) { $ok = $false }
    if (-not (Invoke-SelfTestNegativeControlStdlibLeak)) { $ok = $false }
    if (-not (Invoke-SelfTestNegativeControlBareCSymbol)) { $ok = $false }
    if (-not (Invoke-SelfTestEmptyScanControl)) { $ok = $false }
    if (-not $ok) {
        Write-Error "check-exports-win.ps1 -SelfTest: FALHOU (ver acima)"
        exit 1
    }
    Write-Host "check-exports-win.ps1 -SelfTest: os quatro controles OK"
}

if ($SelfTest) {
    Invoke-SelfTest
} else {
    Invoke-RealMode $Modo $BuildDir
}
