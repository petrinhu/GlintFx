# SPDX-License-Identifier: AGPL-3.0-or-later
# check-install-packager-layout-win.ps1 - Windows twin of tests/tools/
# check_install_packager_layout.sh (FIX-CONSUMO-2 achado QA-4;
# FIX-CONSUMO-3 secao 5), registered as install_packager_layout_win_
# test (tests/CMakeLists.txt, if(WIN32)) and paired with install_
# packager_layout_test in tests/parity_aliases.txt (PARITY-GATE,
# GODS_LAWS.md L-04).
#
# DECLARED NOT APPLICABLE, not a real check - same shape check_public_
# name_collision.py already uses for MSVC ("1 caso, 0 exercido(s), 1
# declarado NAO APLICAVEL nesta execucao: <motivo>. <cobertura
# equivalente>."), and the shape the lider asked for explicitly: "se
# algum deles for genuinamente inviavel no Windows, ele existe, roda e
# passa imprimindo o motivo - ausencia declarada e CONTADA, nunca um
# teste que some."
#
# THE MOTIVE, read for what the Linux script actually proves (not what
# it does): find_package(glintfx) resolves against a Debian-multiarch-
# style install layout (CMAKE_INSTALL_LIBDIR = lib/<arch>, e.g.
# lib/x86_64-linux-gnu). This is not a hard-to-port mechanism, it is a
# CONCEPT that does not exist on this toolchain: CMAKE_LIBRARY_
# ARCHITECTURE and the lib/<arch>/cmake/ search path are populated by
# CMake's OWN Modules/Platform/Linux-Initialize.cmake, pattern-matching
# a GCC-family compiler's implicit link directories - MSVC has no
# multiarch library directory convention at all (CMake's Windows
# install layout is a flat lib/, bin/, include/, the same for every
# architecture), so CMAKE_LIBRARY_ARCHITECTURE is never populated for
# cl.exe and the whole lib/<arch>/cmake/ search branch this test exists
# to exercise is dead code on this platform, by construction of CMake
# itself - not a gap this project's own CMakeLists.txt could close.
#
# THE EQUIVALENT COVERAGE that already exists for Windows's OWN,
# genuinely different install layout question - "does find_package
# (glintfx) resolve the installed package at all" - is consume_win_
# test (tools/ci/check-consume.ps1, paired with consume_test), which
# installs and re-links against a fresh Windows prefix using the
# DEFAULT (non-multiarch) layout this platform actually uses. That is
# the Windows-shaped version of the same underlying worry ("does a
# consumer's find_package(glintfx) actually work against a real
# install"), just without the Debian-specific packaging convention
# that has no Windows analogue.
#
# Usage: check-install-packager-layout-win.ps1
#
# Each function below does one thing (GODS_LAWS.md L-17).

$ErrorActionPreference = "Stop"

function Write-NotApplicableReport() {
    $reason = "MSVC/CMake no Windows nao tem convencao de instalacao Debian-multiarch " +
        "(lib/<arch>/cmake/) - CMAKE_LIBRARY_ARCHITECTURE nunca e populada para cl.exe " +
        "(CMake so a preenche por deteccao do compilador GCC-family, Modules/Platform/" +
        "Linux-Initialize.cmake), entao o cenario inteiro que este teste exercita " +
        "(hinted e native zero-flag contra lib/x86_64-linux-gnu) e inatingivel por " +
        "construcao do proprio CMake, nao por lacuna deste projeto"
    $coverage = "cobertura equivalente: consume_win_test (tools/ci/check-consume.ps1) ja " +
        "prova find_package(glintfx) contra o layout DEFAULT (nao-multiarch) que o " +
        "Windows realmente usa"

    Write-Host "check-install-packager-layout-win.ps1: 1 caso, 0 exercido(s) aqui, 1 declarado NAO APLICAVEL nesta execucao: $reason. $coverage."
}

Write-NotApplicableReport
Write-Host "ok: ausencia declarada e contada (GODS_LAWS.md L-40) - nao e uma reprovacao, nao e um teste que sumiu."
