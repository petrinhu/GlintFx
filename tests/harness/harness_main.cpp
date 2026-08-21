#include <print>
#include <string_view>

#include "check.hpp"
#include "test_registry.hpp"

// harness_main.cpp — ponto de entrada do harness próprio (FUND-1).
//
// Sem argumento: roda todos os casos registrados. Com "--list": só
// imprime os nomes. Com um nome exato: roda só aquele caso (e falha se
// o nome não existir, em vez de ignorar silenciosamente).

namespace {

using glintfx::test::Case;

void print_case_list() {
    for (const Case& c : glintfx::test::all_cases()) {
        std::println("{}", c.name);
    }
}

bool run_single_case(const Case& c) {
    glintfx::test::reset_failure_count();
    c.fn();
    const bool passed = glintfx::test::failure_count() == 0;
    std::println("[{}] {}", passed ? "PASS" : "FAIL", c.name);
    return passed;
}

const Case* find_case_by_name(std::string_view name) {
    for (const Case& c : glintfx::test::all_cases()) {
        if (c.name == name) {
            return &c;
        }
    }
    return nullptr;
}

int run_all_cases() {
    int failures = 0;
    for (const Case& c : glintfx::test::all_cases()) {
        if (!run_single_case(c)) {
            ++failures;
        }
    }
    return failures;
}

int run_named_case(std::string_view name) {
    const Case* c = find_case_by_name(name);
    if (c == nullptr) {
        std::println(stderr, "harness: nenhum caso de teste chamado \"{}\"", name);
        return 1;
    }
    return run_single_case(*c) ? 0 : 1;
}

void print_summary(std::size_t total, int failures) {
    std::println("--- {} caso(s), {} falha(s) ---", total, failures);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view{argv[1]} == "--list") {
        print_case_list();
        return 0;
    }

    if (argc == 2) {
        return run_named_case(argv[1]);
    }

    const int failures = run_all_cases();
    print_summary(glintfx::test::all_cases().size(), failures);
    return failures == 0 ? 0 : 1;
}
