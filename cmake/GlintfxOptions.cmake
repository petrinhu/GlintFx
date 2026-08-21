# GlintfxOptions.cmake
#
# Assunto único: as opções de configuração que o usuário do build da
# glintfx pode ligar/desligar. Nenhuma lógica de alvo entra aqui.

# Ordem do líder (ordem de serviço FUND-1): shared é o default.
option(BUILD_SHARED_LIBS "Compila a glintfx como biblioteca compartilhada" ON)

option(GLINTFX_WERROR "Trata warnings de compilação da glintfx como erro" OFF)

option(GLINTFX_BUILD_TESTS
    "Compila a suíte de testes da glintfx"
    ${PROJECT_IS_TOP_LEVEL}
)

# compile_commands.json só faz sentido quando a glintfx é o projeto de
# topo: um consumidor que embute a glintfx via add_subdirectory tem o
# próprio compile_commands.json e não deve ser sobrescrito pelo nosso.
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL
        "Exporta compile_commands.json" FORCE)
endif()
