#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_image_freshness.sh - proves that a glintfx test image is "of the day" without a version written anywhere
# (KWIN-68-SEM-PLACA; GODS_LAWS.md L-04: the primary target has to fail when the leader's machine would fail, so the
# image follows fedora:latest and the compositor of the Fedora repository). Runs on the HOST, like
# check_isolation.sh: it drives `docker run` and `docker buildx imagetools`, which do not exist inside the image.
#
# Two things make an image old in silence, and each one has its own check here:
#   STAMP: the dnf layer came out of a build cache (it happened on 06/11/2026: a local round came out green on a
#     layer of 31/10). Whoever builds passes --build-arg GLINTFX_IMAGE_STAMP=<a value new for that build>, the layer
#     writes it to /etc/glintfx-image-stamp (tests/container/Containerfile), and this script reads it back and
#     compares it with the value its caller passed.
#   BASE: the base reference of the Containerfile resolved to a local tag older than the registry's (measured on
#     11/11/2026: the local fedora:latest was Fedora 44, the registry's was 45). The Fedora release inside the image
#     is compared with the release the registry gives for that same reference at the time of the check.
# The kwin package of the image is printed for whoever reads the log, and has to be an installed package; it is
# never compared with a version written in a file (a written version goes stale, a command does not).
#
# The network is used once, to read the registry's index of the base reference. Each `docker run` here starts the
# image with no network and no capability, and never starts the compositor (the entrypoint is replaced).
#
# Usage: check_image_freshness.sh <image> <stamp>
# Usage: check_image_freshness.sh --selftest   (the controls of the text judges: no docker, no network)
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

fail() {
    echo "check_image_freshness.sh: $1" >&2
    exit 1
}

HERE="$(dirname "$0")" || fail "nao foi possivel resolver o diretorio deste script (dirname \"\$0\" falhou)"
readonly STAMP_PATH_IN_IMAGE="/etc/glintfx-image-stamp"

require_args() {
    [ "$#" -eq 2 ] || fail "usage: check_image_freshness.sh <image> <stamp>"
}

# The base reference is the one the Containerfile names in its LAST `FROM` (the runtime stage), read from the
# text and never written here a second time. No `FROM` at all is a broken scan, not "nothing to check".
base_reference_from_containerfile() {
    reference="$(printf '%s\n' "$1" | sed -n 's/^FROM[[:space:]][[:space:]]*\([^[:space:]][^[:space:]]*\).*/\1/p' | tail -n 1)"
    [ -n "$reference" ] || fail "nenhuma linha FROM no Containerfile (varredura vazia disfarcada de OK)"
    printf '%s' "$reference"
}

# The release the registry gives for a reference: the annotation org.opencontainers.image.version of its index,
# which every architecture of an official image carries. One distinct value is the answer; none, or more than one,
# fails here instead of being guessed.
registry_release_from_index() {
    versions="$(printf '%s' "$1" | grep -oE '"org\.opencontainers\.image\.version":[[:space:]]*"[^"]*"' \
        | sed -E 's/.*:[[:space:]]*"([^"]*)"/\1/' | sort -u)" || true
    distinct="$(printf '%s\n' "$versions" | sed '/^$/d' | wc -l)"
    [ "$distinct" -eq 1 ] \
        || fail "o indice do registro traz $distinct valor(es) distintos de org.opencontainers.image.version, esperava 1 (varredura vazia ou ambigua)"
    printf '%s' "$versions"
}

# One command inside the image, with no network and no capability; the entrypoint is replaced, so the compositor
# never starts. `--pull never`: an image that is not on this machine fails here, by name, instead of being asked of
# a registry (a local tag is never a registry's business). A failing command is never read as an empty answer.
image_output_or_fail() {
    image="$1"
    label="$2"
    entrypoint="$3"
    shift 3
    output="$(docker run --rm --pull never --network none --cap-drop=ALL --entrypoint "$entrypoint" "$image" "$@" 2>&1)" \
        || fail "$label falhou na imagem '$image': $output"
    printf '%s' "$output"
}

registry_index_or_fail() {
    reference="$1"
    index="$(docker buildx imagetools inspect --raw "$reference" 2>&1)" \
        || fail "nao foi possivel ler o indice de '$reference' no registro: $index"
    printf '%s' "$index"
}

# kwin-<version>-<release>.<arch> is an installed package; "package kwin is not installed" and an empty answer
# are not.
is_installed_kwin_package() {
    case "$1" in
        kwin-[0-9]*) return 0 ;;
        *) return 1 ;;
    esac
}

# The three judges. Each one prints its line with both sides (GODS_LAWS.md L-40) and returns 1 when the image is
# not of the day on its axis. An empty side never compares equal to another empty side: empty is a broken
# measurement, never "nothing to compare".

# judge_stamp <stamp-in-image> <stamp-wanted>: the dnf layer was built for THIS build.
judge_stamp() {
    echo "check_image_freshness.sh: carimbo: imagem='$1' esperado='$2'"
    [ -n "$2" ] && [ "$1" = "$2" ]
}

# judge_base <release-in-image> <release-of-registry> <base-reference>: the base is the registry's, not an old tag.
judge_base() {
    echo "check_image_freshness.sh: base: imagem='$1' registro='$2' ($3)"
    [ -n "$2" ] && [ "$1" = "$2" ]
}

# judge_kwin <kwin-package>: printed for whoever reads the log; it only has to be an installed package.
judge_kwin() {
    echo "check_image_freshness.sh: kwin=$1"
    is_installed_kwin_package "$1"
}

# verdict_line <judges> <diverged>: the count, printed also when it passes (GODS_LAWS.md L-40).
verdict_line() {
    echo "check_image_freshness.sh: $1 conferencia(s), $2 divergente(s)"
    [ "$2" -eq 0 ]
}

main() {
    require_args "$@"
    image="$1"
    wanted_stamp="$2"
    reference="$(base_reference_from_containerfile "$(cat "$HERE/Containerfile")")"
    image_stamp="$(image_output_or_fail "$image" "leitura do carimbo" cat "$STAMP_PATH_IN_IMAGE")"
    image_release="$(image_output_or_fail "$image" "leitura da versao do Fedora" sh -c '. /etc/os-release && printf "%s" "$VERSION_ID"')"
    kwin_package="$(image_output_or_fail "$image" "rpm -q kwin" rpm -q kwin)"
    registry_release="$(registry_release_from_index "$(registry_index_or_fail "$reference")")"
    diverged=0
    judge_stamp "$image_stamp" "$wanted_stamp" || diverged=$((diverged + 1))
    judge_base "$image_release" "$registry_release" "$reference" || diverged=$((diverged + 1))
    judge_kwin "$kwin_package" || diverged=$((diverged + 1))
    verdict_line 3 "$diverged" \
        || fail "a imagem '$image' NAO e do dia: refaca com --pull e com um GLINTFX_IMAGE_STAMP novo (KWIN-68-SEM-PLACA, GODS_LAWS.md L-04)"
    echo "check_image_freshness.sh: imagem do dia provada para '$image'"
}

# --selftest: the controls live beside this script (D-W8-137), load with `.`, and never touch docker or the network.
if [ "$#" -eq 1 ] && [ "$1" = "--selftest" ]; then
    # shellcheck source=check_image_freshness_selftest.sh
    . "$HERE/check_image_freshness_selftest.sh"
    selftest_main
else
    main "$@"
fi
