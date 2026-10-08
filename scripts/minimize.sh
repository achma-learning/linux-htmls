#!/usr/bin/env bash
# Minimize the focused window to i3's scratchpad, with the shrink-and-fall
# animation.
#
# WHY THIS EXISTS: picom sees the same `hide` trigger whether a window is being
# minimized or is merely being left behind by a workspace switch (measured with
# picom --log-level=debug on 2026-08-22; see the trigger table in picom.conf).
# So the marker below is what tells them apart: picom.conf has a window rule
# matching `_I3_MINIMIZED@` that carries the heavy fall-to-the-bottom script,
# while every other window gets the light global hide/show pair.
#
# The property is set BEFORE `move scratchpad` on purpose. X delivers the
# PropertyNotify before the UnmapNotify that follows it, so picom has already
# re-matched the rule by the time the hide animation starts.
set -euo pipefail

id=$(xprop -root -notype _NET_ACTIVE_WINDOW | awk '{print $NF}')
case "$id" in
    0x0|"") exit 0 ;;
    0x*) ;;
    *) exit 0 ;;
esac

# 32c = 32-bit CARDINAL. The value is never read; only its presence matters.
xprop -id "$id" -f _I3_MINIMIZED 32c -set _I3_MINIMIZED 1
i3-msg -q move scratchpad
