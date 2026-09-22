#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Coletor de resultado do laboratorio de VM Windows (item WIN-RUNNER-PROPRIO,
# sub-fatia V-3, GODS_LAWS.md deste projeto L-09/L-11, e L-35/L-36/L-40/L-45
# do arquivo global). Traz de volta, pelo canal org.qemu.guest_agent.0
# (guest-file-read), TODO arquivo de um diretorio de resultados dentro do
# convidado -- log de build, XML de CTest, qualquer coisa que nao caiba na
# saida padrao de guest-exec (ver /var/tmp/glintfx-plan/win-runner-local.md
# secao 1.1). Nunca liga a maquina, nunca altera a definicao persistente do
# dominio -- so fala com um convidado ja em execucao, por fora deste script.
#
# md5 CONFERIDO NAS DUAS PONTAS, e por que as duas: o mesmo defeito que
# rodar-caminho.sh e sessao.sh (nesta mesma pasta) ja documentam -- "afirma
# que mede e nao mede" -- tambem morde aqui. Contar bytes escritos localmente
# prova que ALGO chegou, nunca que o QUE chegou e' o mesmo que estava do
# outro lado. Por isso cada arquivo coletado tem o hash MD5 pedido ao
# CONVIDADO (via certutil -hashfile, ja embutido em qualquer Windows) e o
# hash calculado sobre a COPIA LOCAL depois da reconstrucao; so um casa com
# o outro autoriza declarar sucesso. Se nao casar, o arquivo local parcial e'
# APAGADO e o script GRITA (codigo != 0) -- nunca devolve uma copia que nao
# se sabe se e' confiavel como se fosse boa.
#
# PISO DE VARREDURA NAO-VAZIA (GODS_LAWS.md global L-40): pedir a coleta de
# um diretorio sem nenhum arquivo e' RECUSA, nunca sucesso silencioso, e a
# linha "encontrados=N coletados=M rejeitados=R" sai SEMPRE, mesmo com os
# tres em zero -- zero tem de ser porque o diretorio esta genuinamente
# vazio, nunca porque o portao nao contou.
#
# LISTA DE PERMISSAO PARA NOME DE ARQUIVO (achado do team-lead, 22/09/2026,
# travessia de caminho real - ver o comentario de NOME_ARQUIVO_REGEX mais
# abaixo, junto de _dir_listar): o CONVIDADO escolhe cada nome que esta
# listagem devolve, e o convidado e' NAO CONFIAVEL por desenho deste
# projeto inteiro (trava, rede desligada, portao de isolamento). Todo nome
# que nao casar a lista de permissao e' rejeitado ANTES de virar caminho,
# nunca com `continue` calado - a linha de contagem sempre mostra
# "rejeitados=N", e o script sai com o codigo 2 (distinto de 0/1) se algum
# nome foi rejeitado, mesmo que os demais arquivos tenham sido coletados
# com sucesso.
#
# [A VERIFICAR] O LIMITE REAL DE BYTES POR CHAMADA guest-file-read NAO FOI
# MEDIDO NESTA MAQUINA -- a maquina esta desligada nesta sessao (ordem do
# lider, "dirigir daqui, sem ligar ao servidor", 22/09/2026) e a medicao so
# e' possivel com o convidado vivo. O default abaixo (CHUNK_BYTES_PADRAO)
# reaproveita por PALPITE o mesmo valor que transferir-executar.sh (mesma
# pasta) ja usa para guest-file-WRITE -- mas aquele valor foi medido para
# ESCRITA, nao para LEITURA, e os dois lados do protocolo podem ter limites
# diferentes. Nao presuma este numero como medido; --chunk-bytes existe
# exatamente para nao cravar um palpite nao verificado no corpo do script.
# A medicao real fica pendente para a sessao de arranque que ligar a
# maquina (mesma pendencia que a secao 7 do plano ja nomeia).
#
# Codigos de saida: 0 sucesso (tudo encontrado foi coletado e conferido por
# md5); 1 diretorio vazio (piso de varredura), OU pelo menos um nome VALIDO
# nao bateu md5; 2 pelo menos um nome REJEITADO pela lista de permissao
# (travessia de caminho ou nome hostil - nunca vira caminho no hospedeiro).
#
# Uso:
#   coletar-resultados.sh <diretorio-windows> <diretorio-destino-local>
#                          [--chunk-bytes N] [--prazo N]
#   coletar-resultados.sh --selftest
set -u
set -o pipefail

DOM="glintfx-win11-lab"
CONNECT="qemu:///session"
CHUNK_BYTES_PADRAO=65536   # [A VERIFICAR] - ver o paragrafo acima, nao medido para LEITURA.
PRAZO_PADRAO=60

# GATE-SELFTEST-ORFAO / GODS_LAWS.md L-40: mesma guarda que os tres vizinhos
# nesta pasta (provar-isolamento.sh para xmlstarlet, rodar-caminho.sh e
# rodar-um.sh para jq, sessao.sh para qemu-img/flock) ja usam. Sem ela, os
# `jq -r`/`jq -n` abaixo falhariam em silencio (comando ausente ->
# substituicao vazia), e uma leitura vazia pareceria um campo genuinamente
# ausente do protocolo -- a mesma familia de defeito que este projeto ja
# catalogou como "afirma que mede e nao mede". `exit 77` e' o mesmo codigo
# de AUSENCIA DECLARADA (nunca 0 nem 1) que os quatro vizinhos ja usam, lido
# pelo CTest como "Not Run" (SKIP_RETURN_CODE).
for _ferramenta in jq md5sum base64 dd stat; do
  if ! command -v "$_ferramenta" >/dev/null 2>&1; then
    echo "AUSENTE: ${_ferramenta} nao encontrado no PATH - script pulado (declarado, nunca silencioso)." >&2
    exit 77
  fi
done
unset _ferramenta

# --- guest-exec generico, com prazo passado por argumento -------------------
# Mesma forma do nucleo de rodar-caminho.sh (mesma pasta): guest-exec, poll a
# guest-exec-status ate 'exited:true' ou o PRAZO esgotar. Devolve o resultado
# em tres variaveis globais (_GE_OUT, _GE_ERR, _GE_RC) em vez de por
# substituicao de comando, porque o CHAMADOR precisa do valor de _ARQUIVOS
# (array) que substituicao de comando perderia ao rodar em subshell.
_GE_OUT=""
_GE_ERR=""
_GE_RC=0

_guest_exec_aguardar() {
  local path_exe="$1" args_json="$2" prazo="$3"
  local exec_res pid status_res exited i exitcode

  exec_res=$(virsh -c "$CONNECT" qemu-agent-command "$DOM" \
    "$(jq -n --arg p "$path_exe" --argjson a "$args_json" '{execute:"guest-exec",arguments:{path:$p,arg:$a,"capture-output":true}}')" 2>&1)
  pid=$(echo "$exec_res" | jq -r '.return.pid // empty')
  if [ -z "$pid" ]; then
    _GE_RC=1
    _GE_OUT=""
    _GE_ERR="$exec_res"
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

  if [ "$exited" != "true" ]; then
    _GE_RC=124
    _GE_OUT=""
    _GE_ERR="estourou o prazo de ${prazo}s"
    return 124
  fi

  exitcode=$(echo "$status_res" | jq -r '.return.exitcode // "DESCONHECIDO"')
  _GE_OUT=$(echo "$status_res" | jq -r '.return["out-data"] // empty' | base64 -d 2>/dev/null)
  _GE_ERR=$(echo "$status_res" | jq -r '.return["err-data"] // empty' | base64 -d 2>/dev/null)
  _GE_RC="$exitcode"
  [ "$exitcode" = "0" ] && return 0
  return 3
}

# --- listagem do diretorio de resultados no convidado ------------------------
# Usa cmd.exe /c dir /b /a-d (so nomes de ARQUIVO, uma linha cada) -- o
# resultado de EXIT CODE do proprio dir e' ignorado de proposito (Windows
# varia entre 0 e 1 para "diretorio vazio" dependendo da build); a UNICA
# fonte de verdade para "quantos arquivos existem" e' a CONTAGEM DE LINHAS
# nao-vazias devolvidas, nunca o codigo de saida do listador.
#
# TRAVESSIA DE CAMINHO (achado do team-lead, 22/09/2026, revisor automatico
# de seguranca - real, nao falso positivo): cada NOME que sai desta listagem
# e' escolhido pelo CONVIDADO, um sistema que este projeto inteiro trata
# como NAO CONFIAVEL (e' por isso que a trava, a rede desligada e o portao
# de isolamento existem). O corpo original deste script colava o nome direto
# num caminho do HOSPEDEIRO (coletar_diretorio: "${destino_dir_local}/
# ${arquivo}"), sem validar nada -- um convidado comprometido podia devolver
# algo como "../../escapou" e escrever arquivo arbitrario, com conteudo
# arbitrario, fora do diretorio de destino, na maquina de trabalho do lider.
# Provado vermelho ANTES deste conserto (nao no arquivo, nunca na arvore
# rastreada -- L-27): copia do corpo antigo isolada em scratch, dublê
# devolvendo "../../escapou", RC=0 e arquivo aparecendo um nivel ACIMA do
# destino, com md5 batendo (o script achava que tinha ido tudo bem).
#
# CORRECAO: LISTA DE PERMISSAO (GODS_LAWS.md global L-02 - negativa
# permanente + excecao estreita), nunca lista de proibicao (proibicao
# esquece o caso que ninguem pensou). So' nomes que casem
# NOME_ARQUIVO_REGEX entram em _ARQUIVOS; qualquer outra coisa vai para
# _ARQUIVOS_REJEITADOS e NUNCA chega perto de um caminho do hospedeiro ou
# do convidado - a validacao acontece ANTES de qualquer concatenacao,
# aqui mesmo em _dir_listar, nunca depois.
NOME_ARQUIVO_REGEX='^[A-Za-z0-9][A-Za-z0-9._-]*$'
NOME_ARQUIVO_MAX_LEN=255

_ARQUIVOS=()
_ARQUIVOS_REJEITADOS=()

_dir_listar() {
  local diretorio_win="$1" prazo="$2"
  local linha

  _ARQUIVOS=()
  _ARQUIVOS_REJEITADOS=()
  _guest_exec_aguardar "C:\\Windows\\System32\\cmd.exe" \
    "$(jq -n --arg d "$diretorio_win" '["/c","dir","/b","/a-d",$d]')" "$prazo"
  local rc=$?

  if [ "$rc" -eq 1 ] || [ "$rc" -eq 124 ]; then
    echo "ERRO ao listar '${diretorio_win}' no convidado: ${_GE_ERR}" >&2
    return 1
  fi

  while IFS= read -r linha; do
    linha="${linha%$'\r'}"
    [ -z "$linha" ] && continue
    if [[ "$linha" =~ $NOME_ARQUIVO_REGEX ]] && [ "${#linha}" -le "$NOME_ARQUIVO_MAX_LEN" ]; then
      _ARQUIVOS+=("$linha")
    else
      _ARQUIVOS_REJEITADOS+=("$linha")
    fi
  done <<<"$_GE_OUT"
  return 0
}

# --- hash MD5 pedido ao CONVIDADO (lado remoto da comparacao) ---------------
# certutil -hashfile e' utilitario embutido em qualquer Windows desde o
# Vista -- nao precisa instalar nada no convidado (GODS_LAWS.md global L-51
# nao morde aqui). A saida tem tres linhas: cabecalho, o hash com bytes
# separados por espaco, e o rodape "CertUtil: ... concluido"; a linha do
# meio e' a que interessa.
_hash_remoto() {
  local caminho_win="$1" prazo="$2"
  local hash

  _guest_exec_aguardar "C:\\Windows\\System32\\certutil.exe" \
    "$(jq -n --arg c "$caminho_win" '["-hashfile",$c,"MD5"]')" "$prazo"
  local rc=$?
  if [ "$rc" -eq 1 ] || [ "$rc" -eq 124 ]; then
    echo "ERRO: falha ao pedir hash remoto de '${caminho_win}': ${_GE_ERR}" >&2
    return 1
  fi

  hash=$(printf '%s' "$_GE_OUT" | sed -n '2p' | tr -d ' \r' | tr 'A-F' 'a-f')
  if [ -z "$hash" ]; then
    echo "ERRO: nao consegui extrair o hash MD5 da saida do certutil para '${caminho_win}': ${_GE_OUT}" >&2
    return 1
  fi
  printf '%s' "$hash"
  return 0
}

# --- leitura do conteudo, em blocos, via guest-file-read ---------------------
# Nunca presume o tamanho do arquivo de antemao (o convidado que decide
# quando eof=true chega); TRUNCAMENTO (eof prematuro, ou hash que nao bate
# no fim) e' detectado pelo CHAMADOR (coletar_um), nao aqui -- esta funcao
# so' reconstrui o que o protocolo realmente entregou, fielmente.
_ler_arquivo_remoto() {
  local caminho_win="$1" destino_local="$2" chunk_bytes="$3"
  local open_res handle read_res count buf eof

  open_res=$(virsh -c "$CONNECT" qemu-agent-command "$DOM" \
    "$(jq -n --arg p "$caminho_win" '{execute:"guest-file-open",arguments:{path:$p,mode:"rb"}}')" 2>&1)
  handle=$(echo "$open_res" | jq -r '.return')
  if ! [[ "$handle" =~ ^[0-9]+$ ]]; then
    echo "ERRO ao abrir '${caminho_win}' no convidado para leitura: ${open_res}" >&2
    return 1
  fi

  : >"$destino_local"
  eof="false"
  while [ "$eof" != "true" ]; do
    read_res=$(virsh -c "$CONNECT" qemu-agent-command "$DOM" \
      "$(jq -n --argjson h "$handle" --argjson c "$chunk_bytes" '{execute:"guest-file-read",arguments:{handle:$h,count:$c}}')" 2>&1)
    count=$(echo "$read_res" | jq -r '.return.count // empty')
    if [ -z "$count" ]; then
      echo "ERRO na leitura de '${caminho_win}': ${read_res}" >&2
      virsh -c "$CONNECT" qemu-agent-command "$DOM" \
        "$(jq -n --argjson h "$handle" '{execute:"guest-file-close",arguments:{handle:$h}}')" >/dev/null 2>&1 || true
      return 1
    fi
    buf=$(echo "$read_res" | jq -r '.return["buf-b64"] // empty')
    if [ -n "$buf" ]; then
      printf '%s' "$buf" | base64 -d >>"$destino_local" 2>/dev/null
    fi
    eof=$(echo "$read_res" | jq -r '.return.eof // false')
  done

  virsh -c "$CONNECT" qemu-agent-command "$DOM" \
    "$(jq -n --argjson h "$handle" '{execute:"guest-file-close",arguments:{handle:$h}}')" >/dev/null 2>&1
  return 0
}

# --- coleta de UM arquivo, com a conferencia de md5 nas duas pontas --------
coletar_um() {
  local caminho_win="$1" destino_local="$2" chunk_bytes="$3" prazo="$4"
  local hash_remoto hash_local

  hash_remoto=$(_hash_remoto "$caminho_win" "$prazo") || return 1

  if ! _ler_arquivo_remoto "$caminho_win" "$destino_local" "$chunk_bytes"; then
    return 1
  fi

  hash_local=$(md5sum -- "$destino_local" | cut -d' ' -f1)
  echo "  ${caminho_win}: md5-remoto=${hash_remoto} md5-local=${hash_local}"

  if [ "$hash_remoto" != "$hash_local" ]; then
    echo "ERRO: md5 nao bate para '${caminho_win}' - a copia local NAO e' confiavel; removendo em vez de devolver parcial." >&2
    rm -f -- "$destino_local"
    return 1
  fi
  return 0
}

# --- orquestracao: lista, coleta cada um, imprime o piso SEMPRE -------------
#
# "encontrados" conta TUDO que a listagem devolveu (validos + rejeitados -
# e' o fato bruto observado); "rejeitados" nunca fica escondido dentro de
# "encontrados-coletados" (isso confundiria travessia de caminho com falha
# de md5, dois defeitos bem diferentes) - por isso tem a PROPRIA contagem,
# impressa sempre, mesmo em zero (GODS_LAWS.md global L-40), e o PROPRIO
# codigo de saida (2), distinto de 1 (vazio / md5 nao bateu).
coletar_diretorio() {
  local diretorio_win="$1" destino_dir_local="$2" chunk_bytes="$3" prazo="$4"
  local encontrados rejeitados coletados=0 arquivo

  mkdir -p -- "$destino_dir_local"

  if ! _dir_listar "$diretorio_win" "$prazo"; then
    return 1
  fi
  rejeitados=${#_ARQUIVOS_REJEITADOS[@]}
  encontrados=$((${#_ARQUIVOS[@]} + rejeitados))

  if [ "$encontrados" -eq 0 ]; then
    echo "encontrados=0 coletados=0 rejeitados=0"
    echo "RECUSADO: nada para coletar em '${diretorio_win}' (piso de varredura nao-vazia, GODS_LAWS.md global L-40)." >&2
    return 1
  fi

  if [ "$rejeitados" -gt 0 ]; then
    echo "REJEITADO: ${rejeitados} nome(s) devolvido(s) pelo convidado NAO passou(aram) na lista de permissao (regex ${NOME_ARQUIVO_REGEX}, ate ${NOME_ARQUIVO_MAX_LEN} caracteres) - provavel travessia de caminho ou nome hostil; NUNCA vira caminho no hospedeiro:" >&2
    printf '      -> %s\n' "${_ARQUIVOS_REJEITADOS[@]}" >&2
  fi

  for arquivo in "${_ARQUIVOS[@]}"; do
    if coletar_um "${diretorio_win}\\${arquivo}" "${destino_dir_local}/${arquivo}" "$chunk_bytes" "$prazo"; then
      coletados=$((coletados + 1))
    fi
  done

  echo "encontrados=${encontrados} coletados=${coletados} rejeitados=${rejeitados}"

  if [ "$rejeitados" -gt 0 ]; then
    echo "REPROVADO: pelo menos um nome hostil foi rejeitado - nunca sucesso silencioso com menos arquivos do que o convidado devolveu (GODS_LAWS.md global L-40)." >&2
    return 2
  fi
  if [ "$coletados" -ne "$encontrados" ]; then
    echo "REPROVADO: ${encontrados} arquivo(s) encontrado(s), so ${coletados} coletado(s) com md5 conferido nas duas pontas." >&2
    return 1
  fi
  return 0
}

# --- selftest: duble de virsh, nunca a VM real ------------------------------
# O duble simula os cinco verbos do protocolo que este script fala
# (guest-exec, guest-exec-status, guest-file-open, guest-file-read,
# guest-file-close) contra um "conteudo.bin" que o proprio selftest escreve
# no disco local -- nunca contra libvirt/QEMU real. Estado entre chamadas
# (offset de leitura por handle, qual comando foi o ultimo guest-exec) vive
# em arquivos dentro de DUBLE_STATE_DIR, porque cada invocacao do duble e'
# um PROCESSO NOVO (bash nao preserva variavel entre invocacoes separadas).

escrever_duble_virsh() {
  local stub_dir="$1"
  cat >"${stub_dir}/virsh" <<'STUB'
#!/usr/bin/env bash
set -u
JSON="${*: -1}"
EXECUTE=$(echo "$JSON" | jq -r '.execute')
CENARIO="${DUBLE_CENARIO:-integro}"
STATE_DIR="${DUBLE_STATE_DIR:?DUBLE_STATE_DIR nao definido}"
CONTEUDO="${STATE_DIR}/conteudo.bin"

case "$EXECUTE" in
  guest-exec)
    PATH_ARG=$(echo "$JSON" | jq -r '.arguments.path')
    case "$PATH_ARG" in
      *cmd.exe) echo "listar" >"${STATE_DIR}/tipo"; echo '{"return":{"pid":5001}}' ;;
      *certutil.exe) echo "hash" >"${STATE_DIR}/tipo"; echo '{"return":{"pid":5002}}' ;;
      *) echo "outro" >"${STATE_DIR}/tipo"; echo '{"return":{"pid":5000}}' ;;
    esac
    ;;
  guest-exec-status)
    TIPO=$(cat "${STATE_DIR}/tipo" 2>/dev/null || echo outro)
    if [ "$TIPO" = "listar" ]; then
      if [ "$CENARIO" = "vazio" ]; then
        OUT_B64=""
      else
        OUT_B64=$(printf 'conteudo.bin\r\n' | base64 -w0)
      fi
    elif [ "$TIPO" = "hash" ]; then
      HASH=$(md5sum "$CONTEUDO" | cut -d' ' -f1)
      OUT_B64=$(printf 'Hash MD5 de conteudo.bin:\r\n%s\r\nCertUtil: -hashfile comando concluido com exito.\r\n' "$HASH" | base64 -w0)
    else
      OUT_B64=""
    fi
    printf '{"return":{"exited":true,"exitcode":0,"out-data":"%s","err-data":""}}\n' "$OUT_B64"
    ;;
  guest-file-open)
    rm -f "${STATE_DIR}/offset-77"
    echo '{"return":77}'
    ;;
  guest-file-read)
    COUNT=$(echo "$JSON" | jq -r '.arguments.count')
    OFFSET_FILE="${STATE_DIR}/offset-77"
    OFFSET=$(cat "$OFFSET_FILE" 2>/dev/null || echo 0)
    if [ "$CENARIO" = "truncado" ] && [ "$OFFSET" -gt 0 ]; then
      # simula EOF PREMATURO: so o primeiro bloco (offset 0) sai de verdade;
      # a partir do segundo pedido, o duble para de entregar conteudo -
      # exatamente o "truncar de proposito" que E8 pede.
      printf '{"return":{"count":0,"buf-b64":"","eof":true}}\n'
    else
      TAMANHO=$(stat -c%s "$CONTEUDO")
      RESTANTE=$((TAMANHO - OFFSET))
      if [ "$RESTANTE" -le 0 ]; then
        printf '{"return":{"count":0,"buf-b64":"","eof":true}}\n'
      else
        N="$COUNT"
        [ "$N" -gt "$RESTANTE" ] && N="$RESTANTE"
        B64=$(dd if="$CONTEUDO" bs=1 skip="$OFFSET" count="$N" 2>/dev/null | base64 -w0)
        NOVO_OFFSET=$((OFFSET + N))
        echo "$NOVO_OFFSET" >"$OFFSET_FILE"
        if [ "$NOVO_OFFSET" -ge "$TAMANHO" ]; then EOF_FLAG=true; else EOF_FLAG=false; fi
        printf '{"return":{"count":%d,"buf-b64":"%s","eof":%s}}\n' "$N" "$B64" "$EOF_FLAG"
      fi
    fi
    ;;
  guest-file-close)
    echo '{"return":true}'
    ;;
  *)
    echo '{"return":{}}'
    ;;
esac
STUB
  chmod +x "${stub_dir}/virsh"
}

# Duble DEDICADO da prova de seguranca (achado do team-lead, 22/09/2026):
# a listagem devolve TRES linhas - um nome legitimo e DOIS nomes hostis
# (travessia "../../escapou" e barra invertida "sub\dir\arquivo", simulando
# um agente convidado comprometido fabricando a resposta) - para provar que
# a lista de permissao em _dir_listar barra os dois ANTES de qualquer
# caminho ser montado, sem impedir a coleta do nome legitimo.
escrever_duble_virsh_nomes_hostis() {
  local stub_dir="$1"
  cat >"${stub_dir}/virsh" <<'STUB'
#!/usr/bin/env bash
set -u
JSON="${*: -1}"
EXECUTE=$(echo "$JSON" | jq -r '.execute')
STATE_DIR="${DUBLE_STATE_DIR:?DUBLE_STATE_DIR nao definido}"
CONTEUDO="${STATE_DIR}/conteudo.bin"

case "$EXECUTE" in
  guest-exec)
    PATH_ARG=$(echo "$JSON" | jq -r '.arguments.path')
    case "$PATH_ARG" in
      *cmd.exe) echo "listar" >"${STATE_DIR}/tipo"; echo '{"return":{"pid":5001}}' ;;
      *certutil.exe) echo "hash" >"${STATE_DIR}/tipo"; echo '{"return":{"pid":5002}}' ;;
      *) echo "outro" >"${STATE_DIR}/tipo"; echo '{"return":{"pid":5000}}' ;;
    esac
    ;;
  guest-exec-status)
    TIPO=$(cat "${STATE_DIR}/tipo" 2>/dev/null || echo outro)
    if [ "$TIPO" = "listar" ]; then
      # um nome legitimo + dois nomes HOSTIS fabricados pelo "convidado":
      # travessia de diretorio, e barra invertida (separador do proprio
      # Windows) tentando embutir subcaminho no nome.
      OUT_B64=$(printf 'resultado.log\r\n../../escapou\r\nsub\\dir\\arquivo\r\n' | base64 -w0)
    elif [ "$TIPO" = "hash" ]; then
      HASH=$(md5sum "$CONTEUDO" | cut -d' ' -f1)
      OUT_B64=$(printf 'Hash MD5 de resultado.log:\r\n%s\r\nCertUtil: -hashfile comando concluido com exito.\r\n' "$HASH" | base64 -w0)
    else
      OUT_B64=""
    fi
    printf '{"return":{"exited":true,"exitcode":0,"out-data":"%s","err-data":""}}\n' "$OUT_B64"
    ;;
  guest-file-open)
    rm -f "${STATE_DIR}/offset-77"
    echo '{"return":77}'
    ;;
  guest-file-read)
    COUNT=$(echo "$JSON" | jq -r '.arguments.count')
    OFFSET_FILE="${STATE_DIR}/offset-77"
    OFFSET=$(cat "$OFFSET_FILE" 2>/dev/null || echo 0)
    TAMANHO=$(stat -c%s "$CONTEUDO")
    RESTANTE=$((TAMANHO - OFFSET))
    if [ "$RESTANTE" -le 0 ]; then
      printf '{"return":{"count":0,"buf-b64":"","eof":true}}\n'
    else
      N="$COUNT"
      [ "$N" -gt "$RESTANTE" ] && N="$RESTANTE"
      B64=$(dd if="$CONTEUDO" bs=1 skip="$OFFSET" count="$N" 2>/dev/null | base64 -w0)
      NOVO_OFFSET=$((OFFSET + N))
      echo "$NOVO_OFFSET" >"$OFFSET_FILE"
      if [ "$NOVO_OFFSET" -ge "$TAMANHO" ]; then EOF_FLAG=true; else EOF_FLAG=false; fi
      printf '{"return":{"count":%d,"buf-b64":"%s","eof":%s}}\n' "$N" "$B64" "$EOF_FLAG"
    fi
    ;;
  guest-file-close)
    echo '{"return":true}'
    ;;
  *)
    echo '{"return":{}}'
    ;;
esac
STUB
  chmod +x "${stub_dir}/virsh"
}

selftest_seguranca_nomes() {
  local work_dir="$1" stub_dir state_dir saida rc ok=1
  local pai_do_destino="${work_dir}/seg-fora"
  local destino="${pai_do_destino}/aninhado/destino"

  stub_dir="${work_dir}/seg-bin"
  state_dir="${work_dir}/seg-state"
  mkdir -p "$stub_dir" "$state_dir"
  printf 'conteudo legitimo' >"${state_dir}/conteudo.bin"
  escrever_duble_virsh_nomes_hostis "$stub_dir"

  printf '%s\n' ">>> SEGURANCA (travessia de caminho, achado do team-lead 22/09/2026): listagem devolve 1 nome legitimo + 2 nomes HOSTIS ('../../escapou', 'sub\\dir\\arquivo') - esperado: os hostis sao rejeitados (nunca viram caminho), o legitimo e' coletado, codigo de saida 2, NADA escrito fora de '${destino}'"
  saida="$(PATH="${stub_dir}:$PATH" DUBLE_STATE_DIR="$state_dir" coletar_diretorio 'C:\Users\glintfx\resultados' "$destino" 8 2 2>&1)"
  rc=$?
  echo "$saida"
  echo ">>> codigo obtido: ${rc}"

  if [ "$rc" -ne 2 ]; then
    echo "SEGURANCA FALHOU: esperava codigo 2 (rejeicao de nome hostil), obteve ${rc}."
    ok=0
  fi
  if ! printf '%s' "$saida" | grep -q "rejeitados=2"; then
    echo "SEGURANCA FALHOU: esperava 'rejeitados=2' impresso na linha de contagem."
    ok=0
  fi
  if [ -f "${destino}/resultado.log" ]; then
    echo "SEGURANCA OK (parcial): o nome legitimo foi coletado normalmente, apesar da rejeicao dos hostis."
  else
    echo "SEGURANCA FALHOU: o nome legitimo deveria ter sido coletado e nao foi."
    ok=0
  fi
  if find "$pai_do_destino" -type f -not -path "${destino}/*" 2>/dev/null | grep -q .; then
    echo "SEGURANCA FALHOU: apareceu arquivo FORA do destino pretendido:"
    find "$pai_do_destino" -type f -not -path "${destino}/*" 2>/dev/null
    ok=0
  else
    echo "SEGURANCA OK: nenhum arquivo apareceu fora de '${destino}' - a travessia nao escapou."
  fi

  [ "$ok" -eq 1 ] && return 0
  return 1
}

selftest_e6() {
  local work_dir="$1" stub_dir state_dir saida rc

  stub_dir="${work_dir}/e6-bin"
  state_dir="${work_dir}/e6-state"
  mkdir -p "$stub_dir" "$state_dir"
  : >"${state_dir}/conteudo.bin"
  escrever_duble_virsh "$stub_dir"

  echo ">>> E6: pedir coleta de um diretorio SEM arquivos (esperado: recusa por piso de varredura, encontrados=0 coletados=0 impressos mesmo em zero)"
  saida="$(PATH="${stub_dir}:$PATH" DUBLE_CENARIO=vazio DUBLE_STATE_DIR="$state_dir" coletar_diretorio 'C:\Users\glintfx\resultados' "${work_dir}/e6-saida" 8 2 2>&1)"
  rc=$?
  echo "$saida"
  echo ">>> codigo obtido: ${rc}"

  if [ "$rc" -ne 0 ] && printf '%s' "$saida" | grep -q "encontrados=0 coletados=0"; then
    echo "E6 OK: recusado (codigo ${rc}) com encontrados=0 coletados=0 impresso mesmo em zero."
    return 0
  fi
  echo "E6 FALHOU: deveria recusar com encontrados=0 coletados=0 impresso, e nao recusou assim."
  return 1
}

selftest_e8() {
  local work_dir="$1" stub_dir state_dir saida rc1 rc2 ok=1

  stub_dir="${work_dir}/e8-bin"
  state_dir="${work_dir}/e8-state"
  mkdir -p "$stub_dir" "$state_dir"
  printf '0123456789ABCDEFGHIJ' >"${state_dir}/conteudo.bin" # 20 bytes, mais de um bloco de 8
  escrever_duble_virsh "$stub_dir"

  echo ">>> E8a: trazer um arquivo INTEGRO, maior que um bloco (conteudo=20 bytes, chunk=8) - esperado: md5 confere, sucesso"
  saida="$(PATH="${stub_dir}:$PATH" DUBLE_CENARIO=integro DUBLE_STATE_DIR="$state_dir" coletar_diretorio 'C:\Users\glintfx\resultados' "${work_dir}/e8a-saida" 8 2 2>&1)"
  rc1=$?
  echo "$saida"
  echo ">>> codigo obtido (integro): ${rc1}"
  if [ "$rc1" -eq 0 ] && [ -f "${work_dir}/e8a-saida/conteudo.bin" ]; then
    echo "E8a OK: coleta integra aceita, arquivo local presente e com o mesmo conteudo."
  else
    echo "E8a FALHOU: deveria ter aceitado a copia integra e nao aceitou."
    ok=0
  fi

  echo ">>> E8b: trazer o MESMO arquivo com EOF PREMATURO simulado logo apos o primeiro bloco (truncado de proposito) - esperado: grita (codigo != 0), NUNCA deixa arquivo parcial no destino"
  saida="$(PATH="${stub_dir}:$PATH" DUBLE_CENARIO=truncado DUBLE_STATE_DIR="$state_dir" coletar_diretorio 'C:\Users\glintfx\resultados' "${work_dir}/e8b-saida" 8 2 2>&1)"
  rc2=$?
  echo "$saida"
  echo ">>> codigo obtido (truncado): ${rc2}"
  if [ "$rc2" -ne 0 ] && [ ! -e "${work_dir}/e8b-saida/conteudo.bin" ]; then
    echo "E8b OK: truncamento detectado pelo md5 que nao bate, recusado, nenhum arquivo parcial deixado para tras."
  else
    echo "E8b FALHOU: deveria ter gritado e apagado a copia parcial, e nao fez isso."
    ok=0
  fi

  [ "$ok" -eq 1 ] && return 0
  return 1
}

selftest() {
  local work_dir ok=1
  work_dir="$(mktemp -d /var/tmp/glintfx-coletar-selftest.XXXXXX)"
  trap 'rm -rf -- "$work_dir"' RETURN

  echo "=== SELFTEST coletar-resultados.sh (E6, E8, SEGURANCA) -- diretorio de trabalho: ${work_dir} ==="
  echo
  selftest_e6 "$work_dir" || ok=0
  echo
  selftest_e8 "$work_dir" || ok=0
  echo
  selftest_seguranca_nomes "$work_dir" || ok=0
  echo

  if [ "$ok" -eq 1 ]; then
    echo "SELFTEST OK: E6, E8 e SEGURANCA se comportaram como esperado."
    return 0
  fi
  echo "SELFTEST FALHOU: pelo menos um cenario nao se comportou como esperado."
  return 1
}

# --- despacho ------------------------------------------------------------------

uso() {
  echo "Uso: $0 <diretorio-windows> <diretorio-destino-local> [--chunk-bytes N] [--prazo N] | --selftest" >&2
  exit 2
}

if [ "${1:-}" = "--selftest" ]; then
  selftest
  exit $?
fi

if [ $# -lt 2 ]; then
  uso
fi

DIRETORIO_WIN="$1"
DESTINO_LOCAL="$2"
shift 2
CHUNK_BYTES="$CHUNK_BYTES_PADRAO"
PRAZO="$PRAZO_PADRAO"

while [ $# -gt 0 ]; do
  case "$1" in
    --chunk-bytes) CHUNK_BYTES="${2:?}"; shift 2 ;;
    --prazo) PRAZO="${2:?}"; shift 2 ;;
    -h|--help) uso ;;
    *) uso ;;
  esac
done

coletar_diretorio "$DIRETORIO_WIN" "$DESTINO_LOCAL" "$CHUNK_BYTES" "$PRAZO"
exit $?
