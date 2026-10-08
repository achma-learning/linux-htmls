#!/usr/bin/env bash
# Restore panel-2 (the bottom dock) after it was accidentally removed.
#
# Source of truth: the live `xfconf-query -c xfce4-panel -lv` dump taken on
# 2026-08-22 ~17:00, before deletion. The launcher .desktop files come from
# backups/20260820-223928/panel-layout-backup.tar.bz2 (an xfce4-panel-profiles
# export) -- ~/.config/xfce4/panel/ was wiped when the panel was removed.
#
# Deliberate changes vs. the pre-deletion state:
#   * separators 12 and 17 had expand=true, which is what forced the dock to
#     full screen width. They are recreated with expand=false so the panel
#     honours length=1 + length-adjust=true and shrinks to fit its content.
#   * plugins 20/21/22 are NOT recreated: their .desktop files were deleted
#     with ~/.config/xfce4/panel/ and are not in any backup.
set -euo pipefail

CH="xfce4-panel"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARBALL="$REPO/backups/20260820-223928/panel-layout-backup.tar.bz2"
PANELDIR="${XDG_CONFIG_HOME:-$HOME/.config}/xfce4/panel"

q() { xfconf-query -c "$CH" "$@"; }
set_str()  { q -p "$1" -n -t string -s "$2"; }
set_int()  { q -p "$1" -n -t int    -s "$2"; }
set_uint() { q -p "$1" -n -t uint   -s "$2"; }
set_bool() { q -p "$1" -n -t bool   -s "$2"; }
set_dbl()  { q -p "$1" -n -t double -s "$2"; }

echo "==> quitting panel"
xfce4-panel -q >/dev/null 2>&1 || true
sleep 2

echo "==> restoring launcher .desktop files into $PANELDIR"
mkdir -p "$PANELDIR"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
tar xjf "$TARBALL" -C "$tmp"
for n in 13 14 15 16; do
    mkdir -p "$PANELDIR/launcher-$n"
    cp -f "$tmp/launcher-$n/"*.desktop "$PANELDIR/launcher-$n/"
done

echo "==> recreating plugin definitions"
set_str /plugins/plugin-2  separator ; set_uint /plugins/plugin-2/style 0
set_str /plugins/plugin-11 showdesktop
set_str /plugins/plugin-12 separator ; set_uint /plugins/plugin-12/style 0
set_bool /plugins/plugin-12/expand false
set_str /plugins/plugin-13 launcher
set_str /plugins/plugin-14 launcher
set_str /plugins/plugin-15 launcher
set_str /plugins/plugin-16 launcher
q -p /plugins/plugin-13/items -n -t string -s 17862107061.desktop --force-array
q -p /plugins/plugin-14/items -n -t string -s 17862107062.desktop --force-array
q -p /plugins/plugin-15/items -n -t string -s 17862107063.desktop --force-array
q -p /plugins/plugin-16/items -n -t string -s 17862107064.desktop --force-array
set_str /plugins/plugin-17 separator ; set_uint /plugins/plugin-17/style 0
set_bool /plugins/plugin-17/expand false
set_str /plugins/plugin-18 separator ; set_uint /plugins/plugin-18/style 0
set_str /plugins/plugin-23 whiskermenu
set_str  /plugins/plugin-23/button-icon orca
set_bool /plugins/plugin-23/button-single-row false
set_bool /plugins/plugin-23/show-button-icon true
set_bool /plugins/plugin-23/show-button-title false
set_int  /plugins/plugin-23/profile-shape 0
set_str /plugins/plugin-24 docklike
set_str /plugins/plugin-25 separator ; set_uint /plugins/plugin-25/style 0
set_str /plugins/plugin-26 separator ; set_uint /plugins/plugin-26/style 0

echo "==> recreating panel-2"
set_uint /panels/panel-2/size            48
set_uint /panels/panel-2/icon-size       45
set_uint /panels/panel-2/border-width    5
set_uint /panels/panel-2/mode            0
set_uint /panels/panel-2/autohide-behavior 0
set_dbl  /panels/panel-2/length          1
set_bool /panels/panel-2/length-adjust   true
set_bool /panels/panel-2/position-locked true
set_str  /panels/panel-2/position        "${PANEL2_POSITION:-p=0;x=800;y=860}"

# order preserved from the pre-deletion dump, minus the lost 20/21/22
q -p /panels/panel-2/plugin-ids -n -t int -s 18 -t int -s 23 -t int -s 25 \
    -t int -s 16 -t int -s 14 -t int -s 13 -t int -s 15 -t int -s 12 \
    -t int -s 24 -t int -s 17 -t int -s 2 -t int -s 11 -t int -s 26 --force-array

echo "==> registering the panel"
q -p /panels -n -t int -s 1 -t int -s 2 --force-array

echo "==> starting panel"
setsid xfce4-panel >/dev/null 2>&1 &
sleep 4
echo "done: $(xfconf-query -c xfce4-panel -p /panels | tail -n +2 | tr '\n' ' ')"
