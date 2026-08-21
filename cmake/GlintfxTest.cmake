# GlintfxTest.cmake
#
# Single subject: the function that registers a test executable linked
# against glintfx's own harness (GODS_LAWS.md L-07: no
# Catch2/GoogleTest).

function(glintfx_add_test name)
    add_executable(${name} "${CMAKE_CURRENT_SOURCE_DIR}/${name}.cpp")
    target_link_libraries(${name} PRIVATE glintfx::glintfx glintfx_test_harness)
    glintfx_apply_compile_options(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()
