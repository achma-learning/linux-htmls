#!/bin/sh
# build.sh — cross-compile both tray apps from Linux (or MSYS2 on Windows).
#
#   sudo apt install g++-mingw-w64-i686     # Debian / Ubuntu
#   sudo dnf install mingw32-gcc-c++        # Fedora
#   ./build.sh
#
# Output: ai-hub-tray.exe and prayer-tray.exe next to this script.
#
# 32-bit (i686) on purpose: one binary runs on 32- and 64-bit Windows 7, 8,
# 10 and 11. Linked fully static against msvcrt.dll, which every Windows since
# XP ships — no Visual C++ redistributable, no .NET, no UCRT (Windows 7 lacks
# the UCRT until KB2999226). Nothing to install on the target machine.
set -eu
cd "$(dirname "$0")"

CXX=${CXX:-i686-w64-mingw32-g++}
WINDRES=${WINDRES:-i686-w64-mingw32-windres}
STRIP=${STRIP:-i686-w64-mingw32-strip}

FLAGS="-std=c++17 -Os -municode -mwindows -static -static-libgcc -static-libstdc++
       -fno-exceptions -fno-rtti -finput-charset=UTF-8 -fexec-charset=UTF-8
       -Wall -Wno-missing-field-initializers -Wno-unknown-pragmas
       -D_WIN32_WINNT=0x0601 -DWINVER=0x0601"
# 6.01 = Windows 7 in the PE header; nothing newer is required to load.
LINK="-Wl,--major-subsystem-version,6 -Wl,--minor-subsystem-version,1
      -Wl,--major-os-version,6 -Wl,--minor-os-version,1 -Wl,--nxcompat -Wl,--dynamicbase"
LIBS="-lgdiplus -lwininet -lshlwapi -lcomctl32 -lshell32 -lole32 -luuid -lgdi32 -luser32 -ladvapi32"

[ -f src/icons/prayer-tray.ico ] && [ -f src/icons/ai-hub-tray.ico ] || python3 src/make-icons.py

for app in prayer-tray ai-hub-tray; do
    echo "== $app"
    (cd src && $WINDRES -O coff -i "$app.rc" -o "../$app.res")
    $CXX $FLAGS "src/$app.cpp" "$app.res" -o "$app.exe" $LINK $LIBS
    rm -f "$app.res"
    $STRIP "$app.exe"
    ls -l "$app.exe"
done
