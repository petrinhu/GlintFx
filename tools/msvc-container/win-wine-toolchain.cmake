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
# DUAS HIPOTESES DE CONSERTO TESTADAS EM 07/09/2026, AS DUAS FALHARAM
# (propostas pelo team-lead, causa nomeada: a sonda `cmTC_*` por padrao
# COMPILA E LIGA um executavel, e e' a LIGACAO que chama `mspdbsrv.exe`,
# pendurado esperando um cliente que a emulacao nao fecha direito):
# (1) CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY (sonda para em `/c`,
# nunca liga) - MEDIDO: nao bastou, `mspdbsrv.exe` apareceu do mesmo
# jeito na sonda de deteccao de ABI seguinte, mesmo sem ligar. (2) `/Z7`
# no lugar de `/Zi` (evita o `.pdb` compartilhado) somado a (1) - MEDIDO:
# tambem nao bastou, mesmo sintoma identico. A causa exata que restou
# sem investigar (nao ha' tempo dentro da ordem de tentativa combinada)
# e' mais funda que PDB compartilhado; as duas linhas ficam porque nao
# atrapalham em nada, mas NENHUMA delas destrava o configure sozinha.
# O caminho que FUNCIONA, provado de ponta a ponta (README.md deste
# diretorio, secao "Ligacao"): compilar/ligar por ARQUIVO, direto com
# `cl`/`link.exe`, usando um `export.hpp` escrito a mao (nao o gerado
# pelo CMake, que este travamento impede de obter) - validado contra
# os 13 testes win32_*/wgl_proc_address_test (13/13 compilam E ligam) e
# contra a biblioteca inteira (`glintfx.dll`+`glintfx.lib`, 71 exports
# corretos, imports batendo com `tools/ci/check-dep-zero-win.ps1`).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_FLAGS_INIT "/Z7")
set(CMAKE_CXX_FLAGS_INIT "/Z7")
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
