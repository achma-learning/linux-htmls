# ROLE
You are configuring an i3 window manager setup on my personal laptop. I am new to i3.
This is my daily-driver machine and my study machine — a broken login is a real cost, not
a minor annoyance. Prioritise reversibility over elegance at every decision point.

# MACHINE (VERIFIED in Phase 0, 2026-08-20 — no longer assumptions)
- HP EliteBook 840 G2, i5-5200U Broadwell, HD Graphics 5500. Confirmed via DMI + lspci.
- Fedora 44, X11 (`XDG_SESSION_TYPE=x11`, logind `Type=x11`). Confirmed.
- i3 4.25.1 installed AND CURRENTLY RUNNING as the session WM. `DESKTOP_SESSION=i3`,
  logind `Desktop=i3`, `_NET_WM_NAME="i3"`. I log into `/usr/share/xsessions/i3.desktop`.
  I DO NOT log into XFCE. xfce4-session, xfce4-panel, xfsettingsd, xfdesktop are NOT running.
- `~/.config/i3/config` already exists: i3-config-wizard output, with a `bar {}` block and
  `dmenu`. i3bar ("bar-0") is live and is currently hosting the tray icons.
- dex-autostart IS running my XDG autostart entries with `--environment i3`.
- Outputs: eDP-1 connected; DP-1, DP-2, HDMI-A-1, HDMI-A-2 disconnected. Backlight:
  intel_backlight (root:root 0644 — I am not in a `video` group).
- Audio: PipeWire 1.6.8 + WirePlumber; `wpctl` and `@DEFAULT_AUDIO_SINK@` both work.
- `xrandr` is NOT installed. Detect outputs from /sys/class/drm until Phase 1 installs it.
- Sans fonts actually present: Adwaita Sans, Cantarell, Liberation Sans, Noto Sans.
  DejaVu Sans (proportional) is NOT installed — only DejaVu Sans Mono.
- Package-name traps: the file manager is `Thunar` (capital T), not `thunar`; dex is
  `dex-autostart`, not `dex`. Both installed. Never conclude "missing" from `rpm -q` alone.
- METHOD NOTE: never pipe stderr into a filter (`cmd 2>&1 | grep foo`). In Phase 0 that
  turned `xrandr: command not found` into silent empty output that read as "no outputs
  connected". Check exit status, or test with `command -v`, before believing a negative.

# OBJECTIVE — ROUTE B (supersedes the original xfconf plan)
Keep logging into the i3 session, and run the XFCE components I want AS CLIENTS OF I3,
started from `~/.config/i3/config`:

    exec --no-startup-id xfsettingsd          # GTK theme/font/DPI for every app
    exec --no-startup-id xfce4-panel          # the bar (both my panels)
    exec --no-startup-id xfce4-power-manager  # lid, battery, brightness keys
    exec --no-startup-id thunar --daemon      # file manager daemon

Route A (the `KDEWM` trick: xfconf `/sessions/Failsafe/Client0_Command` = i3) is REJECTED.
It only makes sense if I log into XFCE, and Phase 0 proved I do not. Route B reaches the
same end state by touching ONE file, rolls back with one `git checkout`, and leaves
`xfce.desktop` and the xfconf database completely pristine as my fallback.

DO NOT modify xfconf `Client0_Command`. DO NOT clear the session cache. Those were Route A
steps and they are now out of scope.

Aesthetic target: minimal, macOS-adjacent — thin/no borders, subtle compositing.
BOTH existing panels stay: panel-1 (top, systray + clock + pager + tasklist) and panel-2
(the 48px autohiding launcher dock). The dock is the macOS half of the look. The earlier
"a single top panel" line is SUPERSEDED — it was written before I knew the dock existed.

# HARD CONSTRAINTS — violating any of these is a failed task
1. NEVER run `sudo`. When a root command is needed, print it in a fenced block, explain what
   it does, and STOP until I confirm I ran it.
2. NEVER modify anything under /etc, /usr, or the display manager configuration.
3. NEVER delete or disable the existing XFCE session entry. It is my rollback path.
4. Back up every file before first modification to `~/i3-setup/backups/<timestamp>/`,
   preserving relative paths. The original xfconf Client0_Command value (`[xfwm4]`) and the
   original wizard i3 config are ALREADY recorded in backups/20260820-212843/. Under Route B
   that xfconf value is never changed — it is kept as evidence the DB stayed pristine.
5. Every config path uses `$HOME` or `~`. Zero hardcoded `/home/<username>/`.
6. Zero hardcoded device indices. No `pactl set-sink-volume 2`. Use `@DEFAULT_AUDIO_SINK@`.
   No hardcoded output names — detect from `xrandr` once Phase 1 installs it, or from
   /sys/class/drm/card*-*/status before then.
7. Verify a package exists in Fedora 44 repos (`dnf list --available <pkg>` or `dnf search`)
   BEFORE telling me to install it. Do not install the `i3-desktop-environment` group.
   If a package you expected is absent, say so and propose an alternative — never invent
   package names.
8. Validate the i3 config with `i3 -C -c <file>` after every edit. A config that fails
   validation must never be moved into place.
9. Do not restart X, log me out, or run `i3-msg restart`. I control session restarts.

# GROUND TRUTH — hierarchy of authority
When any config syntax question arises, consult sources in THIS order and say which one
you used. Never answer a syntax question from your own recollection.

1. `i3 -C -c <file>` against the INSTALLED binary — it asks the binary itself instead of
   asking documentation about the binary. There is NO `man 5 i3` on this machine; i3
   documents config syntax in the userguide, not a man page. `man 1 i3` = CLI flags only.
   CAVEAT: `i3 -C` checks SYNTAX ONLY. It does not verify that an exec target exists on
   $PATH, that a for_window/assign criterion ever matches a real window, or that a
   directive does anything in this version. A config can pass and still be inert.
2. `~/i3-setup/docs/userguide.txt` (vendored in pre-flight) — top authority for SYNTAX.
3. https://i3wm.org/docs/userguide.html — same text as #2. The version question is CLOSED:
   installed i3 is 4.25.1 and the site documents 4.25.1, so they match exactly.
4. https://github.com/i3/i3 — use ONLY to answer "when was this feature introduced?" by
   reading `RELEASE-NOTES-*` files, and to consult the upstream default config at
   `etc/config`. Do not copy configs from anywhere else on GitHub.

If a directive is not in the vendored userguide, or `i3 -C` rejects it on this binary, you
do not have it. Do not write it "in case it works." Say so and propose an alternative.
Conversely, remember that PASSING `i3 -C` is not proof a line works — see the caveat above.

# PRE-FLIGHT (do this before Phase 0)
Vendor the reference offline so you can grep it instead of guessing:

    mkdir -p ~/i3-setup/docs
    curl -sL https://i3wm.org/docs/userguide.html -o ~/i3-setup/docs/userguide.html
    curl -sL https://i3wm.org/docs/refcard.html   -o ~/i3-setup/docs/refcard.html
    python3 -c "import html,re,sys;t=open(sys.argv[1]).read();t=re.sub(r'<[^>]+>','',t);print(html.unescape(t))" \
      ~/i3-setup/docs/userguide.html > ~/i3-setup/docs/userguide.txt
    man -P cat 1 i3 > ~/i3-setup/docs/man-i3.txt        # section 1 only; there is no man 5 i3

Commit these as `docs: vendor i3 reference` so the reference is pinned alongside the config
it justifies. Then, in every config comment that uses a non-obvious directive, cite the
userguide section name — e.g. `# see userguide: "Changing (named) workspaces"`.

# WHAT TO IGNORE IN THOSE SOURCES
- The "For developers" docs (hacking-howto, ipc, testsuite, i3bar-protocol) — irrelevant
  unless I ask for scripting against i3's IPC.
- The "User-contributed articles" section — dated 2012–2014, same bit-rot problem as any
  old dotfiles repo. Do not use them as templates.
- Anything about `bar { }`, i3bar, i3status, or i3blocks — I am using xfce4-panel.
  If you catch yourself configuring i3bar, you have misread the objective.

# PHASES — stop and wait for my approval after each
**Phase 0 — Audit.** Report the machine facts above, what's already installed, what's missing,
and the exact package list you propose with one line each on why. No changes.

**Phase 1 — Packages.** Print the `sudo dnf install` command(s). Wait.
Must include `xrandr` (absent, and VERIFY.md's external-display step needs it). Resolve its
package name empirically with `dnf provides '*/bin/xrandr'` — do not guess it.

**Phase 2 — i3 config.** NOT GREENFIELD. `~/.config/i3/config` already exists (wizard
output) and is the config of the session I am currently sitting in. Rewrite it IN PLACE —
bar block out, dmenu out, rofi in — and show me a `diff` of old vs new so I can see every
single change. Do not generate an unrelated fresh file and do not copy any dotfiles repo.
Requirements:
- `set $mod Mod4`, colour and workspace names defined as variables at the top, referenced
  everywhere. Workspaces named via `set $ws1 "1"` style so renaming is a one-line change.
- Vim-direction focus/move (h/j/k/l), plus arrow-key duplicates for while I'm learning.
- Bindings for: terminal (xfce4-terminal), Thunar, rofi drun, rofi window switcher, kill,
  fullscreen, floating toggle, split h/v, tabbed/stacking layouts, reload, restart.
- A `resize` mode and an `exit` mode, with the mode name string acting as on-screen help.
- Laptop keys: volume via wpctl (verify PipeWire first), brightness via brightnessctl,
  media keys via playerctl, Print via xfce4-screenshooter.
- `hide_edge_borders both`, thin borders, a sans font that is actually installed on this box.
- NO `bar { }` block — xfce4-panel is the bar. Say so in a comment.
- Floating rules for XFCE dialogs, pavucontrol, blueman. NOT Anki: Phase 0 found no Anki
  on this machine (no rpm, no binary, no flatpak), so its window criteria cannot be
  verified — and unverifiable criteria are exactly the `i3 -C` blind spot above. Omit the
  rule and document the recipe in CHEATSHEET.md instead (`xprop | grep WM_CLASS`, click
  the window, add the line myself). Never write a criterion you could not test.
- `exec_always picom` only if Phase 1 installed it.
- Every non-obvious line gets a short comment explaining WHY, because I am learning.
Show me the file for review before writing it.

**Phase 3 — XFCE components (replaces the old "Session wiring" phase).** Add the four
`exec --no-startup-id` lines above to the i3 config. No xfconf, no session cache, no
display-manager anything. Before adding each one, CONFIRM dex-autostart is not already
starting it (check OnlyShowIn/NotShowIn in /etc/xdg/autostart against environment `i3`)
and say so, rather than silently duplicating a running process.
Note the xfwm4 compositor setting is irrelevant now — xfwm4 never starts in an i3 session.
Tell me concretely what I lose by not running xfce4-session.

**Phase 4 — Deliverables.**
- `~/i3-setup/ROLLBACK.md`: step-by-step recovery, including the TTY route (Ctrl+Alt+F3)
  if the graphical session fails. Route B rollback is `git checkout` of the config plus
  reload. Also record that `xfce.desktop` remains selectable at the login screen as the
  full fallback, and that xfconf was never touched (pristine value recorded in backups/).
- `~/i3-setup/undo.sh`: idempotent, non-sudo, reverses everything you changed. Explain what
  it cannot reverse.
- `~/i3-setup/VERIFY.md`: a numbered checklist of what I should test after logging in
  (tiling works, panel visible and reserving space, tray icons, brightness keys, volume keys,
  Thunar opens, rofi opens, lid-close suspends, external display via dock).
  Include a check that the tray icons survived the i3bar -> xfce4-panel systray handover.
- `~/i3-setup/CHEATSHEET.md`: every binding you created, grouped, one line each.
- Git-init `~/i3-setup`, symlink or copy the i3 config in, commit each phase separately with
  a clear message.

# ANTI-PATTERNS — I have already been burned by these in old i3 dotfiles
- `~/.i3/config` (old path), `compton` (dead, use picom), `xbacklight` (broken on Broadwell),
  `xmodmap` (use setxkbmap options), rofi CLI theming flags like `-bg`/`-fg`/`-hlbg`
  (removed in rofi 1.4+ — use a .rasi theme), `i3-gaps` (merged into i3 since 4.22).
- If you find yourself copying a config pattern from memory of a popular dotfiles repo,
  stop and check whether the underlying tool still supports it in 2026.

# COMMUNICATION
Explain the mechanism, not just the command — I want to understand X11 struts, why the
xfconf key controls the WM, and what picom actually does. Be concise. If something is
genuinely risky or you are uncertain, say so plainly rather than proceeding.
