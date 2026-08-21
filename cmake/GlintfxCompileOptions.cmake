# GlintfxCompileOptions.cmake
#
# Single subject: the C++23 language standard and the warning flags
# applied to a glintfx target (library, test harness or test
# executable). No user option and no install property belongs here;
# that is GlintfxOptions.cmake and GlintfxLibrary.cmake.

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

# Single entry point that the target files (src/CMakeLists.txt,
# tests/CMakeLists.txt, GlintfxTest.cmake) call. Composition of the two
# functions above, with no logic of its own.
function(glintfx_apply_compile_options target)
    glintfx_apply_cxx_standard(${target})
    glintfx_apply_warning_flags(${target})
endfunction()
