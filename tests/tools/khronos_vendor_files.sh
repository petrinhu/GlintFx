# SPDX-License-Identifier: AGPL-3.0-or-later
#
# khronos_vendor_files.sh - unica fonte de verdade da enumeracao
# FECHADA e NOMEADA (GODS_LAWS.md L-40 item 5: espaco pequeno e
# enumeravel, entao enumerado por inteiro) dos unicos TRES arquivos que
# GODS_LAWS.md L-07 EXCECAO No 1 abre sob third_party/khronos/: os DOIS
# vendorizados verbatim do Khronos Group (gl.xml, LICENSE-APACHE-2.0.txt
# - obrigacao 1 da excecao, o texto Apache-2.0 precisa viajar junto) MAIS
# o README.md do PROPRIO glintfx que registra a proveniencia (obrigacao
# 4 da excecao - ver third_party/khronos/README.md e o proprio
# .gitattributes deste repo, que documenta explicitamente que aquele
# diretorio "also holds README.md, which is OUR OWN prose"). Os tres
# sao "o que a excecao enumerou" (item VENDOR-PURITY); so os dois
# primeiros sao vendorizados de terceiro - a distincao importa para
# quem le o nome desta lista, nao para quem so quer saber "isto
# pertence a este diretorio?".
#
# NAO E EXECUTAVEL SOZINHO. E uma biblioteca de shell: sourced (`.`),
# nunca chamado como script. Nao tem `set -eu` proprio nem chama nada
# no fim do arquivo - quem source-ia herda o proprio `set -eu`.
#
# SOURCED POR UM PORTAO (desde SPDX-GATE-PY, 02/09/2026 - ver abaixo):
#   - check_vendor_purity.sh (third_party/khronos/ contem SOMENTE estes
#                              dois arquivos, nada mais? item
#                              VENDOR-PURITY, GODS_LAWS.md L-07/L-40)
#
# Existe porque duas listas que precisam concordar sem nada as
# obrigando e exatamente o defeito que esta onda ja consertou uma vez
# (ver TODO.md, item VENDOR-PURITY) - um so lugar, este portao le dele.
#
# ATE 02/09/2026, check_spdx.sh tambem sourceava este arquivo (mesma
# pergunta: um arquivo sob third_party/khronos/ e isento do cabecalho
# AGPL? GODS_LAWS.md L-08). Nesse dia o lider fixou paridade de
# comportamento entre as cinco plataformas (GODS_LAWS.md L-04), e
# check_spdx.sh foi portado para tests/tools/check_spdx.py (Python 3,
# roda em Windows tambem) - um script que precisa rodar em Windows nao
# pode fazer `source` de uma biblioteca POSIX sh que so existe la.
# check_spdx.py DUPLICA esta mesma enumeracao de tres arquivos como
# KNOWN_KHRONOS_VENDOR_FILES, uma constante Python, com o mesmo aviso
# ao contrario neste comentario: se esta lista mudar, atualize as DUAS
# (aqui e em check_spdx.py) - a excecao GODS_LAWS.md L-07 No 1 so muda
# por decisao do lider, entao o custo de manter duas copias em sincronia
# a mao e baixo e conhecido, nunca silencioso.
#
# Usage:
#   . "$(dirname "$0")/khronos_vendor_files.sh"
#   depois chame known_khronos_vendor_files (lista, uma linha por
#   arquivo) ou is_known_khronos_vendor_file "<caminho>" (0/1).
#
# `caminho` e SEMPRE relativo a raiz do repositorio, no mesmo formato
# que `git ls-files` e `find "$root" ... | sed "s,^$root/,,"` produzem
# - casado por CAMINHO EXATO, nunca substring
# (third_party/khronos-fake/x nao e third_party/khronos/x so porque
# comeca parecido - prova nos selftests dos dois portoes que sourceiam
# este arquivo).

# A lista em si: uma linha por arquivo, a UNICA forma em que ela existe
# neste repositorio.
known_khronos_vendor_files() {
    printf '%s\n' \
        'third_party/khronos/gl.xml' \
        'third_party/khronos/LICENSE-APACHE-2.0.txt' \
        'third_party/khronos/README.md'
}

# `grep -F` (string literal, nunca regex) e `-x` (linha inteira, nunca
# substring) - a mesma disciplina de "caminho exato" que o cabecalho
# acima promete, agora aplicada com uma ferramenta cujo proprio nome
# ja declara a garantia, em vez de um `case` que precisaria repetir a
# lista uma segunda vez SO para virar comparavel por igualdade.
is_known_khronos_vendor_file() {
    known_khronos_vendor_files | grep -qFx "$1"
}
