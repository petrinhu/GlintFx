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
# SOURCED POR DOIS PORTOES, a mesma lista nos dois:
#   - check_spdx.sh          (um arquivo sob third_party/khronos/ e
#                              isento do cabecalho AGPL exigido em todo
#                              arquivo nao-vendorizado? GODS_LAWS.md L-08)
#   - check_vendor_purity.sh (third_party/khronos/ contem SOMENTE estes
#                              dois arquivos, nada mais? item
#                              VENDOR-PURITY, GODS_LAWS.md L-07/L-40)
#
# Existe porque duas listas que precisam concordar sem nada as
# obrigando e exatamente o defeito que esta onda ja consertou uma vez
# (ver TODO.md, item VENDOR-PURITY) - um so lugar, os dois portoes leem
# dele.
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
