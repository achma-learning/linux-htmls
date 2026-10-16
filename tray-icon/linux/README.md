# tray-icon/linux

The Linux builds of the two tray apps: the same features as the Windows
`.exe` files one folder up, packaged for Debian/Ubuntu (`.deb`) and Fedora
(`.rpm`).

| Command | What it does |
|---|---|
| `prayer-tray` | **Prayer Spine**: city, weather, the day spine and the next prayer |
| `ai-hub-tray` | **AI Hub**: quick launcher for AI chatbots |

## Install

Download the package from [`dist/`](dist/), then:

```sh
# Debian, Ubuntu, Linux Mint, Pop!_OS …
sudo apt install ./achma-tray_1.0.0-1_all.deb

# Fedora (and RHEL/Alma/Rocky 9+)
sudo dnf install ./achma-tray-1.0.0-1.noarch.rpm
```

Then start **Prayer Spine** and **AI Hub** from the applications menu. To have
them start at every login, right-click each icon and tick *Start at login*.

Both packages are architecture-independent (`all` / `noarch`). They need only
Python 3.8+ and GTK 3, which every desktop install of these distros already has.

## Desktop support

These apps use **StatusNotifierItem**, the modern Linux tray protocol, not the
old XEmbed system tray.

| Desktop | Works? |
|---|---|
| **KDE Plasma** 5 / 6 | Yes, out of the box |
| **XFCE** 4.16+ | Yes. It needs the panel's *Status Tray Plugin*, which the default panel already has. If yours doesn't: Panel → *Add New Items* → *Status Tray Plugin*. |
| **GNOME** on Ubuntu | Yes, out of the box (Ubuntu ships the AppIndicator extension) |
| **GNOME** on Fedora / Debian | Needs the AppIndicator extension, once: `sudo dnf install gnome-shell-extension-appindicator` (Fedora) or `sudo apt install gnome-shell-extension-appindicator` (Debian). Log out and back in, then switch it on in the *Extensions* app. The Fedora `.rpm` pulls the extension in automatically when GNOME is installed. |
| i3, sway, others | Any StatusNotifierItem host works: xfce4-panel, waybar, or polybar with snixembed |

If no tray is running about 10 s after start-up, the app sends a notification
saying what your desktop needs.

**Wayland.** On Wayland sessions (GNOME and KDE use Wayland by default) the
flyouts run through XWayland. That way they can open beside the icon and close
when you click elsewhere, just like a menu. A native Wayland window cannot do
either. On a session without XWayland they still work: they open as a small
window, and if `gtk-layer-shell` is installed (KDE, sway) they are pinned to the
screen corner nearest the panel.

## Prayer Spine (`prayer-tray`)

- **Left-click**: the flyout with the city, weather, the day spine and the next-prayer
  countdown. `Esc` or a click elsewhere closes it.
- **Right-click**: *City ▸* (all 57 cities), *Settings…*, *Refresh weather*,
  *Open Start Dock page*, *Start at login*, *Quit*.
- **Middle-click** or clicking the city name opens **Settings**, where you change the city. You
  can pick any of the 57 Moroccan cities or enter a *Custom location* by name, latitude and
  longitude. Settings also has the reminder minutes before each adhan, a notification at the
  adhan itself, sound, and live weather.
- **The icon is itself a small spine.** It updates every minute, and its border turns terracotta
  when an adhan is near.

Prayer times are computed locally using the Moroccan Ministry method (Fajr 19°,
Isha 17°, +5 min on Dhuhr and Maghrib), with your computer's clock and time zone.
`make check` compares them with the JavaScript in `start-dock.html` for every
city on every day of the year, and they are identical.

Weather comes from Open-Meteo, one request every 30 minutes. It is the only
thing that uses the network, and you can turn it off in Settings.

## AI Hub (`ai-hub-tray`)

- **Left-click**: the launcher. Type to filter, use `↑` `↓` to move, and `↵` to open.
  Favourites are listed first.
- **Right-click**: your favourites and *All assistants ▸*, followed by the settings:
  default assistant, favourites, what left-click does, opening in its own
  window, the keyboard shortcut, adding your own assistants, and starting at login.
- **Ctrl + Alt + A** opens your default assistant. Linux does not let an app grab a global
  shortcut on its own, so *Keyboard shortcut Ctrl+Alt+A…* registers the shortcut with your
  desktop. On GNOME and XFCE this happens automatically. On KDE, i3 and sway it shows the
  one line to paste. The command behind the shortcut is:
  ```
  ai-hub-tray --open-default
  ```
- **Own window**: Chrome, Chromium, Edge, Brave and Vivaldi get `--app=` (no tabs, no
  address bar), and Firefox gets `--class=AIHub`. This is the same rule as
  `ai-hub/bin/ai-open`, so the i3 window rules in `ai-hub/i3/config.example` still match.
  Flatpak browsers work too.
- **Your own assistants**: *Add your own assistants…* opens the settings file. Add lines like
  this under `[custom]`:
  ```ini
  [custom]
  Claude Projects = https://claude.ai/projects
  ```

## Command line

```
prayer-tray [--show | --settings]
ai-hub-tray [--launcher | --open-default]
```

Only one copy of each app runs at a time. If you start it again, the request is
passed to the copy that's already running.

## Files

| | |
|---|---|
| Settings | `~/.config/achma-tray/prayer-tray.ini`, `ai-hub-tray.ini` (same keys as the Windows build) |
| Start at login | `~/.config/autostart/achma-prayer-tray.desktop`, `achma-ai-hub-tray.desktop` |
| Pages it opens | `~/.local/share/achma-tray/start-dock.html`, `ai-hub.html` |
| Program | `/usr/share/achma-tray/`, `/usr/bin/prayer-tray`, `/usr/bin/ai-hub-tray` |

Removing the package leaves your settings in place. To remove those too, delete
`~/.config/achma-tray`.

## Build the packages

```sh
make deb          # needs dpkg-deb       (Debian/Ubuntu: always there)
make rpm          # needs rpmbuild       (Fedora: sudo dnf install rpm-build;  Ubuntu: sudo apt install rpm)
make packages     # both, into dist/
make check        # prayer times vs the page (needs node)
```

Both formats are built by the same `make install` into a staging folder, so the
`.deb` and the `.rpm` always contain the same files. Run from a checkout without
installing:

```sh
bin/prayer-tray
bin/ai-hub-tray
```

## Layout

```
achma_tray/
├── sni.py            StatusNotifierItem + DBusMenu, straight on Gio D-Bus
├── common.py         settings, autostart, single instance, notifications, the flyout window
├── prayer_core.py    prayer math, cities, spine geometry — no GTK, testable
├── prayer.py         Prayer Spine: drawing, weather, alerts, settings window
└── aihub.py          AI Hub: assistants, launcher, browser app-mode, shortcut setup
bin/                  the two commands
data/                 .desktop entries and icons
packaging/            debian/ control files, achma-tray.spec
tests/                check_times.py (vs the page), fake_host.py (a headless tray host)
```

The tray protocol is implemented directly on Gio rather than with
libappindicator, for two reasons:

- libappindicator cannot receive a plain left-click, and the flyouts need one.
- Its library has a different name on Debian and on Fedora, which would mean
  two different dependency lists.
