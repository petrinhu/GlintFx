#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Roda UM binario pelo NOME (resolvido para C:\Users\glintfx\<nome>) no
# convidado via guest-exec, espera terminar (poll ate 'exited:true' ou o
# PRAZO esgotar), e imprime nome, exitcode, stdout e stderr - devolvendo
# no proprio CODIGO DE SAIDA DO SCRIPT o resultado, nao so no texto
# "EXITCODE=..." que ele imprime.
#
# ITEM WIN-RUNNER-PROPRIO, sub-fatia V-1 (GODS_LAWS.md L-35/L-36/L-45,
# 22/09/2026): o mesmo defeito de rodar-caminho.sh (mesma pasta), medido
# em separado contra este arquivo, nao suposto por simetria: `set -u`
# sem `set -e`, ultimo comando sempre um `echo`, codigo de saida do
# script sempre 0 mesmo com o convidado falhando ou nunca terminando.
# Ver --selftest abaixo, que prova os seis cenarios contra dubles de
# `virsh`, nunca contra a VM real - nenhuma linha deste corpo novo
# entrou sem ver o vermelho correspondente primeiro.
#
# ITEM WIN-RUNNER-PROPRIO, sub-fatia V-5a (D-7/D-8 em docs/plano-fecho-w7b.md,
# GODS_LAWS.md L-35/L-36/L-45, 23/09/2026): o mesmo defeito de
# rodar-caminho.sh (mesma pasta), medido em separado contra ESTE arquivo,
# nao suposto por simetria - `TODO.md:149` registra a lacuna
# (`CANAL-QUEBRADO-SE-DISFARCA-DE-PRAZO-ESTOURADO`). Vermelho reproduzido
# ANTES deste conserto, contra o corpo antigo, nao editado, com um dublê
# de `virsh` que responde `error: Guest agent is not responding` (o
# mesmo texto que o QEMU emite quando o agente convidado nao responde) a
# toda consulta de `guest-exec-status`: `rodar-um.sh 'bin.exe' 5` sob
# esse dublê saia com codigo 124 ("estourou o prazo"), quando o fato
# real e' que o CANAL nunca respondeu nem uma vez. Segundo vermelho,
# mesma familia: um dublê com `out-truncated:true` e `exitcode:0` saia
# com codigo 0 (sucesso), escondendo que a captura de stdout/stderr nao
# veio inteira. Fonte: qemu-project.gitlab.io/qemu/interop/qemu-ga-ref.html
# (campos `out-truncated`/`err-truncated` de `guest-exec-status`).
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
#       confiavel. Sai ANTES de avaliar o exitcode
#   6   CANAL NAO RESPONDEU (D-8): a consulta a `guest-exec-status`
#       falhou (virsh devolveu erro, ou JSON sem campo `.return`) -
#       distinto de 124, que exige que o canal tenha RESPONDIDO com
#       sucesso pelo menos uma vez. Sai cedo se `PISO_FALHAS_CONSULTA`
#       consultas SEGUIDAS falharem, e tambem no fim do prazo se NENHUMA
#       consulta obteve resposta valida
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
# medido pelo canal virtio-serial. Default 60s, mesma escolha e mesma
# justificativa de rodar-caminho.sh (mesma pasta): generoso o bastante
# para o overhead de round-trip do canal sem pendurar quem chama por
# tempo indefinido; sobrescreva com um valor maior para binario lento,
# ou menor (como o --selftest faz) para manter a prova rapida.
#
# Uso:
#   rodar-um.sh <nome-do-arquivo-em-C:\Users\glintfx\> [prazo-segundos]
#   rodar-um.sh --selftest
set -u
set -o pipefail

DOM="glintfx-win11-lab"
CONNECT="qemu:///session"
PRAZO_PADRAO=60

# Piso de tentativas (D-8), mesmo valor e mesma razao de rodar-caminho.sh
# (mesma pasta): quantas consultas SEGUIDAS a guest-exec-status tem de
# falhar antes de declarar o canal quebrado (codigo 6) em vez de esperar
# o PRAZO inteiro - nao declara por uma UNICA falha isolada.
PISO_FALHAS_CONSULTA=3

# GATE-SELFTEST-ORFAO / GODS_LAWS.md L-40: mesmo guard, mesma razao de
# rodar-caminho.sh (mesma pasta) - sem jq, os `jq -r` abaixo falham em
# silencio e a leitura sai vazia como se o convidado tivesse devolvido
# campo ausente.
#
# MEDIDO, nao a mencao no arquivo (mesmo achado do team-lead que
# corrigiu rodar-caminho.sh, 22/09/2026 - contar aparicao de "jq" no
# texto do workflow mede MENCAO, nao INSTALACAO): nenhuma linha de
# instalacao de pacote em .github/workflows/ci.yml (`dnf install`/
# `apt-get install`/`pacman -S`) cita jq, e a imagem REAL do alvo
# primario ja em cache local confirma por execucao, nao por leitura de
# texto - `docker run --rm fedora:latest sh -c "command -v jq"` nao
# acha nada. NAO MEDIDO: as imagens base de Ubuntu/CachyOS/Arch (fora
# de cache local nesta maquina). O guard abaixo vale de qualquer jeito
# para as quatro plataformas - AUSENCIA DECLARADA (77), nunca
# reprovacao silenciosa.
if ! command -v jq >/dev/null 2>&1; then
  echo "AUSENTE: jq nao encontrado no PATH - script pulado (declarado, nunca silencioso)." >&2
  exit 77
fi

# --- nucleo: roda um nome, espera, e devolve o fato certo no $? -------------

rodar_e_esperar() {
  local nome="$1" prazo="$2"
  local caminho_win exec_res pid status_res exited exitcode out err i
  local falhas_consulta=0 consulta_teve_sucesso="false" out_truncado err_truncado

  caminho_win="C:\\Users\\glintfx\\${nome}"

  exec_res=$(virsh -c "$CONNECT" qemu-agent-command "$DOM" \
    "$(jq -n --arg p "$caminho_win" '{execute:"guest-exec",arguments:{path:$p,"capture-output":true}}')" 2>&1)
  pid=$(echo "$exec_res" | jq -r '.return.pid // empty')
  if [ -z "$pid" ]; then
    echo "NOME=${nome}"
    echo "ERRO_AO_INICIAR: $exec_res"
    echo "---"
    return 1
  fi

  exited="false"
  for ((i = 1; i <= prazo; i++)); do
    sleep 1
    status_res=$(virsh -c "$CONNECT" qemu-agent-command "$DOM" \
      "$(jq -n --argjson p "$pid" '{execute:"guest-exec-status",arguments:{pid:$p}}')" 2>&1)

    # D-8: uma consulta so' conta como "respondeu" se virou JSON valido
    # COM o campo `.return` - o mesmo criterio de rodar-caminho.sh
    # (mesma pasta).
    if echo "$status_res" | jq -e '.return != null' >/dev/null 2>&1; then
      falhas_consulta=0
      consulta_teve_sucesso="true"
      exited=$(echo "$status_res" | jq -r '.return.exited // false')
      [ "$exited" = "true" ] && break
    else
      falhas_consulta=$((falhas_consulta + 1))
      if [ "$falhas_consulta" -ge "$PISO_FALHAS_CONSULTA" ]; then
        echo "NOME=${nome}"
        echo "EXITCODE=CANAL-NAO-RESPONDEU (${falhas_consulta} consultas seguidas ao agente falharam - piso de tentativas atingido, D-8. Ultima resposta: ${status_res})"
        echo "---"
        return 6
      fi
    fi
  done

  echo "NOME=${nome}"
  echo "EXITED=${exited}"

  if [ "$exited" != "true" ]; then
    if [ "$consulta_teve_sucesso" != "true" ]; then
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
  if [ -n "$err" ]; then
    echo "STDERR:"
    echo "${err}"
  fi
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
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=sucesso rodar_e_esperar 'bin.exe' 2
  local rc1=$?
  echo ">>> codigo obtido: ${rc1}"
  [ "$rc1" -eq 0 ] || ok=0

  echo ">>> SELFTEST 2/6: cenario falha_convidado (esperado: codigo 3)"
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=falha_convidado rodar_e_esperar 'bin.exe' 2
  local rc2=$?
  echo ">>> codigo obtido: ${rc2}"
  [ "$rc2" -eq 3 ] || ok=0

  echo ">>> SELFTEST 3/6: cenario estouro_prazo (esperado: codigo 124)"
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=estouro_prazo rodar_e_esperar 'bin.exe' 2
  local rc3=$?
  echo ">>> codigo obtido: ${rc3}"
  [ "$rc3" -eq 124 ] || ok=0

  echo ">>> SELFTEST 4/6: cenario falha_ao_iniciar (esperado: codigo 1)"
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=falha_ao_iniciar rodar_e_esperar 'bin.exe' 2
  local rc4=$?
  echo ">>> codigo obtido: ${rc4}"
  [ "$rc4" -eq 1 ] || ok=0

  echo ">>> SELFTEST 5/6: cenario canal_quebrado, D-8 (esperado: codigo 6, NUNCA 124 - prazo maior que o piso de tentativas para provar o corte precoce)"
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=canal_quebrado rodar_e_esperar 'bin.exe' 10
  local rc5=$?
  echo ">>> codigo obtido: ${rc5}"
  [ "$rc5" -eq 6 ] || ok=0

  echo ">>> SELFTEST 6/6: cenario truncado, D-7 (esperado: codigo 5, NUNCA 0 mesmo com exitcode=0)"
  PATH="$stub_dir:$PATH" DUBLE_CENARIO=truncado rodar_e_esperar 'bin.exe' 2
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
  echo "Uso: $0 <nome> [prazo-segundos] | $0 --selftest" >&2
  exit 2
fi

rodar_e_esperar "$1" "${2:-$PRAZO_PADRAO}"
exit $?
