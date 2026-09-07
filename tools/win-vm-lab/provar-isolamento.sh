#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Portao (L-40, L-45): prova por leitura da definicao libvirt que a VM Windows
# respeita as regras de isolamento do CLAUDE.md do GlintFx (secao da
# maquina virtual, L-09 GODS_LAWS.md). Le a definicao, nunca interage com a VM.
#
# Toda categoria varrida IMPRIME A CONTAGEM, mesmo quando da zero (L-40): zero
# achado tem de ser porque a definicao esta limpa, nunca porque o portao nao
# olhou. Reprova (exit 1) se achar QUALQUER uma das seis coisas proibidas.
#
# Sexta categoria acrescentada em 07/09/2026, ordem do team-lead ao aprovar o
# canal do agente QEMU: "ensine o portao sobre a categoria nova... canal para
# o convidado que nao seja o agente declarado". O canal virtio-serial para o
# qemu-guest-agent nao e nenhuma das cinco coisas ja previstas (nao e entrada
# do hospedeiro, nao e servidor de imagem, nao e pasta do usuario, nao e
# montagem de escrita, nao e area de transferencia) -- e categoria propria,
# e um portao que nao a conhece nao a vigia.
#
# Uso:
#   provar-isolamento.sh --dominio <nome-libvirt> [--allow-ro <caminho>]
#   provar-isolamento.sh --file <caminho-do-xml> [--allow-ro <caminho>]
#   provar-isolamento.sh --selftest
#
# --allow-ro define o UNICO caminho que pode aparecer como <filesystem>
# somente-leitura (default: a raiz deste projeto GlintFx). Qualquer outro
# <filesystem>, ou este mesmo caminho sem <readonly/>, reprova.

set -u
set -o pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
ALLOW_RO_DEFAULT="/home/petrus/IDrive/Documentos/projetos_claudebrain/Projects/GlintFx"
# qemu:///session, nunca qemu:///system: mesma decisao medida de
# criar-vm.sh (07/09/2026) -- o modo de sistema depende de polkit e pode
# reabrir dialogo grafico na sessao do lider; o de sessao nao pede
# autorizacao nenhuma. Configuravel por --connect so para depuracao.
CONNECT_URI_DEFAULT="qemu:///session"

uso() {
  echo "Uso: $0 --dominio <nome> [--connect <uri>] | --file <xml> [--allow-ro <caminho>] | --selftest" >&2
  exit 2
}

XML_SRC=""
MODO=""
ALLOW_RO="$ALLOW_RO_DEFAULT"
CONNECT_URI="$CONNECT_URI_DEFAULT"

while [ $# -gt 0 ]; do
  case "$1" in
    --dominio)
      MODO="dominio"
      XML_SRC="${2:-}"
      shift 2
      ;;
    --file)
      MODO="file"
      XML_SRC="${2:-}"
      shift 2
      ;;
    --allow-ro)
      ALLOW_RO="${2:-}"
      shift 2
      ;;
    --connect)
      CONNECT_URI="${2:-}"
      shift 2
      ;;
    --selftest)
      MODO="selftest"
      shift
      ;;
    *)
      uso
      ;;
  esac
done

[ -z "$MODO" ] && uso

# --- obtencao do XML a varrer, sempre para um arquivo local -----------------

obter_xml() {
  local modo="$1" src="$2" destino="$3"

  if [ "$modo" = "dominio" ]; then
    if [ -z "$src" ]; then
      echo "ERRO: --dominio exige o nome do dominio libvirt." >&2
      return 1
    fi
    local err_file
    err_file="$(mktemp /var/tmp/glintfx-win-lab-provar-err.XXXXXX)"
    virsh -c "$CONNECT_URI" dumpxml --inactive -- "$src" >"$destino" 2>"$err_file"
    local rc=$?
    if [ "$rc" -ne 0 ]; then
      echo "ERRO: 'virsh -c $CONNECT_URI dumpxml --inactive $src' falhou (codigo $rc). Saida:" >&2
      cat "$err_file" >&2 2>/dev/null
      rm -f "$err_file"
      return 1
    fi
    rm -f "$err_file"
    return 0
  fi

  if [ "$modo" = "file" ]; then
    if [ -z "$src" ] || [ ! -f "$src" ]; then
      echo "ERRO: --file exige caminho existente para um XML de dominio. Recebido: '$src'" >&2
      return 1
    fi
    cp -- "$src" "$destino"
    return 0
  fi

  echo "ERRO: modo interno desconhecido '$modo'." >&2
  return 1
}

# --- as cinco varreduras ------------------------------------------------------
# Cada funcao imprime "encontrados=N" (sempre, mesmo zero) e devolve 1 se N>0.

check_entrada_hospedeiro() {
  local xml="$1"
  local n_evdev n_usb total

  n_evdev=$(xmlstarlet sel -t -v "count(/domain/devices/input[@type='evdev'])" "$xml" 2>/dev/null)
  n_evdev=${n_evdev:-0}
  n_usb=$(xmlstarlet sel -t -v "count(/domain/devices/hostdev[@type='usb'])" "$xml" 2>/dev/null)
  n_usb=${n_usb:-0}
  total=$((n_evdev + n_usb))

  echo "[1/6] dispositivo de entrada/USB real do hospedeiro repassado: encontrados=${total} (evdev=${n_evdev}, hostdev-usb=${n_usb})"

  if [ "$total" -gt 0 ]; then
    if [ "$n_evdev" -gt 0 ]; then
      echo "      -> input evdev com <source dev>:"
      xmlstarlet sel -t -m "/domain/devices/input[@type='evdev']" -v "concat('         source dev=', source/@dev)" -n "$xml"
    fi
    if [ "$n_usb" -gt 0 ]; then
      echo "      -> hostdev usb:"
      xmlstarlet sel -t -m "/domain/devices/hostdev[@type='usb']" -v "concat('         vendor=', .//vendor/@id, ' product=', .//product/@id)" -n "$xml"
    fi
    return 1
  fi
  return 0
}

check_servidor_imagem() {
  local xml="$1"
  local n_sdl n_naolocal total

  n_sdl=$(xmlstarlet sel -t -v "count(/domain/devices/graphics[@type='sdl' or @type='desktop'])" "$xml" 2>/dev/null)
  n_sdl=${n_sdl:-0}

  n_naolocal=$(xmlstarlet sel -t -v \
    "count(/domain/devices/graphics[(@type='vnc' or @type='spice') and not(@listen='127.0.0.1' or @listen='localhost') ]) + count(/domain/devices/graphics/listen[not(@address='127.0.0.1' or @address='localhost') and @type='address'])" \
    "$xml" 2>/dev/null)
  n_naolocal=${n_naolocal:-0}
  total=$((n_sdl + n_naolocal))

  echo "[2/6] servidor de imagem (VNC/Spice) fora de 127.0.0.1, ou tipo que abre janela no hospedeiro (sdl/desktop): encontrados=${total} (sdl/desktop=${n_sdl}, listen-nao-local=${n_naolocal})"

  if [ "$total" -gt 0 ]; then
    echo "      -> elementos <graphics> da definicao:"
    xmlstarlet sel -t -m "/domain/devices/graphics" -v "concat('         type=', @type, ' listen-attr=', @listen, ' listen-filho=', listen/@address)" -n "$xml"
    return 1
  fi
  return 0
}

check_pasta_usuario() {
  local xml="$1"
  local n total
  local -a fontes=()

  while IFS= read -r linha; do
    [ -n "$linha" ] && fontes+=("$linha")
  done < <(xmlstarlet sel -t -m "/domain/devices/filesystem/source" -v "@dir" -n "$xml" 2>/dev/null)

  n=0
  local -a achados=()
  for f in "${fontes[@]:-}"; do
    [ -z "$f" ] && continue
    case "$f" in
      /home/*)
        # so a arvore do projeto GlintFx sob /home e permitida (checada tambem
        # na categoria seguinte); qualquer outra coisa sob /home e "pasta do
        # usuario" na acepcao da regra 4.
        if [ "$f" != "$ALLOW_RO" ] && [[ "$f" != "$ALLOW_RO"/* ]]; then
          n=$((n + 1))
          achados+=("$f")
        fi
        ;;
    esac
  done
  total="$n"

  echo "[3/6] pasta do usuario (fora da arvore permitida ${ALLOW_RO}) montada: encontrados=${total}"
  if [ "$total" -gt 0 ]; then
    printf '      -> %s\n' "${achados[@]}"
    return 1
  fi
  return 0
}

check_montagem_escrita_fora_projeto() {
  local xml="$1"
  local total=0
  local -a achados=()

  # todo <filesystem> cujo source NAO seja exatamente o ALLOW_RO com <readonly/>
  # presente conta como escrita (mapped/passthrough sem readonly), ou como
  # escrita para fora do projeto se o source nem for o ALLOW_RO.
  while IFS='|' read -r dir tem_ro; do
    [ -z "$dir" ] && continue
    if [ "$dir" != "$ALLOW_RO" ]; then
      total=$((total + 1))
      achados+=("${dir} (fora da arvore do projeto)")
    elif [ "$tem_ro" != "1" ]; then
      total=$((total + 1))
      achados+=("${dir} (dentro do projeto, mas SEM <readonly/>)")
    fi
  done < <(xmlstarlet sel -t -m "/domain/devices/filesystem" -v "concat(source/@dir, '|', count(readonly))" -n "$xml" 2>/dev/null)

  echo "[4/6] montagem de escrita para fora da arvore do projeto (ou dentro dela sem <readonly/>): encontrados=${total}"
  if [ "$total" -gt 0 ]; then
    printf '      -> %s\n' "${achados[@]}"
    return 1
  fi
  return 0
}

check_canal_area_transferencia() {
  local xml="$1"
  local n_spicevmc n_clipboard total

  n_spicevmc=$(xmlstarlet sel -t -v "count(/domain/devices/channel[@type='spicevmc'])" "$xml" 2>/dev/null)
  n_spicevmc=${n_spicevmc:-0}
  n_clipboard=$(xmlstarlet sel -t -v "count(/domain/devices/graphics/clipboard[@copypaste='yes'])" "$xml" 2>/dev/null)
  n_clipboard=${n_clipboard:-0}
  total=$((n_spicevmc + n_clipboard))

  echo "[5/6] canal de area de transferencia compartilhada (spicevmc / clipboard copypaste=yes): encontrados=${total} (spicevmc=${n_spicevmc}, clipboard-yes=${n_clipboard})"
  if [ "$total" -gt 0 ]; then
    return 1
  fi
  return 0
}

check_canal_nao_declarado() {
  local xml="$1"
  local total=0
  local -a achados=()

  # Todo <channel> cujo target/@name nao seja EXATAMENTE o do agente QEMU
  # declarado conta como canal nao autorizado -- categoria propria, nao se
  # confunde com a 5 (spicevmc/clipboard), que continua valendo por si.
  while IFS='|' read -r ch_type ch_name; do
    [ -z "$ch_type" ] && [ -z "$ch_name" ] && continue
    if [ "$ch_name" != "org.qemu.guest_agent.0" ]; then
      total=$((total + 1))
      achados+=("type=${ch_type} name=${ch_name:-<sem-nome>}")
    fi
  done < <(xmlstarlet sel -t -m "/domain/devices/channel" -v "concat(@type,'|',target/@name)" -n "$xml" 2>/dev/null)

  echo "[6/6] canal para o convidado que nao seja o agente declarado (org.qemu.guest_agent.0): encontrados=${total}"
  if [ "$total" -gt 0 ]; then
    printf '      -> %s\n' "${achados[@]}"
    return 1
  fi
  return 0
}

rodar_varredura() {
  local xml="$1"
  local falhou=0

  echo "=== Varredura de isolamento sobre: ${xml} ==="
  check_entrada_hospedeiro "$xml" || falhou=1
  check_servidor_imagem "$xml" || falhou=1
  check_pasta_usuario "$xml" || falhou=1
  check_montagem_escrita_fora_projeto "$xml" || falhou=1
  check_canal_area_transferencia "$xml" || falhou=1
  check_canal_nao_declarado "$xml" || falhou=1

  if [ "$falhou" -ne 0 ]; then
    echo "=== REPROVADO: pelo menos uma categoria achou violacao acima. ==="
    return 1
  fi
  echo "=== APROVADO: as seis categorias varreram e nenhuma achou violacao. ==="
  return 0
}

# --- selftest: prova que o portao morde antes de alguem confiar nele --------

selftest() {
  local rc_bad rc_clean

  echo ">>> SELFTEST 1/2: rodando contra a definicao SABOTADA (deve REPROVAR)"
  rodar_varredura "$SCRIPT_DIR/fixtures/dominio-sabotado.xml"
  rc_bad=$?
  echo ">>> codigo de saida contra a definicao sabotada: ${rc_bad}"
  echo

  echo ">>> SELFTEST 2/2: rodando contra a definicao LIMPA (deve APROVAR)"
  rodar_varredura "$SCRIPT_DIR/fixtures/dominio-limpo.xml"
  rc_clean=$?
  echo ">>> codigo de saida contra a definicao limpa: ${rc_clean}"
  echo

  if [ "$rc_bad" -eq 1 ] && [ "$rc_clean" -eq 0 ]; then
    echo "SELFTEST OK: o portao reprovou o sabotado (codigo ${rc_bad}) e aprovou o limpo (codigo ${rc_clean})."
    return 0
  fi

  echo "SELFTEST FALHOU: esperado sabotado=1 e limpo=0; obtido sabotado=${rc_bad} limpo=${rc_clean}."
  return 1
}

# --- despacho ------------------------------------------------------------------

if [ "$MODO" = "selftest" ]; then
  selftest
  exit $?
fi

TMP_XML="$(mktemp /var/tmp/glintfx-win-lab-provar.XXXXXX.xml)"
trap 'rm -f "$TMP_XML"' EXIT

if ! obter_xml "$MODO" "$XML_SRC" "$TMP_XML"; then
  exit 1
fi

rodar_varredura "$TMP_XML"
exit $?
