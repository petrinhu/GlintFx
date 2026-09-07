#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Transfere um arquivo para o convidado via qemu-guest-agent, em blocos, e
# fecha o handle. Uso: transferir.sh <origem-hospedeiro> <destino-guest-windows>
set -eu
DOM="glintfx-win11-lab"
CONNECT="qemu:///session"
ORIGEM="$1"
DESTINO_WIN="$2"   # caminho estilo Windows, ex: C:\\Users\\glintfx\\arquivo.exe
CHUNK_BYTES=65536

TAMANHO=$(stat -c%s "$ORIGEM")
echo "  transferindo ${ORIGEM} (${TAMANHO} bytes) -> convidado:${DESTINO_WIN}, em blocos de ${CHUNK_BYTES}"

OPEN_RES=$(virsh -c "$CONNECT" qemu-agent-command "$DOM" \
  "$(jq -n --arg p "$DESTINO_WIN" '{execute:"guest-file-open",arguments:{path:$p,mode:"wb"}}')" 2>&1)
HANDLE=$(echo "$OPEN_RES" | jq -r '.return')
if ! [[ "$HANDLE" =~ ^[0-9]+$ ]]; then
  echo "ERRO ao abrir arquivo no convidado: $OPEN_RES" >&2
  exit 1
fi

OFFSET=0
TOTAL_ESCRITO=0
while [ "$OFFSET" -lt "$TAMANHO" ]; do
  B64=$(dd if="$ORIGEM" bs=1 skip="$OFFSET" count="$CHUNK_BYTES" 2>/dev/null | base64 -w0)
  WRITE_RES=$(virsh -c "$CONNECT" qemu-agent-command "$DOM" \
    "$(jq -n --argjson h "$HANDLE" --arg b "$B64" '{execute:"guest-file-write",arguments:{handle:$h,"buf-b64":$b}}')" 2>&1)
  COUNT=$(echo "$WRITE_RES" | jq -r '.return.count // empty')
  if [ -z "$COUNT" ]; then
    echo "ERRO na escrita em offset ${OFFSET}: $WRITE_RES" >&2
    virsh -c "$CONNECT" qemu-agent-command "$DOM" "$(jq -n --argjson h "$HANDLE" '{execute:"guest-file-close",arguments:{handle:$h}}')" >/dev/null 2>&1 || true
    exit 1
  fi
  TOTAL_ESCRITO=$((TOTAL_ESCRITO + COUNT))
  OFFSET=$((OFFSET + CHUNK_BYTES))
done

virsh -c "$CONNECT" qemu-agent-command "$DOM" "$(jq -n --argjson h "$HANDLE" '{execute:"guest-file-close",arguments:{handle:$h}}')" >/dev/null

echo "  bytes escritos confirmados pelo protocolo: ${TOTAL_ESCRITO} (esperado ${TAMANHO})"
if [ "$TOTAL_ESCRITO" -ne "$TAMANHO" ]; then
  echo "ERRO: contagem de bytes escritos nao bate com o tamanho do arquivo de origem." >&2
  exit 1
fi
