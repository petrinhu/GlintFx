#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# nvidia_cdi_mounts.py - QA-COMPOSED-PHOTOS G4a (errata D-W8-1745). Reads the NVIDIA CDI specification whose
# --host-driver-version equals the loaded driver, and prints the read-only bind mounts that the GeForce photo
# container needs. Only a fixed allow-list of driver files passes; a --link is followed to an allow-listed file.
# Test tooling, not product code. Standard library only (L-07): the specification is read as text, not through
# a YAML library.
#
# USAGE: nvidia_cdi_mounts.py <driver_version> <spec> [<spec> ...]
#        nvidia_cdi_mounts.py --selftest
# Output, sorted by destination: for each mount a "-v" line and an "ORIGIN:DESTINATION:ro" line.
# Exit 0 on success. Exit 1, with the reason named on stderr, when no specification can be used.

import pathlib
import posixpath
import re
import sys
import tempfile

from selftest_controls import CHECKS, check

SAMPLE = pathlib.Path(__file__).resolve().parent.parent / "fixtures" / "nvidia_cdi_sample.yaml"
SAMPLE_DRIVER = "999.1.2"
SAMPLE_GBM = "/usr/lib64/gbm/nvidia-drm_gbm.so"
SAMPLE_GBM_ORIGIN = "/usr/lib64/libnvidia-allocator.so.999.1.2"
GLSI_BLOCK = (
    "        - hostPath: /usr/lib64/libnvidia-glsi.so.999.1.2\n"
    "          containerPath: /usr/lib64/libnvidia-glsi.so.999.1.2\n"
    "          options:\n"
    "            - ro\n"
)
ALLOCATOR_LINK = "            - libnvidia-allocator.so.999.1.2::/usr/lib64/libnvidia-allocator.so.1\n"
DUPLICATE_LINK = "            - --link\n            - libnvidia-glsi.so.999.1.2::/usr/lib64/libEGL_nvidia.so.0\n"
# An emitted destination with a colon or a space: a --link that resolves to an allow-listed file (D-W8-1752).
COLON_LINK = "            - --link\n            - libEGL_nvidia.so.999.1.2::/usr/lib64/libEGL:nvidia.so.0\n"
SPACE_LINK = "            - --link\n            - libEGL_nvidia.so.999.1.2::/usr/lib64/libEGL nvidia.so.0\n"
# The shape of /run/cdi/nvidia.yaml for driver 620.70.06 (D-W8-1753): two by-path links with colons, outside the
# list, repeated identically once per device block.
GBM_LINK = "            - ../libnvidia-allocator.so.1::/usr/lib64/gbm/nvidia-drm_gbm.so\n"
REAL_SPEC_LINKS = (
    "            - --link\n            - ../card0::/dev/dri/by-path/pci-0000:01:00.0-card\n"
    "            - --link\n            - ../renderD129::/dev/dri/by-path/pci-0000:01:00.0-render\n"
)
FORBIDDEN_IN_OUTPUT = (
    "libcuda", "libGLX", "egl-xcb", "xcb.json", "nvidia-smi", "firmware", "vulkan", "card0", "nvidia-uvm",
)

# The 23 mounts the sample yields, in order: a frozen value, decided before the tool existed (esperado-fixture.txt).
EXPECTED_SAMPLE_MOUNTS = (
    "/usr/lib64/libnvidia-allocator.so.999.1.2:/usr/lib64/gbm/nvidia-drm_gbm.so:ro",
    "/usr/lib64/libEGL_nvidia.so.999.1.2:/usr/lib64/libEGL_nvidia.so.0:ro",
    "/usr/lib64/libEGL_nvidia.so.999.1.2:/usr/lib64/libEGL_nvidia.so.999.1.2:ro",
    "/usr/lib64/libGLESv2_nvidia.so.999.1.2:/usr/lib64/libGLESv2_nvidia.so.2:ro",
    "/usr/lib64/libGLESv2_nvidia.so.999.1.2:/usr/lib64/libGLESv2_nvidia.so.999.1.2:ro",
    "/usr/lib64/libnvidia-allocator.so.999.1.2:/usr/lib64/libnvidia-allocator.so.1:ro",
    "/usr/lib64/libnvidia-allocator.so.999.1.2:/usr/lib64/libnvidia-allocator.so.999.1.2:ro",
    "/usr/lib64/libnvidia-egl-gbm.so.1.1.4:/usr/lib64/libnvidia-egl-gbm.so.1:ro",
    "/usr/lib64/libnvidia-egl-gbm.so.1.1.4:/usr/lib64/libnvidia-egl-gbm.so.1.1.4:ro",
    "/usr/lib64/libnvidia-egl-wayland.so.1.1.24:/usr/lib64/libnvidia-egl-wayland.so.1:ro",
    "/usr/lib64/libnvidia-egl-wayland.so.1.1.24:/usr/lib64/libnvidia-egl-wayland.so.1.1.24:ro",
    "/usr/lib64/libnvidia-egl-wayland2.so.1.0.2:/usr/lib64/libnvidia-egl-wayland2.so.1:ro",
    "/usr/lib64/libnvidia-egl-wayland2.so.1.0.2:/usr/lib64/libnvidia-egl-wayland2.so.1.0.2:ro",
    "/usr/lib64/libnvidia-eglcore.so.999.1.2:/usr/lib64/libnvidia-eglcore.so.999.1.2:ro",
    "/usr/lib64/libnvidia-glcore.so.999.1.2:/usr/lib64/libnvidia-glcore.so.999.1.2:ro",
    "/usr/lib64/libnvidia-glsi.so.999.1.2:/usr/lib64/libnvidia-glsi.so.999.1.2:ro",
    "/usr/lib64/libnvidia-gpucomp.so.999.1.2:/usr/lib64/libnvidia-gpucomp.so.999.1.2:ro",
    "/usr/lib64/libnvidia-tls.so.999.1.2:/usr/lib64/libnvidia-tls.so.999.1.2:ro",
    "/usr/lib64/libnvidia-wayland-client.so.999.1.2:/usr/lib64/libnvidia-wayland-client.so.999.1.2:ro",
    "/usr/share/egl/egl_external_platform.d/09_nvidia_wayland2.json:"
    "/usr/share/egl/egl_external_platform.d/09_nvidia_wayland2.json:ro",
    "/usr/share/egl/egl_external_platform.d/10_nvidia_wayland.json:"
    "/usr/share/egl/egl_external_platform.d/10_nvidia_wayland.json:ro",
    "/usr/share/egl/egl_external_platform.d/15_nvidia_gbm.json:"
    "/usr/share/egl/egl_external_platform.d/15_nvidia_gbm.json:ro",
    "/usr/share/glvnd/egl_vendor.d/10_nvidia.json:/usr/share/glvnd/egl_vendor.d/10_nvidia.json:ro",
)

# D-W8-1745: the only library families and JSON files that may be mounted. Everything else is ignored.
LIB_DIR = "/usr/lib64/"
LIBRARY_FAMILIES = (
    "libEGL_nvidia", "libGLESv2_nvidia", "libnvidia-allocator", "libnvidia-egl-gbm", "libnvidia-egl-wayland",
    "libnvidia-egl-wayland2", "libnvidia-eglcore", "libnvidia-glcore", "libnvidia-glsi", "libnvidia-gpucomp",
    "libnvidia-tls", "libnvidia-wayland-client",
)
JSON_MOUNTS = (
    "/usr/share/egl/egl_external_platform.d/09_nvidia_wayland2.json",
    "/usr/share/egl/egl_external_platform.d/10_nvidia_wayland.json",
    "/usr/share/egl/egl_external_platform.d/15_nvidia_gbm.json",
    "/usr/share/glvnd/egl_vendor.d/10_nvidia.json",
)
MAX_HOPS = 8
LIB_NAME = re.compile(r"^(?P<family>[A-Za-z0-9_-]+)\.so\.(?P<version>[0-9]+(?:\.[0-9]+)*)$")
DRIVER_LINE = re.compile(r"--host-driver-version=([0-9][0-9.]*)")
HOST_LINE = re.compile(r"^\s*- hostPath: (.+)$")
CONTAINER_LINE = re.compile(r"^\s*containerPath: (.+)$")
LINK_LINE = re.compile(r"^\s*- (.+?)::(.+)$")


class MountError(Exception):
    """A specification that cannot be used. The message names the reason and the file."""


def read_text(path):
    return pathlib.Path(path).read_text(encoding="utf-8")


def spec_driver_version(text):
    match = DRIVER_LINE.search(text)
    return match.group(1) if match else "SEM_VERSAO"


def choose_spec(driver_version, spec_paths):
    """Return the first spec whose --host-driver-version is the driver's; if none is, name every version seen."""
    seen = []
    for spec in spec_paths:
        version = spec_driver_version(read_text(spec))
        if version == driver_version:
            return spec
        seen.append(f"{spec} traz {version}")
    raise MountError(f"nenhuma especificacao casa com o driver {driver_version}: " + "; ".join(seen))


def check_plain_path(path):
    """Refuse an EMITTED path with a space or a colon: it cannot be written in the -v line without ambiguity.

    Only the origin and the destination of a printed mount are checked (D-W8-1752); an entry outside the list is
    ignored whatever it spells, like the by-path --link destinations of the real specification."""
    if " " in path or ":" in path:
        raise MountError(f"caminho com espaco ou dois-pontos recusado: {path!r}")
    return path


def parse_mounts(text):
    """Return [host, container] for each mounts entry, in file order. container is None when absent."""
    pairs = []
    for line in text.splitlines():
        host = HOST_LINE.match(line)
        if host:
            pairs.append([host.group(1).strip(), None])
            continue
        container = CONTAINER_LINE.match(line)
        if container and pairs and pairs[-1][1] is None:
            pairs[-1][1] = container.group(1).strip()
    return pairs


def allowed_key(host):
    """Return the key of an allow-listed mount (family name or JSON path), or None when it is outside the list."""
    if host in JSON_MOUNTS:
        return host
    if host.startswith(LIB_DIR):
        match = LIB_NAME.match(host[len(LIB_DIR):])
        if match and match.group("family") in LIBRARY_FAMILIES:
            return match.group("family")
    return None


def collect_allowed(pairs):
    """Return {key: host} for the allow-listed mounts. A family missing, repeated or re-targeted is refused by name."""
    found = {}
    for host, container in pairs:
        key = allowed_key(host)
        if key is None:
            continue
        if container != host:
            raise MountError(f"containerPath diferente do hostPath em {host}: {container}")
        found.setdefault(key, []).append(host)
    for key in LIBRARY_FAMILIES + JSON_MOUNTS:
        count = len(found.get(key, []))
        if count != 1:
            raise MountError(f"familia permitida {key} aparece {count} vez(es) na especificacao, esperada 1")
    return {key: hosts[0] for key, hosts in found.items()}


def parse_links(text):
    """Return (target, destination) for each --link entry, in file order."""
    links = []
    for line in text.splitlines():
        match = LINK_LINE.match(line)
        if match:
            links.append((match.group(1).strip(), match.group(2).strip()))
    return links


def link_table(links):
    """Return {destination: target}. The same --link repeated is one entry (the real specification repeats it once
    per device block, D-W8-1753); one destination with two different targets is refused."""
    table = {}
    for target, dest in links:
        known = table.setdefault(dest, target)
        if known != target:
            raise MountError(f"destino repetido nos --link com alvos diferentes: {dest} ({known} e {target})")
    return table


def resolve_link(dest, target, table, allowed_hosts):
    """Follow a --link through the table, at most MAX_HOPS steps, to an allow-listed host file; None when it leaves."""
    path = posixpath.normpath(posixpath.join(posixpath.dirname(dest), target))
    for _ in range(MAX_HOPS):
        if path in allowed_hosts:
            return path
        if path not in table:
            return None
        path = posixpath.normpath(posixpath.join(posixpath.dirname(path), table[path]))
    raise MountError(f"--link de {dest} passa de {MAX_HOPS} saltos sem chegar a um arquivo permitido")


def shortcut_mounts(allowed_hosts, driver_version):
    """For each library whose version differs from the driver's and has two or more numbers, a '.so.<first>' name."""
    shortcuts = []
    for host in allowed_hosts:
        if not host.startswith(LIB_DIR):
            continue
        match = LIB_NAME.match(host[len(LIB_DIR):])
        if match is None or match.group("version") == driver_version or "." not in match.group("version"):
            continue
        family, first = match.group("family"), match.group("version").split(".")[0]
        shortcuts.append((host, f"{LIB_DIR}{family}.so.{first}"))
    return shortcuts


def format_mounts(wanted):
    """Return 'ORIGIN:DESTINATION:ro' sorted by destination. One destination with two origins is refused."""
    by_dest = {}
    for origin, dest in wanted:
        check_plain_path(origin)
        check_plain_path(dest)
        known = by_dest.setdefault(dest, origin)
        if known != origin:
            raise MountError(f"destino {dest} com origens diferentes: {known} e {origin}")
    return [f"{origin}:{dest}:ro" for dest, origin in sorted(by_dest.items())]


def build_mounts(text, driver_version):
    """Return the sorted mount lines of one specification for the given driver."""
    allowed = collect_allowed(parse_mounts(text))
    hosts = sorted(allowed.values())
    table = link_table(parse_links(text))
    wanted = [(host, host) for host in hosts] + shortcut_mounts(hosts, driver_version)
    for dest, target in table.items():
        origin = resolve_link(dest, target, table, set(hosts))
        if origin is not None:
            wanted.append((origin, dest))
    return format_mounts(wanted)


def mounts_for_driver(driver_version, spec_paths):
    """Return the mount lines of the first specification that matches driver_version."""
    spec = choose_spec(driver_version, spec_paths)
    return build_mounts(read_text(spec), driver_version)


def replace_once(text, old, new):
    """Return text with the single occurrence of old replaced by new. A missing or repeated old is a test bug."""
    if text.count(old) != 1:
        raise AssertionError(f"trecho do autoteste aparece {text.count(old)} vez(es)")
    return text.replace(old, new, 1)


def sample_text():
    return SAMPLE.read_text(encoding="utf-8")


def attempt(driver_version, texts):
    """Run mounts_for_driver on specification texts written to a temporary directory.

    Returns ("ok", lines) or ("erro", reason). The reason is the MountError message, with the file name.
    """
    with tempfile.TemporaryDirectory() as tmp:
        paths = []
        for index, text in enumerate(texts):
            path = pathlib.Path(tmp) / f"spec{index}.yaml"
            path.write_text(text, encoding="utf-8")
            paths.append(str(path))
        try:
            return "ok", mounts_for_driver(driver_version, paths)
        except MountError as error:
            return "erro", str(error)


def selftest_sample_mounts():
    status, lines = attempt(SAMPLE_DRIVER, [sample_text()])
    return status == "ok" and tuple(lines) == EXPECTED_SAMPLE_MOUNTS


def selftest_driver_mismatch_is_refused():
    status, reason = attempt("610.43.03", [sample_text()])
    return status == "erro" and "spec0.yaml" in reason and SAMPLE_DRIVER in reason and "610.43.03" in reason


def selftest_first_matching_spec_wins():
    # The copy is the same specification written for another driver: if it were picked, its lines would name 610.43.03.
    other_driver = sample_text().replace(SAMPLE_DRIVER, "610.43.03")
    status, lines = attempt(SAMPLE_DRIVER, [other_driver, sample_text()])
    return status == "ok" and tuple(lines) == EXPECTED_SAMPLE_MOUNTS


def selftest_missing_family_is_named():
    status, reason = attempt(SAMPLE_DRIVER, [replace_once(sample_text(), GLSI_BLOCK, "")])
    return status == "erro" and "libnvidia-glsi" in reason


def selftest_nothing_outside_the_list_enters():
    status, lines = attempt(SAMPLE_DRIVER, [sample_text()])
    return status == "ok" and not any(word in line for line in lines for word in FORBIDDEN_IN_OUTPUT)


def selftest_gbm_link_points_to_the_real_file():
    status, lines = attempt(SAMPLE_DRIVER, [sample_text()])
    return status == "ok" and f"{SAMPLE_GBM_ORIGIN}:{SAMPLE_GBM}:ro" in lines


def selftest_space_or_colon_in_emitted_path_is_refused():
    verdicts = []
    for extra in (COLON_LINK, SPACE_LINK):
        text = replace_once(sample_text(), ALLOCATOR_LINK, ALLOCATOR_LINK + extra)
        status, reason = attempt(SAMPLE_DRIVER, [text])
        verdicts.append(status == "erro" and "espaco ou dois-pontos" in reason and "/usr/lib64/libEGL" in reason)
    return all(verdicts)


def selftest_duplicate_destination_is_refused():
    text = replace_once(sample_text(), ALLOCATOR_LINK, ALLOCATOR_LINK + DUPLICATE_LINK)
    status, reason = attempt(SAMPLE_DRIVER, [text])
    return status == "erro" and "/usr/lib64/libEGL_nvidia.so.0" in reason


def selftest_container_path_change_in_family_is_refused():
    text = replace_once(sample_text(), "containerPath: /usr/lib64/libEGL_nvidia.so.999.1.2",
                        "containerPath: /usr/lib64/libEGL_nvidia.so.999.9.9")
    status, reason = attempt(SAMPLE_DRIVER, [text])
    return status == "erro" and "libEGL_nvidia" in reason


def selftest_real_spec_links_are_accepted():
    text = replace_once(sample_text(), GBM_LINK, GBM_LINK + REAL_SPEC_LINKS * 3)
    status, lines = attempt(SAMPLE_DRIVER, [text])
    return status == "ok" and tuple(lines) == EXPECTED_SAMPLE_MOUNTS


def selftest():
    check("amostra: as 23 montagens esperadas, em ordem", selftest_sample_mounts())
    check("amostra: versao diferente da do driver e recusada pelo nome", selftest_driver_mismatch_is_refused())
    check("duas especificacoes: vale a primeira que bate", selftest_first_matching_spec_wins())
    check("familia permitida ausente: recusa nomeando a familia", selftest_missing_family_is_named())
    check("nada fora da lista entra", selftest_nothing_outside_the_list_enters())
    check("elo do gbm aponta para o arquivo real", selftest_gbm_link_points_to_the_real_file())
    check("caminho emitido com espaco ou dois-pontos: recusa", selftest_space_or_colon_in_emitted_path_is_refused())
    check("destino repetido: recusa", selftest_duplicate_destination_is_refused())
    check("containerPath diferente numa familia permitida: recusa",
          selftest_container_path_change_in_family_is_refused())
    check("CDI real: --link repetido igual e --link com dois-pontos fora da lista passam",
          selftest_real_spec_links_are_accepted())
    print(f"selftest: {len(CHECKS)} controles OK")
    return 0


def main(argv):
    if argv[1:] == ["--selftest"]:
        return selftest()
    if len(argv) < 3:
        print("uso: nvidia_cdi_mounts.py <versao_do_driver> <especificacao> [...] | --selftest", file=sys.stderr)
        return 2
    driver, specs = argv[1], argv[2:]
    try:
        spec = choose_spec(driver, specs)
        lines = build_mounts(read_text(spec), driver)
    except MountError as error:
        print(f"nvidia_cdi_mounts.py: {error}", file=sys.stderr)
        return 1
    print(f"nvidia_cdi_mounts.py: especificacao {spec} (driver {driver}), {len(lines)} montagens", file=sys.stderr)
    for line in lines:
        print("-v")
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
