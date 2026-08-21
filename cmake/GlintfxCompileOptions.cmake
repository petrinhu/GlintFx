# GlintfxCompileOptions.cmake
#
# Assunto único: padrão de linguagem C++23 e flags de warning aplicados
# a um alvo da glintfx (biblioteca, harness de teste ou executável de
# teste). Nenhuma opção de usuário nem propriedade de instalação entra
# aqui — isso é GlintfxOptions.cmake e GlintfxLibrary.cmake.

function(glintfx_apply_cxx_standard target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endfunction()

function(glintfx_apply_warning_flags target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )
    if(GLINTFX_WERROR)
        target_compile_options(${target} PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
            $<$<CXX_COMPILER_ID:MSVC>:/WX>
        )
    endif()
endfunction()

# Ponto de entrada único que os arquivos de alvo (src/CMakeLists.txt,
# tests/CMakeLists.txt, GlintfxTest.cmake) chamam. Composição das duas
# funções acima, sem lógica própria.
function(glintfx_apply_compile_options target)
    glintfx_apply_cxx_standard(${target})
    glintfx_apply_warning_flags(${target})
endfunction()
