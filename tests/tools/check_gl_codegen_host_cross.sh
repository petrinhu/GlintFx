#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_gl_codegen_host_cross.sh - GATE-GL-CROSS-COVERAGE (revisao
# adversarial de 23/09/2026, secao 3 de /var/tmp/glintfx-plan/
# revisao-gl-codegen-host.md): prova de PONTA A PONTA, com um build
# cruzado MinGW REAL, que as tres vias host-native que GL-CODEGEN-
# HOST-TOOL (D-11 de docs/plano-fecho-w7b.md, cmake/
# GlintfxGlCodegenHostTool.cmake) resolve quando CMAKE_CROSSCOMPILING
# e verdadeiro - PATH 1 (GLINTFX_GL_CODEGEN_EXECUTABLE), PATH 2
# (CMAKE_CROSSCOMPILING_EMULATOR) e PATH 3 (a construcao aninhada) -
# geram exatamente o MESMO artefato que a construcao nativa gera, mais
# a PATH 4 (falha alta em CONFIGURE time, nunca "Exec format error" no
# meio da construcao).
#
# O DOR QUE ESTE ARQUIVO EXISTE PARA FECHAR: a revisao mediu que
# CMAKE_CROSSCOMPILING e falso nas cinco pernas da matriz de CI hoje
# (windows-latest compila NATIVAMENTE para Windows, nao cruzado; os
# quatro alvos Linux compilam nativamente para si mesmos) - as quatro
# vias que esta fatia inteira existe para entregar nunca sao
# exercitadas por NENHUMA execucao automatica. Este script e o que
# fecha essa lacuna: um passo de CI dedicado (job
# `gl-codegen-host-cross`, so no alvo primario Fedora - GODS_LAWS.md
# L-04, mesmo raciocinio que ja isola `lint`/`sanitizer`/`gitleaks` la:
# a mecanica cruzada nao muda por distro Linux) faz um configure
# cruzado MinGW de verdade a cada execucao de CI, nunca so uma vez a
# mao.
#
# GATE-GL-NESTED-STALE (o proprio achado critico que motivou esta
# rodada): assert_nested_incremental_rebuild_regenerates() e o gate
# PERMANENTE para o bug que a revisao achou (rebuild incremental do
# modo "nested" deixava gl_functions.hpp/.cpp OBSOLETOS, sem erro) -
# muta uma COPIA do gerador (GODS_LAWS.md L-27: nunca a arvore
# rastreada, nunca a instancia que outro agente possa estar lendo) e
# prova que a regeneracao de fato acontece num rebuild incremental,
# sem reconfigurar a mao.
#
# NAO usa Wine como oraculo para a via "emulator" (PATH 2): um dublê
# POSIX que registra a chamada recebida (prova que CMAKE_CROSSCOMPILING
# _EMULATOR foi de fato prependado pelo CMake) e delega ao binario HOST
# ja construido pela referencia dourada - mais barato, sem GUI, sem
# risco de tocar superficie nenhuma (GODS_LAWS.md L-09/L-50 deste
# projeto), e suficiente para o que esta via precisa provar: que o
# nome NU do alvo (nao $<TARGET_FILE:...>) de fato aciona o emulador
# configurado. Rodar o .exe cruzado de verdade sob Wine fica fora de
# escopo (proximo aparato, se um dia for preciso).
#
# Usage:
#   check_gl_codegen_host_cross.sh <glintfx-source-dir> <host-cxx-compiler> <mingw-toolchain-file> <generator>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

SCRIPT_NAME="check_gl_codegen_host_cross.sh"

fail() {
    echo "$SCRIPT_NAME: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 4 ] || fail "usage: check_gl_codegen_host_cross.sh <glintfx-source-dir> <host-cxx-compiler> <mingw-toolchain-file> <generator>"
    [ -d "$1" ] || fail "glintfx source dir not found: $1"
    [ -f "$3" ] || fail "mingw toolchain file not found: $3"
}

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-codegen-host-cross-XXXXXX"
}

# --- golden: a real, native build of the unmodified source dir -------

build_native_golden() {
    src="$1"; cxx="$2"; generator="$3"; build_dir="$4"
    cmake -S "$src" -B "$build_dir" -G "$generator" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$cxx" -DGLINTFX_BUILD_TESTS=OFF \
        >/dev/null || fail "golden: configure nativo falhou"
    cmake --build "$build_dir" --target glintfx_gl_functions_generated >/dev/null \
        || fail "golden: build nativo falhou"
}

assert_matches_golden() {
    label="$1"; golden_hpp="$2"; golden_cpp="$3"; candidate_hpp="$4"; candidate_cpp="$5"
    [ -f "$candidate_hpp" ] || fail "$label: $candidate_hpp nao foi gerado"
    [ -f "$candidate_cpp" ] || fail "$label: $candidate_cpp nao foi gerado"
    cmp -s "$golden_hpp" "$candidate_hpp" \
        || fail "$label: gl_functions.hpp difere do golden nativo (cmp)"
    cmp -s "$golden_cpp" "$candidate_cpp" \
        || fail "$label: gl_functions.cpp difere do golden nativo (cmp)"
}

# --- PATH 1: GLINTFX_GL_CODEGEN_EXECUTABLE ----------------------------

assert_path_variable_matches_golden() {
    src="$1"; toolchain="$2"; generator="$3"; build_dir="$4"; native_tool="$5"
    golden_hpp="$6"; golden_cpp="$7"

    cmake -S "$src" -B "$build_dir" -G "$generator" \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
        -DCMAKE_BUILD_TYPE=Release -DGLINTFX_BUILD_TESTS=OFF \
        -DGLINTFX_GL_CODEGEN_EXECUTABLE="$native_tool" \
        >/dev/null || fail "PATH 1 (variable): configure cruzado falhou"
    cmake --build "$build_dir" --target glintfx_gl_functions_generated >/dev/null \
        || fail "PATH 1 (variable): build cruzado falhou"
    assert_matches_golden "PATH 1 (variable)" "$golden_hpp" "$golden_cpp" \
        "$build_dir/generated/render/gl_functions.hpp" "$build_dir/generated/render/gl_functions.cpp"
}

# GATE-GL-ABI-HANDSHAKE (achado da revisao adversarial de 23/09/2026
# contra ESTE MESMO script, /var/tmp/glintfx-plan/revisao-codegen-r2.md):
# assert_path_variable_matches_golden() acima SO exercita o binario
# HOST correto - nunca prova que um binario DESALINHADO (--codegen-abi
# diferente do esperado, H-1's own aperto de mao de versao,
# glintfx_check_gl_codegen_host_tool_abi() em
# cmake/GlintfxGlCodegenHostTool.cmake) e de fato rejeitado. O revisor
# comentou aquela chamada, rodou o script INTEIRO, e ele passou verde -
# nada aqui testava a via negativa. Esta funcao fecha essa lacuna: um
# dublê POSIX (nunca um binario compilado, nunca Wine/.exe -
# GODS_LAWS.md L-09/L-50) que responde --codegen-abi com um valor
# ERRADO, usado como GLINTFX_GL_CODEGEN_EXECUTABLE - o configure TEM
# que reprovar, no mesmo padrao de assert_path4_hard_failure() abaixo.
make_mismatched_abi_double() {
    double_path="$1"
    cat > "$double_path" <<'EOF_ABI_DOUBLE'
#!/usr/bin/env sh
# Dublê H-1 (aperto de mao de versao, GATE-GL-ABI-HANDSHAKE): responde
# --codegen-abi com um valor ERRADO (99, esperado 1) - nunca deveria
# ser aceito como GLINTFX_GL_CODEGEN_EXECUTABLE. Qualquer outra
# chamada falha alto: se o aperto de mao estiver mesmo ligado, este
# dublê nunca deveria ser convocado para gerar codigo de verdade.
if [ "$1" = "--codegen-abi" ]; then
    echo 99
    exit 0
fi
echo "dublê de ABI incompativel (GATE-GL-ABI-HANDSHAKE): nao deveria ter sido chamado para gerar codigo de verdade" >&2
exit 1
EOF_ABI_DOUBLE
    chmod +x "$double_path"
}

assert_path_variable_rejects_abi_mismatch() {
    src="$1"; toolchain="$2"; generator="$3"; build_dir="$4"; double="$5"

    set +e
    configure_output=$(cmake -S "$src" -B "$build_dir" -G "$generator" \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
        -DCMAKE_BUILD_TYPE=Release -DGLINTFX_BUILD_TESTS=OFF \
        -DGLINTFX_GL_CODEGEN_EXECUTABLE="$double" 2>&1)
    configure_rc=$?
    set -e

    [ "$configure_rc" -ne 0 ] \
        || fail "PATH 1 (aperto de mao de versao, GATE-GL-ABI-HANDSHAKE): configure deveria ter reprovado um binario com --codegen-abi incompativel, mas passou (rc=0)"
    printf '%s' "$configure_output" | grep -q "codegen-abi" \
        || fail "PATH 1 (aperto de mao de versao, GATE-GL-ABI-HANDSHAKE): configure reprovou, mas a mensagem nao citou o aperto de mao de versao (--codegen-abi)"
}

# GATE-GL-ABI-HANDSHAKE, SEGUNDO TIPO (achado declarado, nao coberto, pela
# propria revisao adversarial: /var/tmp/glintfx-plan/revisao-codegen-r2.md:181
# - "Nao testei um SEGUNDO tipo de mutante de ABI (ex.: binario que nao
# responde --codegen-abi de jeito nenhum, rc≠0 na sondagem)". As duas funcoes
# acima so exercitam a SEGUNDA branch de glintfx_check_gl_codegen_host_tool_abi()
# (cmake/GlintfxGlCodegenHostTool.cmake) - o valor devolvido difere do
# esperado (STREQUAL). A PRIMEIRA branch dessa mesma funcao (RESULT_VARIABLE
# != 0, "nao respondeu a --codegen-abi") continua sem nenhuma asserção: um
# binario que falha a propria sondagem (nao roda neste hospedeiro, ou nao e o
# gl_registry_codegen esperado) nunca foi provado rejeitado por este roteiro.
# Dublê POSIX (nunca binario compilado, nunca Wine/.exe - GODS_LAWS.md
# L-09/L-50) que sai com erro e SEM nada em stdout para --codegen-abi -
# simula exatamente essa falha de sondagem.
make_unresponsive_abi_double() {
    double_path="$1"
    cat > "$double_path" <<'EOF_UNRESPONSIVE_DOUBLE'
#!/usr/bin/env sh
# Dublê H-1 (aperto de mao de versao, segundo tipo, GATE-GL-ABI-HANDSHAKE):
# NAO responde a --codegen-abi de jeito nenhum - sai com codigo de erro sem
# escrever nada em stdout, simulando um binario que nao roda neste
# hospedeiro ou que nao e o gl_registry_codegen esperado (a PRIMEIRA branch
# de glintfx_check_gl_codegen_host_tool_abi(), RESULT_VARIABLE != 0).
# Qualquer chamada falha alto - nunca deveria ser aceito como
# GLINTFX_GL_CODEGEN_EXECUTABLE.
echo "dublê nao-responsivo (GATE-GL-ABI-HANDSHAKE): nao implementa --codegen-abi" >&2
exit 3
EOF_UNRESPONSIVE_DOUBLE
    chmod +x "$double_path"
}

assert_path_variable_rejects_unresponsive_abi() {
    src="$1"; toolchain="$2"; generator="$3"; build_dir="$4"; double="$5"

    set +e
    configure_output=$(cmake -S "$src" -B "$build_dir" -G "$generator" \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
        -DCMAKE_BUILD_TYPE=Release -DGLINTFX_BUILD_TESTS=OFF \
        -DGLINTFX_GL_CODEGEN_EXECUTABLE="$double" 2>&1)
    configure_rc=$?
    set -e

    [ "$configure_rc" -ne 0 ] \
        || fail "PATH 1 (aperto de mao de versao, segundo tipo GATE-GL-ABI-HANDSHAKE): configure deveria ter reprovado um binario que nao responde a --codegen-abi, mas passou (rc=0)"
    printf '%s' "$configure_output" | grep -q "nao respondeu a --codegen-abi" \
        || fail "PATH 1 (aperto de mao de versao, segundo tipo GATE-GL-ABI-HANDSHAKE): configure reprovou, mas a mensagem nao citou 'nao respondeu a --codegen-abi' (pode ter reprovado por outro motivo, nao pela sondagem)"
    # A checagem acima sozinha NAO basta (mutante (c), medido 23/09/2026,
    # H-6): o texto "nao respondeu a --codegen-abi" mora dentro do PROPRIO
    # message(), entao continua aparecendo na saida mesmo se alguem trocar o
    # message(FATAL_ERROR da branch de sondagem (RESULT_VARIABLE != 0,
    # cmake/GlintfxGlCodegenHostTool.cmake:126) por um message(STATUS - a
    # mensagem vira so informativa, a execucao NAO para ali, e quem de fato
    # reprova e a segunda checagem (STREQUAL, linha 133), com uma mensagem
    # DIFERENTE ("respondeu --codegen-abi=", sem o "a" e com "="). Medido:
    # com a branch de sondagem demovida a STATUS, o grep acima ainda
    # encontra a frase (falso verde) porque ela sobrevive no texto do
    # STATUS - so a ausencia da mensagem da OUTRA branch prova que foi a
    # branch certa que barrou.
    ! printf '%s' "$configure_output" | grep -q "respondeu --codegen-abi=" \
        || fail "PATH 1 (aperto de mao de versao, segundo tipo GATE-GL-ABI-HANDSHAKE): configure reprovou, mas pela branch ERRADA (STREQUAL, mensagem 'respondeu --codegen-abi=') - a branch de sondagem falha (RESULT_VARIABLE != 0, 'nao respondeu a --codegen-abi') parou de barrar sozinha e um binario que nunca responde esta sendo pego so por acidente, via a checagem de valor"
}

# --- PATH 2: CMAKE_CROSSCOMPILING_EMULATOR (dublê, nunca Wine) -------

make_emulator_double() {
    double_path="$1"; native_tool="$2"; log_path="$3"
    : > "$log_path"
    cat > "$double_path" <<EOF_DOUBLE
#!/usr/bin/env sh
# Dublê de CMAKE_CROSSCOMPILING_EMULATOR (check_gl_codegen_host_cross.sh):
# registra a chamada recebida (prova que o CMake de fato prependou este
# script antes do binario cruzado - a MESMA mecanica que a H-2 corrige,
# nomeando o alvo nu em vez de \$<TARGET_FILE:...>) e delega ao binario
# HOST nativo ja construido pela referencia dourada, em vez de executar
# o .exe cruzado de verdade sob Wine (fora de escopo, GODS_LAWS.md
# L-09/L-50 deste projeto).
cross_exe="\$1"
shift
printf 'CHAMADO: %s %s\n' "\$cross_exe" "\$*" >> "$log_path"
exec "$native_tool" "\$@"
EOF_DOUBLE
    chmod +x "$double_path"
}

assert_path_emulator_matches_golden() {
    src="$1"; toolchain="$2"; generator="$3"; build_dir="$4"; double="$5"; log_path="$6"
    golden_hpp="$7"; golden_cpp="$8"

    cmake -S "$src" -B "$build_dir" -G "$generator" \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
        -DCMAKE_BUILD_TYPE=Release -DGLINTFX_BUILD_TESTS=OFF \
        -DCMAKE_CROSSCOMPILING_EMULATOR="$double" \
        >/dev/null || fail "PATH 2 (emulator): configure cruzado falhou"
    cmake --build "$build_dir" --target glintfx_gl_functions_generated >/dev/null \
        || fail "PATH 2 (emulator): build cruzado falhou"
    [ -s "$log_path" ] || fail "PATH 2 (emulator): o dublê nunca foi chamado (log vazio - CMAKE_CROSSCOMPILING_EMULATOR nao foi acionado, regressao para \$<TARGET_FILE:...> - H-2)"
    grep -q "^CHAMADO: " "$log_path" || fail "PATH 2 (emulator): log nao registrou nenhuma chamada valida"
    assert_matches_golden "PATH 2 (emulator)" "$golden_hpp" "$golden_cpp" \
        "$build_dir/generated/render/gl_functions.hpp" "$build_dir/generated/render/gl_functions.cpp"
}

# --- PATH 3: nested native build (a COPY, so it can be mutated later) -

copy_source_tree() {
    src="$1"; dst="$2"
    mkdir -p "$dst"
    if git -C "$src" rev-parse --git-dir >/dev/null 2>&1; then
        git -C "$src" archive HEAD | tar -x -C "$dst"
    else
        # Fallback for a source dir that is not itself a git checkout -
        # e.g. this script's own local verification runs against a lab
        # copy made by `git archive HEAD | tar -x`, which has no .git
        # of its own. A single cp -a plus removing any build output
        # produces the same clean source tree git archive would have,
        # without a per-item loop (GODS_LAWS.md global L-11: proibido
        # gastar um processo por item varrido). In real CI, $src is
        # always the actions/checkout clone, so this branch is never
        # taken there.
        cp -a "$src"/. "$dst"/
        rm -rf "$dst"/build "$dst"/build-static "$dst"/.git
    fi
}

assert_path_nested_matches_golden() {
    nested_src="$1"; cxx="$2"; toolchain="$3"; generator="$4"; build_dir="$5"
    golden_hpp="$6"; golden_cpp="$7"

    cmake -S "$nested_src" -B "$build_dir" -G "$generator" \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
        -DCMAKE_BUILD_TYPE=Release -DGLINTFX_BUILD_TESTS=OFF \
        -DGLINTFX_HOST_CXX_COMPILER="$cxx" \
        >/dev/null || fail "PATH 3 (nested): configure cruzado falhou"
    cmake --build "$build_dir" --target glintfx_gl_functions_generated >/dev/null \
        || fail "PATH 3 (nested): build cruzado falhou"
    assert_matches_golden "PATH 3 (nested)" "$golden_hpp" "$golden_cpp" \
        "$build_dir/generated/render/gl_functions.hpp" "$build_dir/generated/render/gl_functions.cpp"
}

# GATE-GL-NESTED-STALE: reusa o MESMO build_dir do teste acima (ja
# configurado, ja construido uma vez), muta o BANNER que o gerador
# imprime numa copia (nested_src, nunca a arvore rastreada - L-27),
# reconstroi o MESMO alvo de forma incremental (nenhum apagar, nenhuma
# reconfiguracao a mao - o comando exato que um desenvolvedor rodaria)
# e prova que o header reflete o texto mutante. Antes do conserto desta
# rodada, isto media rc=0 com o texto ANTIGO (achado critico da
# revisao).
assert_nested_incremental_rebuild_regenerates() {
    nested_src="$1"; build_dir="$2"

    banner_file="$nested_src/tools/gl_registry_codegen/loader_codegen.cpp"
    [ -f "$banner_file" ] || fail "GATE-GL-NESTED-STALE: $banner_file nao encontrado na copia"
    marker="GENERATED FILE MUTANTE GATE-GL-NESTED-STALE"
    sed -i "s/GENERATED FILE - do not edit by hand/$marker - do not edit by hand/" "$banner_file"
    grep -q "$marker" "$banner_file" \
        || fail "GATE-GL-NESTED-STALE: mutacao do banner nao foi aplicada na copia"

    cmake --build "$build_dir" --target glintfx_gl_functions_generated >/dev/null \
        || fail "GATE-GL-NESTED-STALE: rebuild incremental falhou"

    header="$build_dir/generated/render/gl_functions.hpp"
    grep -q "$marker" "$header" \
        || fail "GATE-GL-NESTED-STALE: rebuild incremental do modo nested NAO regenerou o header apos mudanca de fonte (staleness reintroduzida)"
}

# --- PATH 4: falha alta em CONFIGURE time -----------------------------

assert_path4_hard_failure() {
    src="$1"; toolchain="$2"; generator="$3"; build_dir="$4"

    set +e
    configure_output=$(cmake -S "$src" -B "$build_dir" -G "$generator" \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
        -DCMAKE_BUILD_TYPE=Release -DGLINTFX_BUILD_TESTS=OFF \
        -DGLINTFX_HOST_CXX_COMPILER=/nao/existe/compilador-host 2>&1)
    configure_rc=$?
    set -e

    [ "$configure_rc" -ne 0 ] \
        || fail "PATH 4 (falha alta): configure deveria ter reprovado com GLINTFX_HOST_CXX_COMPILER invalido, mas passou (rc=0)"
    printf '%s' "$configure_output" | grep -q "GLINTFX_GL_CODEGEN_EXECUTABLE" \
        || fail "PATH 4 (falha alta): configure reprovou, mas a mensagem nao citou GLINTFX_GL_CODEGEN_EXECUTABLE como saida (H-4)"
}

# --- main --------------------------------------------------------------

real_main() {
    require_args "$@"
    src="$1"; cxx="$2"; toolchain="$3"; generator="$4"

    scratch=$(make_scratch_workdir)
    trap 'rm -rf "$scratch"' EXIT

    golden_build="$scratch/golden"
    build_native_golden "$src" "$cxx" "$generator" "$golden_build"
    golden_hpp="$golden_build/generated/render/gl_functions.hpp"
    golden_cpp="$golden_build/generated/render/gl_functions.cpp"
    native_tool="$golden_build/tools/gl_registry_codegen/gl_registry_codegen"
    [ -x "$native_tool" ] || fail "golden: ferramenta HOST nativa nao encontrada/executavel: $native_tool"

    assert_path_variable_matches_golden "$src" "$toolchain" "$generator" \
        "$scratch/cross-variable" "$native_tool" "$golden_hpp" "$golden_cpp"
    echo "$SCRIPT_NAME: PATH 1 (GLINTFX_GL_CODEGEN_EXECUTABLE) - binario correto aceito, artefato bate com o golden - OK"

    abi_double="$scratch/abi-mismatch-double.sh"
    make_mismatched_abi_double "$abi_double"
    assert_path_variable_rejects_abi_mismatch "$src" "$toolchain" "$generator" \
        "$scratch/cross-variable-abi-reject" "$abi_double"
    echo "$SCRIPT_NAME: PATH 1 (aperto de mao de versao, GATE-GL-ABI-HANDSHAKE) - binario incompativel REJEITADO - OK"

    unresponsive_abi_double="$scratch/abi-unresponsive-double.sh"
    make_unresponsive_abi_double "$unresponsive_abi_double"
    assert_path_variable_rejects_unresponsive_abi "$src" "$toolchain" "$generator" \
        "$scratch/cross-variable-abi-unresponsive" "$unresponsive_abi_double"
    echo "$SCRIPT_NAME: PATH 1 (aperto de mao de versao, segundo tipo GATE-GL-ABI-HANDSHAKE) - binario que nao responde a --codegen-abi REJEITADO - OK"

    double="$scratch/emulator-double.sh"
    double_log="$scratch/emulator-double.log"
    make_emulator_double "$double" "$native_tool" "$double_log"
    assert_path_emulator_matches_golden "$src" "$toolchain" "$generator" \
        "$scratch/cross-emulator" "$double" "$double_log" "$golden_hpp" "$golden_cpp"
    echo "$SCRIPT_NAME: PATH 2 (CMAKE_CROSSCOMPILING_EMULATOR) - dublê acionado pelo nome nu do alvo, artefato bate com o golden - OK"

    nested_src="$scratch/nested-src"
    copy_source_tree "$src" "$nested_src"
    assert_path_nested_matches_golden "$nested_src" "$cxx" "$toolchain" "$generator" \
        "$scratch/cross-nested" "$golden_hpp" "$golden_cpp"
    echo "$SCRIPT_NAME: PATH 3 (construcao aninhada) - build limpo, artefato bate com o golden - OK"

    assert_nested_incremental_rebuild_regenerates "$nested_src" "$scratch/cross-nested"
    echo "$SCRIPT_NAME: PATH 3 (construcao aninhada) - rebuild incremental regenera apos mudanca de fonte - OK (GATE-GL-NESTED-STALE)"

    assert_path4_hard_failure "$src" "$toolchain" "$generator" "$scratch/cross-path4"
    echo "$SCRIPT_NAME: PATH 4 (falha alta em configure time, GLINTFX_HOST_CXX_COMPILER invalido) - configure reprova citando GLINTFX_GL_CODEGEN_EXECUTABLE - OK"

    # Mensagem final: enumera SO o que as asserções acima de fato
    # exercitaram (achado da revisao de 23/09/2026 contra este mesmo
    # script - a versao anterior desta linha dizia "H-1 a H-4 provadas
    # de ponta a ponta" quando a via NEGATIVA do H-1, o aperto de mao de
    # versao, ainda nao tinha assercao nenhuma aqui; o roteiro passava
    # verde mesmo com aquela checagem desligada no cmake). Nao resume
    # "tudo passou" - lista os fatos que cada bloco acima provou.
    echo "$SCRIPT_NAME: provado - PATH 1 aceita o binario correto e REJEITA tanto um binario com ABI incompativel quanto um binario que nao responde a --codegen-abi (aperto de mao de versao, os dois tipos de mutante); PATH 2 aciona o dublê pelo nome nu do alvo; PATH 3 builda limpo E regenera em rebuild incremental apos mudanca de fonte; PATH 4 reprova em configure time citando GLINTFX_GL_CODEGEN_EXECUTABLE. Todos os quatro artefatos gerados (PATH 1/2/3) batem byte a byte com a referencia nativa dourada."
}

real_main "$@"
