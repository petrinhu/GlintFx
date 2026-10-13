#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# composed_photo.sh - QA-COMPOSED-PATH G2 (plano-fechamento-w8-r2.md sec. 5.4): ONE composed
# photo of one example, on ONE card, in a container (GODS_LAWS.md L-09), proved card by
# tests/tools/composed_photo_facts.py and pixel by tests/tools/image_probe.py.
#
# Usage: composed_photo.sh <image> <example> <out-dir> <intel>
#   intel: only /dev/dri/renderD128 enters, after its PCI vendor reads 0x8086 (Intel), and the
#   isolation variant is gpu_integrated (tests/container/check_isolation.sh, G1b). The GeForce
#   variant is the G4's, not this file's.
#
# Two containers, the recipe the isolation gate demands (it refuses --pids-limit): A, identical to
# the variant's baseline, proves the isolation BEFORE anything runs and is removed; B, the same plus
# --pids-limit 512 and the two ScreenShot2 variables, takes the photo. Nothing of the host is mounted.
# Every step writes "<step>=<rc>" to <out-dir>/photo.txt and the first failure ends the script with
# its own code.

set -u

fail() {
    echo "composed_photo.sh: $1" >&2
    exit "${2:-1}"
}

rec() {
    echo "$1=$2" | tee -a "$OUT/photo.txt"
}

container_ready() {
    "$HERE/wait_for_ready_marker.sh" "$1" > "$OUT/$1-ready.log" 2>&1
}

prove_isolation() {
    ctr=glintfx-photo-a
    docker run -d --name "$ctr" --cap-drop=ALL --cap-add=SYS_NICE --device "$NODE" "$IMAGE" \
        > "$OUT/a.id" 2> "$OUT/a.err" || fail "conteiner A nao subiu" 30
    container_ready "$ctr" || { docker rm -f "$ctr" > /dev/null; fail "conteiner A nao ficou pronto" 31; }
    "$HERE/check_isolation.sh" "$ctr" glintfx-test "$VARIANT" > "$OUT/isolation.log" 2>&1
    iso=$?
    rec isolamento "$iso"
    docker rm -f "$ctr" > "$OUT/a-rm.log" 2>&1 || fail "docker rm do conteiner A falhou" 34
    [ "$iso" = 0 ] || fail "isolamento nao provado; nada roda" 32
}

take_photo() {
    ctr=glintfx-photo-b
    docker run -d --name "$ctr" --cap-drop=ALL --cap-add=SYS_NICE --pids-limit 512 --device "$NODE" \
        -e KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1 -e KWIN_WAYLAND_NO_PERMISSION_CHECKS=1 "$IMAGE" \
        > "$OUT/b.id" 2> "$OUT/b.err" || fail "conteiner B nao subiu" 40
    container_ready "$ctr" || { docker rm -f "$ctr" > /dev/null; fail "conteiner B nao ficou pronto" 41; }
    docker exec -i -e GLINTFX_PHOTO_EXAMPLE="$EXAMPLE" "$ctr" sh -s < "$HERE/composed_photo_inside.sh" \
        > "$OUT/inside.log" 2>&1
    inside=$?
    rec dentro "$inside"
    docker cp "$ctr:/tmp/photo" "$OUT/achados" > "$OUT/cp.log" 2>&1
    copied=$?
    rec copia "$copied"
    docker rm -f "$ctr" > "$OUT/b-rm.log" 2>&1 || fail "docker rm do conteiner B falhou" 43
    [ "$copied" = 0 ] || fail "achados nao copiados" 42
    [ "$inside" = 0 ] || fail "a foto nao saiu (rc.txt diz o passo)" 44
}

judge_photo() {
    "$PYTHON" "$TOOLS/composed_photo_facts.py" "$OUT/achados" "$MAKER" "$OUT/capture" \
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
    NODE=/dev/dri/renderD128; VARIANT=gpu_integrated
    vendor=$(cat /sys/class/drm/renderD128/device/vendor 2>&1)
    [ "$vendor" = 0x8086 ] || fail "renderD128 nao e a Intel (fabricante '$vendor'); nada roda" 3
    HERE=$(cd "$(dirname "$0")" && pwd) || fail "diretorio do script" 2
    TOOLS="$HERE/../tools"; PYTHON=${PYTHON:-python3}
    [ ! -e "$OUT" ] || fail "$OUT ja existe; use uma pasta nova" 2
    mkdir -p "$OUT" || fail "nao criou $OUT" 2
    rec fabricante "$vendor"
    prove_isolation
    take_photo
    judge_photo
    grep -q -E '^(fatos|png|sondas)=[1-9]' "$OUT/photo.txt" && fail "veredito: REPROVA" 50
    rec veredito PASSA
}

main "$@"
