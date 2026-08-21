# GlintfxTest.cmake
#
# Assunto único: a função que registra um executável de teste ligado ao
# harness próprio da glintfx (GODS_LAWS.md L-07: sem Catch2/GoogleTest).

function(glintfx_add_test name)
    add_executable(${name} "${CMAKE_CURRENT_SOURCE_DIR}/${name}.cpp")
    target_link_libraries(${name} PRIVATE glintfx::glintfx glintfx_test_harness)
    glintfx_apply_compile_options(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()
