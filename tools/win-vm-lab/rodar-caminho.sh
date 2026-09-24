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
# que prova os seis cenarios (sucesso, falha do convidado, estouro de
# prazo, falha ao iniciar, canal nao respondeu, saida truncada) contra
# dubles de `virsh`, nunca contra a VM real - nenhuma linha deste corpo
# novo entrou sem ver o vermelho correspondente primeiro.
#
# ITEM WIN-RUNNER-PROPRIO, sub-fatia V-5a (D-7/D-8 em docs/plano-fecho-w7b.md,
# GODS_LAWS.md L-35/L-36/L-45, 23/09/2026): `TODO.md:149` registra o
# defeito raiz - a linha da INBOX `CANAL-QUEBRADO-SE-DISFARCA-DE-PRAZO-
# ESTOURADO` (achado do team-lead, 22/09/2026). Medido com um dublê de
# `virsh` que responde `error: Guest agent is not responding` a TODA
# consulta de `guest-exec-status` (o mesmo texto que o QEMU emite quando
# o agente convidado morreu ou nunca subiu): o corpo ANTERIOR deste
# script nao distinguia essa resposta invalida de um `exited:false`
# genuino - `jq` falhava em silencio, `exited` nunca virava `"true"`, o
# laco esgotava o PRAZO, e o script devolvia 124 ("estourou o prazo"). O
# diagnostico levava ao conserto ERRADO (aumentar o prazo) quando o fato
# real e' que o CANAL nunca respondeu nem uma vez - o processo pode ate
# ja ter terminado, ninguem sabe. Vermelho reproduzido ANTES deste
# conserto, contra o corpo antigo, nao editado: `rodar-caminho.sh
# 'C:\fake\bin.exe' 5` sob esse dublê saia com codigo 124. Segundo
# defeito, mesma familia "afirma que mede e nao mede": o agente
# convidado pode avisar `out-truncated`/`err-truncated` (a captura de
# stdout/stderr excedeu o buffer interno do QEMU GA) e o corpo antigo
# ignorava os dois campos - um dublê com `out-truncated:true` e
# `exitcode:0` saia com codigo 0 (sucesso), escondendo que o TEXTO
# capturado nao veio inteiro. Fonte: qemu-project.gitlab.io/qemu/interop/
# qemu-ga-ref.html (campos `out-truncated`/`err-truncated` de
# `guest-exec-status`).
#
# Seis fatos, seis sinais distintos no codigo de saida - nunca um
# escondido atras do outro:
#   0   sucesso - o processo do convidado terminou e devolveu codigo 0
#   1   falha ao INICIAR o processo no convidado (guest-exec sem pid)
#   3   o processo do convidado TERMINOU, mas com codigo != 0
#   5   SAIDA TRUNCADA (D-7): `out-truncated` ou `err-truncated` vieram
#       `true` na resposta de `guest-exec-status` - a captura de
#       stdout/stderr NAO veio inteira, e qualquer veredito tirado do
#       TEXTO capturado (nunca do exitcode, que continua valido) nao e
#       confiavel. Sai ANTES de avaliar o exitcode, porque a mensagem
#       que provaria sucesso ou falha pode estar cortada
#   6   CANAL NAO RESPONDEU (D-8): a consulta a `guest-exec-status`
#       falhou (virsh devolveu erro, ou JSON sem campo `.return`) -
#       distinto de 124, que exige que o canal tenha RESPONDIDO com
#       sucesso pelo menos uma vez. Sai cedo se `PISO_FALHAS_CONSULTA`
#       consultas SEGUIDAS falharem (nao declara canal quebrado por uma
#       unica falha isolada), e tambem no fim do prazo se NENHUMA
#       consulta, do inicio ao fim, obteve resposta valida
#   124 ESTOUROU o prazo, com o canal respondendo normalmente pelo menos
#       uma vez (mesma convencao do `timeout(1)` do GNU coreutils - 124
#       e' o codigo que essa ferramenta amplamente conhecida ja usa para
#       "prazo estourado", reaproveitado aqui para nao inventar um
#       vocabulario novo)
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

# Piso de tentativas (D-8): quantas consultas SEGUIDAS a guest-exec-status
# tem de falhar antes de declarar o canal quebrado (codigo 6) em vez de
# esperar o PRAZO inteiro. Existe para nao declarar canal quebrado por uma
# UNICA falha isolada (ruido de round-trip do virtio-serial) - so declara
# depois de ver o padrao se repetir.
PISO_FALHAS_CONSULTA=3

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
  local falhas_consulta=0 consulta_teve_sucesso="false" out_truncado err_truncado

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

    # D-8: uma consulta so' conta como "respondeu" se virou JSON valido
    # COM o campo `.return` (uma resposta de erro do virsh, ou um JSON
    # `{"error":...}` do proprio QEMU, nao tem esse campo). `jq -e`
    # sai != 0 tanto para JSON invalido quanto para `.return` ausente -
    # os dois casos que hoje se disfarçavam de "exited:false".
    if echo "$status_res" | jq -e '.return != null' >/dev/null 2>&1; then
      falhas_consulta=0
      consulta_teve_sucesso="true"
      exited=$(echo "$status_res" | jq -r '.return.exited // false')
      [ "$exited" = "true" ] && break
    else
      falhas_consulta=$((falhas_consulta + 1))
      if [ "$falhas_consulta" -ge "$PISO_FALHAS_CONSULTA" ]; then
        echo "CAMINHO=${caminho_win}"
        echo "EXITCODE=CANAL-NAO-RESPONDEU (${falhas_consulta} consultas seguidas ao agente falharam - piso de tentativas atingido, D-8. Ultima resposta: ${status_res})"
        echo "---"
        return 6
      fi
    fi
  done

  echo "CAMINHO=${caminho_win}"

  if [ "$exited" != "true" ]; then
    if [ "$consulta_teve_sucesso" != "true" ]; then
      # o prazo esgotou sem que UMA UNICA consulta obtivesse resposta
      # valida - o piso de tentativas seguidas nunca disparou porque o
      # prazo e' menor que ele, mas o fato e' o mesmo: canal quebrado,
      # nao "processo ainda rodando".
      echo "EXITCODE=CANAL-NAO-RESPONDEU (nenhuma consulta ao agente obteve resposta valida dentro do prazo de ${prazo}s. Ultima resposta: ${status_res})"
      echo "---"
      return 6
    fi
    echo "EXITCODE=NAO-TERMINOU"
    echo "---"
    return 124
  fi

  # D-7: truncamento se checa ANTES do exitcode - se a captura veio
  # cortada, o TEXTO nao e' confiavel mesmo que o exitcode em si seja.
  out_truncado=$(echo "$status_res" | jq -r '.return["out-truncated"] // false')
  err_truncado=$(echo "$status_res" | jq -r '.return["err-truncated"] // false')
  if [ "$out_truncado" = "true" ] || [ "$err_truncado" = "true" ]; then
    echo "EXITCODE=SAIDA-TRUNCADA (out-truncated=${out_truncado} err-truncated=${err_truncado}, D-7 - a captura de stdout/stderr do convidado nao veio inteira; nenhum veredito tirado do texto capturado e' confiavel)"
    echo "---"
    return 5
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

# --- selftest: prova que os seis cenarios devolvem os seis sinais ----------
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
      canal_quebrado)
        # mesmo texto que o QEMU devolve quando o agente convidado nao
        # responde (TODO.md:149) - nunca JSON valido, nunca campo `.return`.
        echo "error: Guest agent is not responding: QEMU guest agent is not connected" >&2
        exit 1
        ;;
      truncado) echo '{"return":{"exited":true,"exitcode":0,"out-data":"","err-data":"","out-truncated":true}}' ;;
      *) echo '{"return":{"exited":true,"exitcode":0,"out-data":"","err-data":""}}' ;;
    esac
    ;;
  *) echo '{"return":{}}' ;;
esac
STUB
  chmod +x "$stub_dir/virsh"

  echo ">>> SELFTEST 1/6: cenario sucesso (esperado: codigo 0)"
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=sucesso rodar_e_esperar 'C:\fake\bin.exe' 2
  local rc1=$?
  echo ">>> codigo obtido: ${rc1}"
  [ "$rc1" -eq 0 ] || ok=0

  echo ">>> SELFTEST 2/6: cenario falha_convidado (esperado: codigo 3)"
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=falha_convidado rodar_e_esperar 'C:\fake\bin.exe' 2
  local rc2=$?
  echo ">>> codigo obtido: ${rc2}"
  [ "$rc2" -eq 3 ] || ok=0

  echo ">>> SELFTEST 3/6: cenario estouro_prazo (esperado: codigo 124)"
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=estouro_prazo rodar_e_esperar 'C:\fake\bin.exe' 2
  local rc3=$?
  echo ">>> codigo obtido: ${rc3}"
  [ "$rc3" -eq 124 ] || ok=0

  echo ">>> SELFTEST 4/6: cenario falha_ao_iniciar (esperado: codigo 1)"
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=falha_ao_iniciar rodar_e_esperar 'C:\fake\bin.exe' 2
  local rc4=$?
  echo ">>> codigo obtido: ${rc4}"
  [ "$rc4" -eq 1 ] || ok=0

  echo ">>> SELFTEST 5/6: cenario canal_quebrado, D-8 (esperado: codigo 6, NUNCA 124 - prazo maior que o piso de tentativas para provar o corte precoce)"
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=canal_quebrado rodar_e_esperar 'C:\fake\bin.exe' 10
  local rc5=$?
  echo ">>> codigo obtido: ${rc5}"
  [ "$rc5" -eq 6 ] || ok=0

  echo ">>> SELFTEST 6/6: cenario truncado, D-7 (esperado: codigo 5, NUNCA 0 mesmo com exitcode=0)"
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=truncado rodar_e_esperar 'C:\fake\bin.exe' 2
  local rc6=$?
  echo ">>> codigo obtido: ${rc6}"
  [ "$rc6" -eq 5 ] || ok=0

  if [ "$ok" -eq 1 ]; then
    echo "SELFTEST OK: os seis cenarios devolveram o codigo de saida esperado (0/3/124/1/6/5)."
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
