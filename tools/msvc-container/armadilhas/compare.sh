#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# tools/msvc-container/armadilhas/compare.sh
#
# Roda cada arquivo desta pasta pelos dois compiladores - o cruzado GNU
# (x86_64-w64-mingw32-g++, ja presente nesta maquina) e o cl.exe REAL
# rodando na imagem glintfx-msvc:latest - e imprime os dois lados lado a
# lado. GODS_LAWS.md L-36: nao aceitar autorrelato, mostrar a saida
# literal de cada lado.
#
# Uso: tools/msvc-container/armadilhas/compare.sh
set -u
cd "$(dirname "$0")"

if ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
    echo "ERRO: x86_64-w64-mingw32-g++ nao encontrado nesta maquina (compilador alternativo)." >&2
    exit 1
fi
if ! docker image inspect glintfx-msvc:latest >/dev/null 2>&1; then
    echo "ERRO: imagem glintfx-msvc:latest nao existe. Rode o passo 1 e 2 do README.md primeiro." >&2
    exit 1
fi

encontrados=0
analisados=0

for arquivo in *.cpp; do
    encontrados=$((encontrados + 1))
    echo "############################################################"
    echo "### $arquivo"
    echo "############################################################"

    echo "--- MinGW cruzado (alternativo, ja presente nesta maquina) ---"
    x86_64-w64-mingw32-g++ -std=c++23 -Wall -Wextra -c "$arquivo" -o "/tmp/$(basename "$arquivo").mingw.o" 2>&1
    ec_mingw=$?
    echo "EXIT_CODE(mingw)=$ec_mingw"
    echo ""

    echo "--- cl.exe REAL (glintfx-msvc:latest) ---"
    docker run --rm -v "$(pwd):/arm:ro" glintfx-msvc:latest \
        bash -c "cd /arm && cl /nologo /std:c++latest /Zc:__cplusplus /EHsc /W4 /c '$arquivo' /Fo/tmp/$(basename "$arquivo").obj" 2>&1
    ec_cl=$?
    echo "EXIT_CODE(cl)=$ec_cl"
    echo ""
    analisados=$((analisados + 1))
done

echo "encontrados=$encontrados analisados=$analisados"
[ "$encontrados" -eq "$analisados" ] || { echo "ABORTA: nem todo arquivo encontrado foi analisado"; exit 1; }
