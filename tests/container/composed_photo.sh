#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# composed_photo.sh - QA-COMPOSED-PATH G2 (plano-fechamento-w8-r2.md sec. 5.4) and QA-COMPOSED-PHOTOS G4a: ONE
# composed photo of one example, on ONE card, in a container (GODS_LAWS.md L-09), proved card by
# tests/tools/composed_photo_facts.py and pixel by tests/tools/image_probe.py.
#
# Usage: composed_photo.sh <image> <example> <out-dir> <intel|nvidia>
#        composed_photo.sh --selftest   (G4a-fix1: the guards before any docker call, on fake /sys trees)
#   intel: only /dev/dri/renderD128 enters, after its PCI vendor reads 0x8086 (Intel), and the
#   isolation variant is gpu_integrated_photo (tests/container/check_isolation.sh).
#   nvidia (G4a): the render node is read from /sys by its vendor and driver, never by its number; the
#   nodes /dev/nvidia0, /dev/nvidiactl and /dev/nvidia-modeset enter; the read-only driver binds are the
#   ones tests/tools/nvidia_cdi_mounts.py reads from the CDI specification of the loaded driver; the
#   isolation variant is gpu_geforce, whose baseline names that node. The SELinux label is disabled for
#   this one container only.
#
# ONE container, the one that is photographed: check_isolation.sh proves THIS container BEFORE any
# docker exec (G2-fix1, finding I-3 of the N3 review). It runs with a ceiling of 512 processes, a
# protection of this machine (the recursive test double of 29/09 took it down); the variant
# gpu_integrated_photo is gpu_integrated plus exactly that ceiling, measured by docker create
# (tests/container/hostconfig_baseline_gpu_integrated_photo.txt, D-W8-F18). The two KWin variables
# live in .Config.Env, outside the HostConfig the gate reads; so the script proves them itself: the
# container's environment must be the image's plus exactly these two, or nothing runs. They switch
# off KWin's D-Bus permission checks INSIDE the container (the ScreenShot2 call comes from busctl,
# not from a .desktop file KWin trusts); they open nothing of the host.
# Every step writes "<step>=<rc>" to <out-dir>/photo.txt and the first failure ends the script with
# its own code. The capture must be 640x480 at scale 1 (D-W8-72, the demo window).

set -u

readonly KWIN_FLAGS="KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1 KWIN_WAYLAND_NO_PERMISSION_CHECKS=1"
readonly EXPECTED_SIZE=640x480
readonly CONTAINER=glintfx-photo

fail() {
    echo "composed_photo.sh: $1" >&2
    exit "${2:-1}"
}

rec() {
    echo "$1=$2" | tee -a "$OUT/photo.txt"
}

remove_container_and_fail() {
    docker rm -f "$CONTAINER" > "$OUT/rm.log" 2>&1
    fail "$1" "$2"
}

# The environment of the container, one variable per line, sorted; empty output is a failure.
env_lines() {
    docker "$1" inspect --format '{{range .Config.Env}}{{println .}}{{end}}' "$2" > "$3.raw" 2> "$3.err" || return 1
    sed '/^$/d' "$3.raw" | sort > "$3" && [ -s "$3" ]
}

env_is_image_plus_kwin_flags() {
    env_lines image "$IMAGE" "$OUT/env-imagem.txt" || return 1
    env_lines container "$CONTAINER" "$OUT/env-conteiner.txt" || return 1
    # shellcheck disable=SC2086 # the two flags are words on purpose
    printf '%s\n' $KWIN_FLAGS | cat - "$OUT/env-imagem.txt" | sort > "$OUT/env-esperado.txt" || return 1
    cmp -s "$OUT/env-esperado.txt" "$OUT/env-conteiner.txt"
}

start_and_prove() {
    # The two -e below are KWIN_FLAGS written out; env_is_image_plus_kwin_flags() fails closed if they ever drift.
    # EXTRA_ARGS is empty on intel; on nvidia it holds the GeForce set, split into words on purpose, with pathname
    # expansion off (set -f) so a '*', '?' or '[' in a path can never turn into other files (G4a-fix1).
    set -f
    # shellcheck disable=SC2086
    docker run -d --name "$CONTAINER" --cap-drop=ALL --cap-add=SYS_NICE --pids-limit 512 --device "$NODE" $EXTRA_ARGS \
        -e KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1 -e KWIN_WAYLAND_NO_PERMISSION_CHECKS=1 "$IMAGE" \
        > "$OUT/run.id" 2> "$OUT/run.err"
    run_rc=$?
    set +f
    [ "$run_rc" = 0 ] || fail "conteiner nao subiu" 30
    "$HERE/wait_for_ready_marker.sh" "$CONTAINER" > "$OUT/ready.log" 2>&1 \
        || remove_container_and_fail "conteiner nao ficou pronto" 31
    "$HERE/check_isolation.sh" "$CONTAINER" glintfx-test "$VARIANT" > "$OUT/isolation.log" 2>&1
    iso=$?
    rec isolamento "$iso"
    [ "$iso" = 0 ] || remove_container_and_fail "isolamento nao provado no conteiner da foto; nada roda" 32
    env_is_image_plus_kwin_flags
    envrc=$?
    rec ambiente "$envrc"
    [ "$envrc" = 0 ] || remove_container_and_fail "ambiente difere de imagem + as duas variaveis do KWin" 33
}

take_photo() {
    docker exec -i -e GLINTFX_PHOTO_EXAMPLE="$EXAMPLE" "$CONTAINER" sh -s < "$HERE/composed_photo_inside.sh" \
        > "$OUT/inside.log" 2>&1
    inside=$?
    rec dentro "$inside"
    docker cp "$CONTAINER:/tmp/photo" "$OUT/achados" > "$OUT/cp.log" 2>&1
    copied=$?
    rec copia "$copied"
    docker rm -f "$CONTAINER" > "$OUT/rm.log" 2>&1 || fail "docker rm do conteiner da foto falhou" 43
    [ "$copied" = 0 ] || fail "achados nao copiados" 42
    [ "$inside" = 0 ] || fail "a foto nao saiu (rc.txt diz o passo)" 44
}

judge_photo() {
    "$PYTHON" "$TOOLS/composed_photo_facts.py" "$OUT/achados" "$MAKER" "$OUT/capture" "$EXPECTED_SIZE" \
        > "$OUT/facts.log" 2>&1
    rec fatos "$?"
    "$PYTHON" "$TOOLS/raw_to_png.py" "$OUT/capture" "$OUT/png" > "$OUT/png.log" 2>&1
    rec png "$?"
    probes="$HERE/../fixtures/${EXAMPLE}_probes.txt"
    [ -f "$probes" ] || fail "sem arquivo de sondas: $probes" 51
    "$PYTHON" "$TOOLS/image_probe.py" "$OUT/png/conn1_surface1.png" "$probes" > "$OUT/probes.log" 2>&1
    rec sondas "$?"
}

# The intel branch: the one node the photo has always used, after its PCI vendor reads 0x8086.
# <sys_root>: /sys in the photo, a fake tree in the selftest (an argument, never an environment variable).
pick_intel_node() {
    NODE=/dev/dri/renderD128; VARIANT=gpu_integrated_photo
    vendor=$(cat "$1/class/drm/renderD128/device/vendor" 2>&1)
    [ "$vendor" = 0x8086 ] || fail "renderD128 nao e a Intel (fabricante '$vendor'); nada roda" 3
}

# The nvidia branch, item 1: exactly one render node whose vendor is 0x10de and whose driver is nvidia. Only
# shell builtins and test -ef: nothing here opens a device, and the node is never chosen by its number.
# <sys_root>: /sys in the photo, a fake tree in the selftest.
pick_nvidia_node() {
    found=""; count=0
    for d in "$1"/class/drm/renderD*; do
        [ -e "$d/device/vendor" ] || continue
        read -r v < "$d/device/vendor" || continue
        if [ "$v" = 0x10de ] && [ "$d/device/driver" -ef "$1/bus/pci/drivers/nvidia" ]; then
            found="$found ${d##*/}"; count=$((count + 1))
        fi
    done
    [ "$count" = 1 ] || fail "nos de render da NVIDIA com driver nvidia: $count (esperado 1); nada roda" 3
    NODE="/dev/dri/${found# }"
    vendor=0x10de
}

# Item 2: the node must be the one the baseline names, or the script stops with code 4 before any docker call.
check_node_named_by_baseline() {
    baseline="$HERE/hostconfig_baseline_gpu_geforce.txt"
    [ -f "$baseline" ] || fail "sem linha de base da GeForce: $baseline" 4
    grep -q -F "\"PathOnHost\":\"$NODE\"" "$baseline" \
        || fail "$NODE nao e o no que a linha de base nomeia; remeça a linha de base" 4
}

# Item 3: the three NVIDIA device nodes must exist as character devices; code 5 otherwise.
check_nvidia_device_nodes() {
    [ -c /dev/nvidia0 ] && [ -c /dev/nvidiactl ] && [ -c /dev/nvidia-modeset ] \
        || fail "faltam /dev/nvidia0, /dev/nvidiactl ou /dev/nvidia-modeset; nada roda" 5
}

# Item 4, after mkdir: the read-only binds of the loaded driver, from the CDI specification that matches it.
read_nvidia_mounts() {
    read -r nv < /sys/module/nvidia/version || fail "sem versao do driver NVIDIA em /sys/module/nvidia/version" 6
    "$PYTHON" "$TOOLS/nvidia_cdi_mounts.py" "$nv" /run/cdi/nvidia.yaml /etc/cdi/nvidia.yaml \
        > "$OUT/montagens.txt" 2> "$OUT/montagens.err" \
        || fail "montagens da NVIDIA recusadas (ver montagens.err); nada roda" 6
}

# Item 5: the GeForce set, only in this branch. The SELinux label is disabled for this container alone.
set_nvidia_variant() {
    EXTRA_ARGS="--security-opt label=disable --device /dev/nvidia0 --device /dev/nvidiactl --device /dev/nvidia-modeset $(cat "$OUT/montagens.txt")"
    VARIANT=gpu_geforce
    rec no "$NODE"
}

main() {
    [ "$#" -eq 4 ] || fail "uso: composed_photo.sh <imagem> <exemplo> <saida> <intel|nvidia>" 2
    IMAGE="$1"; EXAMPLE="$2"; OUT="$3"; MAKER="$4"
    EXTRA_ARGS=""
    HERE=$(cd "$(dirname "$0")" && pwd) || fail "diretorio do script" 2
    TOOLS="$HERE/../tools"; PYTHON=${PYTHON:-python3}
    case "$MAKER" in
        intel) pick_intel_node /sys ;;
        nvidia) pick_nvidia_node /sys; check_node_named_by_baseline; check_nvidia_device_nodes ;;
        *) fail "fabricante '$MAKER' recusado: so intel ou nvidia" 2 ;;
    esac
    [ ! -e "$OUT" ] || fail "$OUT ja existe; use uma pasta nova" 2
    mkdir -p "$OUT" || fail "nao criou $OUT" 2
    rec fabricante "$vendor"
    if [ "$MAKER" = nvidia ]; then
        read_nvidia_mounts
        set_nvidia_variant
    fi
    start_and_prove
    take_photo
    judge_photo
    grep -q -E '^(fatos|png|sondas)=[1-9]' "$OUT/photo.txt" && fail "veredito: REPROVA" 50
    rec veredito PASSA
}

# --selftest (G4a-fix1): the only argument besides the four of the photo. The controls live beside the script
# (composed_photo_selftest.sh), load with `.`, and touch no container, no device and no /sys of this machine.
if [ "$#" -eq 1 ] && [ "$1" = "--selftest" ]; then
    HERE=$(cd "$(dirname "$0")" && pwd) || fail "diretorio do script" 2
    # shellcheck source=composed_photo_selftest.sh
    . "$HERE/composed_photo_selftest.sh"
    selftest_main
else
    main "$@"
fi
