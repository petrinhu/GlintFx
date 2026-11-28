#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/ci/env/cachyos.sh - CI-SPLIT-PER-OS A3b (docs/plano-ci-split-
# per-os.md secao 4.4): preparo do alvo CachyOS. TRES responsabilidades
# e nada mais (secao 4.4): instalar, exportar CC/CXX ($GITHUB_ENV) e
# imprimir as versoes. A3c (D-A13): exporta tambem GLINTFX_PREP_SHA
# (sha256 do PROPRIO script) - o passo "Checkout e' repositorio git" do
# ci.yml confere esse valor contra o script real apos o checkout
# definitivo. GODS_LAWS.md L-04: CachyOS NAO reaproveita arch.sh -
# imagem, repositorios e toolchain proprios, script proprio.
#
# CI-PIN-PKG-C (D-WP1-0014, desenho 1a, parecer do CLO secao 7). O repositorio
# da onda e' montado AQUI a partir de tres fontes, cada arquivo provado por
# sha256 contra tools/ci/env/lock/cachyos.lock antes de qualquer uso:
#   - os 35 pacotes de core, extra e multilib: baixados do Arch Linux Archive
#     (archive.archlinux.org), pelo arquivo exato e a assinatura .sig;
#   - mesa e python (origem cachyos): do artifact cachyos-repo-onda do run
#     CACHYOS_REPO_RUN_ID (tools/ci/wave-stamp.env), baixado pela API do GitHub
#     com o mesmo GH_TOKEN de todas as pernas (token em `env:` do passo
#     Instalar toolchain, sem if: novo no ci.yml).
# Nenhum passo republica os 35 do Arch (artifact, cache, imagem ou release):
# o gate tests/tools/check_cachyos_nao_republica.py trava isso no ci.yml.
#
# Falha ALTO, com o run-id na mensagem, quando: o run-id esta vazio; a API nao
# responde; o artifact esta ausente, expirado ou duplicado; o lock do repo
# diverge do lock do artifact; ou qualquer sha256 diverge do lock (GODS_LAWS.md
# L-40). Nunca cai para repositorio vivo, sem sha256.
#
# Depois de provados, os pacotes entram num repositorio file:// local, com
# SigLevel = Required DatabaseOptional (a assinatura original, do chaveiro da
# imagem; o banco local nao e' assinado). Nao e' Never nem TrustAll.
#
# python EXPLICITO (B1): o pacote "python" tambem entrega /usr/bin/python3, e
# o portao de manifesto roda ao fim do preparo.
#
# glintfx-pkgconfig (D-A12 parte 4): ver o comentario em fedora.sh.
# glintfx-pkgconfig: wayland-client=wayland
# glintfx-pkgconfig: wayland-egl=wayland
# glintfx-pkgconfig: egl=libglvnd

set -eu

CC_BIN="cc"
CXX_BIN="c++"

. "$(dirname "$0")/../wave-stamp.env"

LOCK="$(dirname "$0")/lock/cachyos.lock"
ONDA=/var/tmp/glintfx-onda-cachyos
ALA="https://archive.archlinux.org/packages"
run_id="${CACHYOS_REPO_RUN_ID:-}"
NOME_ARTIFACT=cachyos-repo-onda

falhou() {
    echo "cachyos.sh: FALHOU: $1" >&2
    echo "cachyos.sh: run-id do artifact: ${run_id:-<vazio>} (tools/ci/wave-stamp.env)" >&2
    exit 1
}

[ -n "$run_id" ] || falhou "CACHYOS_REPO_RUN_ID vazio: nenhum repositorio da onda gerado ainda. Dispare o job cachyos-repo-gerar e commite o run-id (L-40)."
[ -s "$LOCK" ] || falhou "lock ausente ou vazio: $LOCK (L-40)"
[ -n "${GH_TOKEN:-}" ] || falhou "GH_TOKEN vazio: o passo Instalar toolchain nao recebeu o token do run (L-40)"
[ -n "${GITHUB_REPOSITORY:-}" ] || falhou "GITHUB_REPOSITORY vazio (L-40)"

rm -rf "$ONDA"
mkdir -p "$ONDA/api" "$ONDA/zip" "$ONDA/onda" "$ONDA/pkgs" "$ONDA/repo"
api="https://api.github.com/repos/$GITHUB_REPOSITORY/actions"
cabecalhos=(-H "Authorization: Bearer $GH_TOKEN" -H "Accept: application/vnd.github+json")

# 1. Achar o artifact do run: exatamente um, nao expirado.
curl --fail --silent --show-error --retry 3 --retry-delay 5 "${cabecalhos[@]}" \
    "$api/runs/$run_id/artifacts?name=$NOME_ARTIFACT&per_page=10" -o "$ONDA/api/artifacts.json" \
    || falhou "API de artifacts do run $run_id nao respondeu (token, permissao actions: read ou run inexistente) (L-40)"
achados="$(python3 - "$ONDA/api/artifacts.json" "$NOME_ARTIFACT" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
a = [x for x in d.get("artifacts", []) if x.get("name") == sys.argv[2]]
print(len(a))
for x in a:
    print(x["id"], "expirado" if x.get("expired") else "vigente")
PY
)"
quantos="$(printf '%s\n' "$achados" | sed -n 1p)"
[ "$quantos" = 1 ] || falhou "o run $run_id tem $quantos artifact(s) chamado(s) $NOME_ARTIFACT, esperado 1 (expirado ou duplicado) (L-40)"
art_id="$(printf '%s\n' "$achados" | sed -n 2p | cut -d' ' -f1)"
art_estado="$(printf '%s\n' "$achados" | sed -n 2p | cut -d' ' -f2)"
[ "$art_estado" = vigente ] || falhou "artifact $NOME_ARTIFACT do run $run_id esta expirado (retention-days 90) (L-40)"

# 2. Baixar o zip (302 para o armazenamento; curl -L nao manda o token para o outro host).
curl --fail --silent --show-error --location --retry 3 --retry-delay 5 "${cabecalhos[@]}" \
    "$api/artifacts/$art_id/zip" -o "$ONDA/zip/artifact.zip" \
    || falhou "download do artifact $art_id do run $run_id falhou (L-40)"
bsdtar -xf "$ONDA/zip/artifact.zip" -C "$ONDA/zip" || falhou "zip do artifact nao abriu (L-40)"
for arquivo in cachyos-onda.tar cachyos.lock manifesto.txt cachyos-versoes.txt SOURCES.md; do
    [ -s "$ONDA/zip/$arquivo" ] || falhou "artifact do run $run_id sem $arquivo (L-40)"
done

# 3. O lock do repo tem de ser o lock do artifact: lock velho nao passa.
grep -v '^#' "$ONDA/zip/cachyos.lock" > "$ONDA/lock-artefato.txt"
grep -v '^#' "$LOCK" > "$ONDA/lock-repo.txt"
cmp -s "$ONDA/lock-artefato.txt" "$ONDA/lock-repo.txt" \
    || falhou "tools/ci/env/lock/cachyos.lock diverge do lock do artifact do run $run_id (linhas de dados): o lock do repo esta velho (L-40)"
bsdtar -xf "$ONDA/zip/cachyos-onda.tar" -C "$ONDA/onda" || falhou "cachyos-onda.tar nao abriu (L-40)"

# 4. Montar o repositorio: cada arquivo vem da sua fonte e tem o sha256 do lock.
grep -v '^#' "$LOCK" > "$ONDA/lock-linhas.txt"
n_arch=0
n_cachyos=0
while read -r repo nome versao arq sha pbsha; do
    if [ "$repo" = cachyos ]; then
        origem="$ONDA/onda/binarios/$arq"
        cp "$origem" "$ONDA/pkgs/$arq" || falhou "pacote cachyos ausente no artifact: $arq (L-40)"
        cp "$origem.sig" "$ONDA/pkgs/$arq.sig" || falhou "assinatura ausente no artifact: $arq.sig (L-40)"
        n_cachyos=$((n_cachyos + 1))
    else
        letra="$(printf '%s' "$nome" | cut -c1)"
        url="$ALA/$letra/$nome/$arq"
        curl --fail --silent --show-error --location --retry 5 --retry-delay 5 -o "$ONDA/pkgs/$arq" "$url" \
            || falhou "ALA nao serviu $arq (L-40)"
        curl --fail --silent --show-error --location --retry 5 --retry-delay 5 -o "$ONDA/pkgs/$arq.sig" "$url.sig" \
            || falhou "ALA nao serviu a assinatura de $arq (L-40)"
        n_arch=$((n_arch + 1))
    fi
    sha_real="$(sha256sum "$ONDA/pkgs/$arq" | awk '{ print $1 }')"
    [ "$sha_real" = "$sha" ] || falhou "sha256 de $arq diverge do lock: $sha_real != $sha (L-40)"
done < "$ONDA/lock-linhas.txt"
echo "cachyos.sh: lock conferido: $n_arch do Arch (ALA) e $n_cachyos cachyos (artifact)"
[ "$n_cachyos" -ge 1 ] || falhou "piso: nenhum pacote cachyos no lock (L-40)"
[ "$n_arch" -ge 1 ] || falhou "piso: nenhum pacote do Arch no lock (L-40)"

cp "$ONDA"/pkgs/* "$ONDA/repo/"
repo-add -q "$ONDA/repo/glintfx-onda-cachyos.db.tar.gz" "$ONDA"/repo/*.pkg.tar.zst \
    || falhou "repo-add falhou (L-40)"
[ -s "$ONDA/repo/glintfx-onda-cachyos.db.tar.gz" ] || falhou "banco do repositorio da onda nao foi gravado (L-40)"

# Monta o pacman.conf: so' [options] da imagem, Include comentado, e um
# repositorio file://. A imagem original fica guardada ao lado para diagnostico.
REPO_DIR="$ONDA"
repo="$ONDA/repo"
cp /etc/pacman.conf "$REPO_DIR/pacman.conf.imagem"
novo="$REPO_DIR/pacman.conf.onda"
awk '/^\[/ && $0 != "[options]" { exit } { print }' /etc/pacman.conf \
    | sed 's/^[[:space:]]*Include/#Include/' > "$novo"
cat >> "$novo" <<CONF

[glintfx-onda-cachyos]
SigLevel = Required DatabaseOptional
Server = file://$repo
CONF
install -m 0644 "$novo" /etc/pacman.conf

# Prova (L-40): nenhum Include ativo, um unico repositorio efetivo, e o
# servidor dele e' file:// e nada mais.
if grep -q '^[[:space:]]*Include' /etc/pacman.conf; then
    falhou "Include ativo sobrou em /etc/pacman.conf"
fi
repos="$(pacman-conf --repo-list)"
[ "$repos" = "glintfx-onda-cachyos" ] || falhou "repositorios efetivos inesperados: [$repos]"
servidor="$(pacman-conf -r glintfx-onda-cachyos Server)"
case "$servidor" in
    "file://$repo") echo "cachyos.sh: repositorio efetivo: $servidor" ;;
    *) falhou "servidor efetivo fora de file://: [$servidor]" ;;
esac

pacman -Syy --noconfirm
pacman -Syu --noconfirm gcc cmake ninja pkgconf git wayland wayland-protocols libglvnd python

{
    echo "CC=${CC_BIN}"
    echo "CXX=${CXX_BIN}"
    echo "GLINTFX_PREP_SHA=$(sha256sum "$0" | awk '{print $1}')"
} >> "$GITHUB_ENV"

"$CC_BIN" --version
"$CXX_BIN" --version
cmake --version

# Portao de manifesto (CI-PIN-PKG-C): o instalado tem de bater com
# tools/ci/wave-manifest/cachyos.txt. Divergencia reprova o preparo.
python3 "$(dirname "$0")/../../../tests/tools/check_wave_manifest.py" --slug cachyos
