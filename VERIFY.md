# VERIFY — post-apply checklist

Work through these in order. Each says what "pass" looks like and what to do on failure.

**Before anything:** `i3 -C -c ~/.config/i3/config` must print nothing and exit 0.
It only checks syntax — it cannot tell you a binding points at a missing program, so the
rest of this list is not optional.

## A. Bring the components up by hand (no logout needed)

`exec` lines run only when i3 **starts**. Reloading does not run them, so on the first pass
you start them yourself. Run in a terminal — none of this needs root:

```bash
xfsettingsd &
xfce4-panel &
xfce4-power-manager &
thunar --daemon &
/usr/libexec/xfce-polkit &
```

Then reload i3 to pick up the new config: **`$mod+Shift+c`**.

> **Expected, not a bug:** the moment `xfce4-panel` starts, your tray icons (network,
> bluetooth, updates) migrate from i3bar to the panel's systray. i3bar itself disappears
> only once the new config is live. Seeing both at once means you started the panel but
> have not reloaded yet, or reloaded but not started the panel. Do both and it settles.

From your **next login** onward this section is automatic — that is what the `exec` lines do.

## B. Checklist

1. **Tiling works.** Open two terminals (`$mod+Return` twice). They split the screen, no
   overlap, no titlebars — just a 1px hairline, lighter on the focused one.
2. **Vim focus keys.** `$mod+h/j/k/l` moves focus. `$mod+←↓↑→` does the same.
   *These are not the stock bindings* — the wizard used `j/k/l/semicolon`.
3. **Splits.** `$mod+b` then `$mod+Return` → new terminal appears **beside**. `$mod+v` then
   `$mod+Return` → **below**. (`$mod+h` is focus now, not split.)
4. **Panel visible and reserving space.** The panel is on screen *and* no tiled window slides
   underneath it. Maximise a terminal and confirm it stops at the panel edge.
   → If windows go under it, the panel's `_NET_WM_STRUT_PARTIAL` is not being honoured;
     report it rather than working around it with a gap.
5. **The dock.** Panel-2 (48px, autohiding) still appears when you push the pointer to its
   screen edge, and does not permanently reserve space.
6. **Tray icons.** Network, bluetooth and update icons are in the panel systray and respond
   to clicks. Compare against section A's note before calling this a failure.
7. **rofi launcher.** `$mod+d` opens rofi, typing filters apps, Enter launches.
8. **rofi window switcher.** `$mod+Tab` lists open windows and switches to the one you pick.
9. **Thunar.** `$mod+Shift+Return` opens a file manager window.
10. **Volume keys.** Fn volume up/down/mute change the volume. Verify with `wpctl get-volume
    @DEFAULT_AUDIO_SINK@` before and after. Volume up must stop at `1.00`, not exceed it.
11. **Mic mute key** toggles — `wpctl get-volume @DEFAULT_AUDIO_SOURCE@` shows `[MUTED]`.
12. **Brightness keys.** Fn brightness up/down visibly change the screen. Cross-check
    `cat /sys/class/backlight/intel_backlight/brightness`.
    → If nothing happens, run `brightnessctl set 10%-` in a terminal. If the command works
      but the key does not, the key is not emitting `XF86MonBrightness*` — find out what it
      does emit with `xev` and rebind. If the command itself fails with a permissions error,
      brightnessctl's logind path is not working on this box; report that, do not chmod
      anything under /sys.
13. **Media keys.** With something playing, Play/Pause/Next/Prev work via playerctl.
14. **Screenshot.** `Print` opens xfce4-screenshooter. `$mod+Print` starts a region select.
15. **Compositing.** picom is running (`pgrep picom`). Windows have soft shadows; dragging a
    window or scrolling a video shows no tearing line.
16. **Lock.** `$mod+Escape` locks the screen; your password unlocks it.
17. **Power menu.** `$mod+Shift+e` opens the rofi menu. Press **Escape** — nothing must
    happen. Then pick **Lock** and confirm it locks. Do not test Shut down unless you mean it.
18. **Lid close suspends.** Close the lid, wait, open it. The machine suspends and resumes to
    a locked screen (xss-lock takes the sleep-inhibit lock so the lock is up *before* suspend).
    → If it does not suspend, xfce4-power-manager is not running: `pgrep xfce4-power-manager`.
19. **Polkit.** In Thunar, click a system disk under Devices. A password dialog must appear.
    → No dialog and nothing happens = no polkit agent: `pgrep -f xfce-polkit`. This failure
      is silent by nature, which is exactly why it is on this list.
20. **External display via the dock.** Dock the laptop with a monitor attached, then:
    ```bash
    xrandr --query | grep " connected"
    ```
    The external output (DP-1/DP-2/HDMI-A-1/HDMI-A-2) shows as `connected`. Enable it:
    ```bash
    xrandr --output <EXTERNAL> --auto --right-of eDP-1
    ```
    i3 creates workspaces on the new output; `$mod+1`…`$mod+0` still work.
    → Nothing in the config hardcodes an output name; detect it from the command above.
21. **Restart is safe.** `$mod+Shift+r`. Your layout survives. You do **not** get a second
    panel, a second xfsettingsd, or a second polkit agent — the components use `exec`, which
    does not re-run on restart. Only `picom` (`exec_always`) is re-launched.
22. **Rollback works.** Run `~/i3-setup/undo.sh`, reload with `$mod+Shift+c`, confirm i3bar
    and dmenu return. Then re-apply and reload again. Better to learn this now than at 2am.

23. **Minimize.** Focus a window, press `$mod+m` — it vanishes. Press `$mod+Shift+m` — it
    returns, floating and centred. Press `$mod+Shift+m` again — it hides again.
24. **Minimize animation.** With picom running, `$mod+m` shrinks the window downward toward
    the bottom of the screen while fading; `$mod+Shift+m` reverses it.
    → **Verified 2026-08-22**, which closes the one open question this file used to carry.
      Method: picom restarted with `--log-level=debug --log-file=/tmp/p.log`, then each i3
      action driven in turn and `grep "Starting animation" /tmp/p.log` read back. Result:
      `$mod+m` → `hide`, `$mod+Shift+m` → `show`. Both are the triggers the config uses, so
      the animation is on the right ones.
25. **Workspace switching is animated.** Press `$mod+2`, then `$mod+1`. Windows on the
    workspace you leave scale down and fade out; windows on the one you arrive at scale up
    and fade in. Both are quick (0.14s / 0.15s) on purpose.
    → Measured the same way: leaving a workspace fires `hide` for each window left behind,
      arriving fires `open` for each window revealed. `hide` is therefore shared with
      minimize — see the trigger table at the top of `picom.conf` for why
      `scripts/minimize.sh` tags the window instead of relying on the trigger alone.
26. **Focus changes are animated.** With two windows side by side, press `$mod+j` / `$mod+k`.
    The window you leave fades back to 92% opacity over 0.2s, the one you move to comes up to
    full over 0.13s.
    → picom 13 has no `focus` trigger; this works through the `increase-opacity` /
      `decrease-opacity` triggers, driven by the `!focused` rule in `picom.conf`. Confirmed
      firing on this machine: `Starting animation decrease-opacity for window ... (tool -
      Thunar)` when focus moved away, `increase-opacity` when it moved back.
    → If unfocused windows being see-through is not to your taste, set that rule's
      `opacity = 1.0` — the animations then have nothing to animate and focus changes go back
      to being instant.

27. **Apps land on their assigned workspaces — except the two that should not.**
    Open the browser and the chatbot (`$mod+Shift+a`): expected 5 and 4 wherever you were.
    Then, from some other workspace, open a terminal (`$mod+Return`) and the file manager
    (`$mod+Shift+Return`): both should stay **where you are**, NOT jump to 3 and 1.
    → Verified 2026-08-22 from workspace 7: a new terminal and a new Thunar both stayed on
      7, while an existing Thunar stayed on 1.
    → Their login-only placement is checked separately in item 29.
    → Verified 2026-08-22 by launching them and reading the tree back:
      Brave-origin (instance brave-origin) ws=5, Brave-origin (instance claude.ai__new)
      ws=4.
    → The chatbot rule matches on **instance**, not class: Brave ignores `--class` in
      `--app` mode. If it ever stops working, run `xprop WM_CLASS` on the chatbot window and
      compare the FIRST field against the regex in the config.

28. **Neither bar has holes in it.** Look along the bottom bar, especially behind the Fedora
    menu icon and behind the active window's button in the task list, and along the very
    left edge of the top bar. You should see bar, not wallpaper.
    → A sliver of wallpaper there means something has put `background-color: transparent`
      (or a `border-radius`) on `.xfce4-panel.background` in `~/.config/gtk-3.0/gtk.css`.
      Plugins are separate X windows; that selector hits them too, and their pixels replace
      the panel's. Colour the panels from xfconf instead. Verified clean 2026-08-22 by
      screenshotting both bars over the wallpaper.

29. **The login set opens, on the right workspaces, at the right size.** Log out and back
    in. Expected: Thunar on 1, a terminal on 3 **fullscreen**, cliamp in a terminal on 10,
    the browser on 5, Anki on 8 **fullscreen**. Nothing should be opened twice.
    → The terminal and Thunar get there WITHOUT an assign rule (see item 27). Verified
      2026-08-22 by launching `xfce4-terminal --role=main-term` (landed on 3) and running
      autostart.sh's `place_once 1 'class="^Thunar$"'` against a live Thunar (landed on 1).
    → If Thunar ends up on the wrong workspace at login, `place_once` timed out: it retries
      for 20 seconds, which is generous, but a very slow first boot could exceed it.
    → Check with `i3-msg -t get_tree`; each window's `fullscreen_mode` should be 1 for the
      ws3 terminal and Anki. Verified 2026-08-22 for every app individually by launching it
      and reading the tree back; the full login cycle itself is still untested (see
      STATUS.md open item 3).
    → Opened twice means `exec` was changed to `exec_always` — an i3 restart re-runs
      exec_always.
    → The ws3 terminal is fullscreen because it carries `--role=main-term`, not because it
      is a terminal. A terminal opened by hand with `$mod+Return` is deliberately NOT
      fullscreen.

30. **The AI Hub opens on workspace 4 and fills the screen without scrolling.**
    Press `$mod+Shift+a`. A window titled "AI Hub" should appear on workspace 4 showing a
    flush grid of assistant cards — no scrollbar, no empty last row.
    → Verified 2026-08-22: window `instance=127.0.0.1 class=Brave-origin` on ws4, 4x3 grid,
      screenshotted.
    → An EMPTY grid saying it could not load data/ai.json means the page was opened as a
      `file://` URL. Use `ai-hub/bin/ai-hub`, which serves it over http — browsers block
      `fetch()` on file URLs.
    → Nothing on ws4 at all: check the assign rule matches. Chromium-family browsers give
      every `--app` window the same WM_CLASS and vary only the instance, so the rule keys
      on `instance`, not `class`.

33. **An assistant opened from the hub lands on workspace 4, not 5.** Click a card. A new
    window (not a tab) should appear beside the hub on ws4.
    → Verified 2026-08-22: `curl "http://127.0.0.1:8765/open?id=deepseek"` returned 204 and
      a window `instance=chat.deepseek.com` appeared on ws4, while the ordinary Brave window
      (`instance=brave-origin`) stayed on ws5. `?id=bogus` returns 404 — the endpoint takes
      an id and looks it up in ai.json, so it cannot launch anything not on the list.
    → If an assistant opens as a TAB on ws5 instead, the click was not intercepted: the
      local server is not running, or the page was loaded from somewhere other than
      127.0.0.1. That is the designed fallback, not a crash — a plain link still works.

31. **The ✦ AI button appears inside chatbots.** With the userscript installed in
    Violentmonkey/Tampermonkey, open claude.ai. A small `✦ AI` button sits top-right;
    `alt`+`a` opens the switcher.
    → Verified 2026-08-22 by running the script in a real Brave window against a local test
      page and confirming both that it executed without error and that the button rendered.
      NOT verified inside an actual chatbot site — no userscript manager is installed yet.

32. **Obsidian opens on workspace 6, and updates with everything else.**
    `obsidian` from a terminal, or log in and look at ws6.
    → Verified 2026-08-22: window class `md.obsidian.Obsidian` on ws6.
    → `~/i3-setup/scripts/obsidian-update.sh` prints "already on <tag>" when current; run it
      with `--force` to reinstall. `topgrade` runs it as the "Obsidian" custom command.
    → It runs the UNPACKED AppImage from ~/.local/opt/obsidian, because libfuse.so.2 is
      missing on this box. If `sudo dnf install fuse-libs` is ever run, the AppImage at
      ~/.local/bin/Obsidian.AppImage will also work directly.

34. **Workspace 4 stacks, and nothing else does.** Open the hub (`$mod+Shift+a`) and then
    an assistant from it: they should appear as stacked title bars, not side by side. Then
    open two windows on any other empty workspace: those should split normally.
    → Verified 2026-08-22 from `i3-msg -t get_tree`: ws4 `layout: "stacked"` with three
      windows; ws1, ws3, ws5 `splith`; two fresh windows on ws7 `splith`.
    → The rule fires on the HUB window, so the hub has to be what opens workspace 4 first —
      it is, at login and on `$mod+Shift+a`. If an assistant somehow gets there first, that
      workspace splits until the hub arrives; `$mod+s` fixes it by hand.
    → Beware when changing this: a workspace that was stacked stays stacked. Setting
      `workspace_layout stacking` globally and then removing it left workspace 5 stacked
      until it was reset with `layout splith`.

35. **The setup can be rebuilt from the installer repo.** On the reference machine the
    read-only parts are runnable now:
    `./i3-xfce-fedora/install.sh --dry-run --yes` (every step, nothing changed) and
    `./i3-xfce-fedora/core/90-verify.sh` (checks programs, files and config validity).
    → Verified 2026-08-22: dry-run completes and 90-verify reports all checks passed.
    → `capture.sh` must be runnable from ANYWHERE, not just its own directory — it was
      not, at first. `cd ~ && ~/i3-setup/i3-xfce-fedora/capture.sh` is the check.
    → The package-installing steps (10, 20, 30, 70) have NEVER been run: this machine
      already has everything, so they have only been exercised in dry-run. Do not describe
      them as tested.

36. **`$mod+b` opens the browser and `$mod+e` opens Thunar.** Both displaced layout
    bindings that MOVED rather than vanished: `split h` is now `$mod+x`, and
    `layout toggle split` is now `$mod+Shift+s`.
    → If `$mod+b` does nothing, check `scripts/browser.sh` is executable and that
      `xdg-settings get default-web-browser` names a .desktop file that actually exists.

37. **The panels have no dark blocks behind their plugins after a FRESH LOGIN.**
    This is the one that only shows up on a real login, so it cannot be checked by
    restarting things by hand. Log out, log back in, and look at the Fedora menu icon and
    the task list: their backgrounds should be the same translucent bar as everything else.
    → Cause if they are dark: a plugin's wrapper process started before any compositor
      owned `_NET_WM_CM_S0`, so GTK gave it an opaque visual for the session. `xfce4-panel -r`
      fixes it there and then.
    → Prevented two ways, both now in the config: picom's `exec_always` line comes BEFORE
      every XFCE component, and the panel is started by `scripts/start-panel.sh`, which
      waits for the compositor selection rather than guessing with a sleep.
    → `i3-xfce-fedora/core/99-verify.sh` checks both, so the ordering cannot be undone by
      accident.

## C. Known limitations, not bugs

- **Binding-mode names are invisible.** The resize mode's name string is drawn by i3bar and
  there is no i3bar. The keys (`h/j/k/l`, `Escape`) are the whole interface.
- **No session save/restore, and no XFCE logout dialog** — `xfce4-session` is not running.
  `$mod+Shift+e` replaces the logout dialog.
- **`i3 -C` passing proves nothing about behaviour.** It checks syntax only. Item 19 in
  particular fails silently and is invisible to validation.
