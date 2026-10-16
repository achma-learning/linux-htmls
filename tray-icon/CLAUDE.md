# tray-icon/ — project context

This folder is its own project inside the repo. The i3/XFCE rules in the root
`CLAUDE.md` (sudo, i3 -C, ~/.config/i3, Route B…) are about the Linux desktop
setup and do not govern this folder; work here is done in the repo checkout.

## Audience and platforms

- **Who:** Moroccan residents. Design every default for them first:
  Moroccan cities, the Ministry prayer method, Arabic prayer names, Moroccan time.
- **Windows:** 7 SP1, 8, 8.1, 10, 11; 32- and 64-bit; Windows 11 is the main
  target, 7 and 10 must keep working. Sources in `src/`, binaries in this folder.
- **Linux** (`linux/`): Debian-based and Fedora-based, as one `.deb` and one
  `.rpm` built from the same tree. Must work in **XFCE, KDE Plasma and GNOME**.

## What is here

| | |
|---|---|
| `ai-hub-tray.exe` | tray launcher for the chatbots of `html file/ai-hub.html`; Ctrl+Alt+A opens the default |
| `prayer-tray.exe` | flyout with city · weather · day spine · next prayer, ported from the spine pane of `html file/start-dock.html`; settings window changes the city |
| `src/` | the C++ sources, `.rc`, manifest, icons; `build.sh` rebuilds both `.exe` |
| `linux/` | the Linux port: Python 3 + GTK 3, `make deb` / `make rpm`; packages in `linux/dist/` |

The `.exe` files and the `linux/dist/` packages are committed. **Rebuild them in
the same commit as any change to their sources** (`./build.sh`,
`make -C linux packages`), so binaries never drift from the source.

The two platforms are deliberately feature-identical — same menus, same
settings keys in the .ini, same flyout. A feature added to one is added to both.

## Non-negotiables

- **Windows 7 compatibility.** 32-bit (i686) MinGW-w64, `-static`, msvcrt (not
  UCRT — Windows 7 lacks it), PE subsystem 6.1. Any API newer than Windows 7
  goes through `GetProcAddress` and degrades a feature, never stops startup.
  After a build, check the import table:
  `i686-w64-mingw32-objdump -p <exe>` — nothing newer than Vista may appear.
- **No runtime dependencies.** No .NET, no VC++ redistributable, no installer
  requirement. Single file, runs by double-click, no admin rights.
- **Prayer times must match the deck exactly.** `pt::times()` in
  `prayer-tray.cpp` is a port of `prayTimes()` in `start-dock.html`
  (Morocco method = AlAdhan 21: Fajr 19°, Isha 17°, Asr shadow 1,
  sunrise/sunset 0.833°, +5 min on Dhuhr and Maghrib). After touching either,
  re-run the comparison: compile the `namespace pt` block natively, run the JS
  `prayTimes()` under node, all 57 cities × every day of a year, outputs must be
  byte-identical (last run: 124,830 times, identical).
- **Times follow the PC's clock and time zone** (deliberate; same reasoning as
  the deck's §3 comment). Do not switch to a fixed UTC offset per city.
- **Only one network call:** Open-Meteo weather, optional, every 30 min,
  WinINet with an http fallback (Windows 7 without IE11 has no TLS 1.2).
- **City list is shared with the deck.** Coordinates are copied verbatim from
  `CITIES` in `start-dock.html`. Settings store the city by its stable `id`,
  never by array index.
- **Look:** the deck's palette (`--ink*`, `--saffron`, `--verdigris`,
  `--terracotta`…), colour is state, not decoration. The flyout should keep
  matching the deck's spine pane.

## Build and test

```sh
sudo apt install g++-mingw-w64-i686   # Fedora: mingw32-gcc-c++
./build.sh
```

There is no Windows machine in the cloud sessions. Testing is done with Wine on
Xvfb and screenshots:

- the shipped 32-bit `.exe` needs `wine32:i386` (`dpkg --add-architecture i386`)
  and a prefix booted after it is installed — a 64-bit-only prefix fails with
  `syswow64\ntdll.dll` missing; alternatively build a throwaway x86_64 copy;
- `prayer-tray.exe --show` opens the flyout, `--settings` the settings window;
  menus can be driven with `xdotool` clicks on Wine's tray window;
- test 150 % DPI via `HKCU\Control Panel\Desktop\LogPixels = 144`;
- Wine's UI font has no Arabic glyphs — boxes in native controls under Wine
  are not a bug; real Windows (Segoe UI/Tahoma) renders them.

Never claim something works on real Windows: say "tested under Wine".

## Linux specifics

- **Stack:** Python ≥ 3.8 + PyGObject + GTK 3 + cairo/Pango. Nothing else:
  every Debian/Ubuntu/Fedora desktop already has it, and the package stays
  `noarch`. Keep the code 3.8-compatible (check with
  `ast.parse(src, feature_version=(3, 8))`).
- **Tray:** StatusNotifierItem + DBusMenu implemented directly on Gio
  (`achma_tray/sni.py`) — not libappindicator, which cannot deliver a left
  click and has different typelib names on Debian and Fedora. Never fall back
  to the old XEmbed tray (Gtk.StatusIcon).
- **Desktops:** KDE native; XFCE via the panel's Status Tray plugin (needs a
  dbusmenu-enabled build — Ubuntu/Fedora's are); GNOME only with the
  AppIndicator extension (Ubuntu ships it; the .rpm pulls it in with a rich
  `Recommends: (… if gnome-shell)`; Debian users install it). The app sends a
  notification if no tray host appears.
- **Flyouts** run on X11/XWayland (`GDK_BACKEND=x11` when `DISPLAY` is set): an
  override-redirect popup with a seat grab, so it opens beside the icon and
  closes on an outside click under every WM, i3 included. Pure Wayland falls
  back to a toplevel (+ gtk-layer-shell when present).
- **Global hotkey:** impossible portably on Linux. `ai-hub-tray --open-default`
  is registered with the desktop (GNOME gsettings, XFCE xfconf); KDE/i3 get a
  copy-paste instruction.
- **Prayer math** lives in `linux/achma_tray/prayer_core.py` (no GTK) and is held
  to the same byte-identical rule: `make -C linux check`.

## Testing Linux in a cloud session

- **This container's `/usr/bin/python3` is 3.13 while Ubuntu's PyGObject is
  built for 3.12** — run the apps with `python3.12` here. Not a bug in the
  launchers (`#!/usr/bin/python3` is right on a stock distro).
- `linux/tests/fake_host.py` is a headless SNI watcher/host: run it and the app
  under `dbus-run-session` + Xvfb; it prints the tooltip, pixmap sizes and the
  whole menu tree, and can `activate`, `click "City/Casablanca"`, `type`, `shot`.
- A **real XFCE** check is possible: install `xfce4-panel xfconf`, seed
  `$XDG_CONFIG_HOME/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml` from
  `/etc/xdg/xfce4/panel/default.xml`, start `xfce4-panel` on Xvfb, then the apps.
  Without a WM the panel's systray wrapper is slow — wait ~15 s before
  concluding an icon is missing.
- KDE Plasma and GNOME Shell cannot be run here; say "tested on XFCE and with a
  protocol-level fake host", never "tested on KDE/GNOME".
- `make deb` can be installed in the container with `apt install ./dist/…deb`
  (proves the Depends resolve). `make rpm` works on Ubuntu (`apt install rpm`);
  Fedora package names can be checked at `https://mdapi.fedoraproject.org/rawhide/pkg/<name>`.

## Backlog (ideas agreed as useful, not yet requested)

1. French / Arabic interface (menus, settings, notifications) — most Moroccan
   Windows installs are French.
2. Hijri date with a ±day offset (Morocco fixes months by sighting) → enables
   "Iftar" naming of Maghrib in Ramadan, as the deck does.
3. A small installer (Start-menu entry, uninstall entry).
4. Linux: AppStream metainfo (GNOME Software / Discover listing), and a
   signed apt/dnf repository instead of downloading the files by hand.
