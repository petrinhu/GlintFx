// SPDX-License-Identifier: AGPL-3.0-or-later
#include <string>
#include <vector>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include "gl_registry_codegen/loader_codegen.hpp"

// gl_loader_codegen_test.cpp - GL-LOADER (TODO.md, GODS_LAWS.md L-20,
// L-40). Proves the TEXT ASSEMBLY loader_codegen.hpp/.cpp produces from
// a tiny, fully controlled fixture (two commands: one with no
// parameters, one with a pointer return type and one parameter) -
// substring assertions on the exact lines a human or the compiler has
// to see, not a brittle whole-string comparison against every
// whitespace choice.

using glintfx::gl_codegen::gl_command;
using glintfx::gl_codegen::gl_param;
using glintfx::gl_codegen::render_header;
using glintfx::gl_codegen::render_source;

namespace {

std::vector<gl_command> fixture_commands() {
    gl_command finish;
    finish.name = "glFinish";
    finish.return_type = "void";

    gl_command get_string;
    get_string.name = "glGetString";
    get_string.return_type = "const GLubyte *";
    get_string.params.push_back(gl_param{"GLenum", "name"});

    return {finish, get_string};
}

bool contains(const std::string &haystack, std::string_view needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

GLINTFX_TEST(header_declares_a_function_pointer_typedef_per_command) {
    const std::string header = render_header(fixture_commands());
    GLINTFX_CHECK(contains(header, "using PFNGLFINISHPROC = void (GLINTFX_GL_APIENTRY *)(void);"));
    GLINTFX_CHECK(contains(
        header,
        "using PFNGLGETSTRINGPROC = const GLubyte * (GLINTFX_GL_APIENTRY *)(GLenum name);"));
}

GLINTFX_TEST(header_declares_one_struct_field_named_after_the_gl_function) {
    const std::string header = render_header(fixture_commands());
    GLINTFX_CHECK(contains(header, "PFNGLFINISHPROC glFinish = nullptr;"));
    GLINTFX_CHECK(contains(header, "PFNGLGETSTRINGPROC glGetString = nullptr;"));
}

GLINTFX_TEST(header_declares_the_load_gl_functions_entry_point) {
    const std::string header = render_header(fixture_commands());
    GLINTFX_CHECK(contains(header,
                           "[[nodiscard]] glintfx::gltfx_rslt<gl_function_table> "
                           "load_gl_functions(gl_proc_address_fn get_proc_address) noexcept;"));
}

GLINTFX_TEST(header_carries_the_three_mandatory_attribution_lines) {
    const std::string header = render_header(fixture_commands());
    GLINTFX_CHECK(contains(header, "generated at build time"));
    GLINTFX_CHECK(contains(header, "Khronos"));
    GLINTFX_CHECK(contains(header, "SPDX-License-Identifier: AGPL-3.0-or-later"));
}

GLINTFX_TEST(header_includes_the_hand_written_abi_and_proc_address_headers) {
    const std::string header = render_header(fixture_commands());
    GLINTFX_CHECK(contains(header, "#include \"gl_abi.hpp\""));
    GLINTFX_CHECK(contains(header, "#include \"gl_proc_address.hpp\""));
}

GLINTFX_TEST(source_includes_the_header_by_the_given_path) {
    const std::string source =
        render_source(fixture_commands(), "generated/render/gl_functions.hpp");
    GLINTFX_CHECK(contains(source, "#include \"generated/render/gl_functions.hpp\""));
}

GLINTFX_TEST(source_calls_try_assign_once_per_command_with_its_own_name) {
    const std::string source = render_source(fixture_commands(), "gl_functions.hpp");
    GLINTFX_CHECK(contains(
        source, "try_assign_gl_function_pointer(table.glFinish, get_proc_address, \"glFinish\")"));
    GLINTFX_CHECK(contains(
        source,
        "try_assign_gl_function_pointer(table.glGetString, get_proc_address, \"glGetString\")"));
}

GLINTFX_TEST(source_returns_ok_with_the_populated_table_on_success) {
    const std::string source = render_source(fixture_commands(), "gl_functions.hpp");
    GLINTFX_CHECK(contains(source, "gltfx_rslt<gl_function_table>::ok(std::move(table))"));
}

GLINTFX_TEST(source_returns_not_found_with_the_first_missing_name_on_failure) {
    const std::string source = render_source(fixture_commands(), "gl_functions.hpp");
    GLINTFX_CHECK(contains(source, "gltfx_err_code::not_found"));
    GLINTFX_CHECK(contains(source, "with_rejected_value(first_missing)"));
}

GLINTFX_TEST(source_carries_the_three_mandatory_attribution_lines) {
    const std::string source = render_source(fixture_commands(), "gl_functions.hpp");
    GLINTFX_CHECK(contains(source, "generated at build time"));
    GLINTFX_CHECK(contains(source, "Khronos"));
    GLINTFX_CHECK(contains(source, "SPDX-License-Identifier: AGPL-3.0-or-later"));
}
