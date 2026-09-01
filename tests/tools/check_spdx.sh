#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_spdx.sh - CI gate for repo-wide "SPDX-License-Identifier"
# header coverage (GODS_LAWS.md L-08: publico no GitHub, AGPL-3.0).
# Decisao do lider, onda W2, item SPDX-GATE, 25/08/2026: entre este
# portao e o de ancora morta nos manuais, ele escolheu SO este -
# arquivo nascido sem cabecalho enfraquece a licenca perante quem
# consome a lib de fora (LEI ZERO: consumidor desconhecido, nao um so),
# e o dano e juridico, nao cosmetico.
#
# ORIGEM: modelo herdado de /var/tmp/glintfx-plan/spdx-check/check_spdx.sh,
# deixado pela revisao adversarial de 22/08/2026 (rev-spdx.md), que
# auditou o item A10/e393279 e confirmou cobertura de 100% dos 35
# arquivos de codigo/build entao existentes, com os tres controles da
# GODS_LAWS.md L-40 (nao existia ainda como lei numerada naquele dia,
# mas o metodo ja era esse) ja provados contra aquele modelo. O achado
# 1 daquela revisao propos exatamente este arquivo como portao futuro -
# procurado antes de escrever do zero, como pedido.
#
# O QUE MUDOU em relacao ao modelo herdado: o modelo original recebia
# a LISTA de arquivos a varrer via stdin - a decisao de QUAIS arquivos
# entravam era tomada por fora, por busca dirigida (grep de extensao).
# Este arquivo enumera ele mesmo, com `git ls-files` (TODO arquivo
# rastreado, o universo fechado por construcao que GODS_LAWS.md L-40
# item 5 pede), e classifica cada um: por padrao EXIGE cabecalho, e so
# escapa quem estiver na lista curta e nomeada de excecoes abaixo (ver
# is_exempt). Extensao nova nascida amanha (.rs, .py, o que for) cai
# do lado "exige" sem precisar de outra edicao aqui - e exatamente o
# oposto do defeito (b) que check_no_x11.sh documenta no proprio
# cabecalho (lista de diretorios que nao cobria tools/ nem .github/).
#
# EXCECOES DECLARADAS, e por que cada uma (GODS_LAWS.md L-40: excecao
# nao declarada vira buraco):
#
#   *.md         - documentacao, nao codigo nem build (mesma
#                  classificacao da revisao de 22/08, rev-spdx.md).
#   LICENSE       - e o proprio texto legal da licenca; cabecalho SPDX
#                  dentro dele nao faz sentido (mesmo julgamento).
#   .gitignore     - configuracao declarativa, nao codigo/build (mesmo
#                  julgamento).
#   .bigtech-porte  - marcador de uma linha, sem logica, sem conteudo
#                  autoral relevante para copyright de codigo (mesmo
#                  julgamento; la registrado como achado de baixa
#                  prioridade, aqui formalizado no portao).
#   .claude/**      - configuracao do Claude Code em si (hooks, settings),
#                  nao codigo nem build do PRODUTO GlintFx.
#   *.json          - JSON padrao (RFC 8259) nao aceita comentario; nao
#                  ha sintaxe de cabecalho que nao quebre o arquivo.
#                  Unico caso hoje: .claude/settings.json, ja coberto
#                  pela excecao acima - mantida como regra propria
#                  porque um .json fora de .claude/ tambem cairia aqui
#                  por razao tecnica, nao por ser Claude Code.
#   third_party/khronos/<arquivo nomeado> - arquivo de TERCEIRO,
#                  vendorizado verbatim sob a EXCECAO No 1 de
#                  GODS_LAWS.md L-07 (gl.xml, Apache-2.0, Khronos
#                  Group; ver third_party/khronos/README.md para a
#                  proveniencia completa). Nao e nosso codigo, entao
#                  nao e cabecalho AGPL nosso pra por - e por-lo
#                  quebraria a prova de sha256 que o build usa pra
#                  provar que o arquivo nao foi tocado (gate de
#                  integridade em tools/gl_registry_codegen).
#
#                  ISENCAO ESTREITA DE PROPOSITO, com raciocinio
#                  explicito porque a onda W2 (item SPDX-GATE,
#                  25/08/2026) mediu o risco de terceiro: uma isencao
#                  de DIRETORIO larga demais (ex.: `third_party/*`)
#                  vira um esconderijo permanente onde codigo NOSSO
#                  passaria sem cabecalho de licenca, num repositorio
#                  publico sob AGPL - exatamente o dano juridico que
#                  este portao existe pra evitar. Por isso a isencao
#                  aqui NAO e "todo arquivo dentro de
#                  third_party/khronos/": e uma ENUMERACAO FECHADA e
#                  NOMEADA dos dois arquivos que a lei realmente abriu
#                  (is_known_khronos_vendor_file abaixo) - pequena e
#                  enumeravel, entao enumerada por inteiro
#                  (GODS_LAWS.md L-40 item 5), nao coberta por busca
#                  dirigida tipo "esta sob third_party/khronos/,
#                  entao passa". Um arquivo QUALQUER (nosso, de outro
#                  terceiro, ou so um nome desconhecido) que apareca
#                  dentro de third_party/khronos/ sem estar nessa
#                  lista continua EXIGINDO cabecalho, do mesmo jeito
#                  que exigiria em qualquer outro lugar do repo -
#                  status de vendorizado nao se herda do nome do
#                  diretorio, so se concede arquivo por arquivo, e so
#                  aos que a lei nomeia.
#
#                  DECISAO CONSCIENTE, registrada porque foi pedida:
#                  um `.cpp` NOSSO colocado dentro de
#                  third_party/khronos/ SEM cabecalho continuaria
#                  exigindo cabecalho por esta mesma regra (nao esta
#                  na lista fechada) - a isencao NAO escapa pra ele.
#                  Se esse `.cpp` chegasse COM cabecalho SPDX proprio,
#                  este portao (cuja unica responsabilidade e
#                  cabecalho de licenca) passaria, e corretamente: o
#                  dano juridico que L-08 protege (codigo sem licenca
#                  clara) nao existiria. O que sobraria - um arquivo
#                  nosso morando num diretorio nomeado "third_party",
#                  contra o proprio README daquele diretorio ("this
#                  directory is glintfx's ONLY vendored third-party
#                  file") - e um problema de organizacao/revisao (L-12,
#                  L-17: `git log --stat` mostrando arquivo estranho
#                  aterrissando ali e um sinal e tanto pra quem
#                  revisa), NAO um problema de cabecalho de licenca.
#                  Misturar as duas responsabilidades neste script
#                  seria o proprio portao virando monolito (L-17): um
#                  portao, uma pergunta ("tem cabecalho?"), nao duas.
#
# Cada excecao e conferida no CAMINHO EXATO relativo a raiz do repo,
# nunca por substring - "README.md" nao esconde "README.md.bak", e
# "third_party/khronos-fake/x" nao e "third_party/khronos/x" so
# porque comeca parecido (prova no selftest, ver
# selftest_third_party_khronos_control).
#
# Usage:
#   check_spdx.sh <repo-root-directory>
#   check_spdx.sh --selftest
#
# --selftest monta arvores descartaveis sob mktemp e as transforma em
# repositorio git de verdade (`git init` + `git add`) - a enumeracao
# real E `git ls-files`, entao a fixture PRECISA ser um repo, diferente
# das fixtures de check_no_x11.sh (que usa find puro sobre diretorio).
# Roda os tres controles que GODS_LAWS.md L-40 exige de todo portao:
# positivo (cabecalho presente passa, e arquivo isento sem cabecalho
# tambem passa), negativo (cabecalho ausente reprova e o caminho exato
# aparece na mensagem), e varredura vazia (repo git sem nenhum arquivo
# rastreado reprova, nunca passa em silencio - e o proprio motivo desta
# lei existir, GODS_LAWS.md L-40 caso "portao da lei do Wayland"). Mais
# dois controles proprios deste gate, alem dos tres que L-40 exige de
# todo portao: nao-e-repo (raiz sem .git recusada, nunca presumida
# vazia) e a isencao ISOLADA de third_party/khronos/ (item SPDX-GATE,
# 26/08/2026, EXCECAO No 1 de GODS_LAWS.md L-07) - prova, na mesma
# fixture, que os dois arquivos vendorizados NOMEADOS passam sem
# cabecalho, e que um arquivo desconhecido na MESMA pasta, uma pasta
# irma sob third_party/ e uma pasta com nome parecido continuam
# exigindo, do mesmo jeito que `.claude/hooks/foo.sh` isola o ramo
# `.claude/*` de `*.json` acima: sem esta fixture separada, um mutante
# que alargasse a isencao para `third_party/*` inteiro passaria pelos
# outros controles sem ser pego.
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

REQUIRED_HEADER='SPDX-License-Identifier: AGPL-3.0-or-later'

fail() {
    echo "check_spdx.sh: $1" >&2
    exit 1
}

# --- classificacao, fechada por construcao (GODS_LAWS.md L-40 item 5):
# excecao e uma lista curta e NOMEADA, nunca um padrao amplo tipo "todo
# arquivo sem extensao conhecida". Ver o cabecalho acima para a razao
# de cada linha. -----------------------------------------------------

# Enumeracao FECHADA e NOMEADA (GODS_LAWS.md L-40 item 5: espaco
# pequeno e enumeravel, entao enumerado por inteiro) dos arquivos que
# GODS_LAWS.md L-07 EXCECAO No 1 abre sob third_party/khronos/
# (khronos_vendor_files.sh, sourced abaixo, documenta os tres - dois
# vendorizados do Khronos mais o README.md do proprio glintfx).
# Deliberadamente SEPARADA de is_exempt: e o unico jeito de a isencao
# de diretorio nao virar "todo arquivo que aparecer ali passa" - ver o
# bloco de comentario acima (EXCECOES DECLARADAS) para o raciocinio
# completo do risco e da decisao sobre arquivo nosso na mesma pasta.
# Neste script, README.md nunca chega a esta funcao de qualquer forma
# (o ramo `*.md` de is_exempt, abaixo, casa primeiro) - a funcao so e
# exercitada aqui para os dois vendorizados; quem depende da terceira
# entrada e check_vendor_purity.sh.
#
# A lista em si NAO mora mais aqui: mora em khronos_vendor_files.sh
# (mesmo diretorio), sourced abaixo - a MESMA enumeracao que
# check_vendor_purity.sh usa (item VENDOR-PURITY, TODO.md), nunca duas
# listas que precisam concordar sem nada as obrigando. is_exempt()
# continua chamando is_known_khronos_vendor_file exatamente como antes;
# so a definicao da funcao mudou de lugar.
# shellcheck source=./khronos_vendor_files.sh
. "$(dirname "$0")/khronos_vendor_files.sh"

is_exempt() {
    path="$1"
    case "$path" in
        *.md) return 0 ;;
        LICENSE) return 0 ;;
        .gitignore) return 0 ;;
        .bigtech-porte) return 0 ;;
        .claude/*) return 0 ;;
        *.json) return 0 ;;
        third_party/khronos/*) is_known_khronos_vendor_file "$path" ;;
        *) return 1 ;;
    esac
}

# --- enumeracao -------------------------------------------------------

# `git ls-files` e o universo fechado por construcao: TODO arquivo
# rastreado, nada gerado, nada ignorado por .gitignore, nada de
# artefato de build - sem precisar manter, aqui, uma lista propria de
# diretorios a varrer (a lista propria e exatamente o defeito (b) que
# check_no_x11.sh documenta no proprio cabecalho). Uma falha real do
# git (raiz nao e repositorio, git ausente) NUNCA vira "nao achei nada,
# entao passa" - fica em `|| return 1`, nunca engolida.
scanned_files() {
    root="$1"
    # -c core.quotepath=false (GATE-QUOTEPATH, 01/09/2026): git's own
    # default (core.quotepath=true) prints any tracked path with a byte
    # >= 0x80 as a C-style octal-escaped, double-quoted string, and every
    # downstream lookup in this file uses that string literally as a
    # path on disk - a string that never exists, so the file gets
    # reported "sem cabecalho" even when it has one. Disabling quotepath
    # makes `git ls-files` print the raw UTF-8 bytes instead.
    git -c core.quotepath=false -C "$root" ls-files
}

require_nonempty_scan() {
    files="$1"
    if [ -z "$files" ]; then
        echo "check_spdx.sh: varredura vazia (0 arquivos rastreados)" >&2
        return 1
    fi
}

count_lines() {
    if [ -z "$1" ]; then
        echo 0
        return
    fi
    printf '%s\n' "$1" | wc -l | tr -d ' '
}

# Cada uma destas duas funcoes SO IMPRIME no stdout (nunca acumula em
# variavel dentro do corpo do loop) - o mesmo cuidado que
# check_no_x11.sh registra no proprio codigo (violations_in_file): um
# `| while read` roda em subshell sob `sh`, e variavel setada la dentro
# some ao sair do pipe. Capturar via `$(...)` e o unico jeito seguro.

required_files() {
    files="$1"
    printf '%s\n' "$files" | while IFS= read -r f; do
        [ -z "$f" ] && continue
        is_exempt "$f" && continue
        printf '%s\n' "$f"
    done
}

missing_headers() {
    root="$1"
    required="$2"
    printf '%s\n' "$required" | while IFS= read -r f; do
        [ -z "$f" ] && continue
        head -n 3 "$root/$f" 2>/dev/null | grep -qF "$REQUIRED_HEADER" && continue
        printf '%s\n' "$f"
    done
}

# --- checagem ----------------------------------------------------------

check_spdx() {
    root="$1"

    files="$(scanned_files "$root")" \
        || { echo "check_spdx.sh: 'git ls-files' falhou em '$root' (nao e repositorio git, ou git indisponivel) - varredura recusada, nunca presumida vazia" >&2; return 1; }
    require_nonempty_scan "$files" || return 1
    total_count="$(count_lines "$files")"

    required="$(required_files "$files")"
    required_count="$(count_lines "$required")"
    exempt_count=$((total_count - required_count))

    missing="$(missing_headers "$root" "$required")"

    if [ -n "$missing" ]; then
        missing_count="$(count_lines "$missing")"
        echo "check_spdx.sh: PROIBIDO (GODS_LAWS.md L-08, publico sob AGPL-3.0): $missing_count arquivo(s) sem '$REQUIRED_HEADER' nas 3 primeiras linhas:" >&2
        printf '%s\n' "$missing" | while IFS= read -r f; do
            echo "  $f" >&2
        done
        return 1
    fi

    echo "check_spdx.sh: 0 arquivo(s) sem cabecalho ($required_count exige(m), $exempt_count isento(s), $total_count rastreado(s) no total)"
}

# --- modo real -----------------------------------------------------------

require_root_dir_arg() {
    [ "$#" -eq 1 ] || fail "usage: check_spdx.sh <repo-root-directory>"
    [ -d "$1" ] || fail "directory not found: $1"
}

real_main() {
    require_root_dir_arg "$@"
    check_spdx "$1" || fail "cabecalho SPDX ausente em arquivo nao isento (ver mensagem acima)"
}

# --- fixtures e controles do --selftest -----------------------------------

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-spdx-selftest-XXXXXX"
}

# `git init` isolado e determinista: identidade de commit ficticia, sem
# tocar config global da maquina, para que --selftest nunca dependa de
# `git config --global user.*` estar setado no ambiente que o chama
# (CI ou local).
init_fixture_repo() {
    root="$1"
    mkdir -p "$root"
    git -C "$root" init -q
    git -C "$root" config user.email "selftest@check-spdx.invalid"
    git -C "$root" config user.name "check_spdx selftest"
}

track_all() {
    root="$1"
    git -C "$root" add -A
}

# Arvore com um arquivo de codigo COM cabecalho e um punhado de
# arquivos isentos SEM cabecalho - todos os seis ramos de is_exempt,
# um por um, para que o controle positivo prove que NENHUM deles e
# falsamente cobrado. `.claude/hooks/foo.sh` e DELIBERADAMENTE
# nao-json: `.claude/settings.json` sozinho casaria tanto o ramo
# `.claude/*` quanto o ramo `*.json`, e um mutante que apagasse SO
# `.claude/*` passaria pelo controle sem ser pego (medido ao vivo na
# protocolo de mutacao desta fatia - ver mensagem do commit). Este
# arquivo prova o ramo `.claude/*` isoladamente.
make_positive_fixture() {
    root="$1"
    mkdir -p "$root/src" "$root/.claude/hooks"
    printf '// SPDX-License-Identifier: AGPL-3.0-or-later\nint f();\n' > "$root/src/foo.cpp"
    printf '# doc sem cabecalho\n' > "$root/README.md"
    printf 'texto legal, sem cabecalho\n' > "$root/LICENSE"
    printf 'build/\n' > "$root/.gitignore"
    printf 'porte=bigtech\n' > "$root/.bigtech-porte"
    printf '{\n  "chave": "valor"\n}\n' > "$root/.claude/settings.json"
    printf '#!/bin/sh\necho hook, sem cabecalho\n' > "$root/.claude/hooks/foo.sh"
    printf '{\n  "outra": true\n}\n' > "$root/config.json"
    track_all "$root"
}

selftest_positive_control() {
    scratch="$1"
    root="$scratch/positive"
    make_positive_fixture "$root"

    if output="$(check_spdx "$root" 2>&1)"; then
        echo "selftest: controle POSITIVO OK (arquivo com cabecalho passa, seis ramos de excecao sem cabecalho nao sao cobrados)"
        return 0
    fi
    echo "selftest: controle POSITIVO FALHOU" >&2
    printf '%s\n' "$output" >&2
    return 1
}

# Mesma arvore do positivo, mais um segundo arquivo de codigo SEM
# cabecalho. Esperado: reprova, cita o caminho exato do arquivo faltante
# e NAO cita nenhum dos seis arquivos isentos.
selftest_negative_control() {
    scratch="$1"
    root="$scratch/negative"
    make_positive_fixture "$root"
    printf 'int g();\n' > "$root/src/bar.cpp"
    track_all "$root"

    if output="$(check_spdx "$root" 2>&1)"; then
        echo "selftest: controle NEGATIVO FALHOU (src/bar.cpp sem cabecalho deveria ter sido reprovado)" >&2
        return 1
    fi

    ok=1
    if ! printf '%s\n' "$output" | grep -qF "src/bar.cpp"; then
        echo "selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou src/bar.cpp)" >&2
        ok=0
    fi
    for isento in README.md LICENSE .gitignore .bigtech-porte .claude/settings.json .claude/hooks/foo.sh config.json; do
        if printf '%s\n' "$output" | grep -qF "$isento"; then
            echo "selftest: controle NEGATIVO FALHOU (cobrou arquivo isento '$isento' - excecao vazou)" >&2
            ok=0
        fi
    done

    if [ "$ok" -eq 1 ]; then
        echo "selftest: controle NEGATIVO OK (src/bar.cpp citado, nenhum arquivo isento cobrado)"
        printf '%s\n' "$output" >&2
        return 0
    fi
    printf '%s\n' "$output" >&2
    return 1
}

# Fixture ISOLADA pra o ramo third_party/khronos/*, pelo mesmo motivo
# que make_positive_fixture isola `.claude/hooks/foo.sh` de
# `.claude/*`/`*.json`: sem isolamento, um mutante que alargasse a
# excecao (por exemplo trocando `third_party/khronos/*` por
# `third_party/*`, ou trocando is_known_khronos_vendor_file por "return
# 0" incondicional) passaria pelos controles POSITIVO/NEGATIVO de cima
# sem ser pego, porque nenhum deles toca third_party/ em nenhuma forma.
# Cinco arquivos, cada um provando uma coisa diferente:
#   - src/foo.cpp             - controle: codigo nosso, com cabecalho, passa.
#   - third_party/khronos/gl.xml                   - NOMEADO, sem cabecalho, deve passar.
#   - third_party/khronos/LICENSE-APACHE-2.0.txt   - NOMEADO, sem cabecalho, deve passar.
#   - third_party/khronos/mystery.cpp     - MESMA pasta isenta, mas NAO nomeado -
#                                            deve continuar exigindo (fecha o
#                                            "buraco permanente" que uma isencao
#                                            so-por-diretorio abriria).
#   - third_party/other_vendor/vendor.dat - pasta IRMA sob third_party/, fora de
#                                            khronos/ - prova que a isencao nao
#                                            e `third_party/*` generico.
#   - third_party/khronos-fake/vendor.dat - nome PARECIDO, sem a barra exata
#                                            depois de "khronos" - prova que o
#                                            casamento e por caminho exato, nunca
#                                            por substring (mesma disciplina que
#                                            o cabecalho do arquivo ja documenta
#                                            pra "README.md" vs "README.md.bak").
make_third_party_khronos_fixture() {
    root="$1"
    mkdir -p "$root/src" "$root/third_party/khronos" \
        "$root/third_party/khronos-fake" "$root/third_party/other_vendor"
    printf '// SPDX-License-Identifier: AGPL-3.0-or-later\nint f();\n' > "$root/src/foo.cpp"
    printf '<comment>vendored verbatim, no header on purpose</comment>\n' > "$root/third_party/khronos/gl.xml"
    printf 'Apache License 2.0 full text, no SPDX header on purpose\n' > "$root/third_party/khronos/LICENSE-APACHE-2.0.txt"
    printf 'int surprise();\n' > "$root/third_party/khronos/mystery.cpp"
    printf 'not the exempt directory\n' > "$root/third_party/other_vendor/vendor.dat"
    printf 'looks like the exempt dir, is not\n' > "$root/third_party/khronos-fake/vendor.dat"
    track_all "$root"
}

selftest_third_party_khronos_control() {
    scratch="$1"
    root="$scratch/third_party_khronos"
    make_third_party_khronos_fixture "$root"

    if output="$(check_spdx "$root" 2>&1)"; then
        echo "selftest: controle third_party/khronos FALHOU (deveria ter reprovado - ha arquivos nao isentos sem cabecalho na fixture)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi

    ok=1
    for exigido in third_party/khronos/mystery.cpp third_party/other_vendor/vendor.dat third_party/khronos-fake/vendor.dat; do
        if ! printf '%s\n' "$output" | grep -qF "$exigido"; then
            echo "selftest: controle third_party/khronos FALHOU (nao citou '$exigido', que deveria exigir cabecalho)" >&2
            ok=0
        fi
    done
    for isento in third_party/khronos/gl.xml third_party/khronos/LICENSE-APACHE-2.0.txt; do
        if printf '%s\n' "$output" | grep -qF "$isento"; then
            echo "selftest: controle third_party/khronos FALHOU (cobrou '$isento', que GODS_LAWS.md L-07 EXCECAO No 1 isenta - excecao vazou)" >&2
            ok=0
        fi
    done

    if [ "$ok" -eq 1 ]; then
        echo "selftest: controle third_party/khronos OK (os dois arquivos vendorizados nomeados passam sem cabecalho; arquivo desconhecido na mesma pasta, pasta irma e pasta com nome parecido continuam exigindo)"
        printf '%s\n' "$output" >&2
        return 0
    fi
    printf '%s\n' "$output" >&2
    return 1
}

# Empty-scan floor: repositorio git valido, mas SEM nenhum arquivo
# rastreado (init sem add). Esperado: reprova com "varredura vazia" na
# mensagem - o proprio motivo desta lei existir (GODS_LAWS.md L-40).
selftest_empty_scan_control() {
    scratch="$1"
    root="$scratch/empty"
    init_fixture_repo "$root"

    if output="$(check_spdx "$root" 2>&1)"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (repo git sem arquivo rastreado deveria ter sido recusado, mas passou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "varredura vazia"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de VARREDURA VAZIA OK (repo git sem arquivo rastreado recusado)"
    return 0
}

# Controle extra, especifico deste gate (alem dos tres que a L-40
# exige): raiz que NAO e repositorio git nenhum. Esperado: git ls-files
# falha, e a falha vira reprovacao explicita - nunca "nao achei
# arquivo, entao passa" (a mesma falha de git que preci.sh's
# enumerate_untracked_cpp_hpp ja documenta e recusa).
selftest_not_a_repo_control() {
    scratch="$1"
    root="$scratch/not_a_repo"
    mkdir -p "$root/src"
    printf 'int h();\n' > "$root/src/baz.cpp"

    if output="$(check_spdx "$root" 2>&1)"; then
        echo "selftest: controle de NAO-E-REPO FALHOU (diretorio sem .git deveria ter sido recusado, mas passou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "nao e repositorio git"; then
        echo "selftest: controle de NAO-E-REPO FALHOU (recusou, mas nao disse o motivo)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de NAO-E-REPO OK (diretorio sem .git recusado, nao presumido vazio)"
    return 0
}

# GATE-QUOTEPATH (01/09/2026): git's own default (core.quotepath=true)
# prints any tracked path with a byte >= 0x80 as a C-style octal-escaped,
# double-quoted string (e.g. "Wayl\303\244nd.cpp" instead of
# Waylând.cpp). Every downstream lookup here uses that string literally
# as "$root/$f" - a path that never exists on disk, so `head -n 3` finds
# nothing and the file gets reported "sem cabecalho" even when the real
# file DOES carry it three lines in (a false PROIBIDO, not a silent
# pass, but still wrong for any accented-named source or doc). Proven
# with a file that HAS the header: before the fix this control fails
# (the accented file is wrongly reported missing), after it passes.
selftest_accented_filename_control() {
    scratch="$1"
    root="$scratch/accented"
    init_fixture_repo "$root"
    mkdir -p "$root/src"
    printf '// SPDX-License-Identifier: AGPL-3.0-or-later\nint f();\n' > "$root/src/Waylând.cpp"
    track_all "$root"

    if ! output="$(check_spdx "$root" 2>&1)"; then
        echo "selftest: ACCENTED-FILENAME control FAILED (src/Waylând.cpp tem o cabecalho SPDX nas 3 primeiras linhas e deveria ter passado - GATE-QUOTEPATH)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: ACCENTED-FILENAME control OK (arquivo .cpp com nome acentuado e cabecalho presente reconhecido corretamente, GATE-QUOTEPATH)"
}

selftest_main() {
    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    # positive/negative/third_party_khronos precisam de `git init` +
    # `git config` antes de `git add`; as make_*_fixture so criam
    # arquivos, entao os controles chamam init_fixture_repo primeiro.
    mkdir -p "$scratch/positive" "$scratch/negative" "$scratch/third_party_khronos"
    init_fixture_repo "$scratch/positive"
    init_fixture_repo "$scratch/negative"
    init_fixture_repo "$scratch/third_party_khronos"

    overall=0
    selftest_positive_control "$scratch" || overall=1
    selftest_negative_control "$scratch" || overall=1
    selftest_third_party_khronos_control "$scratch" || overall=1
    selftest_empty_scan_control "$scratch" || overall=1
    selftest_not_a_repo_control "$scratch" || overall=1
    selftest_accented_filename_control "$scratch" || overall=1

    if [ "$overall" -ne 0 ]; then
        echo "check_spdx.sh --selftest: FALHOU (ver acima)" >&2
        exit 1
    fi
    echo "check_spdx.sh --selftest: os seis controles OK"
}

main() {
    if [ "${1:-}" = "--selftest" ]; then
        selftest_main
    else
        real_main "$@"
    fi
}

main "$@"
