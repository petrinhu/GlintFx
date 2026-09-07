#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Portao (L-40): reprova se achar a senha do laboratorio, ou qualquer coisa
# que se pareca com ela, dentro de tools/win-vm-lab/. Nasce provado por
# sabotagem em copia, nunca aceito sem morder primeiro (ver
# check-sem-segredo.selftest.sh).
#
# Toda categoria varrida IMPRIME A CONTAGEM, mesmo quando da zero. Reprova
# (exit 1) se achar QUALQUER uma das tres coisas proibidas.
#
# Uso: check-sem-segredo.sh [<diretorio>]   (default: o proprio diretorio do script)

set -u
set -o pipefail

DIR="${1:-$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)}"

if [ ! -d "$DIR" ]; then
  echo "ERRO: diretorio nao existe: $DIR" >&2
  exit 2
fi

falhou=0

# --- 1. arquivo de senha, pelo NOME -----------------------------------------
n_arquivo=$(find "$DIR" -type f \( -iname '*senha*' -o -iname '*password*' -o -iname '*no-versionar*' -o -iname '*nao-versionar*' -o -iname '*secret*' -o -iname '*credential*' \) 2>/dev/null | wc -l)
echo "[1/3] arquivo cujo NOME sugere senha/segredo: encontrados=${n_arquivo}"
if [ "$n_arquivo" -gt 0 ]; then
  find "$DIR" -type f \( -iname '*senha*' -o -iname '*password*' -o -iname '*no-versionar*' -o -iname '*nao-versionar*' -o -iname '*secret*' -o -iname '*credential*' \) 2>/dev/null | sed 's/^/      -> /'
  falhou=1
fi

# --- 2. campo de senha do autounattend.xml com valor que NAO e o marcador --
# So os tres campos onde o Windows aceita <Value> de senha em texto puro.
n_campo=0
declare -a achados_campo=()
while IFS= read -r arquivo; do
  while IFS= read -r valor; do
    [ -z "$valor" ] && continue
    if [ "$valor" != "__ADMIN_PASSWORD__" ] && [ "$valor" != "__USER_PASSWORD__" ]; then
      n_campo=$((n_campo + 1))
      achados_campo+=("${arquivo}: campo de senha com valor '${valor}' (esperado um dos dois marcadores)")
    fi
  done < <(xmlstarlet sel -N u="urn:schemas-microsoft-com:unattend" -t \
    -m "//u:AdministratorPassword/u:Value | //u:LocalAccount/u:Password/u:Value | //u:AutoLogon/u:Password/u:Value" \
    -v "." -n "$arquivo" 2>/dev/null)
done < <(find "$DIR" -type f -iname '*.xml' 2>/dev/null)

echo "[2/3] campo de senha do autounattend.xml com valor que NAO e marcador: encontrados=${n_campo}"
if [ "$n_campo" -gt 0 ]; then
  printf '      -> %s\n' "${achados_campo[@]}"
  falhou=1
fi

# --- 3. valor com a FORMA de senha gerada (24 caracteres alfanumericos,   --
#        sem espaco, do jeito que 'openssl rand -base64 24 | tr -d "/+="'
#        produz) em qualquer <Value> de qualquer XML, mesmo fora dos tres
#        campos acima -- rede de seguranca contra copiar-colar em campo
#        errado.
n_forma=0
declare -a achados_forma=()
while IFS= read -r arquivo; do
  while IFS= read -r valor; do
    [ -z "$valor" ] && continue
    case "$valor" in
      __ADMIN_PASSWORD__|__USER_PASSWORD__) continue ;;
    esac
    if [[ "$valor" =~ ^[A-Za-z0-9]{24}$ ]]; then
      n_forma=$((n_forma + 1))
      achados_forma+=("${arquivo}: valor com a forma de senha gerada: '${valor}'")
    fi
  done < <(xmlstarlet sel -N u="urn:schemas-microsoft-com:unattend" -t -m "//u:Value" -v "." -n "$arquivo" 2>/dev/null)
done < <(find "$DIR" -type f -iname '*.xml' 2>/dev/null)

echo "[3/3] qualquer <Value> com a FORMA de senha gerada (24 alfanumericos): encontrados=${n_forma}"
if [ "$n_forma" -gt 0 ]; then
  printf '      -> %s\n' "${achados_forma[@]}"
  falhou=1
fi

if [ "$falhou" -ne 0 ]; then
  echo "=== REPROVADO: pelo menos uma categoria achou segredo acima. ==="
  exit 1
fi
echo "=== APROVADO: as tres categorias varreram e nenhuma achou segredo. ==="
exit 0
