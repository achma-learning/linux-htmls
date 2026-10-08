#!/usr/bin/env bash
# powermenu.sh — rofi-driven session actions for i3.
#
# Replaces an i3 "exit" binding mode. A mode's name string is drawn by i3bar,
# and this setup has no i3bar (xfce4-panel is the bar), so a mode's on-screen
# help would be invisible and its keys unmemorable. A rofi menu is visible.
#
# Contract:
#   - never runs sudo
#   - does NOTHING on invocation unless you pick an entry (empty pick = exit 0)
#   - destructive entries ask for confirmation a second time
#
# Reboot/poweroff go through systemd + polkit, which grants them to an active
# local session without a password. They are NOT sudo calls.

set -euo pipefail

SETUP_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v rofi >/dev/null 2>&1; then
    command -v notify-send >/dev/null 2>&1 && \
        notify-send "powermenu" "rofi is not installed — nothing to do."
    echo "powermenu.sh: rofi not found on \$PATH; refusing to act." >&2
    exit 1
fi

# Flags verified against the INSTALLED rofi 2.0.0 manpages, not from memory:
#   -dmenu      rofi(1):163
#   -i          rofi-dmenu(5):72   case-insensitive match
#   -no-custom  rofi-dmenu(5):97   only a listed entry can be returned
#   -p prompt   rofi-dmenu(5):57
# -no-custom matters here: without it, typing free text and pressing Enter would
# return that text, and the case statement below would fall through to exit 0 --
# harmless, but it also means no typo can ever be read as "Shut down".
THEME="$SETUP_DIR/rofi/macos-adjacent.rasi"
menu() { rofi -dmenu -i -no-custom -p "$1" -theme "$THEME"; }

choice=$(printf '%s\n' \
    "  Lock" \
    "  Log out" \
    "  Reboot" \
    "  Shut down" | menu "Power" || true)

# No selection (Escape, or rofi killed) -> do nothing at all.
[ -n "${choice:-}" ] || exit 0

confirm() {
    local answer
    answer=$(printf '%s\n' "No, cancel" "Yes, $1" | menu "Confirm: $1" || true)
    [ "${answer:-}" = "Yes, $1" ]
}

case "$choice" in
    *Lock)      loginctl lock-session ;;
    *"Log out") confirm "Log out"   && i3-msg exit ;;
    *Reboot)    confirm "Reboot"    && systemctl reboot ;;
    *"Shut down") confirm "Shut down" && systemctl poweroff ;;
    *)          exit 0 ;;
esac
