# tests/package/CMakeLists.txt
#
# Assunto único: projeto CMake standalone que prova find_package(glintfx)
# fora da árvore da glintfx (FUND-1, item 9 da ordem de serviço). NÃO é
# incluído pelo build principal via add_subdirectory — roda como
# configure/build independente, apontando CMAKE_PREFIX_PATH pro prefixo
# instalado.

cmake_minimum_required(VERSION 3.28)

project(glintfx_consumer LANGUAGES CXX)

find_package(glintfx 0.1 REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE glintfx::glintfx)
set_target_properties(consumer PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
