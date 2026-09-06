# SPDX-License-Identifier: AGPL-3.0-or-later
#
# GlintfxEgl.cmake
#
# Single subject: resolves the SYSTEM EGL and libwayland-egl libraries
# (GODS_LAWS.md L-07/D-W6b-3: both count as OS API, the exact same
# category libwayland-client already sits in - EGL is the interface
# Wayland's own documentation names as the way to get GL onto a
# wl_surface, and libwayland-egl ships from the SAME wayland-devel
# package wayland-client.h already comes from) and wires them PRIVATE
# into a glintfx target - the W-EGL counterpart of
# GlintfxWaylandProtocols.cmake one file over, which this module
# deliberately does not duplicate the reasoning of (read that file's
# own header comment for the full "why plain variables, not an
# IMPORTED_TARGET" account - the identical reasoning applies here,
# verbatim, and is not repeated a second time).
#
# Included only under if(UNIX) (root CMakeLists.txt) - a platform with
# no EGL/wayland-egl pkg-config module is not one this fatia targets
# (GODS_LAWS.md L-05: Wayland/EGL is a Linux-only surface).

find_package(PkgConfig REQUIRED)
pkg_check_modules(GlintfxEgl REQUIRED egl wayland-egl)

# Attaches EGL + libwayland-egl to `target` as PRIVATE - same plain
# _LIBRARIES/_INCLUDE_DIRS/_LIBRARY_DIRS/_CFLAGS_OTHER technique
# GlintfxWaylandProtocols.cmake's own glintfx_add_wayland_xdg_shell_
# binding() already uses, for the identical static-consumer reason.
function(glintfx_add_egl target)
    target_include_directories(${target} PRIVATE ${GlintfxEgl_INCLUDE_DIRS})
    target_link_directories(${target} PRIVATE ${GlintfxEgl_LIBRARY_DIRS})
    target_link_libraries(${target} PRIVATE ${GlintfxEgl_LIBRARIES})
    target_compile_options(${target} PRIVATE ${GlintfxEgl_CFLAGS_OTHER})
endfunction()
