#!/usr/bin/env bash
# undo.sh — reverse everything this project changed to ~/.config/i3/config.
# Idempotent. Never runs sudo. Safe to run twice.

set -euo pipefail

SETUP_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
LIVE="$HOME/.config/i3/config"
ORIGINAL="$SETUP_DIR/backups/20260820-212843/config-i3-wizard-original"

say() { printf '%s\n' "$*"; }

if [ ! -f "$ORIGINAL" ]; then
    say "ERROR: original config not found at:"
    say "  $ORIGINAL"
    say "Look in $SETUP_DIR/backups/ for another timestamp and restore by hand."
    exit 1
fi

if [ -f "$LIVE" ] && cmp -s "$ORIGINAL" "$LIVE"; then
    say "Already reverted: $LIVE is byte-identical to the original wizard config."
else
    # Snapshot whatever is there now, so undo is itself undoable.
    if [ -f "$LIVE" ]; then
        SNAP="$SETUP_DIR/backups/undo-$(date +%Y%m%d-%H%M%S)"
        mkdir -p "$SNAP"
        cp -a "$LIVE" "$SNAP/config"
        say "Saved the current config to: $SNAP/config"
    fi
    mkdir -p "$(dirname "$LIVE")"
    cp -a "$ORIGINAL" "$LIVE"
    say "Restored the original wizard config to: $LIVE"
fi

# Validate what we just put in place, rather than assuming the backup is good.
if command -v i3 >/dev/null 2>&1; then
    if i3 -C -c "$LIVE" >/dev/null 2>&1; then
        say "Validated: i3 -C accepts the restored config."
    else
        say "WARNING: the restored config does NOT pass 'i3 -C'. Do not reload i3 yet."
        i3 -C -c "$LIVE" || true
        exit 1
    fi
fi

cat <<'NOTE'

Next step (nothing has been reloaded — this script does not touch your running session):
  press $mod+Shift+c in i3 to reload the restored config.

This script does NOT:
  - kill xfce4-panel / xfsettingsd / xfce4-power-manager / xfce-polkit if they are already
    running. They exit on your next login. To stop them now:
      xfce4-panel --quit ; pkill xfsettingsd ; pkill xfce4-power-manager
  - remove packages installed in Phase 1 (picom, rofi, brightnessctl, playerctl, xrandr).
  - touch xfconf. It was never modified; Client0_Command is still the factory 'xfwm4'.
  - restart or reload i3, restart X, or log you out.
NOTE
