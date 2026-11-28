#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/ci/env/lock/gerar_fedora_lock.sh - CI-PIN-PKG-B (ordem do CTO, D-W8-2090, item 3):
# roda DENTRO da imagem Fedora fixada pelo digest, no disparo manual do CI. Instala a
# UNIAO dos pacotes dos pontos Fedora numa imagem viva, trava cada pacote instalado na
# versao exata com o proprio dnf5 (`versionlock add`, que grava /etc/dnf/versionlock.toml)
# e copia o lock e o inventario do instalado para a pasta de saida.
#
# Uso (dentro do contenedor, nunca na maquina do lider):
#   gerar_fedora_lock.sh <arquivo-da-uniao> <pasta-de-saida>
#
# Piso (GODS_LAWS.md L-40): uniao vazia, pacote que nao instalou, lock sem nenhuma
# entrada ou inventario vazio REPROVAM com codigo diferente de zero. Nao escreve no
# repositorio: quem baixa o artifact e commita o lock.

set -eu

uniao="${1:?gerar_fedora_lock: falta o arquivo da uniao}"
saida="${2:?gerar_fedora_lock: falta a pasta de saida}"

[ -s "$uniao" ] || { echo "gerar_fedora_lock: uniao vazia (L-40)" >&2; exit 1; }
pacotes="$(cat "$uniao")"
echo "gerar_fedora_lock: instalando $(printf '%s\n' "$pacotes" | wc -l) pacote(s) da uniao"
# shellcheck disable=SC2086
dnf -y install $pacotes

for pacote in $pacotes; do
    rpm -q "$pacote" >/dev/null 2>&1 || { echo "gerar_fedora_lock: $pacote nao ficou instalado (L-40)" >&2; exit 1; }
done

nomes="$(rpm -qa --qf '%{NAME}\n' | grep -v '^gpg-pubkey$' | sort -u)"
[ -n "$nomes" ] || { echo "gerar_fedora_lock: varredura do instalado vazia (L-40)" >&2; exit 1; }
# shellcheck disable=SC2086
dnf5 versionlock add $nomes >/dev/null

entradas="$(grep -c '^\[\[packages\]\]' /etc/dnf/versionlock.toml || true)"
[ "${entradas:-0}" -gt 0 ] || { echo "gerar_fedora_lock: lock sem nenhuma entrada (L-40)" >&2; exit 1; }
[ "$entradas" -eq "$(printf '%s\n' "$nomes" | wc -l)" ] \
    || { echo "gerar_fedora_lock: $entradas entrada(s) no lock para $(printf '%s\n' "$nomes" | wc -l) nome(s) instalado(s) (L-40)" >&2; exit 1; }

mkdir -p "$saida"
cp /etc/dnf/versionlock.toml "$saida/fedora.versionlock.toml"
cp "$uniao" "$saida/uniao.txt"
rpm -qa --qf '%{NAME} %{EVR}.%{ARCH}\n' | sort > "$saida/instalado.txt"
echo "gerar_fedora_lock: lock com $entradas entrada(s); instalado com $(wc -l < "$saida/instalado.txt") linha(s)"
