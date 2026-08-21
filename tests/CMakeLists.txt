# tests/CMakeLists.txt
#
# Assunto único: a suíte de testes da glintfx — harness próprio e os
# casos que ligam contra ele. tests/package/ é projeto CMake standalone
# e de propósito (prova find_package fora da árvore) — NÃO entra aqui
# por design.

add_library(glintfx_test_harness STATIC
    harness/test_registry.cpp
    harness/check.cpp
    harness/harness_main.cpp
)
target_include_directories(glintfx_test_harness PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
glintfx_apply_compile_options(glintfx_test_harness)

glintfx_add_test(version_test)

# Teste de visibilidade: só faz sentido com símbolos dinâmicos (.so) e
# com a ferramenta `nm -D` do Unix. No Windows a exportação já é
# controlada por __declspec via generate_export_header; provar isso
# exigiria dumpbin, fora do escopo desta fatia.
if(BUILD_SHARED_LIBS AND UNIX)
    add_test(
        NAME visibility_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_exports.sh" "$<TARGET_FILE:glintfx>"
    )
endif()

# Gate de camadas (GODS_LAWS.md L-19, item 1: "um gate de CI reprova a
# violação"). POSIX sh — downgrade declarado: roda nos jobs Linux da
# matriz, não no Windows, que não tem `sh` por padrão.
if(UNIX)
    add_test(
        NAME layers_test
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_layers.sh" "${CMAKE_SOURCE_DIR}"
    )
endif()
