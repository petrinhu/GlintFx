#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Motorista de sessao da VM Windows do laboratorio (item WIN-RUNNER-PROPRIO,
# sub-fatias V-2 e V-5b, GODS_LAWS.md deste projeto L-09/L-11, e
# L-35/L-36/L-40/L-45 do arquivo global). NAO liga a maquina de verdade
# nesta sub-fatia (isso e' V-5, sessao pesada a parte, com aval do lider)
# nem altera a definicao persistente do dominio (isso e' V-4, com aval do
# lider) -- este script cobre o que o plano em
# /var/tmp/glintfx-plan/win-runner-local.md secao 2 e 3 pede (V-2: trava de
# exclusao mutua, segunda verificacao independente por domstate,
# sobreposicao descartavel que nasce e morre dentro da MESMA posse da
# trava, desmontagem incondicional por trap) MAIS o que
# docs/plano-fecho-w7b.md secao 2.2.2 pede da V-5b: o MECANISMO de ligar
# pela copia avulsa (D-2/D-3/D-4), provado inteiro contra DUBLES de
# `virsh` -- nunca contra a maquina real. O primeiro arranque real e' a
# V-5, deliberadamente FORA do escopo deste script/selftest.
#
# DUAS VERIFICACOES INDEPENDENTES, E POR QUE NENHUMA SUBSTITUI A OUTRA:
#   1. flock sobre o arquivo de trava -- protege contra OUTRO sessao.sh,
#      porque so morde quem consulta o mesmo arquivo. E' convencao
#      cooperativa: um `virsh start` digitado a mao nao olha para ela.
#   2. `virsh domstate` -- protege contra QUALQUER OUTRA COISA que tenha
#      ligado a maquina, inclusive por fora deste script (a mao, por outra
#      ferramenta, por outra sessao que nao respeita a trava). E' a unica
#      das duas que enxerga o estado real do hypervisor.
# A trava sozinha nao prova que a maquina esta desligada; o domstate
# sozinho nao impede duas sessoes de tentarem a mesma hora. As duas juntas
# cobrem o que uma sozinha deixa passar (ver GODS_LAWS.md do projeto,
# secao 2.1 do plano).
#
# A SOBREPOSICAO NASCE E MORRE DENTRO DA MESMA POSSE DA TRAVA (plano, secao
# 3): por isso, antes de criar uma nova, este script recusa se achar uma
# sobreposicao ORFA no caminho de sempre -- sinal de que uma sessao
# anterior morreu sem passar pelo teardown (SIGKILL, que nenhum trap
# intercepta). Ausencia de sobreposicao nao prova sessao limpa por si so;
# e' a trava + o domstate + a ausencia de orfa, as tres, que autorizam
# prosseguir.
#
# V-5b -- LIGAR PELA COPIA AVULSA (docs/plano-fecho-w7b.md D-2/D-3/D-4):
# a decisao do lider de 22/09/2026 e' "copia temporaria da configuracao";
# a definicao PERMANENTE nunca muda (nem `define`, nem `edit`, nem
# `--config` -- so leitura via `dumpxml --inactive`). A copia troca
# EXATAMENTE TRES coisas em relacao ao permanente: a fonte do disco do
# sistema operacional (para a sobreposicao qcow2, ja provada em V-2), o
# `<nvram>` (para uma sobreposicao qcow2 IRMA, mesmo mecanismo
# `qemu-img create -b/-F`, porque o NVRAM do OVMF moderno tambem e' qcow2)
# e o `<source>` de estado do TPM emulado (para um DIRETORIO copiado por
# sessao -- o swtpm nao e' qcow2, entao aqui e' `cp -r`, nao overlay). As
# duas ultimas existem porque D-4 mediu que o "descartavel" da V-2 vazava
# por elas: nem NVRAM nem TPM estavam na sobreposicao, e a E3 do plano de
# 22/09 so conferia o disco. Uma copia com UMA QUARTA diferenca qualquer
# (rede religada, VNC exposto, ou qualquer outra coisa) e' tratada como
# SABOTADA e reprovada ANTES de `virsh create` ser chamado -- nunca
# depois. O portao (contar_diferencas_e_validar, mais
# provar-isolamento.sh --file na propria copia) e' quem decide se liga;
# `virsh create` nunca roda sem os dois aprovarem primeiro.
#
# Uso:
#   sessao.sh [--dom <nome>] [--connect <uri>] [--lock <caminho>]
#             [--base <qcow2>] [--overlay <qcow2>]
#             [--ligar [--nvram-overlay <qcow2>] [--tpm-state-dir <dir>]
#                      [--tpm-state-dir-permanente <dir>]
#                      [--prazo-ping <segundos>]]
#             [-- <comando...>]
#   sessao.sh --selftest
#
# Sem `-- <comando>`, a sessao prepara (trava, domstate, sobreposicao, e
# `--ligar` se pedido), imprime que esta pronta, e desfaz tudo em seguida
# (rodada de preparo/prova). Com `-- <comando>`, o comando roda DEPOIS do
# preparo (e do `--ligar`, se pedido) e ANTES da desmontagem -- e' o ponto
# onde a sub-fatia V-5 pluga transferir e executar o trabalho de verdade.
# `--ligar` SEM `--` no fim so prova o mecanismo (liga, espera guest-ping,
# desliga de novo) -- e' o proprio uso que a V-5 vai fazer, so que naquele
# dia contra a maquina real em vez de um dublê.
set -u
set -o pipefail

# GATE-SELFTEST-ORFAO / GODS_LAWS.md L-40: mesma guarda que
# provar-isolamento.sh (xmlstarlet) e rodar-caminho.sh (jq) ja usam nesta
# mesma pasta. Sem ela, `qemu-img`/`flock`/`xmlstarlet` ausentes fariam as
# chamadas falharem em silencio dentro das funcoes, e um --selftest que
# nunca executa a logica real ainda assim poderia imprimir "OK" por
# acidente de fluxo -- a mesma familia de "afirma que mede e nao mede".
# `xmlstarlet` entrou na guarda em V-5b (22/09/2026): a copia avulsa
# nasce de tres edicoes de XML, e sem a ferramenta elas falhariam caladas
# do mesmo jeito. `exit 77` e' o mesmo codigo de AUSENCIA DECLARADA que
# os vizinhos usam, lido pelo CTest como "Not Run" (SKIP_RETURN_CODE),
# nunca como falha nem como sucesso. NAO MEDIDO se as imagens de CI
# (nenhum workflow deste projeto instala estas ferramentas explicitamente)
# as tem — ausencia declarada, nao suposta.
for _ferramenta in qemu-img flock xmlstarlet; do
  if ! command -v "$_ferramenta" >/dev/null 2>&1; then
    echo "AUSENTE: ${_ferramenta} nao encontrado no PATH - script pulado (declarado, nunca silencioso)." >&2
    exit 77
  fi
done
unset _ferramenta

DOM_DEFAULT="glintfx-win11-lab"
CONNECT_DEFAULT="qemu:///session"
LAB_DIR_DEFAULT="/var/tmp/glintfx-win-lab"
LOCK_FILE_DEFAULT="${LAB_DIR_DEFAULT}/.vm.lock"
BASE_DISK_DEFAULT="${LAB_DIR_DEFAULT}/${DOM_DEFAULT}.qcow2"
OVERLAY_DEFAULT="${LAB_DIR_DEFAULT}/${DOM_DEFAULT}.sessao-overlay.qcow2"

# V-5b (docs/plano-fecho-w7b.md D-4): as duas cobertas novas, mesma pasta
# de laboratorio das outras. O default de TPM_STATE_DIR_PERMANENTE_DEFAULT
# e' o caminho que o proprio libvirt em modo `qemu:///session` usa por
# padrao QUANDO o dominio nao declara `<source>` proprio (fato medido em
# 22/09/2026 por `ls ~/.config/libvirt/qemu/swtpm/<uuid>/`, citado em
# docs/plano-fecho-w7b.md secao 1.1.7) -- computado do UUID lido da propria
# definicao permanente, nunca fixado a mao aqui.
NVRAM_OVERLAY_DEFAULT="${LAB_DIR_DEFAULT}/${DOM_DEFAULT}.sessao-nvram.qcow2"
TPM_STATE_DIR_DEFAULT="${LAB_DIR_DEFAULT}/${DOM_DEFAULT}.sessao-tpm-state"
TPM_STATE_DIR_PERMANENTE_BASE_DEFAULT="${HOME}/.config/libvirt/qemu/swtpm"
PRAZO_PING_DEFAULT=120

SCRIPT_PATH="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)/$(basename -- "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(dirname -- "$SCRIPT_PATH")"

uso() {
  echo "Uso: $0 [--dom <nome>] [--connect <uri>] [--lock <caminho>] [--base <qcow2>] [--overlay <qcow2>]" >&2
  echo "          [--ligar [--nvram-overlay <qcow2>] [--tpm-state-dir <dir>] [--tpm-state-dir-permanente <dir>] [--prazo-ping <segundos>]]" >&2
  echo "          [-- <comando...>] | --selftest" >&2
  exit 2
}

# --- 1. trava de exclusao mutua ---------------------------------------------
# Abre o arquivo de trava em modo ANEXAR (>>), nunca truncar (>): duas
# sessoes abrindo o MESMO arquivo com `>` uma depois da outra apagariam o
# conteudo escrito pela primeira antes mesmo de disputar o flock. O registro
# de posse (quem segura) vive num arquivo IRMAO (<lock>.info), escrito so
# depois que a trava foi tomada de verdade -- nunca antes.
tomar_trava() {
  local lock_file="$1" info_file="$2" identificador="$3"
  mkdir -p -- "$(dirname -- "$lock_file")" 2>/dev/null

  exec 9>>"$lock_file"
  if ! flock -n 9; then
    echo "RECUSADO: a trava '${lock_file}' ja esta em posse de outro processo." >&2
    if [ -s "$info_file" ]; then
      echo "quem segura, pelo ultimo registro:" >&2
      sed 's/^/      /' "$info_file" >&2
    else
      echo "(sem registro de posse legivel -- primeira disputa desta trava)" >&2
    fi
    exec 9>&- 2>/dev/null || true
    return 1
  fi

  local tmp
  tmp="$(mktemp "${info_file}.XXXXXX")"
  {
    printf 'pid=%s\n' "$$"
    printf 'data=%s\n' "$(date '+%d/%m/%y - %H:%M:%S')"
    printf 'identificador=%s\n' "$identificador"
  } >"$tmp"
  mv -f -- "$tmp" "$info_file"
  return 0
}

soltar_trava() {
  exec 9>&- 2>/dev/null || true
}

# --- 2. segunda verificacao independente: domstate ---------------------------
verificar_domstate_livre() {
  local dom="$1" connect="$2"
  local estado rc

  estado="$(virsh -c "$connect" domstate -- "$dom" 2>&1)"
  rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "ERRO: 'virsh -c ${connect} domstate ${dom}' falhou (codigo ${rc}). Saida:" >&2
    echo "$estado" >&2
    return 2
  fi

  echo "domstate lido: '${estado}'"
  if [ "$estado" != "desligado" ]; then
    echo "RECUSADO: a maquina '${dom}' nao esta desligada (domstate='${estado}')." >&2
    return 1
  fi
  return 0
}

# --- 3. sobreposicao descartavel ---------------------------------------------
detectar_sobreposicao_orfa() {
  local overlay="$1"
  if [ -e "$overlay" ]; then
    echo "RECUSADO: sobreposicao orfa encontrada em '${overlay}' -- uma sessao anterior nao passou pelo teardown (provavel SIGKILL)." >&2
    echo "Esta execucao ja segura a trava; o teardown dela remove a orfa antes de sair, entao a PROXIMA tentativa encontra o caminho limpo. Esta mesma tentativa recusa, e nao tenta reaproveitar um disco cujo conteudo nao e' confiavel." >&2
    return 1
  fi
  return 0
}

criar_sobreposicao() {
  local base="$1" overlay="$2" rc

  if [ ! -f "$base" ]; then
    echo "ERRO: disco base nao existe: ${base}" >&2
    return 1
  fi
  if [ -e "$overlay" ]; then
    echo "ERRO: sobreposicao ja existe, recuso sobrescrever: ${overlay}" >&2
    return 1
  fi

  qemu-img create -f qcow2 -b "$base" -F qcow2 -- "$overlay" >/dev/null
  rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "ERRO: 'qemu-img create' da sobreposicao falhou (codigo ${rc})." >&2
    return 1
  fi
  echo "sobreposicao criada: ${overlay} (base=${base})"
  return 0
}

remover_sobreposicao() {
  local overlay="$1"
  [ -z "$overlay" ] && return 0
  if [ -e "$overlay" ]; then
    rm -f -- "$overlay"
    echo "sobreposicao removida: ${overlay}"
  fi
  return 0
}

# --- 3b. copia avulsa da definicao (V-5b, D-2/D-3/D-4) -----------------------
# `criar_sobreposicao`/`remover_sobreposicao` acima ja sao genericas (dois
# parametros, base/overlay, sem nada hardcoded do disco da VM) -- V-5b as
# REUSA tal qual para a sobreposicao do NVRAM, porque o NVRAM do OVMF
# moderno tambem e' qcow2 (confirmado no XML real:
# `<nvram ... format='qcow2'>`, medido 22/09/2026). O TPM nao e' qcow2 (e'
# um DIRETORIO de estado do swtpm), por isso ganha o par proprio logo
# abaixo.

criar_copia_tpm_state() {
  local permanente_dir="$1" sessao_dir="$2"

  if [ ! -d "$permanente_dir" ]; then
    echo "ERRO: diretorio de estado do TPM permanente nao existe: ${permanente_dir}" >&2
    return 1
  fi
  if [ -e "$sessao_dir" ]; then
    echo "ERRO: copia de sessao do estado do TPM ja existe, recuso sobrescrever: ${sessao_dir}" >&2
    return 1
  fi

  cp -r -- "$permanente_dir" "$sessao_dir"
  local rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "ERRO: copia do diretorio de estado do TPM falhou (codigo ${rc})." >&2
    return 1
  fi
  echo "copia de sessao do estado do TPM criada: ${sessao_dir} (de ${permanente_dir})"
  return 0
}

remover_copia_tpm_state() {
  local sessao_dir="$1"
  [ -z "$sessao_dir" ] && return 0
  if [ -e "$sessao_dir" ]; then
    rm -rf -- "$sessao_dir"
    echo "copia de sessao do estado do TPM removida: ${sessao_dir}"
  fi
  return 0
}

# aplicar_tres_trocas: FONTE UNICA da transformacao permanente -> copia
# avulsa. Usada tanto para GERAR a copia real quanto, dentro de
# `contar_diferencas_e_validar`, para RECONSTRUIR o que a copia deveria
# ser a partir dos tres valores que ela de fato tem -- se as duas batem
# byte a byte, a copia so' tem essas tres diferencas, nenhuma outra
# (ver essa funcao abaixo para o porque disso provar "exatamente tres").
#
# NAO usar `--` antes do caminho do arquivo nas chamadas de `xmlstarlet
# ed`: medido em 22/09/2026 nesta sub-fatia que `xmlstarlet ed -L ... --
# arquivo.xml` morre com SIGSEGV (codigo de saida 139) nesta versao
# (compilada contra libxml2 2.12.10) -- `--` nao e' opcao reconhecida por
# `ed` e o parser subsequente segfaulta em vez de reportar erro (GODS_
# LAWS.md global L-49: 139 e' a ferramenta morrendo, nunca reprovacao
# legitima). `xmlstarlet sel` do resto deste arquivo ja nunca usou `--`;
# aqui fica documentado o motivo medido, nao só copiado por costume.
aplicar_tres_trocas() {
  local entrada="$1" saida="$2" disco_sessao="$3" nvram_sessao="$4" tpm_dir_sessao="$5"
  local rc

  cp -- "$entrada" "$saida"
  rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "ERRO: nao consegui copiar '${entrada}' para '${saida}' (codigo ${rc})." >&2
    return 1
  fi

  xmlstarlet ed -L -u "/domain/devices/disk[@device='disk']/source/@file" -v "$disco_sessao" "$saida"
  rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "ERRO: falha ao trocar a fonte do disco na copia (codigo ${rc})." >&2
    return 1
  fi

  xmlstarlet ed -L -u "/domain/os/nvram" -v "$nvram_sessao" "$saida"
  rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "ERRO: falha ao trocar o <nvram> na copia (codigo ${rc})." >&2
    return 1
  fi

  # <source> do TPM: a definicao permanente pode ou nao ja trazer um (o
  # dominio real medido em 22/09/2026 NAO traz -- libvirt usa o caminho
  # implicito por UUID); os dois casos tem de funcionar, e o segundo faz
  # este passo IDEMPOTENTE (rodar de novo sobre uma copia ja trocada so'
  # atualiza o path, nao duplica o elemento).
  local n_source
  n_source="$(xmlstarlet sel -t -v "count(/domain/devices/tpm/backend/source)" "$saida" 2>/dev/null)"
  n_source="${n_source:-0}"

  if [ "$n_source" = "0" ]; then
    xmlstarlet ed -L -s "/domain/devices/tpm/backend" -t elem -n source "$saida" &&
      xmlstarlet ed -L -s "/domain/devices/tpm/backend/source" -t attr -n type -v dir "$saida" &&
      xmlstarlet ed -L -s "/domain/devices/tpm/backend/source" -t attr -n path -v "$tpm_dir_sessao" "$saida"
    rc=$?
  else
    xmlstarlet ed -L -u "/domain/devices/tpm/backend/source/@type" -v dir "$saida" &&
      xmlstarlet ed -L -u "/domain/devices/tpm/backend/source/@path" -v "$tpm_dir_sessao" "$saida"
    rc=$?
  fi
  if [ "$rc" -ne 0 ]; then
    echo "ERRO: falha ao trocar o <source> de estado do TPM na copia (codigo ${rc})." >&2
    return 1
  fi

  return 0
}

# contar_diferencas_e_validar: o PORTAO que decide se `virsh create` pode
# rodar. Duas provas, nenhuma dispensa a outra:
#   1. os TRES pontos nomeados (disco, nvram, tpm-source) realmente
#      mudaram em relacao ao permanente -- se algum ficou igual, a copia
#      nao fez o que devia, e conta MENOS que tres.
#   2. reconstruir, a partir do permanente e dos TRES valores que a copia
#      de fato tem, o que a copia "deveria" ser (aplicar_tres_trocas de
#      novo, MESMA funcao), e comparar byte a byte (`cmp`) com a copia
#      real. Identico prova que a copia NAO tem nenhuma quarta diferenca
#      -- se tivesse (rede religada, VNC exposto, ou qualquer outra
#      coisa), o `cmp` reprovaria, porque a reconstrucao nunca teria essa
#      quarta mudanca.
# As duas juntas provam EXATAMENTE tres -- nem menos (prova 1), nem mais
# (prova 2). `virsh create` (em passo_ligar_vm) so' roda depois desta
# funcao E de provar-isolamento.sh --file aprovarem as duas.
contar_diferencas_e_validar() {
  local permanente="$1" copia="$2"
  local disco_perm disco_copia nvram_perm nvram_copia tpm_perm tpm_copia
  local n_mudou=0

  disco_perm="$(xmlstarlet sel -t -v "/domain/devices/disk[@device='disk']/source/@file" "$permanente" 2>/dev/null)"
  disco_copia="$(xmlstarlet sel -t -v "/domain/devices/disk[@device='disk']/source/@file" "$copia" 2>/dev/null)"
  nvram_perm="$(xmlstarlet sel -t -v "/domain/os/nvram" "$permanente" 2>/dev/null)"
  nvram_copia="$(xmlstarlet sel -t -v "/domain/os/nvram" "$copia" 2>/dev/null)"
  tpm_perm="$(xmlstarlet sel -t -v "/domain/devices/tpm/backend/source/@path" "$permanente" 2>/dev/null)"
  tpm_copia="$(xmlstarlet sel -t -v "/domain/devices/tpm/backend/source/@path" "$copia" 2>/dev/null)"

  if [ "$disco_perm" != "$disco_copia" ]; then
    n_mudou=$((n_mudou + 1))
    echo "  [1] fonte do disco: '${disco_perm}' -> '${disco_copia}'"
  fi
  if [ "$nvram_perm" != "$nvram_copia" ]; then
    n_mudou=$((n_mudou + 1))
    echo "  [2] nvram: '${nvram_perm}' -> '${nvram_copia}'"
  fi
  if [ "$tpm_perm" != "$tpm_copia" ]; then
    n_mudou=$((n_mudou + 1))
    echo "  [3] source do TPM: '${tpm_perm}' -> '${tpm_copia}'"
  fi

  echo "diferencas nomeadas encontradas: ${n_mudou} (esperado: 3)"
  if [ "$n_mudou" -ne 3 ]; then
    echo "REPROVADO: a copia nao troca exatamente os tres pontos esperados (achou ${n_mudou})." >&2
    return 1
  fi

  local esperado rc
  esperado="$(mktemp /var/tmp/glintfx-win-lab-v5b-esperado.XXXXXX.xml)"
  if ! aplicar_tres_trocas "$permanente" "$esperado" "$disco_copia" "$nvram_copia" "$tpm_copia"; then
    rm -f -- "$esperado"
    return 1
  fi

  if ! cmp -s "$esperado" "$copia"; then
    echo "REPROVADO: a copia tem pelo menos uma diferenca ALEM das tres nomeadas (quarta diferenca)." >&2
    echo "--- diff entre o esperado (permanente + as tres trocas) e a copia real ---" >&2
    diff -u "$esperado" "$copia" >&2
    rm -f -- "$esperado"
    return 1
  fi
  rm -f -- "$esperado"

  echo "APROVADO: exatamente tres diferencas nomeadas (disco, nvram, tpm-source), nenhuma outra."
  return 0
}

# esperar_guest_ping: prazo por TENTATIVA nao existe aqui de proposito --
# o que existe e' prazo TOTAL, medido por relogio de parede (`date +%s`),
# nao por contagem de tentativas (a mesma licao de C-1/CONT-WARMUP:
# contagem sem prazo deixa uma tentativa pendurada estourar o orcamento
# inteiro sozinha). Cada tentativa individual do `virsh
# qemu-agent-command` ja tem timeout proprio do libvirt; este laço so'
# decide quando PARAR de tentar.
esperar_guest_ping() {
  local dom="$1" connect="$2" prazo_segundos="$3"
  local inicio agora saida rc

  inicio="$(date +%s)"
  while :; do
    saida="$(virsh -c "$connect" qemu-agent-command -- "$dom" '{"execute":"guest-ping"}' 2>&1)"
    rc=$?
    if [ "$rc" -eq 0 ]; then
      echo "guest-ping respondeu: ${saida}"
      return 0
    fi

    agora="$(date +%s)"
    if [ $((agora - inicio)) -ge "$prazo_segundos" ]; then
      echo "ERRO: guest-ping nao respondeu dentro do prazo de ${prazo_segundos}s. Ultima saida: ${saida}" >&2
      return 1
    fi
    sleep 1
  done
}

# validar_e_ligar_maquina: o PORTAO propriamente dito, isolado do resto de
# passo_ligar_vm de proposito -- recebe permanente/copia JA PRONTOS (sem
# side effect nenhum de qemu-img/cp) para que o selftest possa alimentar
# uma copia SABOTADA direto aqui e contar quantas vezes o dublê de `virsh
# create` foi chamado, sem precisar montar disco/nvram/tpm de verdade
# para chegar ate este ponto. NUNCA chama `virsh create` sem os dois
# portoes (contar_diferencas_e_validar E provar-isolamento.sh --file)
# terem aprovado ANTES -- e' essa ordem, sozinha, que faz a estreia
# vermelha da V-5b provavel: uma copia sabotada nunca chega perto do
# `create`.
validar_e_ligar_maquina() {
  local dom="$1" connect="$2" permanente="$3" copia="$4"

  echo "--- validando a copia ANTES de ligar (portao 1/2: diferencas nomeadas) ---"
  if ! contar_diferencas_e_validar "$permanente" "$copia"; then
    echo "RECUSADO: 'virsh create' NUNCA sera chamado -- a copia nao passou no portao de diferencas." >&2
    return 1
  fi

  echo "--- validando a copia ANTES de ligar (portao 2/2: isolamento, provar-isolamento.sh --file) ---"
  if ! "${SCRIPT_DIR}/provar-isolamento.sh" --file "$copia"; then
    echo "RECUSADO: 'virsh create' NUNCA sera chamado -- a copia nao passou no portao de isolamento." >&2
    return 1
  fi

  echo "--- os dois portoes aprovaram: ligando pela copia avulsa (virsh create) ---"
  if ! virsh -c "$connect" create -- "$copia"; then
    echo "ERRO: 'virsh create' recusou a copia avulsa." >&2
    return 1
  fi
  TEARDOWN_MAQUINA_LIGADA=1
  return 0
}

# passo_ligar_vm: a orquestracao inteira do "liga pela copia avulsa" --
# gera as tres copias de verdade (qemu-img/cp), depois delega o portao +
# o `create` a validar_e_ligar_maquina acima, depois espera o guest-ping.
passo_ligar_vm() {
  local dom="$1" connect="$2" overlay="$3" nvram_sessao="$4" tpm_dir_sessao="$5"
  local tpm_dir_permanente="$6" prazo_ping="$7"
  local permanente copia

  permanente="$(mktemp /var/tmp/glintfx-win-lab-v5b-permanente.XXXXXX.xml)"
  copia="$(mktemp /var/tmp/glintfx-win-lab-v5b-copia.XXXXXX.xml)"
  TEARDOWN_PERMANENTE_TMP="$permanente"
  TEARDOWN_COPIA_TMP="$copia"

  if ! virsh -c "$connect" dumpxml --inactive -- "$dom" >"$permanente" 2>&1; then
    echo "ERRO: 'virsh -c ${connect} dumpxml --inactive ${dom}' falhou. Saida:" >&2
    cat "$permanente" >&2
    return 1
  fi

  # tpm_dir_permanente vazio = "computa do UUID" (default lazy: so' da'
  # para calcular depois de ler a definicao permanente, entao nao pode
  # ser um default fixo no parser de CLI).
  if [ -z "$tpm_dir_permanente" ]; then
    local uuid
    uuid="$(xmlstarlet sel -t -v "/domain/uuid" "$permanente" 2>/dev/null)"
    if [ -z "$uuid" ]; then
      echo "ERRO: nao consegui ler /domain/uuid da definicao permanente para computar o diretorio de estado do TPM." >&2
      return 1
    fi
    tpm_dir_permanente="${TPM_STATE_DIR_PERMANENTE_BASE_DEFAULT}/${uuid}"
    echo "diretorio de estado do TPM permanente computado do UUID (${uuid}): ${tpm_dir_permanente}"
  fi

  if ! criar_copia_tpm_state "$tpm_dir_permanente" "$tpm_dir_sessao"; then
    return 1
  fi
  TEARDOWN_TPM_STATE_DIR="$tpm_dir_sessao"

  echo "--- criando sobreposicao de NVRAM por sessao (mesmo mecanismo do disco) ---"
  if ! criar_sobreposicao "$(xmlstarlet sel -t -v "/domain/os/nvram" "$permanente" 2>/dev/null)" "$nvram_sessao"; then
    return 1
  fi
  TEARDOWN_NVRAM_OVERLAY="$nvram_sessao"

  echo "--- gerando a copia avulsa (D-2/D-3/D-4): fonte do disco, nvram e source do TPM ---"
  if ! aplicar_tres_trocas "$permanente" "$copia" "$overlay" "$nvram_sessao" "$tpm_dir_sessao"; then
    return 1
  fi

  if ! validar_e_ligar_maquina "$dom" "$connect" "$permanente" "$copia"; then
    return 1
  fi

  echo "--- maquina ligada; esperando guest-ping (prazo ${prazo_ping}s) ---"
  if ! esperar_guest_ping "$dom" "$connect" "$prazo_ping"; then
    return 1
  fi

  echo "MAQUINA LIGADA E RESPONDENDO pela copia avulsa."
  return 0
}

# --- 4. desmontagem incondicional (trap) -------------------------------------
LOCK_TAKEN=0
TEARDOWN_OVERLAY=""
TEARDOWN_NVRAM_OVERLAY=""
TEARDOWN_TPM_STATE_DIR=""
TEARDOWN_PERMANENTE_TMP=""
TEARDOWN_COPIA_TMP=""
TEARDOWN_MAQUINA_LIGADA=0
TEARDOWN_DOM=""
TEARDOWN_CONNECT=""

teardown() {
  local rc=$?

  if [ "$TEARDOWN_MAQUINA_LIGADA" -eq 1 ] && [ -n "$TEARDOWN_DOM" ]; then
    echo "teardown: desligando a maquina ligada pela copia avulsa (virsh destroy)..." >&2
    virsh -c "$TEARDOWN_CONNECT" destroy -- "$TEARDOWN_DOM" >&2 2>&1
    local estado_final
    estado_final="$(virsh -c "$TEARDOWN_CONNECT" domstate -- "$TEARDOWN_DOM" 2>&1)"
    echo "teardown: domstate apos destroy: '${estado_final}'" >&2
    if [ "$estado_final" != "desligado" ]; then
      echo "teardown: ATENCAO -- domstate esperado 'desligado', obtido '${estado_final}'." >&2
    fi
    TEARDOWN_MAQUINA_LIGADA=0
  fi

  remover_sobreposicao "$TEARDOWN_OVERLAY"
  remover_sobreposicao "$TEARDOWN_NVRAM_OVERLAY"
  remover_copia_tpm_state "$TEARDOWN_TPM_STATE_DIR"
  if [ -n "$TEARDOWN_PERMANENTE_TMP" ]; then
    rm -f -- "$TEARDOWN_PERMANENTE_TMP" 2>/dev/null
  fi
  if [ -n "$TEARDOWN_COPIA_TMP" ]; then
    rm -f -- "$TEARDOWN_COPIA_TMP" 2>/dev/null
  fi

  if [ "$LOCK_TAKEN" -eq 1 ]; then
    soltar_trava
    LOCK_TAKEN=0
  fi
  return "$rc"
}

sinal_recebido() {
  echo "sinal recebido, encerrando a sessao..." >&2
  exit 143
}

# --- orquestracao -------------------------------------------------------------
# Os cinco parametros de `--ligar` vem DEPOIS dos cinco de sempre e ANTES
# do comando (`-- <comando...>`), na mesma convencao posicional que o
# resto do script ja usa. `ligar=0` preserva o comportamento da V-2 byte
# a byte -- nenhum caminho novo roda sem o chamador pedir `--ligar`
# explicitamente (fronteira dura da V-5b: nao ligar a maquina por
# default nem por acidente).
rodar_sessao() {
  local dom="$1" connect="$2" lock_file="$3" base="$4" overlay="$5"
  local ligar="$6" nvram_overlay="$7" tpm_state_dir="$8" tpm_state_dir_permanente="$9" prazo_ping="${10}"
  shift 10
  local info_file="${lock_file}.info"

  TEARDOWN_OVERLAY="$overlay"
  TEARDOWN_DOM="$dom"
  TEARDOWN_CONNECT="$connect"
  trap teardown EXIT
  trap sinal_recebido TERM INT HUP

  if ! tomar_trava "$lock_file" "$info_file" "pid-$$"; then
    return 1
  fi
  LOCK_TAKEN=1

  if ! verificar_domstate_livre "$dom" "$connect"; then
    return 1
  fi

  if ! detectar_sobreposicao_orfa "$overlay"; then
    return 1
  fi

  if ! criar_sobreposicao "$base" "$overlay"; then
    return 1
  fi

  echo "SESSAO PRONTA: trava tomada, maquina confirmada desligada, sobreposicao descartavel criada."

  if [ "$ligar" -eq 1 ]; then
    if ! passo_ligar_vm "$dom" "$connect" "$overlay" "$nvram_overlay" "$tpm_state_dir" "$tpm_state_dir_permanente" "$prazo_ping"; then
      return 1
    fi
  fi

  if [ $# -gt 0 ]; then
    echo "rodando comando sob a sessao: $*"
    "$@"
    return $?
  fi
  echo "nenhum comando passado -- rodada de preparo/prova; desfazendo em seguida."
  return 0
}

# --- selftest ------------------------------------------------------------------

selftest_e1() {
  local work_dir="$1" lock_file info_file rc2 holder_pid

  lock_file="${work_dir}/e1.vm.lock"
  info_file="${lock_file}.info"

  echo ">>> E1 1/2: primeiro processo toma a trava e a mantem aberta"
  (
    exec 9>>"$lock_file"
    flock -x 9
    {
      printf 'pid=%s\n' "$$"
      printf 'data=%s\n' "$(date '+%d/%m/%y - %H:%M:%S')"
      printf 'identificador=%s\n' "processo-A-do-teste"
    } >"$info_file"
    sleep 5
  ) &
  holder_pid=$!
  sleep 1

  echo ">>> E1 2/2: segundo processo tenta tomar a MESMA trava (esperado: recusa)"
  tomar_trava "$lock_file" "$info_file" "processo-B-do-teste"
  rc2=$?
  echo ">>> codigo obtido do segundo processo: ${rc2}"

  kill "$holder_pid" 2>/dev/null
  wait "$holder_pid" 2>/dev/null

  if [ "$rc2" -ne 0 ]; then
    echo "E1 OK: a segunda tentativa foi recusada (codigo ${rc2}) enquanto a primeira segurava a trava."
    return 0
  fi
  echo "E1 FALHOU: a segunda tentativa deveria ter sido recusada e nao foi (codigo ${rc2})."
  return 1
}

selftest_e2() {
  local work_dir="$1" stub_dir saida rc

  stub_dir="${work_dir}/e2-bin"
  mkdir -p "$stub_dir"
  cat >"${stub_dir}/virsh" <<'STUB'
#!/usr/bin/env bash
# duble: sempre diz que o dominio esta executando, mesmo com a trava livre
echo "executando"
exit 0
STUB
  chmod +x "${stub_dir}/virsh"

  echo ">>> E2: domstate simulado 'executando' com a trava LIVRE (esperado: recusa mesmo assim)"
  saida="$(PATH="${stub_dir}:$PATH" verificar_domstate_livre "duble-dom" "qemu:///session" 2>&1)"
  rc=$?
  echo "$saida"
  echo ">>> codigo obtido: ${rc}"

  if [ "$rc" -ne 0 ]; then
    echo "E2 OK: recusado com a maquina simulada ligada, mesmo a trava estando livre."
    return 0
  fi
  echo "E2 FALHOU: deveria ter recusado e nao recusou (codigo ${rc})."
  return 1
}

selftest_e7() {
  local work_dir="$1" base overlay lock_file stub_dir pid rc

  base="${work_dir}/e7-base.qcow2"
  overlay="${work_dir}/e7-overlay.qcow2"
  lock_file="${work_dir}/e7.vm.lock"
  stub_dir="${work_dir}/e7-bin"
  mkdir -p "$stub_dir"
  cat >"${stub_dir}/virsh" <<'STUB'
#!/usr/bin/env bash
echo "desligado"
exit 0
STUB
  chmod +x "${stub_dir}/virsh"
  qemu-img create -f qcow2 -- "$base" 10M >/dev/null

  echo ">>> E7a 1/3: sobe a sessao em segundo plano, com 'sleep 10' como comando"
  (
    PATH="${stub_dir}:$PATH" "$SCRIPT_PATH" --dom duble-dom --connect qemu:///session \
      --lock "$lock_file" --base "$base" --overlay "$overlay" -- sleep 10
  ) &
  pid=$!
  sleep 2

  echo ">>> E7a 2/3: sobreposicao deveria existir agora (sessao em andamento):"
  ls -la "$overlay" 2>&1

  echo ">>> E7a 3/3: mando SIGTERM no meio da sessao"
  kill -TERM "$pid" 2>/dev/null
  wait "$pid" 2>/dev/null
  rc=$?
  echo ">>> codigo de saida do motorista apos SIGTERM: ${rc}"

  local e7a_ok=1
  if [ -e "$overlay" ]; then
    echo "E7a FALHOU: sobreposicao ainda existe depois do SIGTERM (${overlay})."
    e7a_ok=0
  else
    echo "E7a: sobreposicao removida apos SIGTERM -- OK."
  fi
  if PATH="${stub_dir}:$PATH" bash -c "exec 9>>'$lock_file'; flock -n 9"; then
    echo "E7a: trava livre depois do SIGTERM -- OK."
  else
    echo "E7a FALHOU: trava continua presa depois do SIGTERM."
    e7a_ok=0
  fi

  echo
  echo ">>> E7b (espelho SIGKILL) 1/3: sobe outra sessao e mata com SIGKILL (trap NAO roda)"
  (
    PATH="${stub_dir}:$PATH" "$SCRIPT_PATH" --dom duble-dom --connect qemu:///session \
      --lock "$lock_file" --base "$base" --overlay "$overlay" -- sleep 10
  ) &
  pid=$!
  sleep 2
  echo ">>> E7b 2/3: sobreposicao antes do SIGKILL:"
  ls -la "$overlay" 2>&1
  kill -KILL "$pid" 2>/dev/null
  wait "$pid" 2>/dev/null

  local e7b_ok=1
  if [ ! -e "$overlay" ]; then
    echo "E7b FALHOU: a sobreposicao sumiu sozinha -- o cenario nao provou nada (precisa sobrar orfa apos SIGKILL)."
    e7b_ok=0
  else
    echo "E7b: sobreposicao ORFA confirmada em disco apos SIGKILL (esperado -- nenhum trap intercepta SIGKILL)."
  fi

  echo ">>> E7b 3/3: proxima execucao tem de RECUSAR por achar a sobreposicao orfa"
  PATH="${stub_dir}:$PATH" "$SCRIPT_PATH" --dom duble-dom --connect qemu:///session \
    --lock "$lock_file" --base "$base" --overlay "$overlay" -- true
  rc=$?
  echo ">>> codigo da execucao seguinte: ${rc}"
  if [ "$rc" -ne 0 ]; then
    echo "E7b: a execucao seguinte recusou por causa da sobreposicao orfa -- OK (estado sujo detectado, nunca escondido)."
  else
    echo "E7b FALHOU: a execucao seguinte deveria ter recusado e nao recusou."
    e7b_ok=0
  fi
  rm -f -- "$overlay"

  if [ "$e7a_ok" -eq 1 ] && [ "$e7b_ok" -eq 1 ]; then
    return 0
  fi
  return 1
}

# --- selftest V-5b (docs/plano-fecho-w7b.md secao 2.2.2) ---------------------
# As quatro provas abaixo cobrem o mecanismo inteiro da copia avulsa
# CONTRA DUBLES, nunca contra a maquina real (fronteira dura da V-5b: o
# primeiro arranque de verdade e' a V-5, sessao pesada a parte). Usam
# fixtures/dominio-limpo.xml (o mesmo fixture que provar-isolamento.sh ja
# usa no proprio --selftest) como a definicao "permanente" de partida,
# para nao inventar um segundo fixture quase igual.

# V5B-DIFF: estreia vermelha do PORTAO 1 (contar_diferencas_e_validar).
# Uma copia legitima tem de aprovar; qualquer sabotagem (listen exposto,
# enlace religado, ou UMA diferenca a mais qualquer) ou troca incompleta
# (so' duas das tres) tem de reprovar.
selftest_v5b_diff_exato() {
  local work_dir="$1"
  local permanente copia rc_ok rc_listen rc_link rc_extra rc_duas

  permanente="${work_dir}/v5b-permanente.xml"
  cp -- "${SCRIPT_DIR}/fixtures/dominio-limpo.xml" "$permanente"

  echo ">>> V5B-DIFF 1/5: copia legitima (as tres trocas, nada mais) -- esperado APROVAR"
  copia="${work_dir}/v5b-copia-legitima.xml"
  aplicar_tres_trocas "$permanente" "$copia" \
    "${work_dir}/toy-disco.qcow2" "${work_dir}/toy-nvram.qcow2" "${work_dir}/toy-tpm-state" >/dev/null
  contar_diferencas_e_validar "$permanente" "$copia" >/dev/null
  rc_ok=$?
  echo ">>> codigo obtido (esperado 0): ${rc_ok}"

  echo ">>> V5B-DIFF 2/5: quarta diferenca -- graphics listen='0.0.0.0' -- esperado REPROVAR"
  local sabotada_listen="${work_dir}/v5b-sabotada-listen.xml"
  cp -- "$copia" "$sabotada_listen"
  xmlstarlet ed -L -u "/domain/devices/graphics/@listen" -v "0.0.0.0" "$sabotada_listen"
  contar_diferencas_e_validar "$permanente" "$sabotada_listen" >/dev/null
  rc_listen=$?
  echo ">>> codigo obtido (esperado != 0): ${rc_listen}"

  echo ">>> V5B-DIFF 3/5: quarta diferenca -- interface link state='up' -- esperado REPROVAR"
  local sabotada_link="${work_dir}/v5b-sabotada-link.xml"
  cp -- "$copia" "$sabotada_link"
  xmlstarlet ed -L -u "/domain/devices/interface/link/@state" -v "up" "$sabotada_link"
  contar_diferencas_e_validar "$permanente" "$sabotada_link" >/dev/null
  rc_link=$?
  echo ">>> codigo obtido (esperado != 0): ${rc_link}"

  echo ">>> V5B-DIFF 4/5: quarta diferenca arbitraria qualquer -- vcpu -- esperado REPROVAR"
  local sabotada_extra="${work_dir}/v5b-sabotada-vcpu.xml"
  cp -- "$copia" "$sabotada_extra"
  xmlstarlet ed -L -u "/domain/vcpu" -v "8" "$sabotada_extra"
  contar_diferencas_e_validar "$permanente" "$sabotada_extra" >/dev/null
  rc_extra=$?
  echo ">>> codigo obtido (esperado != 0): ${rc_extra}"

  echo ">>> V5B-DIFF 5/5: so' DUAS das tres trocas (tpm intocado) -- esperado REPROVAR"
  local so_duas="${work_dir}/v5b-so-duas.xml"
  cp -- "$permanente" "$so_duas"
  xmlstarlet ed -L -u "/domain/devices/disk[@device='disk']/source/@file" -v "${work_dir}/toy-disco.qcow2" "$so_duas"
  xmlstarlet ed -L -u "/domain/os/nvram" -v "${work_dir}/toy-nvram.qcow2" "$so_duas"
  contar_diferencas_e_validar "$permanente" "$so_duas" >/dev/null
  rc_duas=$?
  echo ">>> codigo obtido (esperado != 0): ${rc_duas}"

  if [ "$rc_ok" -eq 0 ] && [ "$rc_listen" -ne 0 ] && [ "$rc_link" -ne 0 ] && [ "$rc_extra" -ne 0 ] && [ "$rc_duas" -ne 0 ]; then
    echo "V5B-DIFF OK: aprovou a copia legitima e reprovou as quatro variantes com diferenca errada."
    return 0
  fi
  echo "V5B-DIFF FALHOU: pelo menos um dos cinco casos nao se comportou como esperado."
  return 1
}

# V5B-CREATE: estreia vermelha do PORTAO ANTES DO `virsh create`. Prova,
# CONTANDO as chamadas de um dublê, que uma copia sabotada nunca chega
# perto do `create` (zero chamadas) e que uma copia legitima liga
# (exatamente uma chamada).
selftest_v5b_portao_antes_do_create() {
  local work_dir="$1"
  local permanente copia_legitima copia_sabotada stub_dir contador rc rc2
  local chamadas_sabotada chamadas_legitima

  permanente="${work_dir}/v5b2-permanente.xml"
  cp -- "${SCRIPT_DIR}/fixtures/dominio-limpo.xml" "$permanente"

  copia_legitima="${work_dir}/v5b2-copia-legitima.xml"
  aplicar_tres_trocas "$permanente" "$copia_legitima" \
    "${work_dir}/toy-disco-b.qcow2" "${work_dir}/toy-nvram-b.qcow2" "${work_dir}/toy-tpm-state-b" >/dev/null

  # Sabotagem por VCPU (nao por listen/link): provar-isolamento.sh --file
  # NAO varre contagem de vcpu -- so' o portao 1 (contar_diferencas_e_
  # validar) enxerga esta quarta diferenca. Escolhido de proposito para
  # a estreia vermelha aqui provar o portao 1 ISOLADO (V5B-DIFF acima ja'
  # prova listen/link/vcpu/so-duas contra o portao 1; aqui o alvo e'
  # "o portao 1 sozinho ja' basta para nunca chegar no create").
  copia_sabotada="${work_dir}/v5b2-copia-sabotada.xml"
  cp -- "$copia_legitima" "$copia_sabotada"
  xmlstarlet ed -L -u "/domain/vcpu" -v "8" "$copia_sabotada"

  stub_dir="${work_dir}/v5b2-bin"
  mkdir -p "$stub_dir"
  contador="${work_dir}/v5b2-create-count"
  : >"$contador"
  cat >"${stub_dir}/virsh" <<STUB
#!/usr/bin/env bash
if [ "\$1" = "-c" ]; then shift 2; fi
if [ "\${1:-}" = "create" ]; then
  printf 'x' >> "$contador"
  echo "duble: dominio criado (transient)"
  exit 0
fi
echo "duble virsh: subcomando nao esperado neste teste: \$*" >&2
exit 99
STUB
  chmod +x "${stub_dir}/virsh"

  echo ">>> V5B-CREATE 1/2: copia SABOTADA -- 'virsh create' tem de ser chamado ZERO vezes"
  TEARDOWN_MAQUINA_LIGADA=0
  PATH="${stub_dir}:$PATH" validar_e_ligar_maquina "duble-dom" "qemu:///session" "$permanente" "$copia_sabotada" >/dev/null 2>&1
  rc=$?
  chamadas_sabotada="$(wc -c <"$contador" | tr -d ' ')"
  echo ">>> codigo (esperado != 0): ${rc}; chamadas a 'virsh create' (esperado 0): ${chamadas_sabotada}"

  : >"$contador"
  TEARDOWN_MAQUINA_LIGADA=0
  echo ">>> V5B-CREATE 2/2: copia LEGITIMA -- 'virsh create' tem de ser chamado EXATAMENTE 1 vez"
  PATH="${stub_dir}:$PATH" validar_e_ligar_maquina "duble-dom" "qemu:///session" "$permanente" "$copia_legitima" >/dev/null 2>&1
  rc2=$?
  chamadas_legitima="$(wc -c <"$contador" | tr -d ' ')"
  echo ">>> codigo (esperado 0): ${rc2}; chamadas a 'virsh create' (esperado 1): ${chamadas_legitima}"
  TEARDOWN_MAQUINA_LIGADA=0

  if [ "$rc" -ne 0 ] && [ "$chamadas_sabotada" = "0" ] && [ "$rc2" -eq 0 ] && [ "$chamadas_legitima" = "1" ]; then
    echo "V5B-CREATE OK: a sabotada nunca chamou create; a legitima chamou exatamente uma vez."
    return 0
  fi
  echo "V5B-CREATE FALHOU: a contagem de chamadas nao bateu com o esperado."
  return 1
}

# V5B-PING: `esperar_guest_ping` respeita PRAZO TOTAL (nunca gira para
# sempre quando o agente nunca responde) e aprova rapido quando o agente
# responde de primeira (nunca espera o prazo inteiro a toa).
selftest_v5b_espera_prazo() {
  local work_dir="$1"
  local stub_falha stub_ok inicio fim decorrido rc rc2 caso1_ok=1 caso2_ok=1

  stub_falha="${work_dir}/v5b3-bin-falha"
  mkdir -p "$stub_falha"
  cat >"${stub_falha}/virsh" <<'STUB'
#!/usr/bin/env bash
echo "duble: guest-agent nao responde" >&2
exit 1
STUB
  chmod +x "${stub_falha}/virsh"

  echo ">>> V5B-PING 1/2: guest-ping SEMPRE falha -- tem de respeitar o prazo (2s) e reprovar"
  inicio="$(date +%s)"
  PATH="${stub_falha}:$PATH" esperar_guest_ping "duble-dom" "qemu:///session" 2 >/dev/null 2>&1
  rc=$?
  fim="$(date +%s)"
  decorrido=$((fim - inicio))
  echo ">>> codigo (esperado != 0): ${rc}; tempo decorrido (esperado ~2s): ${decorrido}s"
  [ "$rc" -eq 0 ] && caso1_ok=0
  if [ "$decorrido" -gt 6 ]; then
    echo "V5B-PING: demorou muito mais que o prazo pedido (${decorrido}s, folga de 6s)."
    caso1_ok=0
  fi

  stub_ok="${work_dir}/v5b3-bin-ok"
  mkdir -p "$stub_ok"
  cat >"${stub_ok}/virsh" <<'STUB'
#!/usr/bin/env bash
echo '{"return":{}}'
exit 0
STUB
  chmod +x "${stub_ok}/virsh"

  echo ">>> V5B-PING 2/2: guest-ping responde de primeira -- tem de aprovar sem esperar o prazo inteiro"
  inicio="$(date +%s)"
  PATH="${stub_ok}:$PATH" esperar_guest_ping "duble-dom" "qemu:///session" 30 >/dev/null 2>&1
  rc2=$?
  fim="$(date +%s)"
  decorrido=$((fim - inicio))
  echo ">>> codigo (esperado 0): ${rc2}; tempo decorrido (esperado bem menor que 30s): ${decorrido}s"
  [ "$rc2" -ne 0 ] && caso2_ok=0
  if [ "$decorrido" -gt 5 ]; then
    echo "V5B-PING: deveria ter aprovado quase imediatamente e demorou ${decorrido}s."
    caso2_ok=0
  fi

  if [ "$caso1_ok" -eq 1 ] && [ "$caso2_ok" -eq 1 ]; then
    echo "V5B-PING OK: prazo respeitado quando falha sempre; aprovacao rapida quando responde de primeira."
    return 0
  fi
  return 1
}

# V5B-TRES-COPIAS: integra o script inteiro (`--ligar` de ponta a ponta,
# contra o dublê de `virsh`) e prova que as TRES copias de sessao (disco,
# nvram, estado do TPM) nascem, existem DURANTE a sessao, e morrem em
# TODO caminho de saida: termino normal, SIGTERM no meio (com a maquina
# ja ligada -- prova que o teardown desliga por `virsh destroy` antes de
# apagar), e erro no meio (o `create` do dublê recusa -- prova que o
# teardown limpa mesmo sem a maquina ter ligado).
selftest_v5b_tres_copias() {
  local work_dir="$1"
  local base_disco base_nvram tpm_perm permanente_fix
  local bin_ok bin_falha
  local ok=1

  base_disco="${work_dir}/v5b4-disco-base.qcow2"
  base_nvram="${work_dir}/v5b4-nvram-base.qcow2"
  tpm_perm="${work_dir}/v5b4-tpm-permanente"
  qemu-img create -f qcow2 -- "$base_disco" 4M >/dev/null
  qemu-img create -f qcow2 -- "$base_nvram" 1M >/dev/null
  mkdir -p "$tpm_perm"
  echo "duble-tpm-state" >"${tpm_perm}/tpm2-00.permall"

  permanente_fix="${work_dir}/v5b4-permanente.xml"
  cp -- "${SCRIPT_DIR}/fixtures/dominio-limpo.xml" "$permanente_fix"
  xmlstarlet ed -L -u "/domain/devices/disk[@device='disk']/source/@file" -v "$base_disco" "$permanente_fix"
  xmlstarlet ed -L -u "/domain/os/nvram" -v "$base_nvram" "$permanente_fix"

  bin_ok="${work_dir}/v5b4-bin-ok"
  mkdir -p "$bin_ok"
  cat >"${bin_ok}/virsh" <<STUB
#!/usr/bin/env bash
[ "\$1" = "-c" ] && shift 2
sub="\$1"; shift
case "\$sub" in
  domstate) echo "desligado"; exit 0 ;;
  dumpxml) cat "$permanente_fix"; exit 0 ;;
  create) exit 0 ;;
  destroy) exit 0 ;;
  qemu-agent-command) echo '{"return":{}}'; exit 0 ;;
  *) echo "duble virsh: subcomando nao esperado: \$sub \$*" >&2; exit 99 ;;
esac
STUB
  chmod +x "${bin_ok}/virsh"

  bin_falha="${work_dir}/v5b4-bin-falha-create"
  mkdir -p "$bin_falha"
  cat >"${bin_falha}/virsh" <<STUB
#!/usr/bin/env bash
[ "\$1" = "-c" ] && shift 2
sub="\$1"; shift
case "\$sub" in
  domstate) echo "desligado"; exit 0 ;;
  dumpxml) cat "$permanente_fix"; exit 0 ;;
  create) echo "duble: create recusado de proposito" >&2; exit 1 ;;
  destroy) exit 0 ;;
  qemu-agent-command) echo '{"return":{}}'; exit 0 ;;
  *) echo "duble virsh: subcomando nao esperado: \$sub \$*" >&2; exit 99 ;;
esac
STUB
  chmod +x "${bin_falha}/virsh"

  # --- A: termino normal ------------------------------------------------
  local lock_a overlay_a nvram_a tpm_a
  lock_a="${work_dir}/v5b4-a/.vm.lock"
  overlay_a="${work_dir}/v5b4-a/overlay.qcow2"
  nvram_a="${work_dir}/v5b4-a/nvram-sessao.qcow2"
  tpm_a="${work_dir}/v5b4-a/tpm-sessao"
  mkdir -p "${work_dir}/v5b4-a"

  echo ">>> V5B-TRES-COPIAS A: termino normal, com a maquina ligada de verdade (via dublê)"
  local saida_a rc_a
  saida_a="$(PATH="${bin_ok}:$PATH" "$SCRIPT_PATH" --dom duble-dom --connect qemu:///session \
    --lock "$lock_a" --base "$base_disco" --overlay "$overlay_a" \
    --ligar --nvram-overlay "$nvram_a" --tpm-state-dir "$tpm_a" \
    --tpm-state-dir-permanente "$tpm_perm" --prazo-ping 5 \
    -- test -e "$overlay_a" 2>&1)"
  rc_a=$?
  echo "$saida_a" | tail -5
  echo ">>> rc=${rc_a} (esperado 0: o comando 'test -e overlay' rodou DENTRO da sessao e achou o arquivo)"
  if [ "$rc_a" -ne 0 ]; then
    echo "V5B-TRES-COPIAS A FALHOU: sessao com --ligar nao terminou com sucesso."
    ok=0
  fi
  if [ -e "$overlay_a" ] || [ -e "$nvram_a" ] || [ -e "$tpm_a" ]; then
    echo "V5B-TRES-COPIAS A FALHOU: pelo menos uma das tres copias sobrou apos o termino normal."
    ok=0
  else
    echo "V5B-TRES-COPIAS A OK: as tres copias sumiram apos o termino normal."
  fi

  # --- B: SIGTERM no meio, com a maquina ja ligada -----------------------
  local lock_b overlay_b nvram_b tpm_b pid_b
  lock_b="${work_dir}/v5b4-b/.vm.lock"
  overlay_b="${work_dir}/v5b4-b/overlay.qcow2"
  nvram_b="${work_dir}/v5b4-b/nvram-sessao.qcow2"
  tpm_b="${work_dir}/v5b4-b/tpm-sessao"
  mkdir -p "${work_dir}/v5b4-b"

  echo ">>> V5B-TRES-COPIAS B: SIGTERM no meio, com a maquina ja ligada (via dublê)"
  (
    PATH="${bin_ok}:$PATH" "$SCRIPT_PATH" --dom duble-dom --connect qemu:///session \
      --lock "$lock_b" --base "$base_disco" --overlay "$overlay_b" \
      --ligar --nvram-overlay "$nvram_b" --tpm-state-dir "$tpm_b" \
      --tpm-state-dir-permanente "$tpm_perm" --prazo-ping 20 \
      -- sleep 15
  ) >/dev/null 2>&1 &
  pid_b=$!
  sleep 3
  local durante_b_ok=1
  if [ ! -e "$overlay_b" ] || [ ! -e "$nvram_b" ] || [ ! -e "$tpm_b" ]; then
    echo "V5B-TRES-COPIAS B: pelo menos uma copia ainda nao existia 3s depois de iniciar (dublê lento? erro?)."
    durante_b_ok=0
  fi
  kill -TERM "$pid_b" 2>/dev/null
  wait "$pid_b" 2>/dev/null
  if [ "$durante_b_ok" -eq 1 ] && [ ! -e "$overlay_b" ] && [ ! -e "$nvram_b" ] && [ ! -e "$tpm_b" ]; then
    echo "V5B-TRES-COPIAS B OK: as tres existiam durante a sessao ligada e sumiram apos o SIGTERM."
  else
    echo "V5B-TRES-COPIAS B FALHOU."
    ok=0
  fi

  # --- C: erro no meio (virsh create recusa) -----------------------------
  local lock_c overlay_c nvram_c tpm_c rc_c
  lock_c="${work_dir}/v5b4-c/.vm.lock"
  overlay_c="${work_dir}/v5b4-c/overlay.qcow2"
  nvram_c="${work_dir}/v5b4-c/nvram-sessao.qcow2"
  tpm_c="${work_dir}/v5b4-c/tpm-sessao"
  mkdir -p "${work_dir}/v5b4-c"

  echo ">>> V5B-TRES-COPIAS C: erro no meio ('virsh create' recusa) -- copias criadas ANTES tem de sumir"
  PATH="${bin_falha}:$PATH" "$SCRIPT_PATH" --dom duble-dom --connect qemu:///session \
    --lock "$lock_c" --base "$base_disco" --overlay "$overlay_c" \
    --ligar --nvram-overlay "$nvram_c" --tpm-state-dir "$tpm_c" \
    --tpm-state-dir-permanente "$tpm_perm" --prazo-ping 5 \
    -- true >/dev/null 2>&1
  rc_c=$?
  echo ">>> rc=${rc_c} (esperado != 0: o 'create' do dublê recusou)"
  if [ "$rc_c" -eq 0 ]; then
    echo "V5B-TRES-COPIAS C FALHOU: deveria ter reprovado (create recusado) e nao reprovou."
    ok=0
  fi
  if [ -e "$overlay_c" ] || [ -e "$nvram_c" ] || [ -e "$tpm_c" ]; then
    echo "V5B-TRES-COPIAS C FALHOU: pelo menos uma copia sobrou apos o erro no meio."
    ok=0
  else
    echo "V5B-TRES-COPIAS C OK: as copias criadas antes do erro sumiram, a maquina nunca chegou a ligar."
  fi

  if [ "$ok" -eq 1 ]; then
    echo "V5B-TRES-COPIAS OK: os tres caminhos de saida (normal, SIGTERM, erro) limpam as tres copias."
    return 0
  fi
  return 1
}

selftest() {
  local work_dir ok=1
  work_dir="$(mktemp -d /var/tmp/glintfx-sessao-selftest.XXXXXX)"
  trap 'rm -rf -- "$work_dir"' RETURN

  echo "=== SELFTEST sessao.sh (E1, E2, E7, V5B-*) -- diretorio de trabalho: ${work_dir} ==="
  echo
  selftest_e1 "$work_dir" || ok=0
  echo
  selftest_e2 "$work_dir" || ok=0
  echo
  selftest_e7 "$work_dir" || ok=0
  echo
  selftest_v5b_diff_exato "$work_dir" || ok=0
  echo
  selftest_v5b_portao_antes_do_create "$work_dir" || ok=0
  echo
  selftest_v5b_espera_prazo "$work_dir" || ok=0
  echo
  selftest_v5b_tres_copias "$work_dir" || ok=0
  echo

  if [ "$ok" -eq 1 ]; then
    echo "SELFTEST OK: E1, E2, E7 e os quatro cenarios V5B-* se comportaram como esperado."
    return 0
  fi
  echo "SELFTEST FALHOU: pelo menos um cenario nao se comportou como esperado."
  return 1
}

# --- despacho ------------------------------------------------------------------

if [ "${1:-}" = "--selftest" ]; then
  selftest
  exit $?
fi

DOM="$DOM_DEFAULT"
CONNECT="$CONNECT_DEFAULT"
LOCK_FILE="$LOCK_FILE_DEFAULT"
BASE_DISK="$BASE_DISK_DEFAULT"
OVERLAY_PATH="$OVERLAY_DEFAULT"
LIGAR=0
NVRAM_OVERLAY_PATH="$NVRAM_OVERLAY_DEFAULT"
TPM_STATE_DIR_PATH="$TPM_STATE_DIR_DEFAULT"
TPM_STATE_DIR_PERMANENTE_PATH=""
PRAZO_PING="$PRAZO_PING_DEFAULT"
COMANDO=()

while [ $# -gt 0 ]; do
  case "$1" in
    --dom) DOM="${2:?}"; shift 2 ;;
    --connect) CONNECT="${2:?}"; shift 2 ;;
    --lock) LOCK_FILE="${2:?}"; shift 2 ;;
    --base) BASE_DISK="${2:?}"; shift 2 ;;
    --overlay) OVERLAY_PATH="${2:?}"; shift 2 ;;
    --ligar) LIGAR=1; shift ;;
    --nvram-overlay) NVRAM_OVERLAY_PATH="${2:?}"; shift 2 ;;
    --tpm-state-dir) TPM_STATE_DIR_PATH="${2:?}"; shift 2 ;;
    --tpm-state-dir-permanente) TPM_STATE_DIR_PERMANENTE_PATH="${2:?}"; shift 2 ;;
    --prazo-ping) PRAZO_PING="${2:?}"; shift 2 ;;
    --)
      shift
      COMANDO=("$@")
      break
      ;;
    -h|--help) uso ;;
    *) uso ;;
  esac
done

rodar_sessao "$DOM" "$CONNECT" "$LOCK_FILE" "$BASE_DISK" "$OVERLAY_PATH" \
  "$LIGAR" "$NVRAM_OVERLAY_PATH" "$TPM_STATE_DIR_PATH" "$TPM_STATE_DIR_PERMANENTE_PATH" "$PRAZO_PING" \
  "${COMANDO[@]}"
exit $?
