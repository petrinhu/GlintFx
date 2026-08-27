// SPDX-License-Identifier: AGPL-3.0-or-later
#include "gl_registry_codegen/loader_codegen.hpp"

#include <algorithm>
#include <cctype>

// GL-LOADER (TODO.md, GODS_LAWS.md L-07 EXCECAO No 1, L-17): each
// function below is one atom of this file's single subject (domain
// model -> generated text). See loader_codegen.hpp's own header
// comment for why the header and .cpp renderers each carry their own
// copy of the three-line attribution instead of sharing it through a
// helper.

namespace glintfx::gl_codegen {

namespace {

std::string to_upper(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(),
                    [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

// "PFN" + NAME (uppercased; the name already starts with "gl") +
// "PROC" - the exact convention every real GL header uses
// (glActiveTexture -> PFNGLACTIVETEXTUREPROC), so a consumer or a
// reviewer who already knows OpenGL recognizes it on sight instead of
// learning a new one.
std::string typedef_name(const gl_command &command) { return "PFN" + to_upper(command.name) + "PROC"; }

// Joins a command's parameter list into the C parameter-list text of
// its function pointer typedef: "GLenum name, GLsizei count" - "void"
// (the C convention meaning "no parameters", not merely an empty
// string) when there are none, so the generated pointer type is valid
// on every compiler this project targets, MSVC included.
std::string parameter_list_text(const gl_command &command) {
    if (command.params.empty()) {
        return "void";
    }
    std::string text;
    for (std::size_t i = 0; i < command.params.size(); ++i) {
        if (i > 0) {
            text += ", ";
        }
        text += command.params[i].type;
        text += ' ';
        text += command.params[i].name;
    }
    return text;
}

std::string three_line_attribution(std::string_view purpose) {
    std::string text;
    text += "// SPDX-License-Identifier: AGPL-3.0-or-later\n";
    text += "//\n";
    text += "// GENERATED FILE - do not edit by hand, generated at build time by\n";
    text += "// tools/gl_registry_codegen from the vendored third_party/khronos/gl.xml\n";
    text += "// (Copyright The Khronos Group Inc., SPDX-License-Identifier: Apache-2.0,\n";
    text += "// GODS_LAWS.md L-07 EXCECAO No 1 - see third_party/khronos/README.md).\n";
    text += "// This ";
    text += purpose;
    text += " is glintfx's own code (a mechanical restatement of a public,\n";
    text += "// standardized function list), licensed AGPL-3.0-or-later like the rest of\n";
    text += "// this project - the Apache-2.0 license above covers the INPUT registry\n";
    text += "// only, never this generated output.\n";
    return text;
}

} // namespace

std::string render_header(const std::vector<gl_command> &commands) {
    std::string out = three_line_attribution("generated header");
    out += "#pragma once\n\n";
    out += "#include <glintfx/core/err.hpp>\n\n";
    out += "#include \"gl_abi.hpp\"\n";
    out += "#include \"gl_proc_address.hpp\"\n\n";
    out += "namespace glintfx::render {\n\n";

    for (const gl_command &command : commands) {
        out += "using " + typedef_name(command) + " = " + command.return_type + " (GLINTFX_GL_APIENTRY *)(" +
               parameter_list_text(command) + ");\n";
    }
    out += "\n";

    out += "struct gl_function_table {\n";
    for (const gl_command &command : commands) {
        out += "    " + typedef_name(command) + " " + command.name + " = nullptr;\n";
    }
    out += "};\n\n";

    out += "// Resolves every field of gl_function_table by calling get_proc_address()\n";
    out += "// once per GL 3.3 core function - see gl_proc_address.hpp's own header\n";
    out += "// comment for why get_proc_address is a plain function pointer. Defined in\n";
    out += "// the GENERATED .cpp counterpart of this header.\n";
    out += "[[nodiscard]] glintfx::gltfx_rslt<gl_function_table> load_gl_functions(gl_proc_address_fn "
           "get_proc_address) noexcept;\n\n";
    out += "} // namespace glintfx::render\n";
    return out;
}

std::string render_source(const std::vector<gl_command> &commands, std::string_view header_include_path) {
    std::string out = three_line_attribution("generated source");
    out += "#include <string_view>\n";
    out += "#include <utility>\n\n";
    out += "#include \"";
    out += header_include_path;
    out += "\"\n\n";
    out += "namespace glintfx::render {\n\n";
    out += "glintfx::gltfx_rslt<gl_function_table> load_gl_functions(gl_proc_address_fn get_proc_address) noexcept "
           "{\n";
    out += "    gl_function_table table;\n";
    out += "    bool all_resolved = true;\n";
    out += "    std::string_view first_missing;\n\n";

    for (const gl_command &command : commands) {
        out += "    if (!try_assign_gl_function_pointer(table." + command.name + ", get_proc_address, \"" +
               command.name + "\")) {\n";
        out += "        if (all_resolved) {\n";
        out += "            first_missing = \"" + command.name + "\";\n";
        out += "        }\n";
        out += "        all_resolved = false;\n";
        out += "    }\n";
    }

    out += "\n    if (!all_resolved) {\n";
    out += "        return glintfx::gltfx_rslt<gl_function_table>::err(\n";
    out += "            glintfx::gltfx_err(glintfx::gltfx_err_code::not_found)\n";
    out += "                .with_rejected_value(first_missing));\n";
    out += "    }\n";
    out += "    return glintfx::gltfx_rslt<gl_function_table>::ok(std::move(table));\n";
    out += "}\n\n";
    out += "} // namespace glintfx::render\n";
    return out;
}

} // namespace glintfx::gl_codegen
