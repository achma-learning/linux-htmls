# tray-icon

Two tray apps, built from the two pages in `html file/`. Each one puts its own
icon in the notification area (the system tray). They exist for **Windows**
(this folder) and **Linux** ([`linux/`](linux/)).

| App | What it does |
|---|---|
| **AI Hub** | Quick launcher for AI chatbots (`ai-hub.html` as a tray app) |
| **Prayer Spine** | City, weather, the **day spine** and the next prayer (the spine pane of `start-dock.html`) |

| Platform | Get it |
|---|---|
| Windows 7 SP1 · 8 · 10 · 11 | `ai-hub-tray.exe`, `prayer-tray.exe` (this folder): single files, nothing to install |
| Debian · Ubuntu · Mint | [`linux/dist/achma-tray_1.0.0-1_all.deb`](linux/dist/), installed with `sudo apt install ./achma-tray_1.0.0-1_all.deb` |
| Fedora · RHEL 9+ | [`linux/dist/achma-tray-1.0.0-1.noarch.rpm`](linux/dist/), installed with `sudo dnf install ./achma-tray-1.0.0-1.noarch.rpm` |

On Linux it works in KDE Plasma, XFCE and GNOME. Details are in [`linux/README.md`](linux/README.md).

The rest of this page is about the Windows build.

Each `.exe` is a single file of about 100–200 KB. Nothing to install: no .NET,
no Visual C++ runtime, no admin rights. Copy it anywhere and double-click it.

**Runs on** Windows 7 SP1, 8, 8.1, 10 and 11, both 32- and 64-bit (it is a
32-bit build, which runs on both).

---

## prayer-tray.exe — Prayer Spine

```
 ● Marrakech 30° ⛅        ← green dot: live weather · amber: fetching · red: offline (last reading)
 DAY SPINE
 05:10 ─ Fajr
 06:36 ─ Shuruq
   │
 12:22 ─ Jumuʿa            ← Dhuhr is called Jumuʿa on Fridays
 15:32 ─ Asr
   ●────────────           ← now
 18:04 ━ Maghrib           ← next prayer, in terracotta
 19:15 ─ Isha
 ───────────────
 NEXT PRAYER
 Maghrib · المغرب
 1 h, 18 min
```

- **Left-click** the icon to open the flyout. Click outside it, or press `Esc`, to close it.
- **Right-click** for the menu: *City ▸* (quick switch), *Settings…*, *Refresh weather*,
  *Open Start Dock page*, *Start with Windows*, *Exit*.
- **Click the city name** in the flyout (or press `S`) to open Settings. Press `R` to refresh the weather.
- **The icon is a small spine as well.** It shows the rail, how much of the day has passed and where
  you are now. Its border turns terracotta in the minutes before an adhan.
- **Hover** over the icon for a tooltip like `Marrakech · 29° Partly cloudy / Maghrib 18:04 · in 1 h, 2 min`.

### Settings (changing the city)

Open *Settings…* from the right-click menu, or click the city name in the flyout.

- **City**: all 57 Moroccan cities from the deck, sorted A–Z. You can also choose
  **Custom location…** and type a name, latitude and longitude for anywhere else.
- **Remind before adhan**: 0–60 minutes (default 10, 0 turns it off).
- **Notify at the adhan itself**, and **play the notification sound**.
- **Live weather**: Open-Meteo, one request every 30 minutes. This is the program's only network use.
- **Start with Windows**.

Notifications are balloons on Windows 7 and toasts on Windows 10 and 11. Windows
converts them automatically. Clicking one opens the flyout.

### How the times are computed

Times are computed on your PC and need no network. The method is the Moroccan
Ministry method, exactly as `start-dock.html` uses it (AlAdhan method 21): Fajr at
19°, Isha at 17°, Asr with a shadow factor of 1, and +5 min on Dhuhr and Maghrib.
The C++ port was checked against the page's JavaScript for all 57 cities on every
day of 2026. All 124,830 times were identical.

Times follow **the PC's clock and time zone**, the same clock Windows shows. So if
the PC is set to Paris time and you choose Marrakech, you see Marrakech's prayers
in Paris time. That is correct for "when do I need to pray where I am", but set
the zone correctly if you travel.

---

## ai-hub-tray.exe — AI Hub

- **Left-click**: a launcher menu with your favourites first, then *All assistants ▸* (15 of them,
  the same list as `ai-hub.html`), then *Open AI Hub page*.
- **Ctrl + Alt + A**, from any program: opens your **default assistant** directly.
- **Right-click**: settings:
  - *Default assistant ▸*: the one the hotkey opens (Claude by default)
  - *Favourites ▸*: which assistants appear at the top of the left-click menu
  - *Left click ▸*: show the list, or open the default assistant directly
  - *Open as app window*: opens the chatbot in its own window, without tabs or an address bar,
    using Edge, Chrome or Brave (`--app=` mode). If you have none of these, it opens a normal browser tab.
  - *Hotkey Ctrl+Alt+A*: turn the hotkey on or off. If another program already uses it, the menu says so.
  - *Add your own assistants…*: opens the settings file in Notepad. Add lines under `[custom]`:
    ```ini
    [custom]
    My GPT=https://chatgpt.com/g/g-xxxx
    Claude Projects=https://claude.ai/projects
    ```
    They appear in the menus the next time you open one. No restart is needed.
  - *Start with Windows*

---

## Where things are kept

| | |
|---|---|
| Settings | `%APPDATA%\AchmaTray\prayer-tray.ini` and `ai-hub-tray.ini` (plain text, safe to edit or delete) |
| Extracted pages | `%APPDATA%\AchmaTray\start-dock.html` and `ai-hub.html` (embedded copies, written when you first open them) |
| Autostart | `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` → `AchmaPrayerTray` / `AchmaAIHubTray`, only if you turn it on |

**To uninstall:** turn off *Start with Windows*, choose *Exit*, then delete the
`.exe` and the `%APPDATA%\AchmaTray` folder.

## Command line

```
prayer-tray.exe --show       open the flyout now (for a pinned shortcut)
prayer-tray.exe --settings   open the settings window
```

Only one copy of each program runs at a time. Launching it again hands the request
to the copy that is already running: the prayer app opens its flyout or settings,
and the AI Hub opens its launcher at the mouse pointer.

## Notes for older Windows

- **Windows 7 weather.** The weather service uses HTTPS with TLS 1.2. On Windows 7
  that works once Internet Explorer 11 is installed, which turns TLS 1.2 on. Without
  it, the program falls back to plain HTTP. If both fail, the dot turns red and the
  last reading stays on screen. Prayer times never need the network.
- **SmartScreen.** These `.exe` files are not code-signed, so Windows 10 and 11 may say
  "Windows protected your PC" the first time. Choose *More info → Run anyway*. You can
  also build them yourself (below).
- **Fonts.** The flyout uses JetBrains Mono if it is installed, then Cascadia Mono
  (Windows 11), then Consolas (Windows 7+). Arabic uses Noto Naskh Arabic, Amiri,
  Segoe UI or Tahoma, whichever it finds first.
- **High DPI.** Text and layout scale with Windows' display scaling (100–200 %+).
  On Windows 11 the flyout gets rounded corners.

## Building from source

The binaries in this folder are built from `src/` by `build.sh`, with MinGW-w64
on Linux:

```sh
sudo apt install g++-mingw-w64-i686      # Debian/Ubuntu   (Fedora: mingw32-gcc-c++)
./build.sh
```

```
src/
├── common.h            shared Win32 plumbing: INI, autostart, DPI, tray, HTTP, flyout placement
├── prayer-tray.cpp     prayer math (PrayTimes port), Open-Meteo, flyout, settings window
├── ai-hub-tray.cpp     assistants, menus, browser --app detection, hotkey
├── *.rc, resource.h    icons, the embedded HTML page, version info, manifest
├── app.manifest        Win 7–11 compatibility, DPI awareness, themed controls
├── make-icons.py       draws icons/*.ico (Pillow)
└── icons/
```

The build links everything statically against `msvcrt.dll`, which every Windows
version ships. Any API newer than Windows 7 is looked up at run time, so if it is
missing, that one feature switches off and the program still starts. Examples:
Windows 11's rounded corners, and the tray-icon position lookup (introduced in
Windows 7).
