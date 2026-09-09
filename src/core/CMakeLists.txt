# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/core/CMakeLists.txt
#
# Core layer (GODS_LAWS.md L-19): pure types and values (today, the
# runtime version) that know nothing about the operating system. No
# file here includes an OS header or a header from a layer above; the
# gate in tests/tools/check_layers.py reproves the violation instead
# of trusting the discipline of whoever writes the code.
#
# Single subject: the sources of this layer. Properties of the
# `glintfx_library` target stay in src/CMakeLists.txt; this file only
# adds sources to a target already created in the parent directory.

target_sources(glintfx_library PRIVATE version.cpp err_code.cpp err.cpp color.cpp time.cpp
                                       vec2.cpp angle.cpp mat3.cpp interpolate.cpp fixed_step.cpp)

# CORE-LOG (TODO.md W5, GODS_LAWS.md L-19): the structured-log sink
# track lives in its own log/ subfolder of this same core layer (no
# add_subdirectory() - a single visible CMake unit stays simplest for
# a handful of files, same shape err_code.cpp/err.cpp already share
# with the files right above), never its own top-level layer.
target_sources(glintfx_library PRIVATE log/severity.cpp log/event.cpp)
