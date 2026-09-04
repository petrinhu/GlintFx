# SPDX-License-Identifier: AGPL-3.0-or-later
# check-port-privacy-win.ps1 - Windows counterpart of tests/tools/check_
# port_privacy.sh sub-check (d) (the built artifact's own dynamic export
# table). Read check_port_privacy.sh's own header comment in full before
# touching this file - the KNOWN_ADAPTER_CLASSES/KNOWN_PORT_NAMES lists
# below MUST stay identical to that file's, by hand, in the same commit
# that ever changes either.
#
# GAP THIS CLOSES (PORT-PRIVACY-WIN, GODS_LAWS.md L-04 reabertura por
# paridade, 03/09/2026): check_port_privacy.sh's sub-check (d) runs `nm
# -D --defined-only` + `c++filt` on the Linux .so and never touches
# glintfx.dll. Sub-checks (a)/(b)/(c) of that same script already read
# source text under src/platform/ and include/glintfx/ UNCONDITIONALLY
# (its own header comment: "this gate itself is if(UNIX)-only in tests/
# CMakeLists.txt, but the tree it reads is the same tree on every leg"),
# so win32_display_adapter was already on the closed list and already
# proven not to leak into a PUBLIC header by source inspection - what was
# missing is proving it at the BINARY level, the same "truth from the
# artifact, not from a promise" standard sub-check (d) already holds the
# Linux .so to.
#
# TOOL: dumpbin.exe - same MSVC component (VC.Tools.x86.x64) and same
# PATH preparation (vcvarsall.bat x64, workflow step "Preparar ambiente
# do compilador (MSVC x64)") as check-dep-zero-win.ps1 uses; see that
# script's own header for the actions/runner-images citation. No new
# environment preparation added here.
#
# WHY NO DEMANGLER IS NEEDED (undname.exe was considered and rejected):
# check_port_privacy.sh pipes `nm -D` output through `c++filt` before
# grepping, for HUMAN READABILITY of the citation - not because the
# match itself needs demangling. Itanium name mangling (GCC/Clang, the
# Linux side) encodes each identifier as a length-prefixed literal
# substring of the decorated name (e.g. "_ZN8platform21win32_display_
# adapter4openEv" already contains the literal text
# "win32_display_adapter", readable before c++filt ever runs). MSVC's
# decoration scheme (the Linux side's own, unrelated third-party
# reference: docs.microsoft.com/cpp/build/reference/decorated-names)
# shares that same property - it also embeds each identifier as a
# literal substring (e.g. "?open@win32_display_adapter@platform@
# glintfx@@QEAA_NXZ" contains the literal text "win32_display_adapter").
# A plain substring search against the RAW decorated name therefore
# catches exactly what a search against the demangled name would, and
# this script does not add undname.exe as a second, unmeasured tool
# dependency for readability alone.
#
# DECLARED LIMITATION (same as check-dep-zero-win.ps1, read that file's
# header for the full statement): written and syntax-reviewed without
# ever running dumpbin.exe on this machine. -SelfTest exercises the
# parser and the substring-match logic against synthetic, documented-
# shape `dumpbin /exports` text - it proves the MECHANISM, never that
# the real glintfx.dll passes. The first real red/green proof of this
# gate can only happen on the Windows CI server.
#
# Usage:
#   check-port-privacy-win.ps1 -Modo <compartilhado|estatico> -BuildDir <path>
#   check-port-privacy-win.ps1 -SelfTest
#
# Each function below does one thing (GODS_LAWS.md L-17).

param(
    [Parameter(ParameterSetName = "Real", Mandatory = $true)][string]$Modo,
    [Parameter(ParameterSetName = "Real", Mandatory = $true)][string]$BuildDir,
    [Parameter(ParameterSetName = "SelfTest", Mandatory = $true)][switch]$SelfTest
)

$ErrorActionPreference = "Stop"

# MUST match tests/tools/check_port_privacy.sh's own KNOWN_ADAPTER_CLASSES/
# KNOWN_PORT_NAMES verbatim - a single closed list living in two files by
# necessity (one POSIX sh, one PowerShell), not by choice. Adding an
# adapter means editing BOTH in the same commit.
$KNOWN_ADAPTER_CLASSES = @("wayland_display_adapter", "fake_display_adapter", "win32_display_adapter")
$KNOWN_PORT_NAMES = @("display_connection_port", "display_connection")
$CLOSED_NAMES = $KNOWN_ADAPTER_CLASSES + $KNOWN_PORT_NAMES

function Test-NameLeaksPortOrAdapter([string]$exportedName) {
    foreach ($closed in $CLOSED_NAMES) {
        if ($exportedName.Contains($closed)) { return $closed }
    }
    return $null
}

# Parses `dumpbin /exports` text. Documented shape (learn.microsoft.com/
# cpp/build/reference/dumpbin-reference): each exported symbol is one
# line under the "ordinal hint RVA name" header, of the form
#   <ordinal> <hint> <RVA-hex> <name>
# with the first three fields numeric/hex and the name as the fourth
# whitespace-separated token - a forwarder export ("(forwarded to ...)")
# or a data export with no RVA are the two documented variants this
# regex deliberately still matches on the ordinal+hint prefix alone,
# taking whatever token follows as the name (GLINTFX_EXPORT only ever
# marks ordinary functions/classes in this tree today, never a
# forwarder - if one is ever added, this parser sees its name too,
# never silently skips the line).
function Get-DumpbinExportedNames([string[]]$dumpbinOutput) {
    $names = @()
    foreach ($line in $dumpbinOutput) {
        if ($line -match '^\s*\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)') {
            $names += $matches[1]
        }
    }
    return $names
}

function Invoke-CheckPortPrivacyWin([string]$libraryPath) {
    if (-not (Test-Path $libraryPath)) {
        Write-Error "check-port-privacy-win.ps1: DLL not found: $libraryPath"
        exit 1
    }
    Write-Host "check-port-privacy-win.ps1: dumpbin /exports $libraryPath"
    $output = & dumpbin /exports $libraryPath
    if ($LASTEXITCODE -ne 0) {
        Write-Error "check-port-privacy-win.ps1: dumpbin /exports failed with code $LASTEXITCODE"
        exit 1
    }

    $exported = Get-DumpbinExportedNames $output
    if ($exported.Count -eq 0) {
        # GODS_LAWS.md L-40 (piso de varredura nao-vazia): glintfx.dll
        # exports its public API by design (GlintfxLibrary.cmake's
        # generate_export_header()) - zero parsed here means the parser
        # broke or the build genuinely exports nothing, and neither is a
        # legitimate pass (check_port_privacy.sh's own sub-check (d)
        # applies the identical reasoning to nm -D).
        Write-Error "check-port-privacy-win.ps1: varredura vazia (0 exported symbols parsed from $libraryPath) - GODS_LAWS.md L-40."
        exit 1
    }

    $hits = @()
    foreach ($name in $exported) {
        $closed = Test-NameLeaksPortOrAdapter $name
        if ($closed) { $hits += "${name}: names closed identifier '$closed'" }
    }
    if ($hits.Count -gt 0) {
        Write-Error "check-port-privacy-win.ps1: port/adapter symbol found in the exported symbol table:"
        foreach ($h in $hits) { Write-Error "  $h" }
        exit 1
    }
    Write-Host "check-port-privacy-win.ps1: $($exported.Count) exported symbol(s) scanned in $libraryPath, none name the port or an adapter"
}

function Invoke-RealMode([string]$modo, [string]$buildDir) {
    if ($modo -eq "estatico") {
        # Same declared skip shape as check_port_privacy.sh's own sub-
        # check (d) under BUILD_SHARED_LIBS=OFF: a static .lib has no
        # export table to inspect - never silent, always printed
        # (GODS_LAWS.md L-40).
        Write-Host "check-port-privacy-win.ps1: skipped - modo 'estatico' produces a static .lib, which has no dumpbin /exports table to inspect"
        return
    }
    $dll = Get-ChildItem -Recurse -Filter "glintfx.dll" -Path $buildDir -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $dll) {
        Write-Error "check-port-privacy-win.ps1: glintfx.dll not found under $buildDir (modo '$modo' should have built one)"
        exit 1
    }
    Invoke-CheckPortPrivacyWin $dll.FullName
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
        "           2 number of functions",
        "           2 number of names",
        "",
        "    ordinal hint RVA      name",
        "",
        "          1    0 00001040 ?core_init@glintfx@@YAXXZ",
        "          2    1 00001080 ?core_shutdown@glintfx@@YAXXZ",
        "",
        "  Summary"
    )
}

function Invoke-SelfTestPositiveControl() {
    $exported = Get-DumpbinExportedNames (Get-SyntheticCleanDumpbinExports)
    $expected = @("?core_init@glintfx@@YAXXZ", "?core_shutdown@glintfx@@YAXXZ")
    if (@(Compare-Object $exported $expected).Count -ne 0) {
        Write-Error "selftest: controle POSITIVO FALHOU (parser nao extraiu exatamente os 2 nomes sinteticos) - obtido: $($exported -join ', ')"
        return $false
    }
    foreach ($name in $exported) {
        if (Test-NameLeaksPortOrAdapter $name) {
            Write-Error "selftest: controle POSITIVO FALHOU (fixture limpa reprovada em '$name')"
            return $false
        }
    }
    Write-Host "selftest: controle POSITIVO OK (2 exports sinteticos, nenhum cita porta/adaptador)"
    return $true
}

function Invoke-SelfTestNegativeControlAdapter() {
    $poisoned = Get-SyntheticCleanDumpbinExports
    $poisoned += "          3    2 000010C0 ?open@win32_display_adapter@platform@glintfx@@QEAA_NXZ"
    $exported = Get-DumpbinExportedNames $poisoned
    $target = $exported | Where-Object { $_ -like "*win32_display_adapter*" }
    if (-not $target) {
        Write-Error "selftest: controle NEGATIVO (adaptador) FALHOU (parser nao extraiu o nome decorado plantado)"
        return $false
    }
    $closed = Test-NameLeaksPortOrAdapter $target
    if ($closed -ne "win32_display_adapter") {
        Write-Error "selftest: controle NEGATIVO (adaptador) FALHOU (deveria ter citado win32_display_adapter, citou '$closed')"
        return $false
    }
    Write-Host "selftest: controle NEGATIVO (adaptador) OK (win32_display_adapter pego no nome decorado, sem demangling)"
    return $true
}

function Invoke-SelfTestNegativeControlPort() {
    $poisoned = Get-SyntheticCleanDumpbinExports
    $poisoned += "          3    2 000010C0 ?get@display_connection_port@platform@glintfx@@QEAAXXZ"
    $exported = Get-DumpbinExportedNames $poisoned
    $target = $exported | Where-Object { $_ -like "*display_connection_port*" }
    if (-not $target) {
        Write-Error "selftest: controle NEGATIVO (porta) FALHOU (parser nao extraiu o nome decorado plantado)"
        return $false
    }
    $closed = Test-NameLeaksPortOrAdapter $target
    if ($closed -ne "display_connection_port") {
        Write-Error "selftest: controle NEGATIVO (porta) FALHOU (deveria ter citado display_connection_port, citou '$closed')"
        return $false
    }
    Write-Host "selftest: controle NEGATIVO (porta) OK (display_connection_port pego no nome decorado, sem demangling)"
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
    if (-not (Invoke-SelfTestNegativeControlAdapter)) { $ok = $false }
    if (-not (Invoke-SelfTestNegativeControlPort)) { $ok = $false }
    if (-not (Invoke-SelfTestEmptyScanControl)) { $ok = $false }
    if (-not $ok) {
        Write-Error "check-port-privacy-win.ps1 -SelfTest: FALHOU (ver acima)"
        exit 1
    }
    Write-Host "check-port-privacy-win.ps1 -SelfTest: os quatro controles OK"
}

if ($SelfTest) {
    Invoke-SelfTest
} else {
    Invoke-RealMode $Modo $BuildDir
}
