# tray-icon/ — project context

This folder is its own project inside the repo. The i3/XFCE rules in the root
`CLAUDE.md` (sudo, i3 -C, ~/.config/i3, Route B…) are about the Linux desktop
setup and do not govern this folder; work here is done in the repo checkout.

## Audience and platforms

- **Who:** Moroccan residents, on Windows. Design every default for them first:
  Moroccan cities, the Ministry prayer method, Arabic prayer names, Moroccan time.
- **Now:** Windows only — 7 SP1, 8, 8.1, 10, 11; 32- and 64-bit; Windows 11 is
  the main target, 7 and 10 must keep working.
- **Later (not now — do not start without being asked):** a Linux version for
  Debian-based and Fedora-based distros (.deb and .rpm).

## What is here

| | |
|---|---|
| `ai-hub-tray.exe` | tray launcher for the chatbots of `html file/ai-hub.html`; Ctrl+Alt+A opens the default |
| `prayer-tray.exe` | flyout with city · weather · day spine · next prayer, ported from the spine pane of `html file/start-dock.html`; settings window changes the city |
| `src/` | the C++ sources, `.rc`, manifest, icons; `build.sh` rebuilds both `.exe` |

The `.exe` files are committed. **Rebuild them with `./build.sh` in the same
commit as any change to `src/`**, so binaries never drift from the source.

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

## Backlog (ideas agreed as useful, not yet requested)

1. French / Arabic interface (menus, settings, notifications) — most Moroccan
   Windows installs are French.
2. Hijri date with a ±day offset (Morocco fixes months by sighting) → enables
   "Iftar" naming of Maghrib in Ramadan, as the deck does.
3. A small installer (Start-menu entry, uninstall entry).
4. Before the Linux port: split the portable core (prayer math, cities, weather
   parsing, spine layout, alert logic) out of the Win32 shell so both platforms
   share one tested core. Linux tray should use StatusNotifierItem (works in
   GNOME with the AppIndicator extension, KDE, and xfce4-panel's status tray).
