# Status — read this first in any new session

## Decision, final
i3 (with XFCE components, "Route B") is the daily driver. XFCE stock stays as the
login-screen fallback, untouched. KDE Plasma was evaluated and explicitly rejected as a
base — not attempted, no packages installed for it.

## What is APPLIED and confirmed working
- i3 4.25.1, XFCE components (xfsettingsd, xfce4-panel, xfce4-power-manager,
  thunar --daemon, xfce-polkit) launched via `exec` (not exec_always) in i3 config
- picom running: rounded corners, shadows, fading. Panel/dock distinguished by y=0 vs
  y!=0 (both are 1600px full-width, width alone can't tell them apart)
- rofi 2.0 as launcher + window switcher; power menu (scripts/powermenu.sh) replaced the
  old i3 exit mode, bound to $mod+Shift+e
- gaps inner 8, smart_gaps on, default_floating_border pixel 0
- Anki tiles normally (float rule deliberately NOT added — main window should tile)
- Animations (picom 13, all four states verified from picom's debug log on 2026-08-22):
  open/close, workspace switch (light scale+fade both ways), focus change (unfocused normal
  windows sit at 0.92 and the transition is animated), and minimize (shrink + fall to the
  bottom of the screen). $mod+m / $mod+Shift+m now go through scripts/minimize.sh and
  scripts/unminimize.sh, which tag the window with _I3_MINIMIZED so picom can tell a
  minimize apart from a workspace switch — both reach it as the same `hide` trigger.
- panel-2 restored after it was accidentally deleted: scripts/restore-panel-2.sh
- Both panels are plain, full-width, translucent bars — top and bottom the same, XFCE
  stock layout, styled only through per-panel xfconf (background-style 1 +
  background-rgba 0.11/0.11/0.118/0.75). Bottom bar sized down for a 900px-tall screen:
  size 36, icon-size 28, border-width 0 (window is 1600x36). gtk.css carries nothing but
  button-hover polish. This replaced a floating macOS-style dock on 2026-08-22 — see
  "Decisions taken and closed" below.
- Workspace assignments (verified live, each app opened and its workspace read back from
  i3's tree): AI chatbot -> 4, default browser -> 5, Anki -> 8, cliamp-in-a-terminal -> 10.
  The terminal and the file manager deliberately have NO assign rule — they open on the
  active workspace, and are placed on 3 and 1 at LOGIN ONLY (the terminal via a
  --role=main-term for_window rule, Thunar via place_once() in autostart.sh).
  $mod+Shift+a opens the chatbot via scripts/aichat.sh.
- AI Hub (ai-hub/): a one-screen keyboard launcher for 12 assistants + a userscript that
  injects a switcher button into the chatbots. Data lives in ai-hub/data/ai.json.
  $mod+Shift+a opens it; it lands on ws4 (verified from i3's tree). Meant to become its own
  git repo — it has its own README and is self-contained; nothing outside it depends on it
  except two lines of i3 config and one line of autostart.sh.
- Obsidian: AppImage, unpacked to ~/.local/opt/obsidian, `obsidian` wrapper on $PATH,
  installed/updated by scripts/obsidian-update.sh, which topgrade also runs. Lands on ws6
  (verified). Flatpak was installed first and then removed — the user asked for the
  AppImage specifically.
- i3-xfce-fedora/: a SEPARATE git repo (gitignored here) that rebuilds this whole desktop
  on a fresh Fedora box (install.sh), and capture.sh which pulls this machine back into
  it. Applications are one script each under core/apps/ — fastfetch, cmatrix, flameshot,
  xfdashboard, scrcpy, uad, and an opt-in bash-prompt (mybash).
  Structure follows ChrisTitusTech/linutil at the user's request. The config-side steps
  and the full --dry-run have been exercised here; the package-install steps have NOT
  been run on a fresh machine and say so in its README.
- Session autostart: scripts/autostart.sh, called once from the i3 config with `exec`
  (not exec_always). Opens thunar, the session terminal, cliamp, the AI Hub,
  Obsidian, the default browser and Anki, staggered. The session terminal (--role=main-term) and Anki come up fullscreen;
  both verified from i3's tree (fullscreen_mode = 1 on ws3 and ws8). Anki fills the area
  between the two bars rather than covering them — that is Anki's own remembered state,
  not the rule.
- All committed to git in this repo; ROLLBACK.md / undo.sh tested and working

## Still open, in priority order
1. xfce4-statusnotifier-plugin is **not installed** — `rpm -q` says so; an earlier note
   here claiming otherwise was wrong. Add it only if the tray needs SNI apps.
   (xfce4-docklike-plugin IS installed and is on panel-2 — plugin-24, the task list.)
2. Fn keys (XF86MonBrightness*, XF86Audio*) not yet verified via `xev` on this keyboard —
   bindings exist in config but firing is unconfirmed.
3. Full logout/login cycle not yet tested — everything so far was hand-started in a
   terminal. Need to confirm the five `exec` lines actually fire on real session start.

## Decisions taken and closed
**Floating macOS-style dock: TRIED, WORKED, THEN DELIBERATELY REMOVED (2026-08-22).**
The blocker is i3, not xfconf: i3 pins every _NET_WM_WINDOW_TYPE_DOCK client to the top or
bottom edge and stretches it to the output width. Measured on xfce4-panel 4.20.7 / i3
4.25.1: `/panels/panel-2/length` is ignored with `length-adjust` both true and false across
a full `xfce4-panel -q` + restart; `/panels/panel-2/position` is ignored; and
`i3-msg '[id=<dock>] floating enable'` returns success while the con stays
`floating: auto_off`.

It was made to work anyway, by painting instead of resizing: two end separators with
expand = true centred the plugins, and the panel's background was a generated PNG — a
rounded, content-width pill with a shadow baked in, centred by
`background-position: center` — regenerated on every i3 window event, because the docklike
task list changes width whenever a window opens or closes. It looked right.

It was removed anyway, at the user's call: too many moving parts to keep working across
xfce4-panel/i3/picom updates, and the plugin wrappers kept leaking through it (a plug is a
real X window whose pixels replace the panel's, so a transparent plug punches an actual
hole — visible as a blue sliver behind the Fedora menu icon and around the active task).
The code is in git history if it is ever wanted again; nothing of it is left in the tree.

If someone wants a real floating dock later, the honest options are a bar program that can
set override-redirect and so escape i3's dock handling (polybar), or plank / cairo-dock —
all three are in the Fedora 44 repos, none installed, and installing needs sudo.

## Where this is going
The point of the machine is **lifelong learning**, and the workspace layout is the shape of
it — every workspace is one mode of work, and the same app always lands in the same place:

    1  files        3  terminal      6  Obsidian (notes)    8  Anki (recall)
    4  AI           5  browser      10  music

Anki for recall and Obsidian for notes are the two halves; the AI workspace is the third
tool, not the point. When adding anything the test is whether it serves study or gets out
of the way — nothing that needs daily attention of its own.

Not designed yet, ask before assuming: how notes get from reading into Obsidian, and how
they get from Obsidian into Anki. That link is the actual system; everything so far is
only the furniture.

## Standing preferences (visual)
Simple over clever. The desktop should fade into the background: plain full-width bars top
and bottom, translucent, no ornament, nothing that needs babysitting after an update.
The current bars are CONFIRMED as what is wanted (user, 2026-08-22) — do not redesign them.
Rounded corners + fade animations on WINDOWS are still wanted (picom already does this).
Long-term wishlist, both confirmed technically blocked on this stack and not being chased
further: real macOS-style appmenu (no registrar/consumer packaged on Fedora 44), and
Compiz/KWin-style minimize-to-icon animation (picom animates rectangles only, no
_NET_WM_ICON_GEOMETRY support).

## Liked tools (install if missing, not yet all confirmed installed)
fastfetch, htop, topgrade, cmatrix — general utilities, unrelated to desktop config,
install opportunistically.
