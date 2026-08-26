# SPDX-License-Identifier: AGPL-3.0-or-later
#
# GlintfxPkgConfigValidate.cmake - PKG-VALIDATE.
#
# Single subject (GODS_LAWS.md L-17): registers the install(CODE) step
# that validates the INSTALLED glintfx.pc on the machine that actually
# runs `cmake --install` - the packager's own machine, never ours.
#
# WHY THIS IS A DIFFERENT GATE THAN PKG-DIST'S OWN, NOT A DUPLICATE OF
# IT: tests/tools/check_pkgconfig.sh (PKG-DIST's sibling gate) proves
# six layouts, end to end, in OUR OWN CI - default, Debian-multiarch,
# a malformed libdir, absolute install dirs, static, and a
# DESTDIR-staged Fedora install. Every one of those is a layout we, as
# the maintainers, chose to enumerate. GODS_LAWS.md LEI ZERO is that
# glintfx's consumer base is public and unknown - some real packager,
# on some real machine we will never see, WILL eventually combine
# CMAKE_INSTALL_LIBDIR/CMAKE_INSTALL_INCLUDEDIR/CMAKE_INSTALL_PREFIX/
# DESTDIR/--component in a shape none of those six scenarios
# anticipated, and PKG-DIST's own CI gate, by construction, cannot
# reach that machine at all. This file is the piece that DOES reach
# it: an install(CODE) step, appended to the SAME cmake_install.cmake
# every `cmake --install` runs, so it fires on every real install,
# everywhere, not only the ones this project's own CI happens to try.
#
# The actual validation logic lives in
# cmake/GlintfxPkgConfigValidateInstalled.cmake.in, generated (via
# configure_file(@ONLY)) into the BUILD tree, not read from the source
# tree at install time - deliberately: a build directory can outlive
# the source checkout it was configured from in some packaging
# pipelines (a build dir reused for a LATER, separate `cmake --install`
# invocation, possibly after the source tree that produced it was
# cleaned up), and `cmake --install <builddir>` already requires that
# build directory to exist and be intact, so anything this validation
# needs is guaranteed present exactly as long as the install command
# itself would work at all.
#
# Each function below does one thing (GODS_LAWS.md L-17).

# Registers the install(CODE) step, or skips registering it entirely
# when the CONFIGURE-time half of the escape hatch
# (GLINTFX_SKIP_PKGCONFIG_VALIDATION, cmake/GlintfxOptions.cmake) is
# ON - see that option's own comment, and
# GlintfxPkgConfigValidateInstalled.cmake.in's file header, for the
# INSTALL-time half of the same hatch (an ENVIRONMENT variable of the
# same name, read fresh on every `cmake --install` invocation
# regardless of this cache option).
#
# Called from CMakeLists.txt right after glintfx_install_pkgconfig()
# (cmake/GlintfxInstall.cmake) - AFTER, on purpose: install() rules
# execute in the order they are added to the generated
# cmake_install.cmake, and this validation has to run once glintfx.pc,
# the headers and the library artifact are ALREADY on disk, not
# before.
function(glintfx_register_pkgconfig_validation)
    if(GLINTFX_SKIP_PKGCONFIG_VALIDATION)
        message(STATUS
            "glintfx: post-install pkg-config validation disabled at "
            "configure time (GLINTFX_SKIP_PKGCONFIG_VALIDATION=ON) - "
            "glintfx.pc will still be installed, but this build will "
            "not check that it resolves correctly on `cmake --install`."
        )
        return()
    endif()

    # Same computation glintfx_compute_pkgconfig_relocatable_prefix()
    # (cmake/GlintfxInstall.cmake) already applies to CMAKE_INSTALL_LIBDIR
    # before using it for glintfx.pc's OWN content - normalizing here,
    # once, at configure time, means the generated .cmake file below
    # never has to re-normalize a possibly-malformed
    # (trailing-slash/doubled-separator/leading-./) value itself.
    set(normalized_libdir "${CMAKE_INSTALL_LIBDIR}")
    cmake_path(NORMAL_PATH normalized_libdir)
    set(GLINTFX_NORMALIZED_INSTALL_LIBDIR "${normalized_libdir}")

    set(generated_script "${CMAKE_CURRENT_BINARY_DIR}/GlintfxPkgConfigValidateInstalled.cmake")

    # PROJECT_SOURCE_DIR, not CMAKE_SOURCE_DIR (FIX-CONSUMO achado A7):
    # see the comment at the same substitution in the root
    # CMakeLists.txt and in cmake/GlintfxInstall.cmake.
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/GlintfxPkgConfigValidateInstalled.cmake.in"
        "${generated_script}"
        @ONLY
    )

    # One line: include()-ing the generated file both DEFINES
    # glintfx_validate_installed_pkgconfig() and CALLS it, at its own
    # last line - so this install(CODE) block needs no second
    # statement, and `cmake -P "${generated_script}"` (the exact form
    # tests/tools/check_pkgconfig_validate.sh uses to reproduce a
    # broken install directly, see that script's own header) runs the
    # identical code path a real `cmake --install` would.
    install(CODE "include(\"${generated_script}\")")
endfunction()
