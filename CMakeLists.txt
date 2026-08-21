# CMakeLists.txt — raiz do glintfx (FUND-1).
#
# Assunto único: orquestra os módulos de cmake/, o alvo da biblioteca em
# src/, o pacote instalável e a suíte de testes em tests/. Nenhum detalhe
# de opção, warning, propriedade de alvo ou install fica aqui — cada um
# tem módulo próprio em cmake/ (GODS_LAWS.md L-17: arquivo é átomo de
# assunto).

cmake_minimum_required(VERSION 3.28)

project(glintfx VERSION 0.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Diretório único onde os headers gerados (export.hpp, version_macros.hpp)
# nascem no build. Fica no escopo do diretório raiz para que src/ e a
# instalação leiam o mesmo caminho sem redeclará-lo.
set(GLINTFX_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated/include")

include(GlintfxOptions)
include(GlintfxCompileOptions)
include(GlintfxLibrary)
include(GlintfxInstall)

add_subdirectory(src)

glintfx_install_library_target(glintfx)
glintfx_install_public_headers()
glintfx_install_cmake_package()

if(GLINTFX_BUILD_TESTS)
    include(CTest)
    include(GlintfxTest)
    add_subdirectory(tests)
endif()
