# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/msvc-container/win-wine-toolchain.cmake
#
# RASCUNHO, NAO PRONTO (GODS_LAWS.md L-40): medido em 07/09/2026 que
# `cmake -S . -B <dir> -G Ninja` com este toolchain TRAVA numa sonda de
# deteccao do compilador (`cmTC_*`, ninja), com `mspdbsrv.exe`/
# `explorer.exe` pendurados sem progresso por mais de 7 minutos - achado
# operacional (sincronizacao do Wine), nao de incompatibilidade binaria.
# O caminho que funciona hoje e' invocar `cl.exe`/`link.exe` direto por
# arquivo (README.md deste diretorio, secao "Ligacao"), nao via
# `cmake --build` do projeto inteiro. Mantido como rascunho porque a
# mecanica (CMAKE_SYSTEM_NAME=Windows de verdade) continua correta;
# falta resolver o travamento do configure antes de declarar pronto.
#
# HIPOTESE DE CONSERTO, AINDA NAO TESTADA (proposta pelo team-lead,
# 07/09/2026): a sonda que trava (`cmTC_*`) por padrao COMPILA E LIGA um
# executavel para provar que o compilador funciona - e' a LIGACAO da
# sonda que chama `mspdbsrv.exe` (servidor do banco de simbolos), que
# fica pendurado esperando um cliente que a emulacao nao fecha direito.
# CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY faz a sonda parar em
# `/c` (compila, arquiva com `lib.exe`, nunca liga) - `mspdbsrv` nunca
# seria chamado. Medido (grep) que este projeto NAO usa essa variavel
# em lugar nenhum dos `.cmake`/CMakeLists.txt - sem colisao. Vale tentar
# tambem, se isto sozinho nao bastar, trocar `/Zi` por `/Z7` nas flags
# de debug (embute o banco de simbolos no proprio .obj, em vez de um
# arquivo `.pdb` separado que tambem depende do `mspdbsrv`). NENHUMA
# das duas linhas abaixo foi provada ainda - primeira coisa a tentar
# quando o slot pesado abrir, antes de qualquer contorno manual (script
# de ligacao manual com um `export.hpp` escrito a mao como plano B,
# fora da arvore rastreada, exatamente PORQUE isto ainda nao rodou).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
#
# GODS_LAWS.md L-09/L-68 (ordem do lider, 07/09/2026, "matriz de
# roteamento" - README.md deste diretorio): compilar e ligar sao do
# container, sempre; EXECUTAR um binario (o que CMAKE_CROSSCOMPILING_
# EMULATOR abaixo faria o `ctest` fazer via `wine64`) nunca e' oraculo
# aqui - o Wine e' reimplementacao, nao o Windows real. Portanto, se
# este toolchain um dia passar a configurar de verdade,
# CMAKE_CROSSCOMPILING_EMULATOR serve so de PISTA DE DIAGNOSTICO -
# nunca pode virar gate/portao que reprova onda, e nenhum agente deve
# registrar um `ctest` assim como prova de que algo "passa no Windows".
#
# Toolchain de CMake para configurar o glintfx MIRANDO Windows, usando o
# cl.exe REAL da Microsoft que ja roda sob Wine dentro da imagem
# glintfx-msvc:latest (ver README.md deste diretorio). Isto e' um
# cross-compile de verdade do ponto de vista do CMake: o host que roda o
# `cmake`/`ninja` e Linux, mas CMAKE_SYSTEM_NAME=Windows faz o CMake
# tratar o alvo como Windows de fato (a variavel WIN32 fica TRUE dentro
# do projeto, exatamente como um build nativo no windows-latest do CI -
# nenhum #if do lado Windows precisa de gambiarra).
#
# `cl`/`link`/`lib`/`rc` resolvem via PATH (a imagem ja exporta
# /opt/msvc/bin/x64 - ver Dockerfile.full deste diretorio) para os
# wrappers do msvc-wine, que por sua vez chamam `wine64 cl.exe` etc. por
# baixo - nao e' preciso apontar caminho absoluto aqui.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(CMAKE_C_COMPILER cl)
set(CMAKE_CXX_COMPILER cl)
set(CMAKE_RC_COMPILER rc)
set(CMAKE_LINKER link)
set(CMAKE_AR lib)

# O executavel de teste que o CMake produz durante a deteccao do
# compilador e' um PE de Windows; o host Linux nao consegue rodar isso
# diretamente, so o `wine64` consegue. Sem isto, a deteccao inicial do
# compilador (que por padrao TENTA RODAR o executavel-sonda) falha antes
# mesmo de chegar ao projeto.
set(CMAKE_CROSSCOMPILING_EMULATOR wine64)

# Evita a MSVC runtime library debug/release mismatch checks tentando
# usar find_library em caminhos de host Linux por engano.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
