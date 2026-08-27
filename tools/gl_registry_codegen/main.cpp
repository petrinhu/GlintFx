// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include "gl_registry_codegen/loader_codegen.hpp"
#include "gl_registry_codegen/registry_parser.hpp"
#include "gl_registry_codegen/sha256.hpp"

// main.cpp - GL-LOADER (TODO.md, GODS_LAWS.md L-07 EXCECAO No 1, L-40).
//
// CLI entry point of the build-time gl_registry_codegen tool
// (src/render/CMakeLists.txt is the only caller). Every function below
// is one atom (GODS_LAWS.md L-17): read the input, resolve the
// registry, render the two files, write them only if changed, verify
// integrity when asked. main() itself is the composition, not the
// logic.
//
// Usage:
//   gl_registry_codegen <gl.xml path> <output dir> [max-version]
//                        [--expect-sha256=<hex>]
//
// max-version defaults to 3.3 (GODS_LAWS.md L-31/ESCOPO.md SS6: OpenGL
// 3.3 core is the only target this project's shipped loader ever
// generates for) - present as an optional positional argument so
// tests/gl_registry_codegen_cli_test.cpp's fixtures can exercise a
// SMALL, self-contained registry instead of the real 2.7 MB file.
//
// --expect-sha256 is OPTIONAL, deliberately: the real build
// (src/render/CMakeLists.txt) always passes it, pinned to the value
// recorded in third_party/khronos/README.md (GODS_LAWS.md L-07 EXCECAO
// No 1, obligation 3 - the permanent proof the vendored file is still
// byte-for-byte verbatim); a test fixture never does, because a
// fixture's whole point is to NOT be the real file, and forcing every
// fixture to also carry a matching hash would couple every test in
// this suite to one literal string that has nothing to do with what
// each test is actually proving.

namespace {

void fail(std::string_view message) {
    std::fprintf(stderr, "gl_registry_codegen: %s\n", std::string(message).c_str());
    std::exit(1);
}

std::string read_file_or_fail(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        fail("nao foi possivel abrir '" + path.string() + "' para leitura");
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Writes `content` to `path` only if it differs from what is already
// there (or the file does not exist yet) - preserves the OUTPUT file's
// mtime across an unchanged run, so CMake's own dependency tracking
// does not trigger a needless downstream rebuild every time this tool
// runs but gl.xml itself has not changed.
void write_file_if_changed(const std::filesystem::path &path, const std::string &content) {
    if (std::filesystem::exists(path)) {
        const std::string existing = read_file_or_fail(path);
        if (existing == content) {
            return;
        }
    }
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        fail("nao foi possivel escrever '" + path.string() + "'");
    }
    file << content;
}

// Parses "--expect-sha256=<hex>" out of argv, or returns an empty
// string when the flag is absent (see the file header comment for why
// absence is a normal, supported case, not an error).
std::string parse_expect_sha256_flag(int argc, char **argv) {
    constexpr std::string_view prefix = "--expect-sha256=";
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg.starts_with(prefix)) {
            return std::string(arg.substr(prefix.size()));
        }
    }
    return {};
}

double parse_max_version_argument(int argc, char **argv) {
    // Positional argument 3 (argv[3]), only when present AND not
    // itself a --flag (so "gl_registry_codegen a.xml out --expect-
    // sha256=X" without a max-version still resolves the default,
    // instead of trying to parse a flag as a version number).
    if (argc < 4) {
        return 3.3;
    }
    const std::string_view third = argv[3];
    if (third.starts_with("--")) {
        return 3.3;
    }
    return std::stod(std::string(third));
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        fail("uso: gl_registry_codegen <gl.xml> <output-dir> [max-version] [--expect-sha256=<hex>]");
    }

    const std::filesystem::path input_path = argv[1];
    const std::filesystem::path output_dir = argv[2];
    const double max_version = parse_max_version_argument(argc, argv);
    const std::string expect_sha256 = parse_expect_sha256_flag(argc, argv);

    const std::string xml_content = read_file_or_fail(input_path);

    if (!expect_sha256.empty()) {
        const std::string actual_sha256 = glintfx::gl_codegen::sha256_hex(xml_content);
        if (actual_sha256 != expect_sha256) {
            fail("integridade do arquivo vendorizado falhou: '" + input_path.string() + "' tem sha256 " +
                 actual_sha256 + ", esperado " + expect_sha256 +
                 " (GODS_LAWS.md L-07 EXCECAO No 1, obrigacao 3 - o arquivo deve permanecer verbatim; "
                 "atualize third_party/khronos/README.md APENAS se o lider autorizou re-vendorizar)");
        }
    }

    const std::vector<glintfx::gl_codegen::gl_command> commands =
        glintfx::gl_codegen::build_gl_registry(xml_content, max_version);

    // GODS_LAWS.md L-40: zero functions generated is a FAILURE, never
    // a silent success - the exact defect class this law exists to
    // forbid (a gate that scans nothing and prints green).
    if (commands.empty()) {
        fail("varredura vazia: '" + input_path.string() + "' nao resolveu NENHUMA funcao GL <= " +
             std::to_string(max_version) + " core - GODS_LAWS.md L-40 reprova, nunca declara sucesso silencioso");
    }

    const std::filesystem::path header_path = output_dir / "gl_functions.hpp";
    const std::filesystem::path source_path = output_dir / "gl_functions.cpp";

    write_file_if_changed(header_path, glintfx::gl_codegen::render_header(commands));
    write_file_if_changed(source_path,
                           glintfx::gl_codegen::render_source(commands, header_path.filename().string()));

    std::printf("gl_registry_codegen: gerou %zu funcoes GL <= %.1f core (varredura nao-vazia)\n", commands.size(),
                max_version);
    return 0;
}
