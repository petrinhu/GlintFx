# SPDX-License-Identifier: AGPL-3.0-or-later
#
# composed_photo_inside.sh - QA-COMPOSED-PATH G2: runs INSIDE the photo container, fed by
# `docker exec -i -e GLINTFX_PHOTO_EXAMPLE=<name> <container> sh -s < composed_photo_inside.sh`
# (tests/container/composed_photo.sh). Never on the host (GODS_LAWS.md L-09). It reads KWin's
# supportInformation, starts the example with --frames 1800 through the container's own socket,
# waits for "<name>: first frame presented", and asks ScreenShot2 for ONE capture of the active
# window (the file descriptor through a fifo). Every step writes its rc to /tmp/photo/rc.txt, and
# the first step that fails ends the script with its own code: nothing is swallowed.
set -u
O=/tmp/photo
rm -rf "$O" && mkdir -p "$O" || exit 9
rc() { echo "$1=$2" >> "$O/rc.txt"; }
[ -n "${GLINTFX_PHOTO_EXAMPLE:-}" ] || { rc example VAZIO; exit 9; }
kp=$(pgrep -x kwin_wayland | head -n 1)
[ -n "$kp" ] || { rc kwin_pid VAZIO; exit 10; }
rc kwin_pid "$kp"
rc dev_dri "$(for n in /dev/dri/*; do printf '%s ' "${n##*/}"; done)"
DBUS_SESSION_BUS_ADDRESS=$(tr '\0' '\n' < "/proc/$kp/environ" | sed -n 's/^DBUS_SESSION_BUS_ADDRESS=//p')
[ -n "$DBUS_SESSION_BUS_ADDRESS" ] || { rc dbus VAZIO; exit 11; }
export DBUS_SESSION_BUS_ADDRESS
qdbus-qt6 org.kde.KWin /KWin supportInformation > "$O/support.txt" 2> "$O/support.err"; s=$?
rc support "$s"
[ "$s" = 0 ] || exit 12
XDG_RUNTIME_DIR=/run/glintfx-test WAYLAND_DISPLAY=glintfx-test timeout 120 \
    "$GLINTFX_PHOTO_EXAMPLE" --frames 1800 > "$O/example.log" 2>&1 &
ex=$!
ready="$GLINTFX_PHOTO_EXAMPLE: first frame presented"
i=0
while [ "$i" -lt 60 ] && ! grep -q -F "$ready" "$O/example.log" 2>/dev/null; do
  kill -0 "$ex" 2>/dev/null || { rc ready MORREU; exit 13; }
  i=$((i + 1)); sleep 1
done
grep -q -F "$ready" "$O/example.log" || { rc ready TEMPO; kill "$ex" 2>/dev/null; exit 14; }
rc ready "${i}s"
sleep 2
mkfifo "$O/fifo" || { rc fifo FALHOU; kill "$ex" 2>/dev/null; exit 15; }
cat "$O/fifo" > "$O/img.raw" &
reader=$!
exec 3> "$O/fifo"
timeout 20 busctl --user call org.kde.KWin /org/kde/KWin/ScreenShot2 org.kde.KWin.ScreenShot2 \
    CaptureActiveWindow 'a{sv}h' 2 include-decoration b false include-shadow b false 3 \
    > "$O/cap.out" 2> "$O/cap.err"
c=$?
exec 3>&-
wait "$reader"
rc capture "$c"
rc bytes "$(stat -c %s "$O/img.raw")"
kill "$ex" 2>/dev/null
wait "$ex" 2>/dev/null
[ "$c" = 0 ] || exit 16
exit 0
