#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/ci/env/lock/gerar_cachyos_repo.sh - CI-PIN-PKG-C (D-W8-2094, opcao (d),
# ordem do CTO em .trabalho/cto-w8/decisao-ci-pin-pacotes.md, item 4).
#
# Roda DENTRO da imagem CachyOS fixada pelo digest, so no disparo manual do
# job cachyos-repo-gerar. Baixa para um cache LIMPO o fecho dos pacotes da
# linha pacman de tools/ci/env/cachyos.sh (a lista de hoje), monta um
# repositorio local com repo-add, copia os .sig dos pacotes junto e empacota
# tudo num unico .tar, que sobe como artifact com retention-days 90. Nao
# escreve no repositorio: o run-id do artifact vai, depois, para
# tools/ci/wave-stamp.env (quem o commita e' o implementador).
#
# A rotacao de espelho (ate 10 tentativas, rotacionando cachyos-mirrorlist
# quando o log acusa "signature from") MORA AQUI, e nao mais nas pernas
# (GODS_LAWS.md L-42; a historia esta no cabecalho de cachyos.sh).
#
# Uso (dentro do contenedor, nunca na maquina do lider):
#   gerar_cachyos_repo.sh <pasta com cachyos.sh> <pasta de saida>
#
# Piso (GODS_LAWS.md L-40): sem linha pacman na lista, lista vazia, pacote sem
# versao na DB, zero pacotes baixados ou assinatura faltando REPROVAM com codigo
# diferente de zero. Nada disso cai em sucesso silencioso.

set -eu

env_dir="${1:?gerar_cachyos_repo: falta a pasta com cachyos.sh}"
saida="${2:?gerar_cachyos_repo: falta a pasta de saida}"
script="$env_dir/cachyos.sh"

fail() {
    echo "gerar_cachyos_repo: $1" >&2
    exit 1
}

[ -s "$script" ] || fail "cachyos.sh ausente ou vazio: $script (L-40)"

# A lista de hoje e' lida da PROPRIA linha de instalacao de cachyos.sh (uma
# so' fonte: o que a perna instala e' o que o gerador baixa). Linha com barra
# invertida no fim continua na linha seguinte.
linhas_pacman="$(grep -c '^[[:space:]]*pacman -Syu --noconfirm' "$script" || true)"
[ "$linhas_pacman" -eq 1 ] || fail "cachyos.sh deve ter exatamente 1 linha 'pacman -Syu --noconfirm' (com ou sem indentacao) (achei $linhas_pacman) (L-40)"
lista="$(awk '
    /^[ \t]*pacman -Syu --noconfirm/ { p = 1; sub(/^[ \t]*pacman -Syu --noconfirm/, "") }
    p {
        sub(/[ \t]*>.*$/, "")
        continua = ($0 ~ /\\[ \t]*$/)
        sub(/\\[ \t]*$/, "")
        printf "%s ", $0
        if (!continua) exit
    }
' "$script")"
[ -n "$(printf '%s' "$lista" | tr -d ' \t')" ] || fail "lista de pacotes vazia (L-40)"
echo "gerar_cachyos_repo: lista de hoje: $lista"

cache="$saida/cache"
repo="$saida/cachyos-repo"
mkdir -p "$cache" "$repo"
log="$saida/pacman.log"

maximo=10
tentativa=0
codigo=1
while [ "$tentativa" -lt "$maximo" ]; do
    tentativa=$((tentativa + 1))
    echo "=== baixar o fecho CachyOS da onda, tentativa $tentativa/$maximo ==="
    # --cachedir proprio: o repositorio nao herda versoes velhas do cache da imagem.
    if pacman -Syy --noconfirm >"$log" 2>&1 \
        && pacman --noconfirm --cachedir "$cache" -Syu --downloadonly $lista >>"$log" 2>&1; then
        codigo=0
        break
    fi
    cat "$log"
    if [ "$tentativa" -lt "$maximo" ] && grep -q "signature from" "$log"; then
        echo "::warning::assinatura do banco CachyOS invalida (espelho dessincronizado), rotacionando cachyos-mirrorlist e tentando de novo"
        espelhos=/etc/pacman.d/cachyos-mirrorlist
        primeira=$(grep -n "^Server" "$espelhos" | head -1 | cut -d: -f1)
        if [ -n "$primeira" ]; then
            sed -n "${primeira}p" "$espelhos" >>"$espelhos"
            sed -i "${primeira}d" "$espelhos"
        fi
        continue
    fi
    fail "falha ao baixar o fecho CachyOS (codigo do pacman, sem sinal de assinatura ou tentativas esgotadas) (L-40)"
done
[ "$codigo" -eq 0 ] || fail "nao baixou o fecho CachyOS (L-40)"

for arquivo in "$cache"/*.pkg.tar.zst*; do
    [ -e "$arquivo" ] || fail "cache sem nenhum pacote .pkg.tar.zst (L-40)"
    cp "$arquivo" "$repo/"
done

pacotes=0
sigs=0
for arquivo in "$repo"/*.pkg.tar.zst; do
    [ -e "$arquivo" ] || fail "repositorio sem pacote .pkg.tar.zst (L-40)"
    pacotes=$((pacotes + 1))
    [ -s "$arquivo.sig" ] || fail "pacote sem assinatura .sig: $(basename "$arquivo") (L-40)"
    sigs=$((sigs + 1))
done
echo "gerar_cachyos_repo: $pacotes pacote(s) e $sigs assinatura(s)"

repo-add -q "$repo/glintfx-onda-cachyos.db.tar.gz" "$repo"/*.pkg.tar.zst >>"$log" 2>&1 \
    || fail "repo-add falhou (ver $log) (L-40)"
[ -s "$repo/glintfx-onda-cachyos.db.tar.gz" ] || fail "banco do repositorio nao foi gravado (L-40)"

# Versao de cada pacote da lista, como a DB do repositorio a traz. Entra no
# artifact para o implementador copiar o manifesto curto (nunca o fecho inteiro).
versoes="$saida/cachyos-versoes.txt"
: >"$versoes"
for pacote in $lista; do
    versao="$(pacman -Si "$pacote" | awk -F': *' '/^Version/ { print $2; exit }')"
    [ -n "$versao" ] || fail "pacote $pacote sem versao na DB (L-40)"
    echo "$pacote $versao" >>"$versoes"
done
echo "gerar_cachyos_repo: versoes da lista gravadas em $versoes"

# Empacota o repositorio num unico .tar (nomes com ':' de epoch e links
# simbolicos do repo-add nao atravessam o upload-artifact, o tar atravessa).
tar -C "$saida" -cf "$saida/cachyos-repo-onda.tar" cachyos-repo
rm -rf "$cache" "$repo"
echo "gerar_cachyos_repo: artifact gravado em $saida/cachyos-repo-onda.tar"
