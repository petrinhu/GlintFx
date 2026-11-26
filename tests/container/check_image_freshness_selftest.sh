#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_image_freshness_selftest.sh - the controls of check_image_freshness.sh's text judges and readers, loaded once with
# `.` by that script's own --selftest (D-W8-137: a driver's controls live beside it, never inside it). No docker, no
# network, no image: every input is text written here. It has no main and no dispatch of its own; $0 is still
# check_image_freshness.sh.
#
# The three controls GODS_LAWS.md L-40 asks of every gate are all here, for each judge: a good input passes, a bad
# one is refused, and an EMPTY one is refused too (an empty side is a broken measurement, never "nothing to compare").

FRESHNESS_SELFTEST_CHECKS=0

# -- inputs ------------------------------------------------------------------------------------------------------------

FRESH_STAMP="12345-1"
FRESH_RELEASE="45"
FRESH_KWIN="kwin-6.8.2-3.fc45.x86_64"
FRESH_BASE="fedora:latest"

# An index of two architectures that agree on the release, in the shape `docker buildx imagetools inspect --raw` gives.
index_of_release() {
    printf '%s' "{\"manifests\":[{\"annotations\":{\"org.opencontainers.image.version\":\"$1\"},\"platform\":{\"architecture\":\"amd64\"}},{\"annotations\":{\"org.opencontainers.image.version\": \"$2\"},\"platform\":{\"architecture\":\"arm64\"}}]}"
}

# -- helpers -----------------------------------------------------------------------------------------------------------

# run_control <control>: runs one control in a SUBSHELL, so a variable it leaves behind and a fail() it reaches die
# with it.
run_control() {
    ( "$@" )
}

freshness_selftest_check() {
    FRESHNESS_SELFTEST_CHECKS=$((FRESHNESS_SELFTEST_CHECKS + 1))
    if ! run_control "$@"; then
        echo "selftest: controle $FRESHNESS_SELFTEST_CHECKS FALHOU: $*" >&2
        exit 1
    fi
}

# refused <judge> <arguments...>: the judge, silenced, has to return 1.
refused() {
    if ( "$@" ) >/dev/null 2>&1; then echo "o juiz aceitou o que devia reprovar: $*" >&2; return 1; fi
}

# accepted <judge> <arguments...>: the judge, silenced, has to return 0.
accepted() {
    ( "$@" ) >/dev/null 2>&1 || { echo "o juiz reprovou o que devia passar: $*" >&2; return 1; }
}

# -- controls: the stamp of the dnf layer ------------------------------------------------------------------------------

selftest_stamp_of_this_build_passes() {
    accepted judge_stamp "$FRESH_STAMP" "$FRESH_STAMP"
}

# The dnf layer of another build: the stamp in the image is not the one this build passed.
selftest_stamp_of_another_build_is_refused() {
    refused judge_stamp "12000-1" "$FRESH_STAMP"
}

# The caller passed no stamp: nothing was proved, even when the image carries none either.
selftest_empty_wanted_stamp_is_refused() {
    refused judge_stamp "" ""
}

# The image was built with no stamp at all (the default of the build argument).
selftest_image_without_stamp_is_refused() {
    refused judge_stamp "" "$FRESH_STAMP"
}

# -- controls: the base ------------------------------------------------------------------------------------------------

selftest_base_of_the_registry_passes() {
    accepted judge_base "$FRESH_RELEASE" "$FRESH_RELEASE" "$FRESH_BASE"
}

# The local tag of the base was behind the registry's: the shape measured on 11/11/2026.
selftest_base_behind_the_registry_is_refused() {
    refused judge_base "44" "$FRESH_RELEASE" "$FRESH_BASE"
}

selftest_empty_registry_release_is_refused() {
    refused judge_base "" "" "$FRESH_BASE"
}

# -- controls: the kwin package ----------------------------------------------------------------------------------------

selftest_installed_kwin_passes() {
    accepted judge_kwin "$FRESH_KWIN"
}

selftest_kwin_not_installed_is_refused() {
    refused judge_kwin "package kwin is not installed"
}

selftest_empty_kwin_answer_is_refused() {
    refused judge_kwin ""
}

# -- controls: the verdict ---------------------------------------------------------------------------------------------

# The count line says how many judges diverged, and it is printed also when none did (GODS_LAWS.md L-40).
selftest_verdict_prints_the_count_and_passes_on_zero() {
    out="$( (verdict_line 3 0) 2>&1 )" || { echo "veredito reprovou com zero divergentes" >&2; return 1; }
    [ "$out" = "check_image_freshness.sh: 3 conferencia(s), 0 divergente(s)" ] \
        || { echo "linha de contagem diferente: $out" >&2; return 1; }
}

selftest_verdict_fails_on_one_diverged() {
    out="$( (verdict_line 3 1) 2>&1 )" && { echo "veredito passou com um divergente" >&2; return 1; }
    [ "$out" = "check_image_freshness.sh: 3 conferencia(s), 1 divergente(s)" ] \
        || { echo "linha de contagem diferente: $out" >&2; return 1; }
}

# -- controls: the registry index --------------------------------------------------------------------------------------

selftest_index_gives_the_one_release() {
    [ "$(registry_release_from_index "$(index_of_release 45 45)")" = "45" ] \
        || { echo "o indice de uma versao so nao deu essa versao" >&2; return 1; }
}

selftest_index_with_two_releases_is_refused() {
    if ( registry_release_from_index "$(index_of_release 45 44)" ) >/dev/null 2>&1; then
        echo "indice com duas versoes aceito" >&2; return 1
    fi
}

selftest_index_without_the_annotation_is_refused() {
    if ( registry_release_from_index '{"manifests":[{"platform":{"architecture":"amd64"}}]}' ) >/dev/null 2>&1; then
        echo "indice sem a anotacao aceito (varredura vazia)" >&2; return 1
    fi
}

# -- controls: the base reference of the Containerfile -----------------------------------------------------------------

selftest_base_reference_is_the_last_from() {
    text="$(printf '%s\n' "# FROM comentado:nao" "FROM fedora:1 AS builder" "RUN true" "FROM fedora:latest" "COPY a b")"
    [ "$(base_reference_from_containerfile "$text")" = "fedora:latest" ] \
        || { echo "a referencia da base nao e a do ultimo FROM" >&2; return 1; }
}

selftest_containerfile_without_from_is_refused() {
    if ( base_reference_from_containerfile "RUN true" ) >/dev/null 2>&1; then
        echo "Containerfile sem FROM aceito (varredura vazia)" >&2; return 1
    fi
}

selftest_main() {
    freshness_selftest_check selftest_stamp_of_this_build_passes
    freshness_selftest_check selftest_stamp_of_another_build_is_refused
    freshness_selftest_check selftest_empty_wanted_stamp_is_refused
    freshness_selftest_check selftest_image_without_stamp_is_refused
    freshness_selftest_check selftest_base_of_the_registry_passes
    freshness_selftest_check selftest_base_behind_the_registry_is_refused
    freshness_selftest_check selftest_empty_registry_release_is_refused
    freshness_selftest_check selftest_installed_kwin_passes
    freshness_selftest_check selftest_kwin_not_installed_is_refused
    freshness_selftest_check selftest_empty_kwin_answer_is_refused
    freshness_selftest_check selftest_verdict_prints_the_count_and_passes_on_zero
    freshness_selftest_check selftest_verdict_fails_on_one_diverged
    freshness_selftest_check selftest_index_gives_the_one_release
    freshness_selftest_check selftest_index_with_two_releases_is_refused
    freshness_selftest_check selftest_index_without_the_annotation_is_refused
    freshness_selftest_check selftest_base_reference_is_the_last_from
    freshness_selftest_check selftest_containerfile_without_from_is_refused
    echo "selftest: $FRESHNESS_SELFTEST_CHECKS controles OK"
}
