#!/usr/bin/env bash
# autostart.sh -- the apps that should already be open when the session starts.
#
# ONE line per app. This is the file to edit when the daily set changes; the i3
# config just calls this once, with `exec` (not exec_always), so it runs at
# login and not again on every i3 restart.
#
# WHERE each app lands is mostly decided by the `assign` rules in
# ~/.config/i3/config, which key off WM_CLASS / instance / role, so opening one
# of those apps by hand later puts it in the same place as opening it at login.
#
# The TERMINAL and the FILE MANAGER are deliberately excluded from that. They
# are the two things you open constantly, and they should appear where you
# already are — so they have no assign rule, and are placed at login only: the
# terminal by the --role=main-term for_window rule, the file manager by
# place_once() below.
#
#   thunar                     -> workspace 1 AT LOGIN ONLY (place_once below)
#   xfce4-terminal             -> workspace 2 AT LOGIN ONLY, fullscreen
#                              (--role=main-term + a for_window rule)
#   deck                       -> workspace 3
#   brave-origin (default browser) -> workspace 5
#   AI Hub                     -> workspace 4
#   obsidian                   -> workspace 6
#   anki                       -> workspace 8, fullscreen
#   cliamp in a terminal       -> workspace 10   (via --role=cliamp)
#
# The stagger is not superstition: five apps racing for CPU and disk at login
# makes all five slow, and Anki and the browser are the two heavy ones. Nothing
# here depends on the order, so if you add an app just append a line.

set -u

# The i3 session does not necessarily inherit a login shell's PATH, and two of
# the apps below live in ~/.local/bin (deck, cliamp). Without this they fail the
# `command -v` test in launch() and are skipped with a message nobody reads.
export PATH="$HOME/.local/bin:$PATH"

launch() { command -v "$1" >/dev/null || { echo "autostart: $1 not installed, skipped" >&2; return; }; "$@" & }

# place_once <workspace-number> <i3 criteria> -- move a window there ONE time.
#
# Used instead of an `assign` rule for the file manager, because an assign rule
# is permanent: it would send every Thunar window you ever open to workspace 1,
# and the whole point is that opening one later should put it where you already
# are. This runs at login only.
#
# Polling rather than a fixed sleep: the move itself is the existence test.
# `i3-msg` answers {"success":false,"error":"Nothing to move: specified criteria
# don't match any window"} until the window is actually there, so retry until it
# succeeds or we give up.
place_once() {
    ws=$1; crit=$2
    for _ in $(seq 40); do
        if i3-msg "[$crit] move --no-auto-back-and-forth to workspace number $ws" 2>/dev/null \
             | grep -q '"success":true'; then
            return 0
        fi
        sleep 0.5
    done
    echo "autostart: gave up waiting for a window matching $crit" >&2
    return 1
}

# The browser is resolved at run time rather than hardcoded, the same way
# scripts/aichat.sh does it, so this keeps working if the default changes.
default_browser() {
  local desktop d exec_line
  desktop=$(xdg-settings get default-web-browser 2>/dev/null) || return 1
  for d in "${XDG_DATA_HOME:-$HOME/.local/share}/applications" \
           /usr/local/share/applications /usr/share/applications; do
    if [ -r "$d/$desktop" ]; then
      exec_line=$(grep -m1 '^Exec=' "$d/$desktop" | cut -d= -f2-)
      # shellcheck disable=SC2001
      echo "${exec_line//%[UuFfic]/}"
      return 0
    fi
  done
  return 1
}

launch thunar
# Thunar has no usable marker — it ignores --class, because its daemon is what
# creates the window, not the process we just started. So find it afterwards.
place_once 1 'class="^Thunar$"'

# --role=main-term does two jobs: it marks this as THE session terminal (i3
# opens it fullscreen on workspace 3) without making every terminal you open
# later fullscreen too.
launch xfce4-terminal --role=main-term
sleep 1

# cliamp is a terminal program, so it needs a terminal. --role is what makes
# this window distinguishable from the plain terminal above: xfce4-terminal
# gives every window the same WM_CLASS, but it passes --role through, and i3's
# `assign` can match on window_role. Verified with xprop.
launch xfce4-terminal --role=cliamp --title=cliamp --command=cliamp
sleep 2

# The AI Hub launcher. Its own script starts the little local server it needs
# and opens it as an --app window; see ai-hub/bin/ai-hub.
#
# Addressed RELATIVE TO THIS SCRIPT, not as ~/i3-setup/ai-hub. The hub and this
# script are two files in the same tree, and that tree is wherever the repo was
# cloned — a hardcoded ~/i3-setup is right on the machine this was captured
# from and wrong on every other one, where it fails silently and workspace 4
# stays empty with no error anywhere.
AI_HUB="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)/ai-hub/bin/ai-hub"
if [ -x "$AI_HUB" ]; then
  "$AI_HUB" >/dev/null 2>&1 &
else
  echo "autostart: no AI Hub at $AI_HUB, skipped" >&2
fi
sleep 2

# The deck — the orientation console on workspace 3 (day spine, the week, life
# in weeks). Its own launcher starts the static server it needs and opens it as
# an --app window; the i3 config assigns it by instance. Installed separately,
# to ~/.local/share/deck, so this is a plain `launch` like any other app.
launch deck
sleep 2

launch obsidian
sleep 2

if browser=$(default_browser); then
  # shellcheck disable=SC2086
  $browser &
else
  echo "autostart: no default browser found, skipped" >&2
fi
sleep 2

launch anki

wait
