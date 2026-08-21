# src/CMakeLists.txt
#
# Assunto único: o alvo da biblioteca glintfx e as camadas que somam
# fontes a ele. Propriedades, export header e include dirs vêm de
# GlintfxLibrary.cmake; warnings e padrão de linguagem vêm de
# GlintfxCompileOptions.cmake — nenhum dos dois é redefinido aqui.
#
# Cada camada (GODS_LAWS.md L-19) é uma unidade CMake visível: um
# add_subdirectory() próprio, cujo CMakeLists.txt só faz
# target_sources(glintfx ...) sobre o alvo criado abaixo. Hoje só
# core/ existe; a camada que toca o SO nasce como próxima entrada
# desta lista, nunca misturada aos fontes do núcleo.

add_library(glintfx)
add_library(glintfx::glintfx ALIAS glintfx)

add_subdirectory(core)

glintfx_apply_compile_options(glintfx)
glintfx_set_target_properties(glintfx)
glintfx_generate_export_header(glintfx)
glintfx_configure_version_header()
glintfx_set_target_include_dirs(glintfx)
glintfx_apply_static_define(glintfx)
