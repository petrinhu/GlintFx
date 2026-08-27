# SPDX-License-Identifier: AGPL-3.0-or-later
# check-pkgconfig-installed.ps1 - PKG-WIN-SCOPE.
#
# Proves, on a REAL installed Windows prefix, that glintfx DOES place a
# pkg-config module (glintfx.pc) under it. This is the Windows
# regression gate for the decision the lider reverted back to
# (27/08/2026): "O .pc volta a ser instalado no Windows" - CMakeLists.txt
# no longer guards glintfx_install_pkgconfig()/
# glintfx_register_pkgconfig_validation() behind if(UNIX), and this
# script is what proves that unconditional call actually reaches the
# Windows job's own real install, not just the CMake source.
#
# THIS IS THE INVERSE of an earlier gate with almost the same name
# (check-no-pkgconfig.ps1, PKG-WIN-SCOPE's first pass, since renamed and
# rewritten): that one asserted glintfx.pc was ABSENT everywhere under
# an installed Windows prefix, backing a decision (glintfx.pc is a
# Unix-only artifact) the lider has since derrubado. Keeping the OLD
# assertion after the decision flipped would have turned this gate into
# exactly the kind of regression protection that actively fights the
# current, correct behavior - a green check certifying the wrong thing
# is worse than no check at all. This file replaces it outright, rather
# than leaving both a "must be absent" and a "must be present" gate
# racing each other in the same job.
#
# WHY THIS SCRIPT EXISTS INSTEAD OF JUST TRUSTING THAT
# glintfx_install_pkgconfig() RUNS: GODS_LAWS.md L-40 (piso de varredura
# nao-vazia) - a call nobody ever proves against a real Windows install
# is not proof, it is a comment with an opinion. The reverted-back
# CMakeLists.txt call is the SOURCE of the current behavior; this
# script, run on real Windows CI right after a real `cmake --install`,
# is what CONFIRMS the artifact it should produce is actually there.
#
# THE L-20 NOTE: unlike its predecessor, this gate has a genuine,
# executable red-before-green available in THIS repository's own
# history - the if(UNIX) guard this fatia removes was, itself, a real,
# previously-shipped state in which glintfx.pc was absent on Windows by
# design. Invoke-SelfTest below still runs its own three controls
# in-process before judging the real prefix (positivo, negativo,
# varredura vazia) - the same GODS_LAWS.md L-40 item 4 discipline its
# predecessor used - because a fixture proves the ASSERTION logic
# itself is sound on THIS run, independent of whatever the real
# install happens to produce.
#
# Usage: check-pkgconfig-installed.ps1 -InstalledPrefix <path>
#
# Each function below does one thing (GODS_LAWS.md L-17).

param(
    [Parameter(Mandatory = $true)][string]$InstalledPrefix
)

$ErrorActionPreference = "Stop"

# Enumerates every file under $root, recursively - the whole prefix,
# not just lib/pkgconfig/ (GODS_LAWS.md L-40 item 5: the space is small
# and enumerable, so it is enumerated whole, never searched into with a
# path assumption that could itself be wrong - CMAKE_INSTALL_LIBDIR is
# configurable, and glintfx.pc could legitimately land somewhere other
# than the default "lib/pkgconfig" this project's own CI happens to
# use). Returns an empty array, never $null, so a caller can always
# read .Count without a separate null-check - including the empty-prefix
# case this exists to catch.
function Get-AllFilesRecursively([string]$root) {
    if (-not (Test-Path -LiteralPath $root)) {
        return @()
    }
    return @(Get-ChildItem -LiteralPath $root -Recurse -File -Force)
}

# The gate itself. Two ways to fail, both raised by throwing (never by
# returning a boolean a caller could forget to check):
#   1. Zero files scanned under $root (GODS_LAWS.md L-40: contou zero,
#      reprova - an empty or nonexistent prefix is never a silent pass,
#      and a scan that found nothing at all could not possibly have
#      found glintfx.pc either).
#   2. Zero *.pc files found ANYWHERE under $root - the decision the
#      lider reverted back to: "O .pc volta a ser instalado no
#      Windows."
# Prints the scanned count and the *.pc count in every case, pass or
# fail (L-40 item 3): those numbers are what distinguishes "looked,
# found the file" from "never looked at all" or "looked, found nothing."
function Assert-PkgConfigUnderPrefix([string]$root, [string]$label) {
    $allFiles = Get-AllFilesRecursively $root
    Write-Host "check-pkgconfig-installed.ps1: [$label] varreu $($allFiles.Count) arquivo(s) sob '$root'"

    if ($allFiles.Count -eq 0) {
        throw "check-pkgconfig-installed.ps1: [$label] varredura vazia sob '$root' - prefixo inexistente ou vazio nunca e verde (GODS_LAWS.md L-40)."
    }

    $pcFiles = @($allFiles | Where-Object { $_.Extension -eq ".pc" })
    Write-Host "check-pkgconfig-installed.ps1: [$label] $($pcFiles.Count) arquivo(s) *.pc entre eles"
    if ($pcFiles.Count -eq 0) {
        throw "check-pkgconfig-installed.ps1: [$label] nenhum arquivo *.pc sob '$root' - glintfx.pc deveria estar la (decisao do lider, 27/08/2026: 'O .pc volta a ser instalado no Windows')."
    }

    $glintfxPc = @($pcFiles | Where-Object { $_.Name -eq "glintfx.pc" })
    if ($glintfxPc.Count -eq 0) {
        $names = ($pcFiles | ForEach-Object { $_.Name }) -join ", "
        throw "check-pkgconfig-installed.ps1: [$label] $($pcFiles.Count) arquivo(s) *.pc encontrados sob '$root', mas nenhum chamado glintfx.pc (achados: $names)."
    }

    Write-Host "check-pkgconfig-installed.ps1: [$label] ok - $($glintfxPc.Count) glintfx.pc encontrado(s) em $($allFiles.Count) arquivo(s) varridos."
}

# The three controls GODS_LAWS.md L-40 item 4 requires, run in THIS
# process before the real verdict (positivo, negativo, varredura
# vazia): a fixture WITHOUT any .pc has to FAIL; an EMPTY prefix has to
# fail by empty scan; a GOOD fixture (glintfx.pc genuinely present,
# alongside other real files) has to PASS. If Assert-PkgConfigUnderPrefix
# stops biting, this is what notices before the real prefix is ever
# judged.
function Invoke-SelfTest() {
    $selfTestRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("glintfx-pkgconfig-installed-selftest-" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $selfTestRoot | Out-Null
    try {
        # Controle 1 (negativo): prefixo com arquivos reais mas SEM
        # nenhum .pc, tem que reprovar. Esta e a reproducao fiel do
        # estado antigo (arvore com a guarda if(UNIX) ainda de pe) que
        # serve de "vermelho" declarado no cabecalho acima.
        $withoutPcRoot = Join-Path $selfTestRoot "without-pc"
        $withoutPcInclude = Join-Path $withoutPcRoot "include\glintfx"
        New-Item -ItemType Directory -Force -Path $withoutPcInclude | Out-Null
        Set-Content -Path (Join-Path $withoutPcInclude "core.hpp") -Value "// fixture header, GODS_LAWS.md L-40 autoteste"
        $threw = $false
        try { Assert-PkgConfigUnderPrefix $withoutPcRoot "autoteste: sem .pc" }
        catch { $threw = $true }
        if (-not $threw) {
            throw "check-pkgconfig-installed.ps1: AUTOTESTE FALHOU - fixture sem glintfx.pc deveria reprovar e passou. O portao esta cego."
        }

        # Controle 2 (varredura vazia): prefixo vazio, tem que reprovar
        # por contagem zero, nao por ausencia de .pc.
        $emptyRoot = Join-Path $selfTestRoot "empty"
        New-Item -ItemType Directory -Force -Path $emptyRoot | Out-Null
        $threw = $false
        try { Assert-PkgConfigUnderPrefix $emptyRoot "autoteste: prefixo vazio" }
        catch { $threw = $true }
        if (-not $threw) {
            throw "check-pkgconfig-installed.ps1: AUTOTESTE FALHOU - prefixo vazio deveria reprovar por varredura vazia (GODS_LAWS.md L-40) e passou."
        }

        # Controle 3 (positivo): prefixo real, com glintfx.pc entre
        # outros arquivos, tem que passar.
        $goodPkgconfig = Join-Path $selfTestRoot "good\lib\pkgconfig"
        New-Item -ItemType Directory -Force -Path $goodPkgconfig | Out-Null
        Set-Content -Path (Join-Path $goodPkgconfig "glintfx.pc") -Value "prefix=C:/fake`nName: glintfx`nVersion: 0.1.0.0`n"
        $goodInclude = Join-Path $selfTestRoot "good\include\glintfx"
        New-Item -ItemType Directory -Force -Path $goodInclude | Out-Null
        Set-Content -Path (Join-Path $goodInclude "core.hpp") -Value "// fixture header, GODS_LAWS.md L-40 autoteste"
        try {
            Assert-PkgConfigUnderPrefix (Join-Path $selfTestRoot "good") "autoteste: prefixo bom"
        }
        catch {
            throw "check-pkgconfig-installed.ps1: AUTOTESTE FALHOU - fixture com glintfx.pc deveria passar e reprovou: $($_.Exception.Message)"
        }

        Write-Host "check-pkgconfig-installed.ps1: autoteste ok - os tres controles (sem .pc, prefixo vazio, prefixo bom) se comportaram como esperado."
    }
    finally {
        Remove-Item -Recurse -Force $selfTestRoot -ErrorAction SilentlyContinue
    }
}

Invoke-SelfTest

Assert-PkgConfigUnderPrefix $InstalledPrefix "prefixo instalado real"

Write-Host "ok: glintfx.pc encontrado sob o prefixo instalado no Windows (decisao do lider, 27/08/2026: 'O .pc volta a ser instalado no Windows')."
