#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_configure_ack_trace.py - WL-ACK-CONFIGURE-SEGUINTES: judges a WAYLAND_DEBUG=1 trace of one client against
# what xdg-shell.xml asks of xdg_surface.ack_configure. The compositor in the test container (KWin) never complains
# about a missing ack (docs/plano-w7c.md, F4), so only the trace can say whether the library acknowledged at all.
#
# What fails the trace, for each xdg_surface:
#   sem_ack: the newest configure was never acknowledged, with two or more buffer commits after it. One commit does
#     not count: the commit right after a resize carries a buffer of the old size and must not answer the configure.
#   tamanho_errado: the first buffer commit after an ack carries a buffer of another size than the acknowledged
#     configure asked for (weston ends the connection for this; SDL issue 4563). A configure of 0x0 asks no size.
#   serial_invalido: an ack of a serial never sent to this surface, or not newer than the last ack (xdg-shell.xml,
#     invalid_serial; a serial can only be consumed once).
#   Scan floor (GODS_LAWS.md L-40): fewer configures, buffer commits or resizing configures (a configure that asks for
#     another size than the last committed buffer has) than the caller demands fail too, so a trace that exercised
#     nothing is never a pass. How many configures a compositor sends varies from run to run: KWin may merge two.
#
# Object numbers are never compared with a fixed value: they are ids of the protocol, and move when a request above
# them moves. Every id is learned from the trace itself (get_xdg_surface, get_toplevel, create_buffer).
#
# USAGE: check_configure_ack_trace.py <trace> --min-configures N --min-commits N --min-resizes N
#        check_configure_ack_trace.py --selftest
# EXIT: 0 passed; 1 failed; 2 bad usage. The verdict line is always printed, with its counts.
import argparse
import re
import sys

LINE = re.compile(r"^\[[^\]]*\]\s*(?:\{[^}]*\}\s*)?(?P<sent>-> )?(?P<iface>\w+)[#@](?P<id>\d+)\.(?P<msg>\w+)\((?P<args>.*)\)\s*$")
NEW_ID = re.compile(r"new id \w+[#@](\d+)")
OBJECT = re.compile(r"\w+[#@](\d+)")


class surface_record:
    def __init__(self):
        self.configures = {}  # serial -> (width, height), or None for 0x0
        self.newest_configure = None
        self.newest_ack = None
        self.commits_after_newest = 0
        self.invalid_serials = 0
        self.resizes = 0
        self.last_buffer_size = None


class trace_state:
    def __init__(self):
        self.toplevel_of = {}  # xdg_toplevel id -> xdg_surface id
        self.xdg_of_wl = {}  # wl_surface id -> xdg_surface id
        self.asked_size = {}  # xdg_toplevel id -> (width, height), the configure not yet closed by xdg_surface.configure
        self.buffers = {}  # wl_buffer id -> (width, height)
        self.attached = {}  # wl_surface id -> wl_buffer id
        self.ack_waiting = {}  # wl_surface id -> serial acknowledged and not yet answered by a buffer commit
        self.surfaces = {}  # xdg_surface id -> surface_record
        self.buffer_commits = 0
        self.wrong_sizes = 0

    def record(self, xdg_id):
        return self.surfaces.setdefault(xdg_id, surface_record())


def first_two_ints(args):
    fields = [field.strip() for field in args.split(",")]
    return int(fields[0]), int(fields[1])


def on_shell_request(state, message):
    new_id = int(NEW_ID.search(message["args"]).group(1))
    if message["msg"] == "get_xdg_surface":
        wl_id = int(OBJECT.findall(message["args"].split(",", 1)[1])[0])
        state.xdg_of_wl[wl_id] = new_id
        state.record(new_id)
    else:
        state.toplevel_of[new_id] = int(message["id"])


def on_configure(state, message):
    if message["iface"] == "xdg_toplevel":
        state.asked_size[int(message["id"])] = first_two_ints(message["args"])
        return
    xdg_id = int(message["id"])
    serial = int(message["args"])
    record = state.record(xdg_id)
    asked = next((state.asked_size.pop(top) for top, xdg in state.toplevel_of.items()
                  if xdg == xdg_id and top in state.asked_size), None)
    size = asked if asked and asked != (0, 0) else None
    record.configures[serial] = size
    record.newest_configure = serial
    record.commits_after_newest = 0
    if size is not None and record.last_buffer_size is not None and size != record.last_buffer_size:
        record.resizes += 1


def on_ack(state, message):
    xdg_id = int(message["id"])
    serial = int(message["args"])
    record = state.record(xdg_id)
    if serial not in record.configures or (record.newest_ack is not None and serial <= record.newest_ack):
        record.invalid_serials += 1
    record.newest_ack = serial
    for wl_id, xdg in state.xdg_of_wl.items():
        if xdg == xdg_id:
            state.ack_waiting[wl_id] = serial


def on_commit(state, wl_id):
    buffer_id = state.attached.pop(wl_id, None)
    xdg_id = state.xdg_of_wl.get(wl_id)
    if buffer_id is None or xdg_id is None:
        return
    state.buffer_commits += 1
    record = state.record(xdg_id)
    record.commits_after_newest += 1
    record.last_buffer_size = state.buffers.get(buffer_id)
    serial = state.ack_waiting.pop(wl_id, None)
    if serial is None:
        return
    asked = record.configures.get(serial)
    if asked is not None and state.buffers.get(buffer_id) != asked:
        state.wrong_sizes += 1
        print(f"tamanho_errado: ack_configure({serial}) pediu {asked}, buffer {state.buffers.get(buffer_id)}")


def feed(state, line):
    found = LINE.match(line)
    if found is None:
        return
    message = found.groupdict()
    name = message["iface"] + "." + message["msg"]
    sent = message["sent"] is not None
    if sent and name in ("xdg_wm_base.get_xdg_surface", "xdg_surface.get_toplevel"):
        on_shell_request(state, message)
    elif not sent and name in ("xdg_toplevel.configure", "xdg_surface.configure"):
        on_configure(state, message)
    elif sent and name == "xdg_surface.ack_configure":
        on_ack(state, message)
    elif sent and name == "wl_shm_pool.create_buffer":
        new_id = int(NEW_ID.search(message["args"]).group(1))
        fields = [field.strip() for field in message["args"].split(",")]
        state.buffers[new_id] = (int(fields[2]), int(fields[3]))
    elif sent and name == "zwp_linux_buffer_params_v1.create_immed":
        new_id = int(NEW_ID.search(message["args"]).group(1))
        fields = [field.strip() for field in message["args"].split(",")]
        state.buffers[new_id] = (int(fields[1]), int(fields[2]))
    elif sent and name == "wl_surface.attach" and not message["args"].startswith("nil"):
        state.attached[int(message["id"])] = int(OBJECT.search(message["args"]).group(1))
    elif sent and name == "wl_surface.commit":
        on_commit(state, int(message["id"]))


def judge(lines, min_configures, min_commits, min_resizes):
    state = trace_state()
    for line in lines:
        feed(state, line)
    records = state.surfaces.values()
    configures = sum(len(record.configures) for record in records)
    invalid = sum(record.invalid_serials for record in records)
    resizes = sum(record.resizes for record in records)
    no_ack = sum(1 for record in records if record.newest_configure is not None
                 and record.newest_ack != record.newest_configure and record.commits_after_newest >= 2)
    print(f"configure_ack_trace: superficies={len(state.surfaces)} configures={configures} "
          f"redimensionamentos={resizes} commits_com_buffer={state.buffer_commits} "
          f"tamanho_errado={state.wrong_sizes} sem_ack={no_ack} serial_invalido={invalid}")
    floor_ok = configures >= min_configures and state.buffer_commits >= min_commits and resizes >= min_resizes
    if not floor_ok:
        print(f"configure_ack_trace: FAIL piso: pedidos {min_configures} configures, {min_commits} commits, "
              f"{min_resizes} redimensionamentos")
    return floor_ok and state.wrong_sizes == 0 and no_ack == 0 and invalid == 0


def fake_trace(steps):
    prefix = "[10:00:00.000000] {Default Queue} "
    head = ["-> xdg_wm_base#4.get_xdg_surface(new id xdg_surface#6, wl_surface#5)",
            "-> xdg_surface#6.get_toplevel(new id xdg_toplevel#7)"]
    return [prefix + step for step in head + steps]


def configure(width, height, serial):
    return [f"xdg_toplevel#7.configure({width}, {height}, array[0])", f"xdg_surface#6.configure({serial})"]


def commit(buffer_id, width, height):
    return [f"-> wl_shm_pool#13.create_buffer(new id wl_buffer#{buffer_id}, 0, {width}, {height}, 2560, 0)",
            f"-> wl_surface#5.attach(wl_buffer#{buffer_id}, 0, 0)", "-> wl_surface#5.commit()"]


def selftest():
    first = configure(0, 0, 1) + ["-> xdg_surface#6.ack_configure(1)"] + commit(14, 640, 480)
    held = configure(800, 600, 7) + commit(15, 640, 480) + ["-> xdg_surface#6.ack_configure(7)"] + commit(16, 800, 600)
    cases = [
        ("the ack waits one commit: green", first + held, True, 2, 3, 0),
        ("two configures, one ack: red", first + configure(640, 480, 7) + commit(15, 640, 480) + commit(16, 640, 480), False, 2, 3, 0),
        ("ack with the old size: red", first + configure(800, 600, 7) + ["-> xdg_surface#6.ack_configure(7)"] + commit(15, 640, 480), False, 2, 2, 0),
        ("ack of a serial never sent: red", first + ["-> xdg_surface#6.ack_configure(9)"] + commit(15, 640, 480), False, 1, 2, 0),
        ("the same serial twice: red", first + ["-> xdg_surface#6.ack_configure(1)"] + commit(15, 640, 480), False, 1, 2, 0),
        ("the newest of two, the older unacked: green", first + configure(640, 480, 7) + configure(640, 480, 9) + ["-> xdg_surface#6.ack_configure(9)"] + commit(15, 640, 480), True, 2, 2, 0),
        ("only one commit after the configure: not judged yet", first + configure(800, 600, 7) + commit(15, 640, 480), True, 2, 2, 0),
        ("empty trace: red by the floor", [], False, 1, 1, 0),
        ("no resize when one was demanded: red by the floor", first + configure(640, 480, 7) + ["-> xdg_surface#6.ack_configure(7)"] + commit(15, 640, 480), False, 1, 1, 1),
        ("a resize counts against the last buffer, not the last request", first + configure(800, 600, 7) + commit(15, 640, 480) + ["-> xdg_surface#6.ack_configure(7)"] + commit(16, 800, 600), True, 2, 3, 1),
    ]
    failed = 0
    for name, lines, expected, min_configures, min_commits, min_resizes in cases:
        got = judge(fake_trace(lines), min_configures, min_commits, min_resizes)
        print(f"selftest: {name}: {'ok' if got == expected else 'FALHOU'}")
        failed += got != expected
    print(f"selftest: encontrados={len(cases)} analisados={len(cases)} falharam={failed}")
    return failed == 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", nargs="?")
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--min-configures", type=int, default=2)
    parser.add_argument("--min-commits", type=int, default=2)
    parser.add_argument("--min-resizes", type=int, default=0)
    args = parser.parse_args()
    if args.selftest:
        return 0 if selftest() else 1
    if args.trace is None:
        parser.error("trace missing")
    with open(args.trace, encoding="utf-8", errors="replace") as trace:
        passed = judge(trace, args.min_configures, args.min_commits, args.min_resizes)
    print("configure_ack_trace: " + ("ok" if passed else "FAIL"))
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
