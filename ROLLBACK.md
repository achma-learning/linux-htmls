# ROLLBACK

## What was actually changed

Exactly one file in your live configuration: `~/.config/i3/config`.

Nothing else. Specifically **not** changed:

- **xfconf was never written to.** `/sessions/Failsafe/Client0_Command` is still `[xfwm4]`,
  its factory value, recorded in `backups/20260820-212843/xfconf/pristine-values.txt`.
  The original plan (Route A) would have changed it; Route B never needed to.
- **No session cache was cleared.** `~/.cache/sessions/` is untouched.
- `/etc`, `/usr`, and the display manager were never modified.
- `xfce.desktop` is intact and selectable at the login screen.
- The XFCE packages (`xfwm4`, `xfce4-session`, …) are all still installed.

## Rollback, easiest first

### 1. Undo the config (keeps you in i3)

```bash
~/i3-setup/undo.sh
```

Then press `$mod+Shift+c` to reload. You are back on the original wizard config: i3bar
returns, dmenu returns, the XFCE panel components stop being launched at startup.

Components already running (xfce4-panel and friends) are **not killed** by this — i3 does
not own them once started. They disappear on your next login. To stop them now:

```bash
xfce4-panel --quit ; pkill xfsettingsd ; pkill xfce4-power-manager
```

### 2. Log into XFCE instead

At the login screen, click the session chooser and pick **Xfce Session**. This has been
available the whole time and nothing in this project touched it. This is the full fallback:
xfwm4, the XFCE panel, the XFCE session manager, exactly as before i3.

### 3. If the graphical session will not start at all

Switch to a text console with **Ctrl+Alt+F3**, log in, and run:

```bash
~/i3-setup/undo.sh
```

Then Ctrl+Alt+F1 (or F2) to return to the login screen and log in again. If i3 itself is
broken rather than the config, pick the Xfce session at the login screen instead.

If `undo.sh` is unavailable, restore the config by hand:

```bash
cp ~/i3-setup/backups/<timestamp>/config-i3-wizard-original ~/.config/i3/config
```

`<timestamp>` — `ls ~/i3-setup/backups/` to see them; the first is `20260820-212843`.

### 4. Verifying xfconf was never touched

```bash
xfconf-query -c xfce4-session -p /sessions/Failsafe/Client0_Command
```

Expected: an array containing `xfwm4`. If it says anything else, restore it with:

```bash
xfconf-query -c xfce4-session -p /sessions/Failsafe/Client0_Command -t string -sa xfwm4
```

## Things worth knowing

**Reload is not restart, and neither re-runs `exec`.** `$mod+Shift+c` (reload) re-reads the
config and applies bindings, colours, and window rules immediately. It does **not** start or
stop the `exec` lines. `$mod+Shift+r` (restart) re-executes i3 itself, keeps your layout, and
re-runs only `exec_always` lines — which here is just `picom`. The five XFCE components use
plain `exec`, so a restart leaves the already-running ones alone rather than starting a
second copy of each.

**What this rollback cannot undo:** packages installed in Phase 1 (`picom`, `rofi`,
`brightnessctl`, `playerctl`, `xrandr`). They are inert if unused; remove them with
`sudo dnf remove` if you want. Nor can it un-run anything you did by hand outside this
project.
