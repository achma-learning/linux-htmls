#!/usr/bin/env bash
# Restore a window from i3's scratchpad, with the reverse of the fall
# animation, then drop the marker that scripts/minimize.sh set.
#
# The marker has to go: it is matched by a picom window rule, so a window that
# kept it would drop off the bottom of the screen every time you switched away
# from its workspace, not just when you minimize it.
#
# It is removed in the background after the `show` animation has had time to
# start (picom evaluates the rule when the trigger fires; removing the property
# mid-flight risks a re-match that restarts the animation).
set -euo pipefail

i3-msg -q 'scratchpad show' || exit 0

(
    sleep 0.6
    id=$(xprop -root -notype _NET_ACTIVE_WINDOW | awk '{print $NF}')
    case "$id" in
        0x0|"") exit 0 ;;
        0x*) xprop -id "$id" -remove _I3_MINIMIZED || true ;;
    esac
) >/dev/null 2>&1 &
