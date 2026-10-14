#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# composed_photo.sh - QA-COMPOSED-PATH G2 (plano-fechamento-w8-r2.md sec. 5.4): ONE composed
# photo of one example, on ONE card, in a container (GODS_LAWS.md L-09), proved card by
# tests/tools/composed_photo_facts.py and pixel by tests/tools/image_probe.py.
#
# Usage: composed_photo.sh <image> <example> <out-dir> <intel>
#   intel: only /dev/dri/renderD128 enters, after its PCI vendor reads 0x8086 (Intel), and the
#   isolation variant is gpu_integrated_photo (tests/container/check_isolation.sh). The GeForce
#   variant is the G4's, not this file's.
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
    docker run -d --name "$CONTAINER" --cap-drop=ALL --cap-add=SYS_NICE --pids-limit 512 --device "$NODE" \
        -e KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1 -e KWIN_WAYLAND_NO_PERMISSION_CHECKS=1 "$IMAGE" \
        > "$OUT/run.id" 2> "$OUT/run.err" || fail "conteiner nao subiu" 30
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

main() {
    [ "$#" -eq 4 ] || fail "uso: composed_photo.sh <imagem> <exemplo> <saida> <intel>" 2
    IMAGE="$1"; EXAMPLE="$2"; OUT="$3"; MAKER="$4"
    [ "$MAKER" = intel ] || fail "fabricante '$MAKER' recusado: so intel (a GeForce e a G4)" 2
    NODE=/dev/dri/renderD128; VARIANT=gpu_integrated_photo
    vendor=$(cat /sys/class/drm/renderD128/device/vendor 2>&1)
    [ "$vendor" = 0x8086 ] || fail "renderD128 nao e a Intel (fabricante '$vendor'); nada roda" 3
    HERE=$(cd "$(dirname "$0")" && pwd) || fail "diretorio do script" 2
    TOOLS="$HERE/../tools"; PYTHON=${PYTHON:-python3}
    [ ! -e "$OUT" ] || fail "$OUT ja existe; use uma pasta nova" 2
    mkdir -p "$OUT" || fail "nao criou $OUT" 2
    rec fabricante "$vendor"
    start_and_prove
    take_photo
    judge_photo
    grep -q -E '^(fatos|png|sondas)=[1-9]' "$OUT/photo.txt" && fail "veredito: REPROVA" 50
    rec veredito PASSA
}

main "$@"
