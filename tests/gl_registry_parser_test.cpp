// SPDX-License-Identifier: AGPL-3.0-or-later
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include "gl_registry_codegen/registry_parser.hpp"

// gl_registry_parser_test.cpp - GL-LOADER (TODO.md, GODS_LAWS.md L-20,
// L-27). Proves registry_parser.hpp/.cpp against small, self-contained
// fixtures that reproduce the exact shapes measured live in the real
// vendored gl.xml (docs/api-conventions.md's own methodology: enumerate
// the small space, do not guess it) - never against the 2.7 MB real
// file here, that is main_generates_functions_from_the_real_vendored_file
// in gl_registry_codegen_cli_test.cpp instead.

using glintfx::gl_codegen::build_gl_registry;
using glintfx::gl_codegen::gl_command;
using glintfx::gl_codegen::parse_command_signatures;
using glintfx::gl_codegen::resolve_core_profile_command_names;

namespace {

const gl_command *find_command(const std::vector<gl_command> &commands, std::string_view name) {
    for (const gl_command &command : commands) {
        if (command.name == name) {
            return &command;
        }
    }
    return nullptr;
}

constexpr std::string_view k_commands_fixture = R"(<commands namespace="GL">
    <command>
        <proto>void <name>glFinish</name></proto>
    </command>
    <command>
        <proto>void <name>glClear</name></proto>
        <param group="ClearBufferMask"><ptype>GLbitfield</ptype> <name>mask</name></param>
    </command>
    <command>
        <proto kind="String">const <ptype>GLubyte</ptype> *<name>glGetString</name></proto>
        <param group="StringName"><ptype>GLenum</ptype> <name>name</name></param>
    </command>
    <command>
        <proto>void <name>glShaderSource</name></proto>
        <param class="shader"><ptype>GLuint</ptype> <name>shader</name></param>
        <param><ptype>GLsizei</ptype> <name>count</name></param>
        <param len="count">const <ptype>GLchar</ptype> *const*<name>string</name></param>
        <param len="count">const <ptype>GLint</ptype> *<name>length</name></param>
    </command>
</commands>)";

} // namespace

GLINTFX_TEST(parses_a_command_with_no_parameters) {
    const auto commands = parse_command_signatures(k_commands_fixture);
    const gl_command *finish = find_command(commands, "glFinish");
    GLINTFX_CHECK(finish != nullptr && finish->return_type == "void");
    GLINTFX_CHECK(finish != nullptr && finish->params.empty());
}

GLINTFX_TEST(parses_a_command_with_one_parameter) {
    const auto commands = parse_command_signatures(k_commands_fixture);
    const gl_command *clear = find_command(commands, "glClear");
    GLINTFX_CHECK(clear != nullptr && clear->params.size() == 1u);
    GLINTFX_CHECK(clear != nullptr && !clear->params.empty() &&
                  clear->params[0].type == "GLbitfield");
    GLINTFX_CHECK(clear != nullptr && !clear->params.empty() && clear->params[0].name == "mask");
}

GLINTFX_TEST(parses_a_pointer_return_type_split_across_text_and_ptype) {
    const auto commands = parse_command_signatures(k_commands_fixture);
    const gl_command *get_string = find_command(commands, "glGetString");
    GLINTFX_CHECK(get_string != nullptr && get_string->return_type == "const GLubyte *");
}

GLINTFX_TEST(parses_a_pointer_to_const_pointer_parameter_in_order) {
    const auto commands = parse_command_signatures(k_commands_fixture);
    const gl_command *shader_source = find_command(commands, "glShaderSource");
    GLINTFX_CHECK(shader_source != nullptr && shader_source->params.size() == 4u);
    const bool has_third_param = shader_source != nullptr && shader_source->params.size() > 2;
    GLINTFX_CHECK(has_third_param && shader_source->params[2].type == "const GLchar *const*");
    GLINTFX_CHECK(has_third_param && shader_source->params[2].name == "string");
}

namespace {

constexpr std::string_view k_feature_fixture = R"(<registry>
    <commands namespace="GL">
        <command><proto>void <name>glOne</name></proto></command>
        <command><proto>void <name>glTwo</name></proto></command>
        <command><proto>void <name>glThree</name></proto></command>
        <command><proto>void <name>glFuture</name></proto></command>
        <command><proto>void <name>glCompatOnly</name></proto></command>
    </commands>
    <feature api="gl" name="GL_VERSION_1_0" number="1.0">
        <require>
            <command name="glOne"/>
            <command name="glTwo"/>
        </require>
        <require profile="compatibility">
            <command name="glCompatOnly"/>
        </require>
    </feature>
    <feature api="gl" name="GL_VERSION_2_0" number="2.0">
        <require profile="core">
            <command name="glThree"/>
        </require>
    </feature>
    <feature api="gl" name="GL_VERSION_3_2" number="3.2">
        <remove profile="core" comment="fixed-function removed from core">
            <command name="glOne"/>
        </remove>
    </feature>
    <feature api="gl" name="GL_VERSION_4_0" number="4.0">
        <require>
            <command name="glFuture"/>
        </require>
    </feature>
</registry>)";

} // namespace

GLINTFX_TEST(resolves_the_union_of_require_across_features_up_to_max_version) {
    const auto names = resolve_core_profile_command_names(k_feature_fixture, 3.3);
    GLINTFX_CHECK_EQ(names.size(), 2u); // glTwo, glThree (glOne removed, glFuture out of range)
}

GLINTFX_TEST(a_command_removed_by_a_core_profile_remove_block_is_excluded) {
    const auto names = resolve_core_profile_command_names(k_feature_fixture, 3.3);
    const bool has_one = std::find(names.begin(), names.end(), "glOne") != names.end();
    GLINTFX_CHECK(!has_one);
}

GLINTFX_TEST(a_feature_above_max_version_never_contributes_its_requires) {
    const auto names = resolve_core_profile_command_names(k_feature_fixture, 3.3);
    const bool has_future = std::find(names.begin(), names.end(), "glFuture") != names.end();
    GLINTFX_CHECK(!has_future);
}

GLINTFX_TEST(a_require_with_profile_core_still_contributes) {
    const auto names = resolve_core_profile_command_names(k_feature_fixture, 3.3);
    const bool has_three = std::find(names.begin(), names.end(), "glThree") != names.end();
    GLINTFX_CHECK(has_three);
}

GLINTFX_TEST(a_require_with_a_non_core_profile_never_contributes) {
    // Catches a mutant that survived the first pass of this fatia's own
    // mutation testing: `profile_applies = true` unconditionally passed
    // every other test here, because none of them had an ACTUAL
    // non-core profile block to be wrongly let through - this is the
    // fixture that closes that gap.
    const auto names = resolve_core_profile_command_names(k_feature_fixture, 3.3);
    const bool has_compat_only =
        std::find(names.begin(), names.end(), "glCompatOnly") != names.end();
    GLINTFX_CHECK(!has_compat_only);
}

GLINTFX_TEST(raising_max_version_pulls_in_the_later_feature) {
    const auto names = resolve_core_profile_command_names(k_feature_fixture, 4.0);
    const bool has_future = std::find(names.begin(), names.end(), "glFuture") != names.end();
    GLINTFX_CHECK(has_future);
    GLINTFX_CHECK_EQ(names.size(), 3u);
}

GLINTFX_TEST(build_gl_registry_returns_commands_sorted_alphabetically_by_name) {
    const auto registry = build_gl_registry(k_feature_fixture, 3.3);
    GLINTFX_CHECK_EQ(registry.size(), 2u);
    GLINTFX_CHECK_EQ(registry[0].name, "glThree");
    GLINTFX_CHECK_EQ(registry[1].name, "glTwo");
}

GLINTFX_TEST(build_gl_registry_carries_the_real_signature_for_each_resolved_name) {
    const auto registry = build_gl_registry(k_feature_fixture, 3.3);
    const gl_command *two = find_command(registry, "glTwo");
    GLINTFX_CHECK(two != nullptr && two->return_type == "void");
    GLINTFX_CHECK(two != nullptr && two->params.empty());
}
