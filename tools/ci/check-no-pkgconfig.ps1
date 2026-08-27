# SPDX-License-Identifier: AGPL-3.0-or-later
# check-no-pkgconfig.ps1 - PKG-WIN-SCOPE.
#
# Proves, on a REAL installed Windows prefix, that glintfx never places
# a pkg-config module (*.pc) anywhere under it. This is the Windows
# regression gate for the decision registered in PACKAGING.md
# ("pkg-config has no role in glintfx's Windows story today") and
# enforced at build time by CMakeLists.txt's if(UNIX) guard around
# glintfx_install_pkgconfig()/glintfx_register_pkgconfig_validation()
# (cmake/GlintfxInstall.cmake, cmake/GlintfxPkgConfigValidate.cmake).
#
# WHY THIS SCRIPT EXISTS INSTEAD OF JUST TRUSTING THE if(UNIX) GUARD:
# GODS_LAWS.md L-40 (piso de varredura nao-vazia) - a guard nobody ever
# exercises on the platform it protects is not a guard, it is a comment
# with an opinion. Before this fatia, glintfx.pc WAS installed on
# Windows by accident (no platform guard existed at all), and the
# validator that ran against it (cmake/GlintfxPkgConfigValidateInstalled.cmake.in)
# already documented, in its own header, that its library-artifact glob
# would report a false failure there (it looks for
# libglintfx.so*/libglintfx.a, never glintfx.lib). This script is what
# proves the accident cannot recur, on the real Windows CI runner, not
# by reasoning about the CMake source.
#
# THE L-20 DOWNGRADE, DECLARED (GODS_LAWS.md L-20, honest downgrade over
# silent overclaiming): the textbook red-before-green run - reverting
# CMakeLists.txt's if(UNIX) guard, watching THIS script fail against a
# real Windows install, then re-applying the guard - is not executable
# here. There is no Windows machine in this repository's own CI to run
# that red on demand outside a real push, and deliberately leaving
# `main` red to prove a point would violate GODS_LAWS.md L-18's "GHA
# verde e todos os testes verdes" push gate. Invoke-SelfTest below is
# the substitute: its "com .pc plantado" fixture is a byte-for-byte
# reproduction of what the OLD, unguarded install tree looked like (a
# *.pc file sitting under an installed prefix), and asserting that
# Assert-NoPkgConfigUnderPrefix throws against it is the closest
# equivalent of "watch it fail before watching it pass" this script can
# offer without an unguarded build to point it at. It is a downgrade
# from true red-before-green, not a replacement for it, and this
# paragraph exists so nobody mistakes the self-test for the real thing.
#
# Usage: check-no-pkgconfig.ps1 -InstalledPrefix <path>
#
# Each function below does one thing (GODS_LAWS.md L-17).

param(
    [Parameter(Mandatory = $true)][string]$InstalledPrefix
)

$ErrorActionPreference = "Stop"

# Enumerates every file under $root, recursively - the whole prefix,
# not just lib/pkgconfig/ (GODS_LAWS.md L-40 item 5: the space is small
# and enumerable, so it is enumerated whole, never searched into with a
# path assumption that could itself be wrong). Returns an empty array,
# never $null, so a caller can always read .Count without a separate
# null-check - including the empty-prefix case this exists to catch.
function Get-AllFilesRecursively([string]$root) {
    if (-not (Test-Path -LiteralPath $root)) {
        return @()
    }
    return @(Get-ChildItem -LiteralPath $root -Recurse -File -Force)
}

# The gate itself. Two ways to fail, both raised by throwing (never by
# returning a boolean a caller could forget to check):
#   1. Zero files scanned under $root (GODS_LAWS.md L-40: contou zero,
#      reprova - an empty or nonexistent prefix is never a silent pass).
#   2. One or more *.pc files found ANYWHERE under $root, named
#      individually - PACKAGING.md: "pkg-config has no role in
#      glintfx's Windows story today".
# Prints the scanned count in every case, pass or fail (L-40 item 3):
# that number is what distinguishes "looked, found nothing" from
# "never looked at all".
function Assert-NoPkgConfigUnderPrefix([string]$root, [string]$label) {
    $allFiles = Get-AllFilesRecursively $root
    Write-Host "check-no-pkgconfig.ps1: [$label] varreu $($allFiles.Count) arquivo(s) sob '$root'"

    if ($allFiles.Count -eq 0) {
        throw "check-no-pkgconfig.ps1: [$label] varredura vazia sob '$root' - prefixo inexistente ou vazio nunca e verde (GODS_LAWS.md L-40)."
    }

    $pcFiles = @($allFiles | Where-Object { $_.Extension -eq ".pc" })
    if ($pcFiles.Count -gt 0) {
        $paths = ($pcFiles | ForEach-Object { $_.FullName }) -join "`n  "
        throw "check-no-pkgconfig.ps1: [$label] encontrado(s) $($pcFiles.Count) arquivo(s) *.pc sob '$root', que nao deveriam existir no Windows (PACKAGING.md: 'pkg-config has no role in glintfx's Windows story today'):`n  $paths"
    }

    Write-Host "check-no-pkgconfig.ps1: [$label] ok - zero arquivos *.pc em $($allFiles.Count) arquivo(s) varridos."
}

# The three controls GODS_LAWS.md L-40 item 4 requires, run in THIS
# process before the real verdict (positivo, negativo, varredura
# vazia): a fixture WITH a .pc planted has to FAIL; an EMPTY prefix has
# to fail by empty scan; a GOOD fixture (real files, no .pc) has to
# PASS. If Assert-NoPkgConfigUnderPrefix stops biting, this is what
# notices before the real prefix is ever judged.
function Invoke-SelfTest() {
    $selfTestRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("glintfx-no-pkgconfig-selftest-" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $selfTestRoot | Out-Null
    try {
        # Controle 1 (negativo): .pc plantado, tem que reprovar. Esta e
        # a reproducao fiel do estado antigo (arvore sem a guarda
        # if(UNIX)) que serve de "vermelho" declarado no cabecalho acima.
        $withPcRoot = Join-Path $selfTestRoot "with-pc"
        $withPcLib = Join-Path $withPcRoot "lib\pkgconfig"
        New-Item -ItemType Directory -Force -Path $withPcLib | Out-Null
        Set-Content -Path (Join-Path $withPcLib "glintfx.pc") -Value "prefix=C:/fake`nName: glintfx`nVersion: 0.1.0.0`n"
        $threw = $false
        try { Assert-NoPkgConfigUnderPrefix $withPcRoot "autoteste: com .pc plantado" }
        catch { $threw = $true }
        if (-not $threw) {
            throw "check-no-pkgconfig.ps1: AUTOTESTE FALHOU - fixture com glintfx.pc plantado deveria reprovar e passou. O portao esta cego."
        }

        # Controle 2 (varredura vazia): prefixo vazio, tem que reprovar
        # por contagem zero, nao por ausencia de .pc.
        $emptyRoot = Join-Path $selfTestRoot "empty"
        New-Item -ItemType Directory -Force -Path $emptyRoot | Out-Null
        $threw = $false
        try { Assert-NoPkgConfigUnderPrefix $emptyRoot "autoteste: prefixo vazio" }
        catch { $threw = $true }
        if (-not $threw) {
            throw "check-no-pkgconfig.ps1: AUTOTESTE FALHOU - prefixo vazio deveria reprovar por varredura vazia (GODS_LAWS.md L-40) e passou."
        }

        # Controle 3 (positivo): prefixo real, sem .pc, tem que passar.
        $goodInclude = Join-Path $selfTestRoot "good\include\glintfx"
        New-Item -ItemType Directory -Force -Path $goodInclude | Out-Null
        Set-Content -Path (Join-Path $goodInclude "core.hpp") -Value "// fixture header, GODS_LAWS.md L-40 autoteste"
        $goodCmakePkg = Join-Path $selfTestRoot "good\lib\cmake\glintfx"
        New-Item -ItemType Directory -Force -Path $goodCmakePkg | Out-Null
        Set-Content -Path (Join-Path $goodCmakePkg "glintfxConfig.cmake") -Value "# fixture CMake package, GODS_LAWS.md L-40 autoteste"
        try {
            Assert-NoPkgConfigUnderPrefix (Join-Path $selfTestRoot "good") "autoteste: prefixo bom"
        }
        catch {
            throw "check-no-pkgconfig.ps1: AUTOTESTE FALHOU - fixture sem .pc deveria passar e reprovou: $($_.Exception.Message)"
        }

        Write-Host "check-no-pkgconfig.ps1: autoteste ok - os tres controles (com .pc plantado, prefixo vazio, prefixo bom) se comportaram como esperado."
    }
    finally {
        Remove-Item -Recurse -Force $selfTestRoot -ErrorAction SilentlyContinue
    }
}

Invoke-SelfTest

Assert-NoPkgConfigUnderPrefix $InstalledPrefix "prefixo instalado real"

Write-Host "ok: nenhum arquivo *.pc sob o prefixo instalado no Windows (PACKAGING.md: pkg-config has no role in glintfx's Windows story today)."
