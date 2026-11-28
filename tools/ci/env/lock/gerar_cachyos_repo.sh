#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/ci/env/lock/gerar_cachyos_repo.sh - CI-PIN-PKG-C, desenho 1a
# (D-WP1-0014, meio-termo com o fonte; parecer do CLO, secao 7).
#
# Roda DENTRO da imagem CachyOS fixada pelo digest, so no disparo manual do job
# cachyos-repo-gerar. Faz tres coisas, nesta ordem:
#   1. baixa para um cache LIMPO o fecho da lista de hoje (a linha pacman de
#      tools/ci/env/cachyos.sh) e identifica a origem de cada arquivo;
#   2. os 35 pacotes de core, extra e multilib NAO saem daqui para publicacao:
#      o lock grava nome, versao, sha256 e origem, e cada sha256 e provado no
#      Arch Linux Archive (ALA) antes de o lock ser escrito;
#   3. os pacotes de origem cachyos (hoje mesa e python) saem com o fonte exato:
#      a pasta da receita no commit cujo PKGBUILD tem o pkgbuild_sha256sum do
#      .BUILDINFO, as fontes conferidas contra o .SRCINFO, as licencas e os
#      binarios com .sig.
#
# Saidas (so' esta pasta vai ao artifact, por upload-artifact):
#   <saida>/publicar/cachyos-onda.tar  binarios cachyos, fonte exato, licencas
#   <saida>/publicar/cachyos.lock      lock: repo nome versao arquivo sha256 pkgbuild_sha256
#   <saida>/publicar/cachyos-versoes.txt  "pacote versao" da lista, para o manifesto
#   <saida>/publicar/manifesto.txt     pkgname pkgbase versao origem sha256 sha256-PKGBUILD licenca
#   <saida>/publicar/SOURCES.md        o que e' o artifact e as ressalvas do CLO
# Fica so' no disco do runner, nunca sobe: <saida>/trabalho/ (cache, ALA, fontes).
#
# Uso (dentro do contenedor, nunca na maquina do lider):
#   gerar_cachyos_repo.sh <pasta com cachyos.sh> <pasta de saida> <pasta tests/tools>
#
# Piso (GODS_LAWS.md L-40): cada falha reprova com codigo diferente de zero. A
# verificacao final e' o portao tests/tools/check_cachyos_onda.py, rodado sobre o
# que sobe; se ele reprova, nada sobe.

set -eu

env_dir="${1:?gerar_cachyos_repo: falta a pasta com cachyos.sh}"
saida="${2:?gerar_cachyos_repo: falta a pasta de saida}"
testes="${3:?gerar_cachyos_repo: falta a pasta tests/tools}"
script="$env_dir/cachyos.sh"

fail() {
    echo "gerar_cachyos_repo: FALHOU: $1" >&2
    exit 1
}

[ -s "$script" ] || fail "cachyos.sh ausente ou vazio: $script (L-40)"

# A lista de hoje e' lida da PROPRIA linha de instalacao de cachyos.sh (uma so' fonte).
linhas_pacman="$(grep -c '^[[:space:]]*pacman -Syu --noconfirm' "$script" || true)"
[ "$linhas_pacman" -eq 1 ] || fail "cachyos.sh deve ter exatamente 1 linha 'pacman -Syu --noconfirm' (achei $linhas_pacman) (L-40)"
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

trab="$saida/trabalho"
pub="$saida/publicar"
cache="$trab/cache"
ala="$trab/ala"
mkdir -p "$cache" "$ala" "$pub"
log="$trab/pacman.log"

# ---------------------------------------------------------------------------
# 1. Baixar o fecho para cache limpo, com rotacao de espelho (L-42).
# ---------------------------------------------------------------------------
maximo=10
tentativa=0
codigo=1
while [ "$tentativa" -lt "$maximo" ]; do
    tentativa=$((tentativa + 1))
    echo "=== baixar o fecho da onda, tentativa $tentativa/$maximo ==="
    if pacman -Syy --noconfirm >"$log" 2>&1 \
        && pacman --noconfirm --cachedir "$cache" -Syu --downloadonly $lista >>"$log" 2>&1; then
        codigo=0
        break
    fi
    cat "$log"
    if [ "$tentativa" -lt "$maximo" ] && grep -q "signature from" "$log"; then
        echo "::warning::assinatura do banco invalida (espelho dessincronizado), rotacionando cachyos-mirrorlist"
        espelhos=/etc/pacman.d/cachyos-mirrorlist
        primeira=$(grep -n "^Server" "$espelhos" | head -1 | cut -d: -f1)
        if [ -n "$primeira" ]; then
            sed -n "${primeira}p" "$espelhos" >>"$espelhos"
            sed -i "${primeira}d" "$espelhos"
        fi
        continue
    fi
    fail "falha ao baixar o fecho (sem sinal de assinatura ou tentativas esgotadas) (L-40)"
done
[ "$codigo" -eq 0 ] || fail "nao baixou o fecho (L-40)"

# ---------------------------------------------------------------------------
# 2. Origem de cada repositorio: a primeira secao do pacman-conf que tem o
#    par nome+versao vence, a mesma regra que o pacman aplica ao -Syu.
# ---------------------------------------------------------------------------
origem="$trab/origem.txt"
: >"$origem"
for repo in $(pacman-conf --repo-list); do
    pacman -Sl "$repo" | awk -v r="$repo" 'NF >= 3 { print r, $2, $3 }' >>"$origem"
done
[ -s "$origem" ] || fail "origem vazia: nenhum repositorio listou pacote (L-40)"

itens="$trab/itens.txt"
: >"$itens"
n_arquivos=0
for arq in "$cache"/*.pkg.tar.zst; do
    [ -e "$arq" ] || fail "cache sem nenhum .pkg.tar.zst (L-40)"
    n_arquivos=$((n_arquivos + 1))
    base_arq="$(basename "$arq")"
    [ -s "$arq.sig" ] || fail "pacote sem assinatura .sig: $base_arq (L-40)"
    info="$(bsdtar -xOf "$arq" .PKGINFO)"
    nome="$(printf '%s\n' "$info" | sed -n 's/^pkgname = //p' | head -1)"
    pkgbase="$(printf '%s\n' "$info" | sed -n 's/^pkgbase = //p' | head -1)"
    versao="$(printf '%s\n' "$info" | sed -n 's/^pkgver = //p' | head -1)"
    [ -n "$nome" ] && [ -n "$pkgbase" ] && [ -n "$versao" ] || fail "PKGINFO sem pkgname/pkgbase/pkgver: $base_arq (L-40)"
    repo="$(awk -v n="$nome" -v v="$versao" '$2 == n && $3 == v { print $1; exit }' "$origem")"
    [ -n "$repo" ] || fail "origem nao achada para $nome $versao (L-40)"
    sha="$(sha256sum "$arq" | awk '{ print $1 }')"
    bsha=""
    if [ "$repo" = cachyos ]; then
        bsha="$(bsdtar -xOf "$arq" .BUILDINFO | sed -n 's/^pkgbuild_sha256sum = //p' | head -1)"
        [ -n "$bsha" ] || fail "cachyos sem pkgbuild_sha256sum no .BUILDINFO: $nome (L-40)"
    fi
    printf '%s %s %s %s %s %s %s %s\n' "$repo" "$nome" "$versao" "$base_arq" "$sha" "${bsha:--}" "$pkgbase" "$arq" >>"$itens"
done
[ "$n_arquivos" -gt 0 ] || fail "zero pacotes no cache (L-40)"
n_cachyos=$(awk '$1 == "cachyos"' "$itens" | wc -l)
n_arch=$(awk '$1 != "cachyos"' "$itens" | wc -l)
echo "gerar_cachyos_repo: $n_arquivos arquivo(s): $n_arch de core/extra/multilib, $n_cachyos de cachyos"
[ "$n_cachyos" -ge 1 ] || fail "piso: nenhum pacote cachyos no fecho (L-40)"
[ "$n_arch" -ge 1 ] || fail "piso: nenhum pacote do Arch no fecho (L-40)"

# ---------------------------------------------------------------------------
# 3. Os pacotes do Arch: prova cada sha256 no ALA. Nunca basta HTTP 200 (o
#    soft-404 ja mentiu uma vez); o arquivo tem de ser igual byte a byte ao do
#    espelho usado pelo gerador.
# ---------------------------------------------------------------------------
awk '$1 != "cachyos"' "$itens" | while read -r repo nome versao arq sha bsha pkgbase caminho; do
    letra="$(printf '%s' "$nome" | cut -c1)"
    url="https://archive.archlinux.org/packages/$letra/$nome/$arq"
    # Baixa para .part e so' renomeia ao fim, para que um download cortado nunca
    # passe por completo. Um arquivo ja completo de uma rodada anterior e' reusado,
    # e mesmo assim o sha256 e' conferido contra o espelho em todo caso.
    if [ ! -s "$ala/$arq" ]; then
        curl -fsSL --retry 5 --retry-delay 5 -o "$ala/$arq.part" "$url" \
            || fail "ALA nao serviu $arq (L-40)"
        mv "$ala/$arq.part" "$ala/$arq"
    fi
    if [ ! -s "$ala/$arq.sig" ]; then
        curl -fsSL --retry 5 --retry-delay 5 -o "$ala/$arq.sig.part" "$url.sig" \
            || fail "ALA nao serviu a assinatura de $arq (L-40)"
        mv "$ala/$arq.sig.part" "$ala/$arq.sig"
    fi
    sha_ala="$(sha256sum "$ala/$arq" | awk '{ print $1 }')"
    [ "$sha_ala" = "$sha" ] || fail "sha256 do ALA diverge do espelho para $arq: ALA=$sha_ala espelho=$sha (L-40)"
    cmp -s "$ala/$arq.sig" "$caminho.sig" || fail "assinatura do ALA diverge da do espelho para $arq (L-40)"
    echo "gerar_cachyos_repo: ALA confirma $arq"
done
# Ate aqui, a pipeline do while so' prova em subshell; o veredito real e' o
# de cada fail acima, que sai com codigo nao zero e para o script pelo set -e.
# Conferencia de contagem: todo arquivo do Arch tem de ter copia no ALA.
n_ala=$(find "$ala" -maxdepth 1 -name '*.pkg.tar.zst' | wc -l)
[ "$n_ala" -eq "$n_arch" ] || fail "ALA: $n_ala arquivo(s) provados, esperados $n_arch (L-40)"

# ---------------------------------------------------------------------------
# 4. Lock: repo nome versao arquivo sha256 pkgbuild_sha256 (- para o Arch).
# ---------------------------------------------------------------------------
lock="$pub/cachyos.lock"
{
    echo "# cachyos.lock - CI-PIN-PKG-C (D-WP1-0014). Gerado por gerar_cachyos_repo.sh."
    echo "# Formato: repo nome versao arquivo sha256 pkgbuild_sha256 (- para o Arch)."
    echo "# Os de repo core/extra/multilib vem do ALA e sao provados por sha256; os cachyos saem do artifact."
    awk '{ print $1, $2, $3, $4, $5, $6 }' "$itens" | sort -k1,1 -k2,2
} >"$lock"
n_lock=$(grep -cv '^#' "$lock" || true)
[ "$n_lock" -eq "$n_arquivos" ] || fail "lock com $n_lock linha(s), esperadas $n_arquivos (L-40)"

# ---------------------------------------------------------------------------
# 5. Os dois pacotes cachyos: achar o commit da receita pelo hash do PKGBUILD.
# ---------------------------------------------------------------------------
pkgb="$trab/pkgbuilds"
rm -rf "$pkgb"
git clone -q --filter=blob:none --no-checkout https://github.com/CachyOS/CachyOS-PKGBUILDS.git "$pkgb" \
    || fail "nao clonou CachyOS-PKGBUILDS (L-40)"

stage="$trab/stage"
rm -rf "$stage"
mkdir -p "$stage/binarios" "$stage/fonte" "$stage/licencas"
manifesto="$pub/manifesto.txt"
: >"$manifesto"
commits="$trab/commits.txt"
: >"$commits"

awk '$1 == "cachyos"' "$itens" | while read -r repo nome versao arq sha bsha pkgbase caminho; do
    # Pasta da receita no HEAD: <categoria>/<pkgbase>/PKGBUILD, exatamente uma.
    pk_path="$(git -C "$pkgb" ls-tree -r --name-only HEAD \
        | awk -v b="$pkgbase" '{ n = split($0, p, "/"); if (n >= 2 && p[n] == "PKGBUILD" && p[n - 1] == b) print }')"
    [ "$(printf '%s\n' "$pk_path" | grep -c .)" -eq 1 ] \
        || fail "receita de $pkgbase: esperava 1 PKGBUILD, achei $(printf '%s\n' "$pk_path" | grep -c .) (L-40)"
    dir="$(dirname "$pk_path")"

    # Commit cujo PKGBUILD tem o pkgbuild_sha256sum do binario (provado por hash, nao por versao).
    achado=""
    for c in $(git -C "$pkgb" log -n 500 --format=%H -- "$dir"); do
        h="$(git -C "$pkgb" show "$c:$dir/PKGBUILD" 2>/dev/null | sha256sum | awk '{ print $1 }')"
        if [ "$h" = "$bsha" ]; then
            achado="$c"
            break
        fi
    done
    [ -n "$achado" ] || fail "nenhum commit dos ultimos 500 de $dir tem o PKGBUILD de $nome (sha $bsha) (L-40)"

    # O .SRCINFO daquele commit tem de dar a mesma versao do binario (epoch:pkgver-pkgrel).
    srcinfo="$(git -C "$pkgb" show "$achado:$dir/.SRCINFO")" || fail "sem .SRCINFO em $dir@$achado (L-40)"
    s_ep="$(printf '%s\n' "$srcinfo" | sed -n 's/^[[:space:]]*epoch = //p' | head -1)"
    s_pv="$(printf '%s\n' "$srcinfo" | sed -n 's/^[[:space:]]*pkgver = //p' | head -1)"
    s_pr="$(printf '%s\n' "$srcinfo" | sed -n 's/^[[:space:]]*pkgrel = //p' | head -1)"
    esperado="$s_pv-$s_pr"
    [ -n "$s_ep" ] && esperado="$s_ep:$esperado"
    [ "$esperado" = "$versao" ] || fail "versao do .SRCINFO ($esperado) diverge do binario ($versao) em $dir@$achado (L-40)"

    # Extrai a pasta da receita daquele commit e confere as fontes.
    fonte_dir="$trab/fonte-$pkgbase"
    rm -rf "$fonte_dir"
    mkdir -p "$fonte_dir"
    git -C "$pkgb" archive "$achado" "$dir" | tar -x -C "$fonte_dir" \
        || fail "git archive falhou para $dir@$achado (L-40)"
    rec="$fonte_dir/$dir"
    [ -s "$rec/PKGBUILD" ] || fail "receita extraida sem PKGBUILD: $dir (L-40)"

    # Pares source/somas do pkgbase, arquivo por arquivo, sem excecao. A familia de
    # soma e' a que a receita declara, na ordem sha256sums, sha512sums, b2sums (o
    # python declara so' sha512sums). Qualquer familia e' a integridade da propria
    # receita; a conferencia usa a mesma familia que o portao.
    awk '
        /^[[:space:]]*pkgname = / { fim = 1 }
        fim == 0 && /^[[:space:]]*source = /  { sub(/^[[:space:]]*source = /, ""); S[++ns] = $0 }
        fim == 0 && /^[[:space:]]*sha256sums = / { sub(/^[[:space:]]*sha256sums = /, ""); H256[++n256] = $0 }
        fim == 0 && /^[[:space:]]*sha512sums = / { sub(/^[[:space:]]*sha512sums = /, ""); H512[++n512] = $0 }
        fim == 0 && /^[[:space:]]*b2sums = / { sub(/^[[:space:]]*b2sums = /, ""); HB2[++nb2] = $0 }
        /^[[:space:]]*source_[A-Za-z0-9_]+ = / { X = 1 }
        /^[[:space:]]*(sha256|sha512|b2)sums_[A-Za-z0-9_]+ = / { X = 1 }
        END {
            if (X) { print "X"; exit }
            if (ns == 0) { print "N0"; exit }
            if (n256 > 0) { fam = "sha256sums"; nh = n256; for (i = 1; i <= nh; i++) H[i] = H256[i] }
            else if (n512 > 0) { fam = "sha512sums"; nh = n512; for (i = 1; i <= nh; i++) H[i] = H512[i] }
            else if (nb2 > 0) { fam = "b2sums"; nh = nb2; for (i = 1; i <= nh; i++) H[i] = HB2[i] }
            else { print "N0"; exit }
            if (ns != nh) { print "N " ns " " nh; exit }
            print "F " fam
            for (i = 1; i <= ns; i++) print S[i] "\t" H[i]
        }
    ' "$rec/.SRCINFO" >"$trab/pares-$pkgbase.txt"
    cabecalho="$(head -1 "$trab/pares-$pkgbase.txt")"
    case "$cabecalho" in
        X) fail "$pkgbase: .SRCINFO com fonte ou soma por arquitetura, nao tratado (L-40)" ;;
        N0) fail "$pkgbase: .SRCINFO sem fonte, ou sem nenhuma soma (sha256sums, sha512sums ou b2sums) (L-40)" ;;
        "N "*) fail "$pkgbase: .SRCINFO com fonte e soma em numero diferente: $cabecalho (L-40)" ;;
        "F "*) familia="${cabecalho#F }" ;;
        *) fail "$pkgbase: saida inesperada do leitor de .SRCINFO (L-40)" ;;
    esac
    case "$familia" in
        sha256sums) cmdsoma=sha256sum ;;
        sha512sums) cmdsoma=sha512sum ;;
        b2sums) cmdsoma=b2sum ;;
        *) fail "$pkgbase: familia de soma desconhecida: $familia (L-40)" ;;
    esac
    sed '1d' "$trab/pares-$pkgbase.txt" >"$trab/pares2-$pkgbase.txt"
    npares=$(grep -c . "$trab/pares2-$pkgbase.txt")
    [ "$npares" -ge 1 ] || fail "$pkgbase: .SRCINFO sem nenhuma fonte (L-40)"
    grep -q 'SKIP' "$trab/pares2-$pkgbase.txt" && fail "$pkgbase: fonte com soma SKIP, nao ha prova de integridade (L-40)"

    tarballs="$trab/src-$pkgbase"
    mkdir -p "$tarballs"
    while IFS="$(printf '\t')" read -r entrada soma; do
        nome_arq="$entrada"
        url=""
        case "$entrada" in
            *"::"*) nome_arq="${entrada%%::*}"; url="${entrada#*::}" ;;
            *"://"*) url="$entrada"; nome_arq="${entrada##*/}" ;;
        esac
        if [ -n "$url" ]; then
            curl -fsSL --retry 5 --retry-delay 5 -o "$tarballs/$nome_arq" "$url" \
                || fail "fonte $nome_arq nao baixou de $url (L-40)"
        else
            [ -s "$rec/$nome_arq" ] || fail "fonte local ausente na receita: $nome_arq (L-40)"
            cp "$rec/$nome_arq" "$tarballs/$nome_arq"
        fi
        h="$($cmdsoma "$tarballs/$nome_arq" | awk '{ print $1 }')"
        [ "$h" = "$soma" ] || fail "$familia de $nome_arq diverge do .SRCINFO ($h != $soma) (L-40)"
    done <"$trab/pares2-$pkgbase.txt"
    echo "gerar_cachyos_repo: $pkgbase: $npares fonte(s) conferida(s) contra o .SRCINFO ($familia)"

    # Licencas: o que a fonte traz (licenses/, LICENSE*, COPYING*) e o que o binario instala.
    lic="$stage/licencas/$pkgbase"
    mkdir -p "$lic/fonte" "$lic/binario"
    for t in "$tarballs"/*; do
        case "$t" in
            *.tar|*.tar.*|*.tgz) ;;
            *) continue ;;
        esac
        xtmp="$trab/xtar-$pkgbase-$(basename "$t")"
        rm -rf "$xtmp"
        mkdir -p "$xtmp"
        bsdtar -xf "$t" -C "$xtmp" '*/licenses/*' '*/LICENSE*' '*/COPYING*' 2>/dev/null || true
        if [ -n "$(find "$xtmp" -type f | head -1)" ]; then
            mkdir -p "$lic/fonte/$(basename "$t")"
            cp -a "$xtmp/." "$lic/fonte/$(basename "$t")/"
        fi
    done
    bsdtar -xf "$caminho" -C "$lic/binario" 'usr/share/licenses/*' 2>/dev/null || true

    # Fonte exato para o artifact: a receita daquele commit, mais as fontes conferidas.
    fonte_out="$stage/fonte/$pkgbase"
    mkdir -p "$fonte_out/tarballs"
    cp -a "$rec/." "$fonte_out/"
    cp -a "$tarballs/." "$fonte_out/tarballs/"
    printf '%s\n' "$achado" >"$fonte_out/COMMIT"
    cp "$caminho" "$stage/binarios/$arq"
    cp "$caminho.sig" "$stage/binarios/$arq.sig"

    pb_sha="$(sha256sum "$rec/PKGBUILD" | awk '{ print $1 }')"
    lics="$(bsdtar -xOf "$caminho" .PKGINFO | sed -n 's/^license = //p' | paste -sd'|' -)"
    printf '%s %s %s %s %s %s %s\n' "$nome" "$pkgbase" "$versao" "$repo" "$sha" "$pb_sha" "${lics:-desconhecida}" >>"$manifesto"
    printf '%s %s %s\n' "$pkgbase" "$achado" "$dir" >>"$commits"
done

# ---------------------------------------------------------------------------
# 6. Arquivos de publicacao: tar unico (o ':' do epoch nao atravessa o
#    upload-artifact), e as folhas soltas na raiz.
# ---------------------------------------------------------------------------
n_manifesto=$(grep -c . "$manifesto" || true)
[ "$n_manifesto" -eq "$n_cachyos" ] || fail "manifesto com $n_manifesto linha(s), esperadas $n_cachyos (L-40)"

tar -C "$stage" -cf "$pub/cachyos-onda.tar" binarios fonte licencas \
    || fail "tar da publicacao falhou (L-40)"

versoes="$pub/cachyos-versoes.txt"
: >"$versoes"
for pacote in $lista; do
    linha="$(awk -v n="$pacote" '$2 == n { print $2, $3; exit }' "$origem")"
    [ -n "$linha" ] || fail "pacote $pacote da lista sem versao na origem (L-40)"
    echo "$linha" >>"$versoes"
done

{
    echo "# SOURCES.md - artifact cachyos-repo-onda (CI-PIN-PKG-C, D-WP1-0014)"
    echo
    echo "Este artifact traz dois pacotes do repositorio [cachyos] que o CI usa na onda: mesa e python."
    echo "Os demais pacotes da onda (core, extra e multilib) NAO estao aqui; sao baixados do Arch Linux Archive (archive.archlinux.org) pelo sha256 gravado em cachyos.lock, e nao sao redistribuidos por este artifact."
    echo
    echo "## Ressalvas de licenca (parecer do CLO, secao 7.4)"
    echo
    echo "> pode conter material GPL-2.0 (uapi) e um módulo ligado a GPL-3.0 (\`gdbm\`); o fonte correspondente está neste artifact"
    echo
    echo "- mesa: a etiqueta MIT AND BSD-3-Clause AND SGI-B-2.0 nao cobre tudo o que o fonte traz. A pasta licencas/mesa/fonte/ traz textos GPL-1.0-or-later e GPL-2.0-only, e cabecalhos uapi do kernel marcados com GPL."
    echo "- python: PSF-2.0 nao exige distribuir o fonte. O pacote depende de gdbm, GPL-3.0-or-later; o modulo _gdbm pode estar ligado a essa biblioteca. O fonte correspondente esta em fonte/python/."
    echo
    echo "## Conteudo"
    echo
    echo "- binarios/: os .pkg.tar.zst de mesa e python, com as assinaturas .sig."
    echo "- fonte/<pkgbase>/: a receita (PKGBUILD, .SRCINFO, patches) no commit exato, mais tarballs/ com as fontes conferidas por sha256 contra o .SRCINFO. O commit fica em fonte/<pkgbase>/COMMIT."
    echo "- licencas/<pkgbase>/fonte/ e licencas/<pkgbase>/binario/: textos de licenca tirados das fontes e do pacote."
    echo "- manifesto.txt: pkgname pkgbase versao origem sha256(binario) sha256(PKGBUILD) licenca."
    echo "- cachyos.lock: lock de toda a onda, com a origem e o sha256 de cada pacote."
    echo
    echo "## Como obter o fonte"
    echo
    echo "Extraia cachyos-onda.tar e leia fonte/<pkgbase>/COMMIT. Para reconstruir, use a receita nessa pasta com o commit indicado no repositorio CachyOS/CachyOS-PKGBUILDS."
    echo
    echo "## Retencao"
    echo
    echo "Este artifact tem retencao de 90 dias, e o fonte e o binario saem juntos. Nenhum passo do CI republica os pacotes do Arch."
    echo
    echo "## Origem (commit da receita)"
    echo
    echo "| pkgbase | commit | pasta |"
    echo "|---|---|---|"
    awk '{ print "| " $1 " | " $2 " | " $3 " |" }' "$commits"
} >"$pub/SOURCES.md"
rm -f "$commits"


# Portao final (L-40 e sabotagem): o que sobe tem de passar no mesmo portao
# que a perna usa para o artifact. Reprovacao aqui impede o upload.
python3 "$testes/check_cachyos_onda.py" --publicar "$pub" \
    || fail "portao de correspondencia reprovou o que ia subir (L-40)"

echo "gerar_cachyos_repo: publicacao pronta em $pub: $(ls "$pub" | tr '\n' ' ')"
