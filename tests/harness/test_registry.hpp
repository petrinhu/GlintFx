#pragma once

#include <string_view>
#include <vector>

// test_registry.hpp — registro de casos de teste do harness próprio da
// glintfx (FUND-1, GODS_LAWS.md L-07: sem Catch2/GoogleTest).
//
// Cada TU de teste declara um caso com GLINTFX_TEST(nome); a macro cria
// um objeto estático cujo construtor se registra na lista global antes
// de main() rodar (idioma clássico de auto-registro por static init).

namespace glintfx::test {

struct Case {
    std::string_view name;
    void (*fn)();
};

void register_case(Case c);
[[nodiscard]] const std::vector<Case>& all_cases();

struct CaseRegistrar {
    CaseRegistrar(std::string_view name, void (*fn)());
};

}  // namespace glintfx::test

#define GLINTFX_TEST(name)                                                  \
    static void glintfx_test_fn_##name();                                  \
    namespace {                                                             \
    const ::glintfx::test::CaseRegistrar glintfx_test_reg_##name(           \
        #name, &glintfx_test_fn_##name);                                   \
    }                                                                       \
    static void glintfx_test_fn_##name()
