# SPDX-License-Identifier: AGPL-3.0-or-later
# check-no-undef-glintfx-win.ps1 - Windows counterpart of tests/tools/
# check_no_undef_glintfx.py (LIB-NO-UNDEF gate). Read that file's own
# header comment in full before touching this one - it documents the
# real defect (commit 8eb7d73, five glintfx:: symbols left undefined
# in the shared library because attribute_match.cpp/structural_
# match.cpp were compiled for the test targets only, never added to
# glintfx_library's own target_sources()) and the division of labor
# with check-exports-win.ps1 (that one catches too much leaving the
# DLL; this one is about the Linux side's "too little went IN").
#
# VERDICT, RESEARCHED BEFORE WRITING (GODS_LAWS.md L-22/L-43, so the
# next reader does not redo it): learn.microsoft.com/cpp/error-
# messages/tool-errors/linker-tools-error-lnk2019 and .../linker-
# tools-error-lnk1120 (fetched 05/09/2026). The LNK2019 page's own
# first "possible cause" is, verbatim, "The source file that contains
# the definition of the symbol isn't compiled" - exactly this
# project's real 8eb7d73 defect, described by Microsoft's own
# documentation as a HARD, fatal linker error (LNK2019, escalated to
# LNK1120 "N unresolved externals") on Windows, not a silent pass.
#
# The reason the two platforms diverge: GNU ld's default for a shared
# object (.so) is to ALLOW a reference that stays undefined at link
# time (it becomes a dynamic relocation resolved - or not - at load
# or call time; `-Wl,--no-undefined` is opt-in and this project's link
# line does not pass it, per tests/tools/check_no_undef_glintfx.py's
# own header). MSVC's link.exe has no equivalent opt-in: EVERY symbol
# referenced by the object files handed to a single link invocation -
# whether the target is a DLL or an EXE - must resolve within that
# same link, or the linker refuses to produce ANY output and reports
# LNK2019/LNK1120. A DLL cannot exist, on this toolchain, with an
# internal call left dangling the way 8eb7d73's libglintfx.so.0.2.0.0
# did.
#
# WHAT THIS MEANS FOR THE GATE: there is no dumpbin equivalent of `nm
# -D --undefined-only` that could show a REMAINING internal hole in a
# built glintfx.dll, because the toolchain never lets such a DLL come
# into existence in the first place - the check that matters already
# ran, inside link.exe, before this script's job even starts. This
# script does not take that argument on faith alone, though (matching
# check-exports-win.ps1's own stance, "not by construction alone"):
# it mechanically confirms the SPECIFIC premise of the historical
# defect - that attribute_match.cpp and structural_match.cpp were
# compiled as part of the glintfx_library TARGET ITSELF (not only a
# test target) - by checking for their .obj files inside glintfx_
# library's own Ninja intermediate directory, the same directory
# layout .github/workflows/ci.yml's own "windows" job produces (-G
# Ninja, confirmed in that file's CI-WIN-GEN comment) and the one the
# Linux build already showed live: <builddir>/src/CMakeFiles/glintfx_
# library.dir/gfui/<file>.cpp.o(bj). Finding both .obj files there,
# with a nonzero size, is the positive, counted evidence (GODS_LAWS.md
# L-40) that this specific historical hole cannot have reopened;
# their ABSENCE would mean the same defect came back - and this
# script would have failed to notice only if link.exe had ALSO failed
# to notice, which its own documentation says it cannot.
#
# DECLARED LIMITATION (same shape as check-exports-win.ps1's own,
# read its header for the full statement): written and syntax-
# reviewed without ever running on a Windows machine or MSVC here (no
# Windows, no dumpbin/link.exe on this box). -SelfTest exercises the
# path-shape logic against synthetic, documented directory layouts -
# it proves the MECHANISM, never that the real build produced the
# real .obj files. The first real red/green proof of this gate can
# only happen on the Windows CI server - and "red" here would mean
# the historical defect returned, which is why -SelfTest also feeds a
# fixture with ONE of the two .obj files missing (the real 8eb7d73
# shape: attribute_match.cpp/structural_match.cpp were both left OUT
# of the target, but the two are independent files and either one
# missing alone reproduces the same class of hole).
#
# WHAT THIS SCRIPT DOES NOT CATCH (read before trusting its green -
# GODS_LAWS.md L-41 applied to the next reader, not only to the
# leader; a check sold as stronger than it is becomes false comfort,
# and this project already paid for that shape once: 88 green tests
# next to a broken shipped artifact):
#
#   * It never inspects glintfx.dll's own symbol content the way the
#     Linux gate inspects libglintfx.so with `nm -D`. It proves the
#     PREMISE (the two .cpp files compiled inside the right target),
#     not the RESULT - if a future glintfx:: function calls some OTHER
#     undefined internal symbol from a THIRD file this script does not
#     name, that hole is invisible here; only link.exe's own LNK2019
#     at build time would catch it, before this script even runs.
#   * A symbol that resolves to the WRONG definition (ODR violation)
#     is out of scope entirely - this script (like its Linux sibling)
#     only ever reasons about total absence, never about correctness.
#   * `-Modo estatico` never reproves by content, ever - it confirms
#     the same two .obj files compiled, then declares the mode not
#     applicable (a static .lib has no link step of its own); it is
#     not evidence that static consumption is safe.
#   * Written and reviewed without ever running on Windows (see
#     DECLARED LIMITATION above) - "the four selftest controls pass"
#     is proof of the PARSER's logic, never of the real toolchain's
#     behavior until the CI server exercises it.
# missing alone reproduces the same class of hole).
#
# Usage:
#   check-no-undef-glintfx-win.ps1 -Modo <compartilhado|estatico> -BuildDir <path>
#   check-no-undef-glintfx-win.ps1 -SelfTest
#
# Each function below does one thing (GODS_LAWS.md L-17).

param(
    [Parameter(ParameterSetName = "Real", Mandatory = $true)][string]$Modo,
    [Parameter(ParameterSetName = "Real", Mandatory = $true)][string]$BuildDir,
    [Parameter(ParameterSetName = "SelfTest", Mandatory = $true)][switch]$SelfTest
)

$ErrorActionPreference = "Stop"

# The two translation units whose absence from glintfx_library's own
# target_sources() call WAS the real defect (src/gfui/CMakeLists.txt's
# own header comment on the GFSS-MATCH-ATTR/GFSS-MATCH-STRUCT fix).
$REQUIRED_OBJECT_STEMS = @("attribute_match", "structural_match")

# Same relative shape the Ninja generator uses on both platforms for a
# CMake target named glintfx_library with sources added from an
# add_subdirectory("gfui") layer: <builddir>/src/CMakeFiles/glintfx_
# library.dir/gfui/<stem>.cpp.<obj-ext>. Kept as a function (not a
# literal) so -SelfTest can hand it a synthetic root instead of a real
# $BuildDir.
function Get-GfuiObjectDir([string]$buildDir) {
    return Join-Path $buildDir "src\CMakeFiles\glintfx_library.dir\gfui"
}

function Find-CompiledStems([string]$gfuiObjectDir) {
    $found = @()
    if (-not (Test-Path $gfuiObjectDir)) {
        return $found
    }
    foreach ($stem in $REQUIRED_OBJECT_STEMS) {
        $candidate = Join-Path $gfuiObjectDir "$stem.cpp.obj"
        if ((Test-Path $candidate) -and ((Get-Item $candidate).Length -gt 0)) {
            $found += $stem
        }
    }
    return $found
}

function Invoke-CheckNoUndefGlintfxWin([string]$gfuiObjectDir) {
    $found = Find-CompiledStems $gfuiObjectDir

    # GODS_LAWS.md L-40 (piso de varredura nao-vazia): checking for
    # zero of two expected stems and calling that "nothing to report"
    # would be exactly the shape L-40 exists to reprove - the scan
    # target (REQUIRED_OBJECT_STEMS) is fixed and non-empty by
    # construction, so print how many of it were actually found,
    # always, pass or fail.
    Write-Host "check-no-undef-glintfx-win.ps1: $($REQUIRED_OBJECT_STEMS.Count) objeto(s) esperado(s) em $gfuiObjectDir, $($found.Count) encontrado(s) com tamanho > 0: $($found -join ', ')"

    $missing = @($REQUIRED_OBJECT_STEMS | Where-Object { $found -notcontains $_ })
    if ($missing.Count -gt 0) {
        Write-Error "check-no-undef-glintfx-win.ps1: $($missing.Count) objeto(s) da glintfx_library ausente(s) ou vazio(s) - o defeito historico (GODS_LAWS.md L-36, commit 8eb7d73) volta a existir se qualquer um destes nao compilar DENTRO do alvo da biblioteca: $($missing -join ', ')"
        exit 1
    }

    Write-Host "check-no-undef-glintfx-win.ps1: ok - os $($REQUIRED_OBJECT_STEMS.Count) objeto(s) que fecharam o buraco de 8eb7d73 estao dentro do alvo glintfx_library. Por construcao do link.exe (LNK2019/LNK1120 documentados pela Microsoft), nenhum .dll teria sido produzido se uma chamada interna a glintfx:: tivesse ficado sem definicao."
}

function Invoke-RealMode([string]$modo, [string]$buildDir) {
    if ($modo -eq "estatico") {
        # Mesma forma de ausencia declarada de check-exports-win.ps1
        # (ver header): um .lib estatico do MSVC e, como o .a do GNU
        # ar, um saco de .obj sem etapa de link propria - a mesma
        # razao pela qual tests/tools/check_no_undef_glintfx.py
        # --static nunca reprova por conteudo de simbolo. Ainda assim
        # roda de verdade e imprime a contagem (GODS_LAWS.md L-40),
        # verificando o MESMO par de .obj que o modo compartilhado
        # verifica - o link estatico do consumidor e quem decide se
        # eles bastam, mas eles precisam ter sido compilados aqui de
        # qualquer forma.
        $gfuiObjectDir = Get-GfuiObjectDir $buildDir
        $found = Find-CompiledStems $gfuiObjectDir
        Write-Host "check-no-undef-glintfx-win.ps1: modo 'estatico' - NAO APLICAVEL por natureza (declarado, GODS_LAWS.md L-04/L-40): um .lib estatico nao tem etapa de link propria, a resolucao real acontece so no link final do consumidor. $($found.Count) de $($REQUIRED_OBJECT_STEMS.Count) objeto(s) confirmados compilados em $gfuiObjectDir de qualquer forma: $($found -join ', ')"
        return
    }
    Invoke-CheckNoUndefGlintfxWin (Get-GfuiObjectDir $buildDir)
}

# --- -SelfTest: synthetic directory layouts under a scratch root -
# see this file's own header, "DECLARED LIMITATION", for why this is
# the only proof possible off the real Windows CI server. ---

function New-SyntheticGfuiObjectDir([string[]]$stemsToCreate) {
    # Returns BOTH the root scratch dir and the gfui subdirectory, as
    # a PSCustomObject - re-deriving the root from the gfui path via
    # repeated Split-Path -Parent was tried and measured fragile (an
    # off-by-one there deletes the wrong ancestor and litters TEMP on
    # every run instead of cleaning it up), so the caller is handed
    # the exact path it needs to remove, never asked to recompute it.
    $root = Join-Path ([System.IO.Path]::GetTempPath()) ([System.Guid]::NewGuid().ToString())
    $gfuiDir = Join-Path $root "src\CMakeFiles\glintfx_library.dir\gfui"
    New-Item -ItemType Directory -Path $gfuiDir -Force | Out-Null
    foreach ($stem in $stemsToCreate) {
        # Non-empty content: Find-CompiledStems also checks Length -gt
        # 0, mirroring a real compiler never emitting a zero-byte
        # .obj for a translation unit with actual code in it.
        Set-Content -Path (Join-Path $gfuiDir "$stem.cpp.obj") -Value "synthetic object body"
    }
    return [PSCustomObject]@{ Root = $root; GfuiDir = $gfuiDir }
}

function Invoke-SelfTestPositiveControl() {
    $synthetic = New-SyntheticGfuiObjectDir $REQUIRED_OBJECT_STEMS
    $found = Find-CompiledStems $synthetic.GfuiDir
    Remove-Item -Recurse -Force $synthetic.Root
    if ($found.Count -ne $REQUIRED_OBJECT_STEMS.Count) {
        Write-Error "selftest: controle POSITIVO FALHOU (esperava os $($REQUIRED_OBJECT_STEMS.Count) objetos sinteticos, achou $($found.Count): $($found -join ', '))"
        return $false
    }
    Write-Host "selftest: controle POSITIVO OK (os $($REQUIRED_OBJECT_STEMS.Count) objetos sinteticos, ambos presentes, foram reconhecidos)"
    return $true
}

function Invoke-SelfTestNegativeControlRealDefectShape() {
    # O formato real de 8eb7d73: os DOIS arquivos ficaram fora do alvo
    # (nenhum .obj de nenhum dos dois existia dentro de glintfx_
    # library.dir), nao apenas um - mas o controle abaixo cria so UM
    # dos dois para provar que a ausencia de QUALQUER um sozinho ja
    # reprova, nao so a ausencia dos dois juntos.
    $synthetic = New-SyntheticGfuiObjectDir @("attribute_match")
    $found = Find-CompiledStems $synthetic.GfuiDir
    Remove-Item -Recurse -Force $synthetic.Root
    $missing = @($REQUIRED_OBJECT_STEMS | Where-Object { $found -notcontains $_ })
    if ($missing.Count -ne 1 -or $missing[0] -ne "structural_match") {
        Write-Error "selftest: controle NEGATIVO FALHOU (esperava exatamente 'structural_match' ausente, obtido: $($missing -join ', '))"
        return $false
    }
    Write-Host "selftest: controle NEGATIVO OK (um unico objeto ausente - o formato real do defeito de 8eb7d73 - foi pego)"
    return $true
}

function Invoke-SelfTestEmptyScanControl() {
    # Diretorio gfui existente mas vazio (nenhum .obj): equivalente do
    # controle de varredura vazia do lado Linux - os dois stems
    # esperados ficam ausentes, e isso reprova, nunca passa em
    # silencio (GODS_LAWS.md L-40).
    $synthetic = New-SyntheticGfuiObjectDir @()
    $found = Find-CompiledStems $synthetic.GfuiDir
    Remove-Item -Recurse -Force $synthetic.Root
    if ($found.Count -ne 0) {
        Write-Error "selftest: controle de VARREDURA VAZIA FALHOU (esperava 0 objetos encontrados num diretorio vazio, achou $($found.Count))"
        return $false
    }
    Write-Host "selftest: controle de VARREDURA VAZIA OK (diretorio gfui sem nenhum .obj - os $($REQUIRED_OBJECT_STEMS.Count) esperados ficam ausentes, reprovaria em Invoke-CheckNoUndefGlintfxWin)"
    return $true
}

function Invoke-SelfTestMissingDirControl() {
    # Diretorio que nunca foi criado (build nem rodou): Find-
    # CompiledStems precisa devolver lista vazia, nao lancar excecao.
    $bogusDir = Join-Path ([System.IO.Path]::GetTempPath()) ([System.Guid]::NewGuid().ToString())
    $found = Find-CompiledStems $bogusDir
    if ($found.Count -ne 0) {
        Write-Error "selftest: controle de DIRETORIO AUSENTE FALHOU (esperava lista vazia para diretorio inexistente, achou $($found.Count))"
        return $false
    }
    Write-Host "selftest: controle de DIRETORIO AUSENTE OK (Find-CompiledStems nao lanca excecao quando o build nem rodou)"
    return $true
}

function Invoke-SelfTest() {
    $overallOk = $true
    if (-not (Invoke-SelfTestPositiveControl)) { $overallOk = $false }
    if (-not (Invoke-SelfTestNegativeControlRealDefectShape)) { $overallOk = $false }
    if (-not (Invoke-SelfTestEmptyScanControl)) { $overallOk = $false }
    if (-not (Invoke-SelfTestMissingDirControl)) { $overallOk = $false }

    if (-not $overallOk) {
        Write-Error "check-no-undef-glintfx-win.ps1 -SelfTest: FALHOU (ver acima)"
        exit 1
    }
    Write-Host "check-no-undef-glintfx-win.ps1 -SelfTest: os quatro controles OK"
}

if ($SelfTest) {
    Invoke-SelfTest
} else {
    Invoke-RealMode $Modo $BuildDir
}
