#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_install_packager_layout.sh - proves find_package(glintfx) works
# against a Debian-multiarch-style install layout (CMAKE_INSTALL_LIBDIR
# and CMAKE_INSTALL_INCLUDEDIR both non-default), the scenario named by
# the FIX-CONSUMO auditoria (A2/A6) and confirmed still open by the
# FIX-CONSUMO-2 revisao adversarial (achado QA-4).
#
# Root cause investigated (not just re-asserted): CMake's Config-mode
# search only tries "<prefix>/lib/<arch>/cmake/<name>*/" when
# CMAKE_LIBRARY_ARCHITECTURE is set for the CONSUMER's own configure.
# That variable is not a Debian-only patch - it is populated by stock
# CMake itself (cmake_parse_library_architecture(),
# Modules/CMakeParseLibraryArchitecture.cmake) by pattern-matching the
# COMPILER's own implicit link directories against
# CMAKE_LIBRARY_ARCHITECTURE_REGEX (Modules/Platform/Linux-Initialize.cmake).
# On a genuine Debian/Ubuntu multiarch toolchain, gcc reports an
# arch-suffixed implicit lib dir (e.g. /usr/lib/x86_64-linux-gnu), so
# CMAKE_LIBRARY_ARCHITECTURE auto-populates and find_package resolves
# with no extra flag. On Fedora/Arch/CachyOS (no multiarch), gcc never
# reports such a dir, so the variable stays empty and the automatic
# lib/<arch> search path is never tried - this is upstream CMake
# behaviour tied to the COMPILER, not a bug in glintfx-config.cmake.in
# (confirmed empirically: pointing glintfx_DIR or setting
# CMAKE_LIBRARY_ARCHITECTURE by hand on the consumer always resolves it).
#
# TWO scenarios, not one (FIX-CONSUMO-3, secao 5 da revisao adversarial
# de FIX-CONSUMO-2: a versao anterior deste script injetava o hint de
# arquitetura INCONDICIONALMENTE, inclusive na imagem Ubuntu da matriz
# de CI - o unico dos cinco alvos onde a variavel se autopreencheria de
# verdade - entao o caminho zero-flag, o mais comum na pratica
# Debian/Ubuntu, nunca era exercitado em lugar nenhum):
#
#   1. "hinted": configura o consumidor com CMAKE_LIBRARY_ARCHITECTURE
#      explicito - o formato que cross-compilacao, sysroot ou um
#      pipeline de empacotamento de terceiros que fixa a variavel a mao
#      realmente usa. Roda de verdade quando o compilador NAO reporta
#      arquitetura nativa nenhuma (hoje: GCC no Fedora/Arch/CachyOS).
#   2. "native zero-flag": configura o consumidor SEM nenhum -D extra,
#      contra um install cuja CMAKE_INSTALL_LIBDIR usa a MESMA
#      arquitetura que ESTE compilador reporta nativamente
#      (CMAKE_CXX_LIBRARY_ARCHITECTURE, lido do proprio
#      CMakeCXXCompiler.cmake que a deteccao de ABI do CMake escreve -
#      nao uma reimplementacao propria do regex). Roda de verdade
#      quando o compilador da imagem REPORTA essa arquitetura (hoje:
#      Ubuntu, e Clang no Fedora).
#
#   CLANG-INSTALL-LAYOUT-ARCH (26/08/2026, TODO.md): medido que o
#   Clang do Fedora TAMBEM auto-detecta uma arquitetura nativa
#   (CMAKE_CXX_LIBRARY_ARCHITECTURE=x86_64-redhat-linux-gnu), e esse
#   valor SEMPRE sobrepoe o hint explicito do cenario 1 antes de
#   find_package procurar - quatro formas de forcar o hint de volta
#   foram medidas e as quatro perderam (cache -D, -D especifico de
#   CXX, as duas juntas, arquivo de cadeia de ferramentas; ver
#   TODO.md). Por isso os dois cenarios agora sao MUTUAMENTE
#   EXCLUSIVOS por construcao: exatamente um dos dois roda de verdade
#   em qualquer compilador, decidido por essa MESMA deteccao de
#   arquitetura nativa - o outro DECLARA a limitacao com a causa, em
#   vez de fingir que provou um caminho inalcancavel (GODS_LAWS.md
#   L-27). main() confere essa invariante contando quantos cenarios
#   rodaram de verdade e reprovando se for zero (GODS_LAWS.md L-40).
#
# Usage: check_install_packager_layout.sh <glintfx-source-dir> <package-src-dir> <cxx-compiler>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

readonly LIBDIR_ARCH="x86_64-linux-gnu"
readonly NONDEFAULT_LIBDIR="lib/${LIBDIR_ARCH}"
readonly NONDEFAULT_INCLUDEDIR="include/glintfx-packager"

fail() {
    echo "check_install_packager_layout.sh: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 3 ] || fail "usage: check_install_packager_layout.sh <glintfx-source-dir> <package-src-dir> <cxx-compiler>"
    [ -d "$1" ] || fail "glintfx source dir not found: $1"
    [ -d "$2" ] || fail "package source dir not found: $2"
}

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-pkglayout-XXXXXX"
}

configure_glintfx_with_packager_layout() {
    glintfx_src="$1"
    build_dir="$2"
    cxx="$3"
    libdir="$4"
    cmake -S "$glintfx_src" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DCMAKE_INSTALL_LIBDIR="$libdir" \
        -DCMAKE_INSTALL_INCLUDEDIR="$NONDEFAULT_INCLUDEDIR" \
        -DGLINTFX_BUILD_TESTS=OFF
}

build_and_install_glintfx() {
    build_dir="$1"
    prefix="$2"
    cmake --build "$build_dir"
    cmake --install "$build_dir" --prefix "$prefix"
}

# PKG-WIN-SCOPE, perna Unix de regressao: CMakeLists.txt chama
# glintfx_install_pkgconfig()/glintfx_register_pkgconfig_validation()
# incondicionalmente hoje (o guard if(UNIX) que existiu entre a fatia
# original de PKG-WIN-SCOPE e a reversao dela por ordem do lider foi
# removido - ver PACKAGING.md, "Packaging on Windows", e o commit que
# reverte). Esta funcao prova o que sempre foi verdade e continua
# sendo: no Linux, onde este script sempre roda, o .pc TEM de ser
# instalado - nenhuma mudanca de escopo em outra plataforma pode ter
# como efeito colateral silencioso o .pc sumir aqui. Enumeracao, nao
# busca por caminho fixo (GODS_LAWS.md L-40: a contagem aparece na
# saida mesmo quando passa, e varredura vazia reprova).
assert_pkgconfig_pc_installed() {
    prefix="$1"
    label="$2"
    count="$(find "$prefix" -type f -name 'glintfx.pc' 2>/dev/null | wc -l | tr -d '[:space:]')"
    echo "check_install_packager_layout.sh: [$label] varreu '$prefix' e achou $count arquivo(s) glintfx.pc"
    [ "$count" -ge 1 ] || fail "[$label] glintfx.pc nao encontrado sob '$prefix' apos o install no Linux (PACKAGING.md)"
}

configure_consumer_with_architecture_hint() {
    package_src="$1"
    consumer_build="$2"
    prefix="$3"
    cxx="$4"
    cmake -S "$package_src" -B "$consumer_build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DCMAKE_PREFIX_PATH="$prefix" \
        -DCMAKE_LIBRARY_ARCHITECTURE="$LIBDIR_ARCH"
}

configure_consumer_without_architecture_hint() {
    package_src="$1"
    consumer_build="$2"
    prefix="$3"
    cxx="$4"
    cmake -S "$package_src" -B "$consumer_build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER="$cxx" \
        -DCMAKE_PREFIX_PATH="$prefix"
}

build_and_run_consumer() {
    consumer_build="$1"
    cmake --build "$consumer_build"
    binary="$consumer_build/consumer"
    [ -x "$binary" ] || fail "consumer binary not found after build: $binary"
    echo "check_install_packager_layout.sh: running $binary"
    "$binary"
}

# Reads back what CMake's OWN compiler-ABI detection already decided for
# this compiler (Modules/CMakeDetermineCompilerABI.cmake writes it into
# CMakeCXXCompiler.cmake) - ground truth, not a reimplementation of
# CMAKE_LIBRARY_ARCHITECTURE_REGEX. Empty output means this compiler
# reports no arch-suffixed implicit library directory, i.e. the
# automatic lib/<arch> search this test proves can never trigger here.
detect_native_library_architecture() {
    build_dir="$1"
    compiler_info_file="$(find "$build_dir/CMakeFiles" -maxdepth 2 -name 'CMakeCXXCompiler.cmake' 2>/dev/null | head -n1)"
    [ -n "$compiler_info_file" ] || fail "CMakeCXXCompiler.cmake not found under $build_dir/CMakeFiles (compiler ABI detection did not run)"
    sed -n 's/^set(CMAKE_CXX_LIBRARY_ARCHITECTURE "\(.*\)")$/\1/p' "$compiler_info_file"
}

declare_zero_flag_limitation() {
    cxx="$1"
    build_dir="$2"
    echo "check_install_packager_layout.sh: zero-flag find_package path NOT exercised for '$cxx' on this image - CMAKE_CXX_LIBRARY_ARCHITECTURE is empty in $build_dir (compiler ABI detection found no arch-suffixed implicit library directory). CMake's automatic lib/<arch> search (Modules/CMakeParseLibraryArchitecture.cmake) never activates for this toolchain; that is a property of the compiler/distro packaging (Fedora/Arch/CachyOS gcc report no multiarch implicit dir), not a defect in glintfx or in glintfx-config.cmake.in. Proven instead by the hinted scenario above, which reproduces exactly what CMAKE_LIBRARY_ARCHITECTURE is set to automatically on a genuine Debian/Ubuntu multiarch toolchain."
}

# CLANG-INSTALL-LAYOUT-ARCH (GODS_LAWS.md L-40 DECISAO AUTONOMA 1): the
# mirror image of declare_zero_flag_limitation() above. On a compiler
# whose OWN ABI detection reports a native library architecture (e.g.
# Clang on Fedora: CMAKE_CXX_LIBRARY_ARCHITECTURE=x86_64-redhat-linux-
# gnu), that auto-detected value ALWAYS overrides the consumer's
# explicit -DCMAKE_LIBRARY_ARCHITECTURE hint before find_package's
# Config-mode search ever runs - measured, not assumed: four override
# forms were tried (cache -D, the CXX-specific -D, both together, a
# toolchain file) and all four lost to the detected value. No consumer
# can force this variable to something other than what CMake detected
# (CMAKE_LIBRARY_ARCHITECTURE is documented as "if detected", i.e. an
# OUTPUT of ABI detection, never an INPUT), so asserting that
# find_package resolves here would prove an unreachable premise.
# Declares the limitation instead; the zero-flag scenario below proves
# the equivalent coverage for this exact compiler (see the invariant
# in main()).
declare_hinted_limitation() {
    cxx="$1"
    native_arch="$2"
    echo "check_install_packager_layout.sh: hinted find_package path NOT exercised for '$cxx' on this image - this compiler's own ABI detection reports CMAKE_CXX_LIBRARY_ARCHITECTURE=$native_arch, which CMake's Config-mode search prefers over the consumer's explicit -DCMAKE_LIBRARY_ARCHITECTURE=$LIBDIR_ARCH hint (four override forms measured and lost: cache -D, the CXX-specific -D, both together, and a toolchain file - see TODO.md CLANG-INSTALL-LAYOUT-ARCH). Proven instead by the zero-flag scenario below, which installs into lib/$native_arch and configures the consumer with NO flag at all - exactly what this compiler's own detection makes happen automatically, the same coverage the hinted scenario gives on a compiler that stays silent."
}

# Orchestrates the zero-flag scenario in its own build/prefix, separate
# from the hinted one above (CMAKE_INSTALL_LIBDIR is a cache variable
# fixed at glintfx's own configure time, so it needs its own build dir).
# Declares the limitation instead of running when this image's compiler
# cannot autopopulate CMAKE_LIBRARY_ARCHITECTURE (GODS_LAWS.md L-27:
# honest negative result over a gate that overclaims). Runs for real
# exactly when run_hinted_scenario() below declares instead of running
# - see the invariant asserted in main() (GODS_LAWS.md L-40 DECISAO
# AUTONOMA 2).
run_native_zero_flag_scenario() {
    glintfx_src="$1"
    package_src="$2"
    cxx="$3"
    scratch="$4"
    native_arch="$5"

    if [ -z "$native_arch" ]; then
        declare_zero_flag_limitation "$cxx" "$scratch/glintfx-build-hinted"
        return 0
    fi

    native_build="$scratch/glintfx-build-native"
    native_prefix="$scratch/prefix-native"
    native_consumer_build="$scratch/consumer-build-native"

    configure_glintfx_with_packager_layout "$glintfx_src" "$native_build" "$cxx" "lib/${native_arch}"
    build_and_install_glintfx "$native_build" "$native_prefix"
    assert_pkgconfig_pc_installed "$native_prefix" "zero-flag ($native_arch)"
    configure_consumer_without_architecture_hint "$package_src" "$native_consumer_build" "$native_prefix" "$cxx"
    build_and_run_consumer "$native_consumer_build"

    echo "ok: find_package(glintfx) resolves a non-default multiarch-style install layout with NO hint flag at all (zero-flag path), using this toolchain's own native library architecture ($native_arch) - the scenario a real Debian/Ubuntu packager or end user experiences after installing the -dev package and calling find_package(glintfx), with no -D flag involved."
    scenarios_executed_for_real=$((scenarios_executed_for_real + 1))
}

# Runs the hinted scenario for REAL only when this compiler stays
# silent about its own library architecture (native_arch empty) - see
# declare_hinted_limitation() above for why a non-empty native_arch
# makes the premise unreachable instead.
run_hinted_scenario() {
    package_src="$1"
    hinted_prefix="$2"
    hinted_consumer_build="$3"
    cxx="$4"
    native_arch="$5"

    if [ -n "$native_arch" ]; then
        declare_hinted_limitation "$cxx" "$native_arch"
        return 0
    fi

    configure_consumer_with_architecture_hint "$package_src" "$hinted_consumer_build" "$hinted_prefix" "$cxx"
    build_and_run_consumer "$hinted_consumer_build"
    echo "ok: find_package(glintfx) resolves a non-default multiarch-style install layout when the consumer supplies the architecture hint (CMAKE_INSTALL_LIBDIR=$NONDEFAULT_LIBDIR, CMAKE_INSTALL_INCLUDEDIR=$NONDEFAULT_INCLUDEDIR)."
    scenarios_executed_for_real=$((scenarios_executed_for_real + 1))
}

# GODS_LAWS.md L-40 DECISAO AUTONOMA 2: counts scenarios that resolved
# find_package FOR REAL (never merely declared), and reprova on zero -
# the piso de varredura nao-vazia applied to this pair of scenarios.
# The count is printed even when it passes, so "looked and proved" is
# never indistinguishable from "never looked". By construction
# (run_hinted_scenario and run_native_zero_flag_scenario branch on the
# SAME native_arch emptiness, in opposite directions), this value is
# always exactly 1 in practice - but the assertion only enforces the
# L-40 floor (count >= 1), which is the order actually given.
assert_scenario_coverage_nonempty() {
    count="$1"
    echo "check_install_packager_layout.sh: $count cenario(s) executado(s) de verdade (find_package resolvido de fato, nao apenas declarado)"
    [ "$count" -ge 1 ] || fail "varredura vazia: 0 cenarios executados de verdade - nem o hinted nem o zero-flag resolveram find_package por conta propria (GODS_LAWS.md L-40)"
}

main() {
    require_args "$@"
    glintfx_src="$1"
    package_src="$2"
    cxx="$3"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    hinted_build="$scratch/glintfx-build-hinted"
    hinted_prefix="$scratch/prefix-hinted"
    hinted_consumer_build="$scratch/consumer-build-hinted"

    configure_glintfx_with_packager_layout "$glintfx_src" "$hinted_build" "$cxx" "$NONDEFAULT_LIBDIR"
    build_and_install_glintfx "$hinted_build" "$hinted_prefix"
    assert_pkgconfig_pc_installed "$hinted_prefix" "hinted"

    native_arch="$(detect_native_library_architecture "$hinted_build")"
    scenarios_executed_for_real=0

    run_hinted_scenario "$package_src" "$hinted_prefix" "$hinted_consumer_build" "$cxx" "$native_arch"
    run_native_zero_flag_scenario "$glintfx_src" "$package_src" "$cxx" "$scratch" "$native_arch"

    assert_scenario_coverage_nonempty "$scenarios_executed_for_real"
}

main "$@"
