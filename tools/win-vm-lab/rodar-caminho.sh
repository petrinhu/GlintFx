#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Roda um binario Windows arbitrario, por CAMINHO completo, no convidado via
# guest-exec, espera terminar (poll ate 'exited:true' ou o PRAZO esgotar), e
# devolve no proprio CODIGO DE SAIDA DO SCRIPT o resultado - nao so no texto
# "EXITCODE=..." que ele imprime.
#
# ITEM WIN-RUNNER-PROPRIO, sub-fatia V-1 (GODS_LAWS.md L-35/L-36/L-45,
# 22/09/2026): o corpo anterior deste script tinha `set -u` mas NUNCA
# `set -e`, e o ultimo comando executado era sempre um `echo` - o codigo
# de saida do script era sempre 0, mesmo quando (a) o processo do
# convidado terminava com codigo diferente de zero, ou (b) o processo
# nunca terminava dentro do prazo (cravado em 20s no corpo antigo).
# Medido com dubles de `virsh`: `rodar-caminho.sh X && echo ok` imprimia
# "ok" nos dois casos de falha, um falso verde. Ver --selftest abaixo,
# que prova os quatro cenarios (sucesso, falha do convidado, estouro de
# prazo, falha ao iniciar) contra dubles de `virsh`, nunca contra a VM
# real - nenhuma linha deste corpo novo entrou sem ver o vermelho
# correspondente primeiro.
#
# Quatro fatos, quatro sinais distintos no codigo de saida - nunca um
# escondido atras do outro:
#   0   sucesso - o processo do convidado terminou e devolveu codigo 0
#   1   falha ao INICIAR o processo no convidado (guest-exec sem pid)
#   3   o processo do convidado TERMINOU, mas com codigo != 0
#   124 ESTOUROU o prazo sem o processo terminar (mesma convencao do
#       `timeout(1)` do GNU coreutils - 124 e' o codigo que essa
#       ferramenta amplamente conhecida ja usa para "prazo estourado",
#       reaproveitado aqui para nao inventar um vocabulario novo)
#   2   uso incorreto (argumento faltando)
#
# PRAZO por argumento, nunca mais cravado no corpo (item V-1): o
# segundo argumento posicional e' o numero de segundos de espera - cada
# segundo e' uma rodada de poll a guest-exec-status, ~1s de ida-e-volta
# medido pelo canal virtio-serial. Default 60s: generoso o bastante para
# o overhead de round-trip do canal sem pendurar quem chama por tempo
# indefinido; sobrescreva com um valor maior para binario lento, ou
# menor (como o --selftest faz) para manter a prova rapida.
#
# Uso:
#   rodar-caminho.sh <caminho-windows> [prazo-segundos]
#   rodar-caminho.sh --selftest
set -u
set -o pipefail

DOM="glintfx-win11-lab"
CONNECT="qemu:///session"
PRAZO_PADRAO=60

# GATE-SELFTEST-ORFAO / GODS_LAWS.md L-40: sem jq, os `jq -r` abaixo
# falham em silencio (comando ausente -> substituicao vazia) e as
# leituras saem vazias exatamente como se o convidado tivesse devolvido
# campo ausente - a mesma classe de defeito que o guard de xmlstarlet em
# provar-isolamento.sh (mesma pasta) ja fecha.
#
# MEDIDO, nao a mencao no arquivo (achado do team-lead, 22/09/2026: contar
# aparicao de "jq" no texto do workflow mede MENCAO, nao INSTALACAO - a
# mesma familia de "afirma medir e nao mede"): nenhuma linha de
# instalacao de pacote em .github/workflows/ci.yml (`dnf install`/
# `apt-get install`/`pacman -S`) cita jq, e a imagem REAL do alvo
# primario ja em cache local confirma a ausencia por execucao, nao por
# leitura de texto - `docker run --rm fedora:latest sh -c "command -v
# jq"` nao acha nada. NAO MEDIDO, dito com todas as letras: as imagens
# base de Ubuntu/CachyOS/Arch (nao estavam em cache local nesta maquina
# no momento da medicao, e baixa-las concorreria com o trabalho pesado
# ja em curso, GODS_LAWS.md L-11). O guard abaixo vale de qualquer jeito
# para as quatro plataformas - AUSENCIA DECLARADA (77) em qualquer uma
# onde jq realmente falte, nunca reprovacao silenciosa.
if ! command -v jq >/dev/null 2>&1; then
  echo "AUSENTE: jq nao encontrado no PATH - script pulado (declarado, nunca silencioso)." >&2
  exit 77
fi

# --- nucleo: roda um caminho, espera, e devolve o fato certo no $? ----------

rodar_e_esperar() {
  local caminho_win="$1" prazo="$2"
  local exec_res pid status_res exited exitcode out err i

  exec_res=$(virsh -c "$CONNECT" qemu-agent-command "$DOM" \
    "$(jq -n --arg p "$caminho_win" '{execute:"guest-exec",arguments:{path:$p,"capture-output":true}}')" 2>&1)
  pid=$(echo "$exec_res" | jq -r '.return.pid // empty')
  if [ -z "$pid" ]; then
    echo "ERRO_AO_INICIAR: $exec_res"
    return 1
  fi

  exited="false"
  for ((i = 1; i <= prazo; i++)); do
    sleep 1
    status_res=$(virsh -c "$CONNECT" qemu-agent-command "$DOM" \
      "$(jq -n --argjson p "$pid" '{execute:"guest-exec-status",arguments:{pid:$p}}')" 2>&1)
    exited=$(echo "$status_res" | jq -r '.return.exited // false')
    [ "$exited" = "true" ] && break
  done

  echo "CAMINHO=${caminho_win}"

  if [ "$exited" != "true" ]; then
    echo "EXITCODE=NAO-TERMINOU"
    echo "---"
    return 124
  fi

  exitcode=$(echo "$status_res" | jq -r '.return.exitcode // "DESCONHECIDO"')
  out=$(echo "$status_res" | jq -r '.return["out-data"] // empty' | base64 -d 2>/dev/null)
  err=$(echo "$status_res" | jq -r '.return["err-data"] // empty' | base64 -d 2>/dev/null)
  echo "EXITCODE=${exitcode}"
  echo "STDOUT:"
  echo "${out}"
  [ -n "$err" ] && { echo "STDERR:"; echo "${err}"; }
  echo "---"

  [ "$exitcode" = "0" ] && return 0
  return 3
}

# --- selftest: prova que os quatro cenarios devolvem os quatro sinais -------
# corretos, contra um dubl\u00ea de virsh, nunca contra a VM real.

selftest() {
  local stub_dir ok=1

  stub_dir="$(mktemp -d /var/tmp/glintfx-win-lab-rodar-selftest.XXXXXX)"
  trap 'rm -rf "$stub_dir"' RETURN

  cat >"$stub_dir/virsh" <<'STUB'
#!/usr/bin/env bash
set -u
JSON="${*: -1}"
EXECUTE=$(echo "$JSON" | jq -r '.execute')
CENARIO="${DUBLE_CENARIO:-sucesso}"
case "$EXECUTE" in
  guest-exec)
    if [ "$CENARIO" = "falha_ao_iniciar" ]; then
      echo '{"error":{"class":"GenericError","desc":"duble: falha proposital ao iniciar"}}'
    else
      echo '{"return":{"pid":4242}}'
    fi
    ;;
  guest-exec-status)
    case "$CENARIO" in
      sucesso) echo '{"return":{"exited":true,"exitcode":0,"out-data":"","err-data":""}}' ;;
      falha_convidado) echo '{"return":{"exited":true,"exitcode":1,"out-data":"","err-data":""}}' ;;
      estouro_prazo) echo '{"return":{"exited":false}}' ;;
      *) echo '{"return":{"exited":true,"exitcode":0,"out-data":"","err-data":""}}' ;;
    esac
    ;;
  *) echo '{"return":{}}' ;;
esac
STUB
  chmod +x "$stub_dir/virsh"

  echo ">>> SELFTEST 1/4: cenario sucesso (esperado: codigo 0)"
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=sucesso rodar_e_esperar 'C:\fake\bin.exe' 2
  local rc1=$?
  echo ">>> codigo obtido: ${rc1}"
  [ "$rc1" -eq 0 ] || ok=0

  echo ">>> SELFTEST 2/4: cenario falha_convidado (esperado: codigo 3)"
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=falha_convidado rodar_e_esperar 'C:\fake\bin.exe' 2
  local rc2=$?
  echo ">>> codigo obtido: ${rc2}"
  [ "$rc2" -eq 3 ] || ok=0

  echo ">>> SELFTEST 3/4: cenario estouro_prazo (esperado: codigo 124)"
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=estouro_prazo rodar_e_esperar 'C:\fake\bin.exe' 2
  local rc3=$?
  echo ">>> codigo obtido: ${rc3}"
  [ "$rc3" -eq 124 ] || ok=0

  echo ">>> SELFTEST 4/4: cenario falha_ao_iniciar (esperado: codigo 1)"
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=falha_ao_iniciar rodar_e_esperar 'C:\fake\bin.exe' 2
  local rc4=$?
  echo ">>> codigo obtido: ${rc4}"
  [ "$rc4" -eq 1 ] || ok=0

  if [ "$ok" -eq 1 ]; then
    echo "SELFTEST OK: os quatro cenarios devolveram o codigo de saida esperado (0/3/124/1)."
    return 0
  fi
  echo "SELFTEST FALHOU: pelo menos um cenario nao devolveu o codigo esperado."
  return 1
}

# --- despacho ----------------------------------------------------------------

if [ "${1:-}" = "--selftest" ]; then
  selftest
  exit $?
fi

if [ $# -lt 1 ]; then
  echo "Uso: $0 <caminho-windows> [prazo-segundos] | $0 --selftest" >&2
  exit 2
fi

rodar_e_esperar "$1" "${2:-$PRAZO_PADRAO}"
exit $?
