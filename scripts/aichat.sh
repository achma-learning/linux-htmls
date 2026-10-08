#!/usr/bin/env bash
# aichat.sh -- open an AI chatbot in the DEFAULT browser, in a window that has
# its own WM_CLASS so i3 can assign it to its own workspace.
#
# Why a separate class: `assign [class="^Brave-origin$"] -> 5` would catch every
# browser window, including this one. Chromium- and Firefox-derived browsers
# both accept --class=<name>, which sets the second field of WM_CLASS, so the
# chatbot window is a different class from an ordinary browser window and the
# two can be assigned to different workspaces. The matching rule lives in
# ~/.config/i3/config ("WORKSPACE ASSIGNMENTS").
#
# Usage: aichat.sh [claude|chatgpt|deepseek|<url>]
set -euo pipefail

WM_CLASS_NAME=AIChat

case "${1:-claude}" in
  claude)   URL="https://claude.ai/new" ;;
  chatgpt)  URL="https://chatgpt.com/" ;;
  deepseek) URL="https://chat.deepseek.com/" ;;
  http*)    URL="$1" ;;
  *) echo "aichat.sh: unknown target '$1' (claude|chatgpt|deepseek|<url>)" >&2; exit 2 ;;
esac

# --- resolve the default browser -------------------------------------------
# xdg-settings names a .desktop file; read its Exec= and drop the field codes
# (%U, %u, %F...) so the URL can be appended explicitly.
desktop=$(xdg-settings get default-web-browser 2>/dev/null || true)
exec_line=""
if [ -n "$desktop" ]; then
  for d in "${XDG_DATA_HOME:-$HOME/.local/share}/applications" \
           /usr/local/share/applications /usr/share/applications; do
    if [ -r "$d/$desktop" ]; then
      exec_line=$(grep -m1 '^Exec=' "$d/$desktop" | cut -d= -f2-)
      break
    fi
  done
fi
[ -n "$exec_line" ] || exec_line=$(command -v xdg-open)
[ -n "$exec_line" ] || { echo "aichat.sh: no default browser found" >&2; exit 1; }

# shellcheck disable=SC2206
cmd=( ${exec_line//%[UuFfic]/} )
bin="${cmd[0]}"

case "$(basename "$bin")" in
  *firefox*|*librewolf*)
    # Firefox has no app mode; --class still sets WM_CLASS.
    exec "${cmd[@]}" --class="$WM_CLASS_NAME" --new-window "$URL"
    ;;
  *)
    # Chromium family (brave, chrome, chromium, vivaldi): --app gives a
    # chrome-less window, which is what makes it read as a chat app.
    exec "${cmd[@]}" --class="$WM_CLASS_NAME" --app="$URL"
    ;;
esac
