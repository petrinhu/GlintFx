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
# "rejeitados=N", e o script sai com o codigo 3 (proprio, distinto de
# 0/1/2 - ver a tabela completa abaixo) se algum nome foi rejeitado, mesmo
# que os demais arquivos tenham sido coletados com sucesso.
#
# LIMITE DE BYTES POR CHAMADA guest-file-read: MEDIDO em 23/09/2026, contra
# a maquina real (`/var/tmp/glintfx-plan/win-lab-estreia/V5-RELATORIO.md`,
# secao "P4, em detalhe" - sub-fatia V-5, sessao 4). Sequencia testada:
# 65536, 1 MiB e 2 MiB (2.097.152 bytes) ACEITOS, com `return.count` batendo
# e nenhum erro; 4 MiB (4.194.304 bytes) RECUSADO pelo RPC do libvirt com
# "Unable to encode message payload" - o campo que carrega o `buf-b64` da
# resposta bate no teto de uma STRING individual dentro da mensagem RPC,
# `VIR_NET_MESSAGE_STRING_MAX = 4194304`, definido em
# `src/rpc/virnetprotocol.x` do libvirt
# (https://github.com/libvirt/libvirt/blob/master/src/rpc/virnetprotocol.x),
# com o comentario *"This is an arbitrary limit designed to stop the decoder
# from trying to allocate unbounded amounts of memory when fed with a bad
# message"*. O default abaixo (CHUNK_BYTES_PADRAO) passa a ser o MAIOR
# tamanho TESTADO que passou limpo, **2.097.152 bytes (2 MiB)** - nao mais
# palpite. A fronteira teorica exata (base64 de 3.145.728 bytes crus cruza
# os 4.194.304 do STRING_MAX) fica so' como pista para quem quiser apertar
# o numero depois; nao foi testada diretamente, e por isso nao vira o
# default. `--chunk-bytes` continua existindo para quem precisar de outro
# valor, mas o corpo do script deixa de citar um numero nao verificado.
#
# Codigos de saida (achado do team-lead, 22/09/2026: o codigo 2 chegou a
# significar DUAS coisas - uso incorreto E rejeicao de nome hostil -, e
# quem le o codigo de saida nao distinguia as duas; mesma familia do
# defeito "CANAL-QUEBRADO-SE-DISFARCA-DE-PRAZO-ESTOURADO" ja catalogado
# neste projeto: dois fatos, um sinal so. Consertado dando um codigo
# PROPRIO a cada fato, nunca reaproveitando um numero que outro caso ja
# usa neste MESMO arquivo):
#   0   sucesso - tudo encontrado foi coletado e conferido por md5
#   1   diretorio vazio (piso de varredura), pelo menos um nome VALIDO
#       nao bateu md5, OU a listagem/pedido de hash falhou por um motivo
#       GENERICO (o comando rodou dentro do convidado mas terminou com
#       erro - ver a mensagem impressa, que traz o texto do Windows)
#   2   USO INCORRETO (argumento faltando/invalido) - mesma convencao que
#       rodar-caminho.sh/rodar-um.sh (mesma pasta) ja usam para uso
#       incorreto; nunca reaproveitado para outro fato
#   3   pelo menos um nome REJEITADO pela lista de permissao (travessia de
#       caminho ou nome hostil - nunca vira caminho no hospedeiro) - EVENTO
#       DE SEGURANCA, distinto de erro de digitacao de quem chamou
#   5   SAIDA TRUNCADA (D-7, docs/plano-fecho-w7b.md, achado do
#       team-lead, 23/09/2026 - mesmo defeito de rodar-caminho.sh/
#       rodar-um.sh, mesma pasta): a LISTAGEM ou o PEDIDO DE HASH
#       devolveram `out-truncated`/`err-truncated` = true na resposta de
#       guest-exec-status - o TEXTO capturado (nomes de arquivo, ou o
#       hash extraido do certutil) pode ter vindo cortado, e nenhum
#       veredito tirado dele e' confiavel. MESMO NUMERO que os irmaos
#       (5) por escolha deliberada - este arquivo nunca usou 5 para
#       outro fato, entao nao ha colisao
#   6   CANAL NAO RESPONDEU (D-8, mesmo achado, mesma pasta): a consulta
#       a guest-exec-status falhou (virsh devolveu erro, ou JSON sem
#       campo `.return`) - piso de tentativas seguidas, ou nenhuma
#       resposta valida ate o prazo esgotar. Distinto do 1 generico
#       (que exige que o comando tenha RODADO e TERMINADO, com ou sem
#       erro, dentro do convidado) e distinto de "ESTOUROU o prazo" (que
#       exige que o canal tenha RESPONDIDO com sucesso pelo menos uma
#       vez). MESMO NUMERO que os irmaos (6), mesma razao
#
# Uso:
#   coletar-resultados.sh <diretorio-windows> <diretorio-destino-local>
#                          [--chunk-bytes N] [--prazo N]
#   coletar-resultados.sh --selftest
set -u
set -o pipefail

DOM="glintfx-win11-lab"
CONNECT="qemu:///session"
CHUNK_BYTES_PADRAO=2097152   # 2 MiB - medido (V-5, D-6(a)), ver o paragrafo acima.
PRAZO_PADRAO=60

# Piso de tentativas (D-8), mesmo valor e mesma razao de rodar-caminho.sh/
# rodar-um.sh (mesma pasta): quantas consultas SEGUIDAS a guest-exec-status
# tem de falhar antes de _guest_exec_aguardar declarar o canal quebrado
# (codigo 6) em vez de esperar o PRAZO inteiro - nao declara por uma
# UNICA falha isolada.
PISO_FALHAS_CONSULTA=3

# Caminho absoluto do proprio script - usado so' pelo selftest de codigos
# distintos (selftest_codigos_distintos) para invocar o SCRIPT COMO
# SUBPROCESSO no caminho de uso() (que faz `exit 2` direto, nao `return`;
# so' testavel de fora, nao chamando a funcao interna).
SCRIPT_PATH="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)/$(basename -- "${BASH_SOURCE[0]}")"

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
  local falhas_consulta=0 consulta_teve_sucesso="false" out_truncado err_truncado

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

    # D-8, mesmo criterio de rodar-caminho.sh/rodar-um.sh (mesma pasta):
    # uma consulta so' conta como "respondeu" se virou JSON valido COM o
    # campo `.return` - erro do virsh ou `{"error":...}` do QEMU nao tem
    # esse campo, e os dois casos hoje se disfarçavam de "exited:false".
    if echo "$status_res" | jq -e '.return != null' >/dev/null 2>&1; then
      falhas_consulta=0
      consulta_teve_sucesso="true"
      exited=$(echo "$status_res" | jq -r '.return.exited // false')
      [ "$exited" = "true" ] && break
    else
      falhas_consulta=$((falhas_consulta + 1))
      if [ "$falhas_consulta" -ge "$PISO_FALHAS_CONSULTA" ]; then
        _GE_RC=6
        _GE_OUT=""
        _GE_ERR="canal nao respondeu: ${falhas_consulta} consultas seguidas ao agente falharam (piso de tentativas atingido, D-8). Ultima resposta: ${status_res}"
        return 6
      fi
    fi
  done

  if [ "$exited" != "true" ]; then
    if [ "$consulta_teve_sucesso" != "true" ]; then
      # o prazo esgotou sem que UMA UNICA consulta obtivesse resposta
      # valida - o fato e' canal quebrado, nunca "processo ainda rodando".
      _GE_RC=6
      _GE_OUT=""
      _GE_ERR="canal nao respondeu: nenhuma consulta ao agente obteve resposta valida dentro do prazo de ${prazo}s. Ultima resposta: ${status_res}"
      return 6
    fi
    _GE_RC=124
    _GE_OUT=""
    _GE_ERR="estourou o prazo de ${prazo}s"
    return 124
  fi

  # D-7: truncamento se checa ANTES do exitcode - se a captura veio
  # cortada, o TEXTO nao e' confiavel mesmo que o exitcode em si seja.
  out_truncado=$(echo "$status_res" | jq -r '.return["out-truncated"] // false')
  err_truncado=$(echo "$status_res" | jq -r '.return["err-truncated"] // false')
  if [ "$out_truncado" = "true" ] || [ "$err_truncado" = "true" ]; then
    _GE_RC=5
    _GE_OUT=""
    _GE_ERR="saida truncada: out-truncated=${out_truncado} err-truncated=${err_truncado} (D-7) - a captura de stdout/stderr do convidado nao veio inteira"
    return 5
  fi

  exitcode=$(echo "$status_res" | jq -r '.return.exitcode // "DESCONHECIDO"')
  _GE_OUT=$(echo "$status_res" | jq -r '.return["out-data"] // empty' | base64 -d 2>/dev/null)
  _GE_ERR=$(echo "$status_res" | jq -r '.return["err-data"] // empty' | base64 -d 2>/dev/null)
  _GE_RC="$exitcode"
  [ "$exitcode" = "0" ] && return 0
  return 3
}

# ROTULO POR CODIGO, NUNCA LISTA DE PROIBICAO NOS CHAMADORES (achado do
# team-lead, 22/09/2026): os dois chamadores de _guest_exec_aguardar
# tratavam como erro SO' 1 e 124 - `[ "$rc" -eq 1 ] || [ "$rc" -eq 124 ]` -
# e o 3 (comando rodou DENTRO do convidado, mas terminou com erro, ex.:
# `dir` numa pasta que nao existe) passava direto pro resto do codigo,
# que seguia analisando a saida de um comando que tinha FALHADO. Provado
# com dublê: `dir` numa pasta inexistente (exitcode=1, err-data=
# "File Not Found") virava "encontrados=0 coletados=0" - "pasta vazia",
# quando o fato real era "a listagem falhou" e a mensagem do Windows
# desaparecia. Mesma familia de CANAL-QUEBRADO-SE-DISFARCA-DE-PRAZO-
# ESTOURADO: dois fatos (vazio genuino x listagem falhou) escondidos atras
# do mesmo sinal.
#
# CORRECAO: tratar como erro TUDO que nao for 0 (`[ "$rc" -ne 0 ]`), nunca
# aumentar a lista de codigos aceitos - lista de proibicao esquece o
# proximo codigo que alguem inventar. Esta funcao so' da' o ROTULO certo
# para cada codigo, sempre incluindo _GE_ERR (o que o convidado disse) na
# mensagem - nunca engolido.
_rotulo_rc_guest_exec() {
  case "$1" in
    1) echo "falha ao INICIAR o comando no convidado" ;;
    124) echo "ESTOUROU o prazo" ;;
    3) echo "o comando TERMINOU com erro dentro do convidado (codigo de saida do convidado: ${_GE_RC})" ;;
    5) echo "SAIDA TRUNCADA - a captura de stdout/stderr nao veio inteira (D-7)" ;;
    6) echo "CANAL NAO RESPONDEU - a consulta ao agente falhou (D-8)" ;;
    *) echo "codigo de retorno desconhecido (${1})" ;;
  esac
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

  if [ "$rc" -ne 0 ]; then
    echo "ERRO: a LISTAGEM de '${diretorio_win}' no convidado FALHOU - $(_rotulo_rc_guest_exec "$rc"). Mensagem do convidado: ${_GE_ERR}" >&2
    # D-7/D-8: os dois fatos novos (5 truncado, 6 canal quebrado) tem
    # sinal PROPRIO - nunca decaem para o 1 generico que os demais
    # motivos de falha de _guest_exec_aguardar ainda usam.
    if [ "$rc" -eq 5 ] || [ "$rc" -eq 6 ]; then
      return "$rc"
    fi
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
  if [ "$rc" -ne 0 ]; then
    echo "ERRO: o PEDIDO DE HASH remoto de '${caminho_win}' FALHOU - $(_rotulo_rc_guest_exec "$rc"). Mensagem do convidado: ${_GE_ERR}" >&2
    # D-7/D-8: mesmo criterio de _dir_listar (mesmo arquivo) - 5 e 6 tem
    # sinal proprio, nunca decaem para o 1 generico.
    if [ "$rc" -eq 5 ] || [ "$rc" -eq 6 ]; then
      return "$rc"
    fi
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
  local hash_remoto hash_local rc_hash

  hash_remoto=$(_hash_remoto "$caminho_win" "$prazo")
  rc_hash=$?
  if [ "$rc_hash" -ne 0 ]; then
    # propaga 5/6 distintos (D-7/D-8); qualquer outro motivo continua 1.
    return "$rc_hash"
  fi

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
# codigo de saida (3), distinto de 1 (vazio / md5 nao bateu) e de 2 (uso
# incorreto - ver a tabela de codigos no cabecalho do arquivo).
coletar_diretorio() {
  local diretorio_win="$1" destino_dir_local="$2" chunk_bytes="$3" prazo="$4"
  local encontrados rejeitados coletados=0 arquivo rc_listar rc_arquivo codigo_especial=0

  mkdir -p -- "$destino_dir_local"

  _dir_listar "$diretorio_win" "$prazo"
  rc_listar=$?
  if [ "$rc_listar" -ne 0 ]; then
    # D-7/D-8: propaga 5/6 quando a LISTAGEM em si e' quem carrega o
    # fato especifico (nunca decai para o 1 generico que os outros
    # motivos ainda usam).
    return "$rc_listar"
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
    else
      rc_arquivo=$?
      # D-7/D-8: o fato mais especifico (5/6) nunca fica escondido atras
      # do "so N de M coletados" generico - guarda o ultimo visto para
      # decidir o codigo de saida DEPOIS da linha de contagem impressa
      # (GODS_LAWS.md global L-40: contagem sempre sai, mesmo quando o
      # motivo real e' mais especifico que ela).
      if [ "$rc_arquivo" -eq 5 ] || [ "$rc_arquivo" -eq 6 ]; then
        codigo_especial="$rc_arquivo"
      fi
    fi
  done

  echo "encontrados=${encontrados} coletados=${coletados} rejeitados=${rejeitados}"

  if [ "$codigo_especial" -ne 0 ]; then
    echo "REPROVADO: pelo menos um arquivo teve um problema mais especifico que 'nao coletado' - $(_rotulo_rc_guest_exec "$codigo_especial")." >&2
    return "$codigo_especial"
  fi
  if [ "$rejeitados" -gt 0 ]; then
    echo "REPROVADO: pelo menos um nome hostil foi rejeitado - nunca sucesso silencioso com menos arquivos do que o convidado devolveu (GODS_LAWS.md global L-40)." >&2
    return 3
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
    # V-5d (BLOCO-PADRAO-2MIB, achado do team-lead 23/09/2026: a prova
    # anterior conferia o VALOR da constante, nunca o USO - grava cada
    # count PEDIDO DE VERDADE, para quem chama poder conferir o que a
    # producao realmente mandou, nao o que a constante diz que deveria
    # mandar).
    echo "$COUNT" >>"${STATE_DIR}/counts-recebidos"
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
    # V-5d (BLOCO-PADRAO-2MIB, achado do team-lead 23/09/2026: a prova
    # anterior conferia o VALOR da constante, nunca o USO - grava cada
    # count PEDIDO DE VERDADE, para quem chama poder conferir o que a
    # producao realmente mandou, nao o que a constante diz que deveria
    # mandar).
    echo "$COUNT" >>"${STATE_DIR}/counts-recebidos"
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

  printf '%s\n' ">>> SEGURANCA (travessia de caminho, achado do team-lead 22/09/2026): listagem devolve 1 nome legitimo + 2 nomes HOSTIS ('../../escapou', 'sub\\dir\\arquivo') - esperado: os hostis sao rejeitados (nunca viram caminho), o legitimo e' coletado, codigo de saida 3 (proprio, distinto do 2 de uso incorreto), NADA escrito fora de '${destino}'"
  saida="$(PATH="${stub_dir}:$PATH" DUBLE_STATE_DIR="$state_dir" coletar_diretorio 'C:\Users\glintfx\resultados' "$destino" 8 2 2>&1)"
  rc=$?
  echo "$saida"
  echo ">>> codigo obtido: ${rc}"

  if [ "$rc" -ne 3 ]; then
    echo "SEGURANCA FALHOU: esperava codigo 3 (rejeicao de nome hostil), obteve ${rc}."
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

# Achado do team-lead, 22/09/2026: o codigo 2 chegou a significar DUAS
# coisas neste arquivo - uso incorreto (uso(), `exit 2` direto) e rejeicao
# de nome hostil (coletar_diretorio, ANTES do conserto tambem `return 2`) -
# a mesma familia de "dois fatos, um sinal so" ja catalogada neste projeto
# (CANAL-QUEBRADO-SE-DISFARCA-DE-PRAZO-ESTOURADO). Este controle prova que
# os dois codigos SAO DISTINTOS: chama o SCRIPT como subprocesso sem
# argumento nenhum (caminho de uso()) e compara contra a rejeicao de nome
# hostil (via coletar_diretorio, no processo) - se os dois codigos forem
# iguais, o controle reprova.
selftest_codigos_distintos() {
  local work_dir="$1" stub_dir state_dir saida rc_uso rc_rejeicao ok=1

  echo ">>> CODIGOS-DISTINTOS 1/2: uso incorreto (script chamado sem argumento nenhum)"
  "$SCRIPT_PATH" >/dev/null 2>&1
  rc_uso=$?
  echo ">>> codigo obtido (uso incorreto): ${rc_uso}"

  echo ">>> CODIGOS-DISTINTOS 2/2: rejeicao de nome hostil (mesmo dublê do cenario SEGURANCA)"
  stub_dir="${work_dir}/cod-bin"
  state_dir="${work_dir}/cod-state"
  mkdir -p "$stub_dir" "$state_dir"
  printf 'conteudo' >"${state_dir}/conteudo.bin"
  escrever_duble_virsh_nomes_hostis "$stub_dir"
  saida="$(PATH="${stub_dir}:$PATH" DUBLE_STATE_DIR="$state_dir" coletar_diretorio 'C:\Users\glintfx\resultados' "${work_dir}/cod-destino" 8 2 2>&1)"
  rc_rejeicao=$?
  echo ">>> codigo obtido (rejeicao de nome hostil): ${rc_rejeicao}"

  if [ "$rc_uso" -eq "$rc_rejeicao" ]; then
    echo "CODIGOS-DISTINTOS FALHOU: uso incorreto (${rc_uso}) e rejeicao de nome hostil (${rc_rejeicao}) colidem no MESMO codigo - quem le o codigo de saida nao consegue distinguir os dois fatos."
    ok=0
  else
    echo "CODIGOS-DISTINTOS OK: uso incorreto=${rc_uso}, rejeicao de nome hostil=${rc_rejeicao} - codigos distintos, cada fato com o seu proprio sinal."
  fi

  [ "$ok" -eq 1 ] && return 0
  return 1
}

# Achado do team-lead, 22/09/2026, ao varrer TODOS os return/exit do
# arquivo atras de colisao: _dir_listar e _hash_remoto tratavam como erro
# SO' rc=1 e rc=124 de _guest_exec_aguardar - o rc=3 (comando RODOU dentro
# do convidado mas terminou com erro, ex.: `dir` numa pasta que nao
# existe) passava direto, e o resto do codigo seguia analisando a saida
# de um comando que tinha FALHADO. Nao era falso verde (ainda saia
# diferente de zero), mas o DIAGNOSTICO MENTIA: "pasta vazia" em vez de
# "a listagem falhou", com a mensagem do Windows (err-data) desaparecendo
# - mesma familia de CANAL-QUEBRADO-SE-DISFARCA-DE-PRAZO-ESTOURADO.
escrever_duble_virsh_comando_falha() {
  local stub_dir="$1"
  cat >"${stub_dir}/virsh" <<'STUB'
#!/usr/bin/env bash
set -u
JSON="${*: -1}"
EXECUTE=$(echo "$JSON" | jq -r '.execute')
STATE_DIR="${DUBLE_STATE_DIR:?DUBLE_STATE_DIR nao definido}"
CENARIO="${DUBLE_FALHA:-listagem}"

case "$EXECUTE" in
  guest-exec)
    PATH_ARG=$(echo "$JSON" | jq -r '.arguments.path')
    case "$PATH_ARG" in
      *cmd.exe) echo "listar" >"${STATE_DIR}/tipo" ;;
      *certutil.exe) echo "hash" >"${STATE_DIR}/tipo" ;;
      *) echo "outro" >"${STATE_DIR}/tipo" ;;
    esac
    echo '{"return":{"pid":1}}'
    ;;
  guest-exec-status)
    TIPO=$(cat "${STATE_DIR}/tipo" 2>/dev/null || echo outro)
    if [ "$TIPO" = "listar" ] && [ "$CENARIO" = "listagem" ]; then
      # dir numa pasta que NAO EXISTE: o comando RODOU, terminou com erro.
      ERR_B64=$(printf 'File Not Found' | base64 -w0)
      printf '{"return":{"exited":true,"exitcode":1,"out-data":"","err-data":"%s"}}\n' "$ERR_B64"
    elif [ "$TIPO" = "listar" ]; then
      # cenario "hash": listagem tem de dar CERTO, com um nome valido, para
      # o fluxo chegar ate o pedido de hash.
      OUT_B64=$(printf 'resultado.log\r\n' | base64 -w0)
      printf '{"return":{"exited":true,"exitcode":0,"out-data":"%s","err-data":""}}\n' "$OUT_B64"
    elif [ "$TIPO" = "hash" ] && [ "$CENARIO" = "hash" ]; then
      # certutil FALHA (ex.: acesso negado ao arquivo).
      ERR_B64=$(printf 'Access is denied.' | base64 -w0)
      printf '{"return":{"exited":true,"exitcode":1,"out-data":"","err-data":"%s"}}\n' "$ERR_B64"
    else
      printf '{"return":{"exited":true,"exitcode":0,"out-data":"","err-data":""}}\n'
    fi
    ;;
  *) echo '{"return":{}}' ;;
esac
STUB
  chmod +x "${stub_dir}/virsh"
}

selftest_diagnostico_falha_convidado() {
  local work_dir="$1" stub_dir state_dir saida rc ok=1

  stub_dir="${work_dir}/diag-bin"
  state_dir="${work_dir}/diag-state"
  mkdir -p "$stub_dir" "$state_dir"
  escrever_duble_virsh_comando_falha "$stub_dir"

  echo ">>> DIAGNOSTICO 1/2: a LISTAGEM falha dentro do convidado (pasta que nao existe, exitcode=1, 'File Not Found') - esperado: mensagem diz que a listagem FALHOU e mostra 'File Not Found', NUNCA 'nada para coletar' (isso mentiria: a pasta pode ate existir e ter arquivo, so' nao foi possivel saber)"
  saida="$(PATH="${stub_dir}:$PATH" DUBLE_STATE_DIR="$state_dir" DUBLE_FALHA=listagem coletar_diretorio 'C:\nao\existe' "${work_dir}/diag-destino-a" 8 2 2>&1)"
  rc=$?
  echo "$saida"
  echo ">>> codigo obtido: ${rc}"
  if printf '%s' "$saida" | grep -qi "nada para coletar\|piso de varredura"; then
    echo "DIAGNOSTICO 1/2 FALHOU: ainda diz 'nada para coletar'/'piso de varredura' quando o fato real e' que a listagem FALHOU."
    ok=0
  elif printf '%s' "$saida" | grep -q "FALHOU" && printf '%s' "$saida" | grep -q "File Not Found"; then
    echo "DIAGNOSTICO 1/2 OK: diz que a listagem FALHOU e mostra a mensagem do convidado ('File Not Found')."
  else
    echo "DIAGNOSTICO 1/2 FALHOU: nao diz claramente que a listagem falhou, ou engoliu a mensagem do convidado."
    ok=0
  fi

  echo
  echo ">>> DIAGNOSTICO 2/2: o CERTUTIL (pedido de hash) falha dentro do convidado (exitcode=1, 'Access is denied.') - esperado: mensagem diz que o PEDIDO DE HASH falhou e mostra a mensagem do convidado, nunca confundido com md5 que nao bate"
  saida="$(PATH="${stub_dir}:$PATH" DUBLE_STATE_DIR="$state_dir" DUBLE_FALHA=hash coletar_diretorio 'C:\Users\glintfx\resultados' "${work_dir}/diag-destino-b" 8 2 2>&1)"
  rc=$?
  echo "$saida"
  echo ">>> codigo obtido: ${rc}"
  if printf '%s' "$saida" | grep -q "PEDIDO DE HASH" && printf '%s' "$saida" | grep -q "Access is denied."; then
    echo "DIAGNOSTICO 2/2 OK: diz que o pedido de hash FALHOU e mostra a mensagem do convidado ('Access is denied.')."
  else
    echo "DIAGNOSTICO 2/2 FALHOU: nao identificou a falha do certutil separada de uma divergencia de md5."
    ok=0
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

# Duble DEDICADO de canal quebrado (D-8, achado do team-lead, 23/09/2026 -
# mesmo gemeo de rodar-caminho.sh/rodar-um.sh, mesma pasta): guest-exec
# inicia normalmente (devolve pid), mas TODA consulta de guest-exec-status
# falha com o MESMO texto que o QEMU emite quando o agente convidado nao
# responde (TODO.md:149) - nunca JSON valido, nunca campo `.return`.
escrever_duble_virsh_canal_quebrado() {
  local stub_dir="$1"
  cat >"${stub_dir}/virsh" <<'STUB'
#!/usr/bin/env bash
set -u
JSON="${*: -1}"
EXECUTE=$(echo "$JSON" | jq -r '.execute')
case "$EXECUTE" in
  guest-exec)
    echo '{"return":{"pid":6001}}'
    ;;
  guest-exec-status)
    echo "error: Guest agent is not responding: QEMU guest agent is not connected" >&2
    exit 1
    ;;
  *) echo '{"return":{}}' ;;
esac
STUB
  chmod +x "${stub_dir}/virsh"
}

selftest_canal_quebrado() {
  local work_dir="$1" stub_dir saida rc ok=1

  stub_dir="${work_dir}/canal-bin"
  mkdir -p "$stub_dir"
  escrever_duble_virsh_canal_quebrado "$stub_dir"

  echo ">>> CANAL-NAO-RESPONDEU, D-8: toda consulta a guest-exec-status falha (prazo=5s, maior que o piso de tentativas=${PISO_FALHAS_CONSULTA}, para provar o corte precoce) - esperado: codigo 6, NUNCA o 1 generico nem 'ESTOUROU o prazo'"
  saida="$(PATH="${stub_dir}:$PATH" coletar_diretorio 'C:\Users\glintfx\resultados' "${work_dir}/canal-destino" 8 5 2>&1)"
  rc=$?
  echo "$saida"
  echo ">>> codigo obtido: ${rc}"

  if [ "$rc" -ne 6 ]; then
    echo "CANAL-NAO-RESPONDEU FALHOU: esperava codigo 6, obteve ${rc}."
    ok=0
  elif ! printf '%s' "$saida" | grep -qi "CANAL NAO RESPONDEU"; then
    echo "CANAL-NAO-RESPONDEU FALHOU: codigo certo (6), mas o rotulo 'CANAL NAO RESPONDEU' nao apareceu na mensagem."
    ok=0
  elif printf '%s' "$saida" | grep -qi "ESTOUROU o prazo"; then
    echo "CANAL-NAO-RESPONDEU FALHOU: a mensagem ainda diz 'ESTOUROU o prazo' - o canal quebrado nao pode se disfarcar de timeout."
    ok=0
  else
    echo "CANAL-NAO-RESPONDEU OK: codigo 6, rotulo correto, nunca confundido com estouro de prazo."
  fi

  [ "$ok" -eq 1 ] && return 0
  return 1
}

# Duble DEDICADO de saida truncada (D-7, mesmo achado, mesma pasta): a
# LISTAGEM roda e termina (exited:true, exitcode:0), mas com
# out-truncated:true - o nome do arquivo devolvido pode ter vindo cortado.
escrever_duble_virsh_truncado() {
  local stub_dir="$1"
  cat >"${stub_dir}/virsh" <<'STUB'
#!/usr/bin/env bash
set -u
JSON="${*: -1}"
EXECUTE=$(echo "$JSON" | jq -r '.execute')
case "$EXECUTE" in
  guest-exec)
    echo '{"return":{"pid":6002}}'
    ;;
  guest-exec-status)
    OUT_B64=$(printf 'resultado.log\r\n' | base64 -w0)
    printf '{"return":{"exited":true,"exitcode":0,"out-data":"%s","err-data":"","out-truncated":true}}\n' "$OUT_B64"
    ;;
  *) echo '{"return":{}}' ;;
esac
STUB
  chmod +x "${stub_dir}/virsh"
}

selftest_truncado() {
  local work_dir="$1" stub_dir saida rc ok=1

  stub_dir="${work_dir}/trunc-bin"
  mkdir -p "$stub_dir"
  escrever_duble_virsh_truncado "$stub_dir"

  echo ">>> SAIDA-TRUNCADA, D-7: a LISTAGEM devolve out-truncated=true - esperado: codigo 5, NUNCA sucesso silencioso com uma lista de nomes possivelmente cortada"
  saida="$(PATH="${stub_dir}:$PATH" coletar_diretorio 'C:\Users\glintfx\resultados' "${work_dir}/trunc-destino" 8 2 2>&1)"
  rc=$?
  echo "$saida"
  echo ">>> codigo obtido: ${rc}"

  if [ "$rc" -ne 5 ]; then
    echo "SAIDA-TRUNCADA FALHOU: esperava codigo 5, obteve ${rc}."
    ok=0
  elif ! printf '%s' "$saida" | grep -qi "SAIDA TRUNCADA"; then
    echo "SAIDA-TRUNCADA FALHOU: codigo certo (5), mas o rotulo 'SAIDA TRUNCADA' nao apareceu na mensagem."
    ok=0
  else
    echo "SAIDA-TRUNCADA OK: codigo 5, rotulo presente, nada coletado a partir de uma listagem que pode estar cortada."
  fi

  [ "$ok" -eq 1 ] && return 0
  return 1
}

# V-5d (docs/plano-fecho-w7b.md, D-6(a), decisao do team-lead 23/09/2026
# 20:48): o bloco padrao do coletor deixa de ser palpite (65536, nunca
# medido para LEITURA - o comentario de CHUNK_BYTES_PADRAO ja avisava
# disso havia semanas) e passa a ser o maior tamanho MEDIDO limpo contra a
# maquina real na V-5 (`/var/tmp/glintfx-plan/win-lab-estreia/
# V5-RELATORIO.md`, secao "P4, em detalhe"): 2.097.152 bytes (2 MiB).
# Tamanhos maiores (4 MiB) batem em VIR_NET_MESSAGE_STRING_MAX (constante
# do libvirt, `src/rpc/virnetprotocol.x`, valor 4194304) e o RPC recusa
# com "Unable to encode message payload" antes mesmo de chegar ao agente.
# CONFERE O USO, NAO SO O VALOR (achado do team-lead, 23/09/2026: uma
# prova que so olha a constante e' cega ao coletor IGNORAR a constante e
# continuar pedindo 65536 na chamada real - "afirma que mede e nao mede").
# SEGUNDO ACHADO do team-lead, na mesma tarde: a primeira versao desta
# prova chamava `_ler_arquivo_remoto` DIRETO, pulando o DESPACHO da CLI
# (o trecho final do script, que decide `CHUNK_BYTES="$CHUNK_BYTES_PADRAO"`
# quando `--chunk-bytes` nao e' passado) - um mutante que trocasse esse
# UM PONTO especifico (o despacho) por um literal continuaria passando,
# porque a prova nunca exercitava aquele codigo. Corrigido: chama o
# SCRIPT COMO SUBPROCESSO (`$SCRIPT_PATH`, mesmo padrao de
# selftest_codigos_distintos acima), sem `--chunk-bytes` nenhum, com o
# duble de virsh no PATH - exatamente como um usuario real chamaria.
#
# O duble de virsh (escrever_duble_virsh, acima) grava cada `count`
# PEDIDO DE VERDADE em `${STATE_DIR}/counts-recebidos`; esta prova
# compara o PRIMEIRO count recebido contra o numero ESPERADO fixo,
# 2097152 -- nunca contra a propria variavel (se a variavel regredisse a
# 65536, comparar contra ela mesma esconderia a regressao).
selftest_bloco_padrao_2mib() {
  local work_dir="$1" stub_dir state_dir conteudo destino_dir primeiro_count saida rc ok=1

  stub_dir="${work_dir}/bloco-bin"
  state_dir="${work_dir}/bloco-state"
  destino_dir="${work_dir}/bloco-destino"
  mkdir -p "$stub_dir" "$state_dir" "$destino_dir"
  escrever_duble_virsh "$stub_dir"

  conteudo="${state_dir}/conteudo.bin"
  head -c 3145728 /dev/urandom >"$conteudo"   # 3 MiB: maior que os 2 MiB do bloco padrao

  echo ">>> BLOCO-PADRAO-2MIB: o SCRIPT COMO SUBPROCESSO, sem --chunk-bytes, tem de PEDIR count=2097152 no primeiro guest-file-read (exercita o DESPACHO da CLI, nao so a funcao de leitura por dentro)"
  saida="$(DUBLE_STATE_DIR="$state_dir" PATH="${stub_dir}:$PATH" "$SCRIPT_PATH" 'C:\Users\glintfx\resultados' "$destino_dir" 2>&1)"
  rc=$?
  echo "$saida"
  echo ">>> rc do script (esperado 0): ${rc}"

  primeiro_count="$(head -1 "${state_dir}/counts-recebidos" 2>/dev/null)"
  echo "primeiro count PEDIDO DE VERDADE ao agente: ${primeiro_count} (esperado 2097152, literal fixo)"

  if [ "$rc" -ne 0 ]; then
    echo "BLOCO-PADRAO-2MIB FALHOU: o script (subprocesso) falhou (rc=${rc})."
    ok=0
  elif [ "$primeiro_count" != "2097152" ]; then
    echo "BLOCO-PADRAO-2MIB FALHOU: o primeiro count pedido foi '${primeiro_count}', nao 2097152 -- ou a constante regrediu, ou o despacho/a chamada ignora a constante e usa outro valor."
    ok=0
  else
    echo "BLOCO-PADRAO-2MIB OK: o despacho real, sem --chunk-bytes, pediu 2097152 de verdade."
  fi

  [ "$ok" -eq 1 ] && return 0
  return 1
}

selftest() {
  local work_dir ok=1
  work_dir="$(mktemp -d /var/tmp/glintfx-coletar-selftest.XXXXXX)"
  trap 'rm -rf -- "$work_dir"' RETURN

  echo "=== SELFTEST coletar-resultados.sh (E6, E8, SEGURANCA, CODIGOS-DISTINTOS, DIAGNOSTICO, CANAL-NAO-RESPONDEU, SAIDA-TRUNCADA, BLOCO-PADRAO-2MIB) -- diretorio de trabalho: ${work_dir} ==="
  echo
  selftest_e6 "$work_dir" || ok=0
  echo
  selftest_e8 "$work_dir" || ok=0
  echo
  selftest_seguranca_nomes "$work_dir" || ok=0
  echo
  selftest_codigos_distintos "$work_dir" || ok=0
  echo
  selftest_diagnostico_falha_convidado "$work_dir" || ok=0
  echo
  selftest_canal_quebrado "$work_dir" || ok=0
  echo
  selftest_truncado "$work_dir" || ok=0
  echo
  selftest_bloco_padrao_2mib "$work_dir" || ok=0
  echo

  if [ "$ok" -eq 1 ]; then
    echo "SELFTEST OK: E6, E8, SEGURANCA, CODIGOS-DISTINTOS, DIAGNOSTICO, CANAL-NAO-RESPONDEU, SAIDA-TRUNCADA e BLOCO-PADRAO-2MIB se comportaram como esperado."
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
