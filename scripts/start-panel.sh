#!/usr/bin/env bash
# start-panel.sh -- start xfce4-panel, but not before a compositor is running.
#
# WHY THIS EXISTS
#   Every out-of-process panel plugin (whiskermenu, the docklike task list) is a
#   GtkPlug in its own process, and GTK picks that window's visual ONCE, when it
#   starts. If no compositing manager owns _NET_WM_CM_S0 at that moment, the plug
#   gets a plain RGB visual and paints an opaque rectangle for the rest of the
#   session — which is the dark block behind the Fedora menu icon and behind the
#   active window in the task list. Restarting the panel afterwards fixes it,
#   which is why this only ever showed up on a fresh login.
#
#   i3 runs `exec` lines in config order, so picom's line is now above the panel's
#   — but "started" is not "ready": picom still has to acquire the selection. So
#   wait for the selection itself rather than guessing with a sleep.
#
# Falls through and starts the panel anyway after the timeout: a bar with the
# wrong visual is still far better than no bar.

set -uo pipefail

TIMEOUT=${PANEL_WAIT:-10}     # seconds

compositor_running() {
    python3 - <<'PY'
import ctypes, ctypes.util, sys
lib = ctypes.util.find_library("X11")
if not lib:
    sys.exit(1)
x11 = ctypes.CDLL(lib)
x11.XOpenDisplay.restype = ctypes.c_void_p
d = x11.XOpenDisplay(None)
if not d:
    sys.exit(1)
d = ctypes.c_void_p(d)
# _NET_WM_CM_S<screen> is the selection a compositing manager owns (EWMH).
atom = x11.XInternAtom(d, b"_NET_WM_CM_S0", False)
owner = x11.XGetSelectionOwner(d, atom)
sys.exit(0 if owner else 1)
PY
}

waited=0
while ! compositor_running; do
    if [ "$waited" -ge "$TIMEOUT" ]; then
        echo "start-panel: no compositor after ${TIMEOUT}s — starting the panel anyway." >&2
        echo "start-panel: plugin backgrounds may be opaque; 'xfce4-panel -r' fixes it." >&2
        break
    fi
    sleep 0.5
    waited=$((waited + 1))
done

exec xfce4-panel "$@"
