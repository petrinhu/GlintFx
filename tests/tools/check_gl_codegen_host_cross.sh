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
#
# --- CROSS_* configuration globals --------------------------------------
#
# H-6b (revisao adversarial de 23/09/2026 contra check_gl_codegen_host_cross.sh,
# /var/tmp/glintfx-plan/revisao-codegen-h6.md, Fase 3): antes desta fatia, os
# quatro argumentos de linha de comando (fonte, compilador HOST, toolchain,
# gerador) e os dois caminhos do golden nativo eram copiados POSICIONALMENTE
# para dentro de CADA funcao assert_*/build_* que precisava deles, mesmo
# nunca mudando entre uma chamada e outra - a familia "copia em vez de
# fonte". Isso inflava assinaturas ate 8 parametros (GODS_LAWS.md L-17,
# bloqueante) so re-passando o que ja tinha dono. As seis globais abaixo sao
# a fonte unica, com prefixo proprio (nunca confundir com uma variavel local
# de funcao) e documentadas aqui - resolve o acoplamento escondido que uma
# copia solta (`src="$1"` sem marca nenhuma) deixaria.
#
#   CROSS_SRC        - diretorio fonte do glintfx (argv[1])
#   CROSS_CXX        - compilador C++ de HOST (argv[2])
#   CROSS_TOOLCHAIN  - arquivo de toolchain cruzado MinGW (argv[3])
#   CROSS_GENERATOR  - gerador do CMake (argv[4])
#   CROSS_GOLDEN_HPP - caminho do gl_functions.hpp do golden nativo
#   CROSS_GOLDEN_CPP - caminho do gl_functions.cpp do golden nativo
#
# Atribuidas UMA UNICA VEZ em real_main() - as quatro primeiras logo apos
# require_args(), as duas do golden logo apos build_native_golden() - e
# marcadas `readonly` na sequencia. Nenhuma funcao abaixo tem permissao de
# atribuir a elas, so de ler pelo nome; uma tentativa de reatribuicao morre
# (`readonly`, medido pela revisao: RC=1, "a variavel permite somente
# leitura", nunca aceita em silencio).

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
    build_dir="$1"
    cmake -S "$CROSS_SRC" -B "$build_dir" -G "$CROSS_GENERATOR" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CROSS_CXX" -DGLINTFX_BUILD_TESTS=OFF \
        >/dev/null || fail "golden: configure nativo falhou"
    cmake --build "$build_dir" --target glintfx_gl_functions_generated >/dev/null \
        || fail "golden: build nativo falhou"
}

assert_matches_golden() {
    label="$1"; candidate_hpp="$2"; candidate_cpp="$3"
    [ -f "$candidate_hpp" ] || fail "$label: $candidate_hpp nao foi gerado"
    [ -f "$candidate_cpp" ] || fail "$label: $candidate_cpp nao foi gerado"
    cmp -s "$CROSS_GOLDEN_HPP" "$candidate_hpp" \
        || fail "$label: gl_functions.hpp difere do golden nativo (cmp)"
    cmp -s "$CROSS_GOLDEN_CPP" "$candidate_cpp" \
        || fail "$label: gl_functions.cpp difere do golden nativo (cmp)"
}

# --- PATH 1: GLINTFX_GL_CODEGEN_EXECUTABLE ----------------------------

assert_path_variable_matches_golden() {
    build_dir="$1"; native_tool="$2"

    cmake -S "$CROSS_SRC" -B "$build_dir" -G "$CROSS_GENERATOR" \
        -DCMAKE_TOOLCHAIN_FILE="$CROSS_TOOLCHAIN" \
        -DCMAKE_BUILD_TYPE=Release -DGLINTFX_BUILD_TESTS=OFF \
        -DGLINTFX_GL_CODEGEN_EXECUTABLE="$native_tool" \
        >/dev/null || fail "PATH 1 (variable): configure cruzado falhou"
    cmake --build "$build_dir" --target glintfx_gl_functions_generated >/dev/null \
        || fail "PATH 1 (variable): build cruzado falhou"
    assert_matches_golden "PATH 1 (variable)" \
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
    build_dir="$1"; double="$2"

    set +e
    configure_output=$(cmake -S "$CROSS_SRC" -B "$build_dir" -G "$CROSS_GENERATOR" \
        -DCMAKE_TOOLCHAIN_FILE="$CROSS_TOOLCHAIN" \
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
    build_dir="$1"; double="$2"

    set +e
    configure_output=$(cmake -S "$CROSS_SRC" -B "$build_dir" -G "$CROSS_GENERATOR" \
        -DCMAKE_TOOLCHAIN_FILE="$CROSS_TOOLCHAIN" \
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
    build_dir="$1"; double="$2"; log_path="$3"

    cmake -S "$CROSS_SRC" -B "$build_dir" -G "$CROSS_GENERATOR" \
        -DCMAKE_TOOLCHAIN_FILE="$CROSS_TOOLCHAIN" \
        -DCMAKE_BUILD_TYPE=Release -DGLINTFX_BUILD_TESTS=OFF \
        -DCMAKE_CROSSCOMPILING_EMULATOR="$double" \
        >/dev/null || fail "PATH 2 (emulator): configure cruzado falhou"
    cmake --build "$build_dir" --target glintfx_gl_functions_generated >/dev/null \
        || fail "PATH 2 (emulator): build cruzado falhou"
    [ -s "$log_path" ] || fail "PATH 2 (emulator): o dublê nunca foi chamado (log vazio - CMAKE_CROSSCOMPILING_EMULATOR nao foi acionado, regressao para \$<TARGET_FILE:...> - H-2)"
    grep -q "^CHAMADO: " "$log_path" || fail "PATH 2 (emulator): log nao registrou nenhuma chamada valida"
    assert_matches_golden "PATH 2 (emulator)" \
        "$build_dir/generated/render/gl_functions.hpp" "$build_dir/generated/render/gl_functions.cpp"
}

# --- PATH 3: nested native build (a COPY, so it can be mutated later) -

copy_source_tree() {
    dst="$1"
    mkdir -p "$dst"
    if git -C "$CROSS_SRC" rev-parse --git-dir >/dev/null 2>&1; then
        git -C "$CROSS_SRC" archive HEAD | tar -x -C "$dst"
    else
        # Fallback for a source dir that is not itself a git checkout -
        # e.g. this script's own local verification runs against a lab
        # copy made by `git archive HEAD | tar -x`, which has no .git
        # of its own. A single cp -a plus removing any build output
        # produces the same clean source tree git archive would have,
        # without a per-item loop (GODS_LAWS.md global L-11: proibido
        # gastar um processo por item varrido). In real CI, CROSS_SRC is
        # always the actions/checkout clone, so this branch is never
        # taken there.
        cp -a "$CROSS_SRC"/. "$dst"/
        rm -rf "$dst"/build "$dst"/build-static "$dst"/.git
    fi
}

assert_path_nested_matches_golden() {
    nested_src="$1"; build_dir="$2"

    cmake -S "$nested_src" -B "$build_dir" -G "$CROSS_GENERATOR" \
        -DCMAKE_TOOLCHAIN_FILE="$CROSS_TOOLCHAIN" \
        -DCMAKE_BUILD_TYPE=Release -DGLINTFX_BUILD_TESTS=OFF \
        -DGLINTFX_HOST_CXX_COMPILER="$CROSS_CXX" \
        >/dev/null || fail "PATH 3 (nested): configure cruzado falhou"
    cmake --build "$build_dir" --target glintfx_gl_functions_generated >/dev/null \
        || fail "PATH 3 (nested): build cruzado falhou"
    assert_matches_golden "PATH 3 (nested)" \
        "$build_dir/generated/render/gl_functions.hpp" "$build_dir/generated/render/gl_functions.cpp"
}

# GATE-GL-NESTED-STALE: reusa o MESMO build_dir do teste acima (ja
# configurado, ja construido uma vez), muta o BANNER que o gerador
# imprime numa copia (nested_src, nunca a arvore rastreada - L-27),
# reconstroi o MESMO alvo de forma incremental (nenhum apagar, nenhuma
# reconfiguracao a mao - o comando exato que um desenvolvedor rodaria)
# e prova que o header reflete o texto mutante. Antes do conserto desta
# rodada, isto media rc=0 com o texto ANTIGO (achado critico da
# revisao). nested_src e build_dir continuam parametros (nao globais
# CROSS_*): os dois variam por chamada - sao a copia mutavel e o
# diretorio de build reusado do teste anterior, nunca constantes da
# execucao inteira como CROSS_SRC/CXX/TOOLCHAIN/GENERATOR.
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
    build_dir="$1"

    set +e
    configure_output=$(cmake -S "$CROSS_SRC" -B "$build_dir" -G "$CROSS_GENERATOR" \
        -DCMAKE_TOOLCHAIN_FILE="$CROSS_TOOLCHAIN" \
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

# H-6b (revisao adversarial de 23/09/2026 contra este script, Fase 3):
# real_main() sozinha tinha 52-55 linhas, acima do teto de L-17 - nao por
# excesso de parametros (ainda usa "$@", igual require_args), mas por reunir
# cinco fases num corpo so. Quebrada em cinco funcoes nomeadas por fase,
# cada uma ≤40 linhas e sem parametro nenhum (le os globais que a fase
# anterior deixou prontos). Ordem e comportamento de cada asserção
# permanecem exatamente os mesmos - so a fronteira de funcao mudou.
#
# CROSS_RUN_* (correcao do orquestrador sobre a 1ª extração): `scratch`,
# `native_tool` e `golden_build` tambem sao atribuidos UMA vez (em
# prepare_run) e so lidos pelas fases seguintes - o MESMO padrao de fonte
# unica que motivou os CROSS_* de configuracao, so que aqui e estado de UMA
# execucao (diretorio de scratch, ferramenta HOST construida), nao valor
# vindo de argv. Prefixo proprio `CROSS_RUN_` (para nao confundir com os
# seis CROSS_* de configuracao) e `readonly` logo apos a atribuicao - global
# mutavel sem marca e o mesmo acoplamento escondido que os CROSS_* vieram
# evitar, so que entre fases de real_main em vez de entre argv e funcao.

prepare_run() {
    require_args "$@"
    CROSS_SRC="$1"; CROSS_CXX="$2"; CROSS_TOOLCHAIN="$3"; CROSS_GENERATOR="$4"
    readonly CROSS_SRC CROSS_CXX CROSS_TOOLCHAIN CROSS_GENERATOR

    CROSS_RUN_SCRATCH=$(make_scratch_workdir)
    readonly CROSS_RUN_SCRATCH
    trap 'rm -rf "$CROSS_RUN_SCRATCH"' EXIT

    CROSS_RUN_GOLDEN_BUILD="$CROSS_RUN_SCRATCH/golden"
    readonly CROSS_RUN_GOLDEN_BUILD
    build_native_golden "$CROSS_RUN_GOLDEN_BUILD"
    CROSS_GOLDEN_HPP="$CROSS_RUN_GOLDEN_BUILD/generated/render/gl_functions.hpp"
    CROSS_GOLDEN_CPP="$CROSS_RUN_GOLDEN_BUILD/generated/render/gl_functions.cpp"
    readonly CROSS_GOLDEN_HPP CROSS_GOLDEN_CPP
    CROSS_RUN_NATIVE_TOOL="$CROSS_RUN_GOLDEN_BUILD/tools/gl_registry_codegen/gl_registry_codegen"
    readonly CROSS_RUN_NATIVE_TOOL
    [ -x "$CROSS_RUN_NATIVE_TOOL" ] || fail "golden: ferramenta HOST nativa nao encontrada/executavel: $CROSS_RUN_NATIVE_TOOL"
}

run_path1_checks() {
    assert_path_variable_matches_golden "$CROSS_RUN_SCRATCH/cross-variable" "$CROSS_RUN_NATIVE_TOOL"
    echo "$SCRIPT_NAME: PATH 1 (GLINTFX_GL_CODEGEN_EXECUTABLE) - binario correto aceito, artefato bate com o golden - OK"

    abi_double="$CROSS_RUN_SCRATCH/abi-mismatch-double.sh"
    make_mismatched_abi_double "$abi_double"
    assert_path_variable_rejects_abi_mismatch "$CROSS_RUN_SCRATCH/cross-variable-abi-reject" "$abi_double"
    echo "$SCRIPT_NAME: PATH 1 (aperto de mao de versao, GATE-GL-ABI-HANDSHAKE) - binario incompativel REJEITADO - OK"

    unresponsive_abi_double="$CROSS_RUN_SCRATCH/abi-unresponsive-double.sh"
    make_unresponsive_abi_double "$unresponsive_abi_double"
    assert_path_variable_rejects_unresponsive_abi "$CROSS_RUN_SCRATCH/cross-variable-abi-unresponsive" "$unresponsive_abi_double"
    echo "$SCRIPT_NAME: PATH 1 (aperto de mao de versao, segundo tipo GATE-GL-ABI-HANDSHAKE) - binario que nao responde a --codegen-abi REJEITADO - OK"
}

run_path2_and_path3_checks() {
    double="$CROSS_RUN_SCRATCH/emulator-double.sh"
    double_log="$CROSS_RUN_SCRATCH/emulator-double.log"
    make_emulator_double "$double" "$CROSS_RUN_NATIVE_TOOL" "$double_log"
    assert_path_emulator_matches_golden "$CROSS_RUN_SCRATCH/cross-emulator" "$double" "$double_log"
    echo "$SCRIPT_NAME: PATH 2 (CMAKE_CROSSCOMPILING_EMULATOR) - dublê acionado pelo nome nu do alvo, artefato bate com o golden - OK"

    nested_src="$CROSS_RUN_SCRATCH/nested-src"
    copy_source_tree "$nested_src"
    assert_path_nested_matches_golden "$nested_src" "$CROSS_RUN_SCRATCH/cross-nested"
    echo "$SCRIPT_NAME: PATH 3 (construcao aninhada) - build limpo, artefato bate com o golden - OK"

    assert_nested_incremental_rebuild_regenerates "$nested_src" "$CROSS_RUN_SCRATCH/cross-nested"
    echo "$SCRIPT_NAME: PATH 3 (construcao aninhada) - rebuild incremental regenera apos mudanca de fonte - OK (GATE-GL-NESTED-STALE)"
}

run_path4_check() {
    assert_path4_hard_failure "$CROSS_RUN_SCRATCH/cross-path4"
    echo "$SCRIPT_NAME: PATH 4 (falha alta em configure time, GLINTFX_HOST_CXX_COMPILER invalido) - configure reprova citando GLINTFX_GL_CODEGEN_EXECUTABLE - OK"
}

# Mensagem final: enumera SO o que as asserções acima de fato exercitaram
# (achado da revisao de 23/09/2026 contra este mesmo script - a versao
# anterior desta linha dizia "H-1 a H-4 provadas de ponta a ponta" quando a
# via NEGATIVA do H-1, o aperto de mao de versao, ainda nao tinha assercao
# nenhuma aqui; o roteiro passava verde mesmo com aquela checagem desligada
# no cmake). Nao resume "tudo passou" - lista os fatos que cada bloco
# acima provou.
print_summary() {
    echo "$SCRIPT_NAME: provado - PATH 1 aceita o binario correto e REJEITA tanto um binario com ABI incompativel quanto um binario que nao responde a --codegen-abi (aperto de mao de versao, os dois tipos de mutante); PATH 2 aciona o dublê pelo nome nu do alvo; PATH 3 builda limpo E regenera em rebuild incremental apos mudanca de fonte; PATH 4 reprova em configure time citando GLINTFX_GL_CODEGEN_EXECUTABLE. Todos os quatro artefatos gerados (PATH 1/2/3) batem byte a byte com a referencia nativa dourada."
}

real_main() {
    prepare_run "$@"
    run_path1_checks
    run_path2_and_path3_checks
    run_path4_check
    print_summary
}

real_main "$@"
