#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/ci/prova_checkout.sh - CI-SPLIT-PER-OS D-A14 (item 1, G2
# estendida): prova que o checkout do job e' um clone git de verdade e
# nao o fallback REST API do actions/checkout (tarball, sem `.git`),
# que e' o que acontece quando o container ainda nao tem git no PATH.
# Fonte UNICA da prova para as 6 pernas Linux com `container:` (o job
# `linux` e os 5 fixos: lint, sanitizer, gl-codegen-host-cross, debug e
# clang) - tests/tools/check_ci_step_independence.py (G2) le o CONTEUDO
# deste arquivo, nao so' a chamada dele no ci.yml.
#
# Duas provas, cada uma imprime o proprio valor (GODS_LAWS.md L-40):
#   1. `git rev-parse --is-inside-work-tree` == "true";
#   2. `git rev-parse HEAD` == $GITHUB_SHA.
# safe.directory vem PRIMEIRO: dentro do container o processo e' root e
# o dono do diretorio e' o runner, e o git recusa por "dubious
# ownership" (CVE-2022-24765) antes de qualquer `rev-parse` rodar.
# Idempotente (`--add safe.directory` duas vezes nao tem efeito
# colateral), entao o passo "Configurar diretorio seguro do git" do job
# `linux` continua valendo como esta escrito.
#
# Roda so' DEPOIS do checkout que traz o codigo - o script mora no repo.

set -eu

git config --global --add safe.directory "$GITHUB_WORKSPACE"

inside="$(git rev-parse --is-inside-work-tree 2>&1)" \
    || { echo "FALHOU: git rev-parse --is-inside-work-tree: $inside"; exit 1; }
if [ "$inside" != "true" ]; then
    echo "FALHOU: --is-inside-work-tree devolveu '$inside', esperado 'true' - o checkout NAO e' um clone git real (provavel fallback REST API)"
    exit 1
fi

head="$(git rev-parse HEAD 2>&1)" \
    || { echo "FALHOU: git rev-parse HEAD: $head"; exit 1; }
if [ "$head" != "$GITHUB_SHA" ]; then
    echo "FALHOU: HEAD='$head', GITHUB_SHA='$GITHUB_SHA' - divergem"
    exit 1
fi

echo "inside-work-tree=$inside"
echo "HEAD=$head"
