#!/usr/bin/env python3
"""fake_host.py — a minimal StatusNotifierWatcher/Host, for headless tests.

Plays the panel's part on a private session bus: accepts the item's
registration, reads every SNI property, saves the icon pixmap as PNG, prints
the DBusMenu tree, then performs scripted actions:

    fake_host.py OUTDIR [activate X Y] [click "Label/Sub label"] [wait SECONDS] [shot FILE] [menu]
                [type TEXT] [key KEY] [clickat X Y] ...

Exit status 0 only if an item registered and its menu could be read.
"""
import os
import sys
import struct
import zlib

import gi
gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib

XML = """<node><interface name="org.kde.StatusNotifierWatcher">
  <method name="RegisterStatusNotifierItem"><arg type="s" direction="in"/></method>
  <method name="RegisterStatusNotifierHost"><arg type="s" direction="in"/></method>
  <property name="RegisteredStatusNotifierItems" type="as" access="read"/>
  <property name="IsStatusNotifierHostRegistered" type="b" access="read"/>
  <property name="ProtocolVersion" type="i" access="read"/>
  <signal name="StatusNotifierItemRegistered"><arg type="s"/></signal>
</interface></node>"""

out = sys.argv[1]
script = sys.argv[2:]
os.makedirs(out, exist_ok=True)
conn = Gio.bus_get_sync(Gio.BusType.SESSION, None)
loop = GLib.MainLoop()
items = []
ok = {"menu": False}


def png(path, w, h, argb):
    raw = b"".join(b"\0" + bytes(sum(([argb[i + 1], argb[i + 2], argb[i + 3], argb[i]]
                                      for i in range(y * w * 4, (y + 1) * w * 4, 4)), []))
                   for y in range(h))
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def call(dest, path, iface, method, args, rtype=None):
    return conn.call_sync(dest, path, iface, method, args, GLib.VariantType(rtype) if rtype else None,
                          Gio.DBusCallFlags.NONE, 5000, None)


def walk(node, depth, paths, prefix=""):
    iid, props, kids = node
    label = props.get("label", "").replace("__", "\x00").replace("_", "").replace("\x00", "_")
    if props.get("type") == "separator":
        label = "────"
    flags = []
    if props.get("toggle-type"):
        flags.append(("x" if props.get("toggle-state") else " ") + props["toggle-type"][0])
    if props.get("enabled") is False:
        flags.append("disabled")
    if "icon-data" in props:
        flags.append("icon %dB" % len(props["icon-data"]))
    if depth >= 0:
        print("  " * depth + "%s%s" % (label, ("  [" + ", ".join(flags) + "]") if flags else ""))
    full = (prefix + "/" + label).lstrip("/") if depth >= 0 else ""
    paths[full] = iid
    for k in kids:
        walk(k, depth + 1, paths, full)


def inspect(name):
    dest, path = (name.split("/", 1) + [""])[:2]
    path = "/" + path if path else "/StatusNotifierItem"
    props = call(dest, path, "org.freedesktop.DBus.Properties", "GetAll",
                 GLib.Variant("(s)", ("org.kde.StatusNotifierItem",)), "(a{sv})").unpack()[0]
    print("ITEM", dest, "Id=%s Title=%s ItemIsMenu=%s Menu=%s" % (props["Id"], props["Title"], props["ItemIsMenu"], props["Menu"]))
    tt = props["ToolTip"]
    print("TOOLTIP", repr(tt[2]), "|", repr(tt[3]))
    pms = props["IconPixmap"]
    print("PIXMAPS", [(w, h) for w, h, _ in pms])
    if pms:
        w, h, data = max(pms, key=lambda p: p[0])
        png(os.path.join(out, props["Id"] + "-icon.png"), w, h, bytes(data))
    rev, layout = call(dest, props["Menu"], "com.canonical.dbusmenu", "AboutToShow",
                       GLib.Variant("(i)", (0,)), "(b)").unpack(), None
    rev, layout = call(dest, props["Menu"], "com.canonical.dbusmenu", "GetLayout",
                       GLib.Variant("(iias)", (0, -1, [])), "(u(ia{sv}av))").unpack()
    print("MENU rev", rev)
    paths = {}
    walk(layout, -1, paths)
    ok["menu"] = True
    return dest, path, props["Menu"], paths


def run_script(target):
    dest, path, menu, paths = target
    steps = list(script)

    def step():
        if not steps:
            loop.quit()
            return False
        op = steps.pop(0)
        if op == "activate":
            x, y = int(steps.pop(0)), int(steps.pop(0))
            print("> Activate", x, y)
            call(dest, path, "org.kde.StatusNotifierItem", "Activate", GLib.Variant("(ii)", (x, y)))
        elif op == "click":
            lab = steps.pop(0)
            print("> click", lab)
            call(dest, menu, "com.canonical.dbusmenu", "Event",
                 GLib.Variant("(isvu)", (paths[lab], "clicked", GLib.Variant("i", 0), 0)))
        elif op == "wait":
            GLib.timeout_add(int(float(steps.pop(0)) * 1000), step)
            return False
        elif op == "shot":                  # screenshot of the X display, while things are open
            import subprocess
            f = os.path.join(out, steps.pop(0))
            subprocess.run(["import", "-window", "root", f], check=False)
            print("> shot", f)
        elif op == "type":                  # keystrokes into whatever has the grab
            import subprocess
            subprocess.run(["xdotool", "type", "--delay", "60", steps.pop(0)], check=False)
            print("> typed")
        elif op == "key":
            import subprocess
            subprocess.run(["xdotool", "key", steps.pop(0)], check=False)
        elif op == "clickat":               # a real mouse click at X Y
            import subprocess
            x, y = steps.pop(0), steps.pop(0)
            subprocess.run(["xdotool", "mousemove", x, y, "click", "1"], check=False)
            print("> clickat", x, y)
        elif op == "menu":                  # re-read the menu (after a settings change)
            target2 = inspect(dest)
            paths.clear()
            paths.update(target2[3])
        GLib.timeout_add(300, step)
        return False

    GLib.timeout_add(500, step)


def method(c, sender, p, iface, m, params, inv):
    if m == "RegisterStatusNotifierItem":
        name = params.unpack()[0]
        if not name.startswith(":") and not name.startswith("org."):
            name = sender
        items.append(name)
        inv.return_value(None)
        GLib.timeout_add(800, lambda: (run_script(inspect(name)), False)[1])
    else:
        inv.return_value(None)


def getp(c, sender, p, iface, prop):
    return {"RegisteredStatusNotifierItems": GLib.Variant("as", items),
            "IsStatusNotifierHostRegistered": GLib.Variant("b", True),
            "ProtocolVersion": GLib.Variant("i", 0)}[prop]


node = Gio.DBusNodeInfo.new_for_xml(XML)
conn.register_object("/StatusNotifierWatcher", node.interfaces[0], method, getp, None)
Gio.bus_own_name_on_connection(conn, "org.kde.StatusNotifierWatcher", Gio.BusNameOwnerFlags.NONE, None, None)
GLib.timeout_add_seconds(60, loop.quit)
loop.run()
sys.exit(0 if ok["menu"] else 1)
