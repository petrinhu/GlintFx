#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Digita uma string no convidado via 'virsh send-key', um caractere por vez.
# So caracteres comuns de linha de comando (letras, numeros, espaco, alguns
# simbolos). Uso: type-string.sh <dominio> "texto a digitar"
set -eu
DOM="$1"
TEXT="$2"
CONNECT="qemu:///session"

send_char() {
  local c="$1"
  # Layout ativo no convidado e pt-BR (ABNT2): scancodes 'KEY_*' do
  # codeset linux sao posicoes FISICAS, e o layout ABNT2 remapeia varias
  # delas de forma diferente do layout US. Medido ao vivo, por 'echo' no
  # cmd.exe e leitura da tela (nao suposto):
  #   KEY_BACKSLASH (posicao US do '\') -> produz ']' em ABNT2
  #   KEY_102ND (tecla extra ISO, entre Shift esquerdo e Z)  -> produz '\'
  #   KEY_LEFTSHIFT+KEY_SEMICOLON -> produz 'Ç' (ABNT2 tem Ç nessa posicao)
  #   KEY_LEFTSHIFT+KEY_SLASH -> produz ':' em ABNT2
  if [ "$c" = '\' ]; then
    virsh -c "$CONNECT" send-key "$DOM" --codeset linux KEY_102ND >/dev/null
    sleep 0.05
    return 0
  fi
  case "$c" in
    [a-z]) virsh -c "$CONNECT" send-key "$DOM" --codeset linux "KEY_${c^^}" >/dev/null ;;
    [A-Z]) virsh -c "$CONNECT" send-key "$DOM" --codeset linux KEY_LEFTSHIFT "KEY_${c}" >/dev/null ;;
    [0-9]) virsh -c "$CONNECT" send-key "$DOM" --codeset linux "KEY_${c}" >/dev/null ;;
    ' ') virsh -c "$CONNECT" send-key "$DOM" --codeset linux KEY_SPACE >/dev/null ;;
    ':') virsh -c "$CONNECT" send-key "$DOM" --codeset linux KEY_LEFTSHIFT KEY_SLASH >/dev/null ;;
    '.') virsh -c "$CONNECT" send-key "$DOM" --codeset linux KEY_DOT >/dev/null ;;
    ',') virsh -c "$CONNECT" send-key "$DOM" --codeset linux KEY_COMMA >/dev/null ;;
    '&') virsh -c "$CONNECT" send-key "$DOM" --codeset linux KEY_LEFTSHIFT KEY_7 >/dev/null ;;
    '%') virsh -c "$CONNECT" send-key "$DOM" --codeset linux KEY_LEFTSHIFT KEY_5 >/dev/null ;;
    '_') virsh -c "$CONNECT" send-key "$DOM" --codeset linux KEY_LEFTSHIFT KEY_MINUS >/dev/null ;;
    '-') virsh -c "$CONNECT" send-key "$DOM" --codeset linux KEY_MINUS >/dev/null ;;
    '=') virsh -c "$CONNECT" send-key "$DOM" --codeset linux KEY_EQUAL >/dev/null ;;
    *) echo "AVISO: caractere sem mapeamento: '$c' (pulado)" >&2 ;;
  esac
  sleep 0.05
}

len=${#TEXT}
for ((i=0; i<len; i++)); do
  send_char "${TEXT:$i:1}"
done
