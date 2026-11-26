#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# composed_photo_selftest.sh - the controls of composed_photo.sh's choice of card (QA-COMPOSED-PHOTOS G4a-fix1),
# loaded once with `.` by that script's own --selftest (D-W8-137: a driver's controls live beside it, never inside
# it). They prove the guards that run BEFORE any docker call: the render node is chosen by vendor and driver, never
# by number; the GeForce node must be the one its baseline names; label=disable exists only in the nvidia branch.
# No container, no docker, no device, no window: every /sys is a fake tree under mktemp, and the baseline is the
# committed file. It has no main and no dispatch of its own; $0 is still composed_photo.sh.

SELFTEST_CHECKS=0

# selftest_check <control>: runs one control in a SUBSHELL, so a variable it sets or a fail() it calls dies with it.
selftest_check() {
    SELFTEST_CHECKS=$((SELFTEST_CHECKS + 1))
    if ! ( "$@" ); then
        echo "selftest: controle $SELFTEST_CHECKS FALHOU: $*" >&2
        exit 1
    fi
}

# fake_render <root> <renderDN> <vendor> <driver>: one render node of a fake /sys, with its driver link.
fake_render() {
    mkdir -p "$1/class/drm/$2/device" "$1/bus/pci/drivers/$4" || return 1
    printf '%s\n' "$3" > "$1/class/drm/$2/device/vendor" || return 1
    ln -s "$1/bus/pci/drivers/$4" "$1/class/drm/$2/device/driver"
}

# expect_fail_code <code> <command...>: the command, in a subshell, ends with exactly that code.
expect_fail_code() {
    want="$1"; shift
    ( "$@" ) > /dev/null 2>&1
    got=$?
    [ "$got" = "$want" ] || { echo "esperava rc $want, veio $got: $*" >&2; return 1; }
}

# The intel branch adds nothing to the docker command: no label=disable, no other device (M08 of the N2 review).
selftest_intel_branch_adds_nothing() {
    root="$(mktemp -d)" || return 1
    fake_render "$root" renderD128 0x8086 i915 || return 1
    EXTRA_ARGS=""
    pick_intel_node "$root"
    rc=$?
    rm -rf "$root"
    [ "$rc" = 0 ] && [ "$NODE" = /dev/dri/renderD128 ] && [ "$VARIANT" = gpu_integrated_photo ] && [ -z "$EXTRA_ARGS" ]
}

selftest_intel_branch_refuses_other_vendor() {
    root="$(mktemp -d)" || return 1
    fake_render "$root" renderD128 0x10de nvidia || return 1
    expect_fail_code 3 pick_intel_node "$root"
    rc=$?
    rm -rf "$root"
    return "$rc"
}

# The GeForce node comes from vendor and driver: renderD131 here, a number the real machine does not have.
selftest_nvidia_node_by_vendor_and_driver_not_number() {
    root="$(mktemp -d)" || return 1
    fake_render "$root" renderD128 0x8086 i915 && fake_render "$root" renderD131 0x10de nvidia || return 1
    pick_nvidia_node "$root"
    rc=$?
    rm -rf "$root"
    [ "$rc" = 0 ] && [ "$NODE" = /dev/dri/renderD131 ]
}

selftest_nvidia_two_nodes_are_refused() {
    root="$(mktemp -d)" || return 1
    fake_render "$root" renderD129 0x10de nvidia && fake_render "$root" renderD130 0x10de nvidia || return 1
    expect_fail_code 3 pick_nvidia_node "$root"
    rc=$?
    rm -rf "$root"
    return "$rc"
}

# An NVIDIA card bound to another driver is not the GeForce of the photo.
selftest_nvidia_other_driver_is_refused() {
    root="$(mktemp -d)" || return 1
    fake_render "$root" renderD129 0x10de nouveau || return 1
    expect_fail_code 3 pick_nvidia_node "$root"
    rc=$?
    rm -rf "$root"
    return "$rc"
}

selftest_baseline_guard_accepts_the_named_node() {
    NODE=/dev/dri/renderD129
    check_node_named_by_baseline
}

# Another node is refused with code 4 before any docker call (M09 of the N2 review).
selftest_baseline_guard_refuses_another_node() {
    NODE=/dev/dri/renderD128
    expect_fail_code 4 check_node_named_by_baseline
}

selftest_baseline_guard_refuses_a_missing_baseline() {
    dir="$(mktemp -d)" || return 1
    HERE="$dir"; NODE=/dev/dri/renderD129
    expect_fail_code 4 check_node_named_by_baseline
    rc=$?
    rm -rf "$dir"
    return "$rc"
}

# The nvidia set: label=disable first, then the three nodes and the mounts of the tool, and the GeForce variant.
selftest_nvidia_variant_disables_the_label_only_there() {
    OUT="$(mktemp -d)" || return 1
    printf '%s\n' -v /usr/lib64/x.so.1:/usr/lib64/x.so.1:ro > "$OUT/montagens.txt"
    NODE=/dev/dri/renderD129; EXTRA_ARGS=""
    set_nvidia_variant > /dev/null
    rm -rf "$OUT"
    case "$EXTRA_ARGS" in "--security-opt label=disable --device /dev/nvidia0 "*) ;; *) return 1 ;; esac
    case "$EXTRA_ARGS" in *"/usr/lib64/x.so.1:/usr/lib64/x.so.1:ro"*) ;; *) return 1 ;; esac
    [ "$VARIANT" = gpu_geforce ]
}

selftest_main() {
    selftest_check selftest_intel_branch_adds_nothing
    selftest_check selftest_intel_branch_refuses_other_vendor
    selftest_check selftest_nvidia_node_by_vendor_and_driver_not_number
    selftest_check selftest_nvidia_two_nodes_are_refused
    selftest_check selftest_nvidia_other_driver_is_refused
    selftest_check selftest_baseline_guard_accepts_the_named_node
    selftest_check selftest_baseline_guard_refuses_another_node
    selftest_check selftest_baseline_guard_refuses_a_missing_baseline
    selftest_check selftest_nvidia_variant_disables_the_label_only_there
    echo "selftest: $SELFTEST_CHECKS controles OK"
}
