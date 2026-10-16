"""common.py — plumbing shared by the two tray apps (Linux).

The Linux twin of src/common.h: settings file, autostart, single instance,
notifications, the flyout window and where to put it.
"""
import configparser
import os
import shlex
import shutil
import subprocess
import sys

# Under a Wayland session, prefer XWayland: an X11 window can be placed
# next to the tray icon and can grab the pointer to close on an outside
# click, exactly like a menu. A native Wayland toplevel can do neither
# (the compositor decides where it goes). Pure-Wayland sessions with no
# XWayland still work; see Flyout.
if os.environ.get("DISPLAY") and "GDK_BACKEND" not in os.environ:
    os.environ["GDK_BACKEND"] = "x11"

import gi  # noqa: E402

gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
gi.require_version("PangoCairo", "1.0")
from gi.repository import Gdk, Gio, GLib, Gtk  # noqa: E402

DBUS_PREFIX = "io.github.achma_learning"


def config_dir():
    d = os.path.join(os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config"), "achma-tray")
    os.makedirs(d, exist_ok=True)
    return d


def data_dir():
    d = os.path.join(os.environ.get("XDG_DATA_HOME") or os.path.expanduser("~/.local/share"), "achma-tray")
    os.makedirs(d, exist_ok=True)
    return d


class Config:
    """An .ini in ~/.config/achma-tray — same sections and keys as the
    Windows build's %APPDATA%\\AchmaTray, so the files read alike."""

    def __init__(self, name):
        self.path = os.path.join(config_dir(), name)
        self.cp = configparser.ConfigParser(interpolation=None)
        self.cp.optionxform = str           # keep key case (custom assistant names)
        self.reload()

    def reload(self):
        self.cp.clear()
        try:
            self.cp.read(self.path, encoding="utf-8")
        except (configparser.Error, UnicodeDecodeError):
            pass                            # a broken file must not stop the app

    def get(self, sec, key, default=""):
        return self.cp.get(sec, key, fallback=default)

    def get_int(self, sec, key, default=0):
        try:
            return int(self.cp.get(sec, key, fallback=str(default)))
        except ValueError:
            return default

    def get_float(self, sec, key, default=0.0):
        try:
            return float(self.cp.get(sec, key, fallback=str(default)).replace(",", "."))
        except ValueError:
            return default

    def set(self, sec, key, value):
        if not self.cp.has_section(sec):
            self.cp.add_section(sec)
        self.cp.set(sec, key, str(value))

    def section(self, sec):
        return list(self.cp.items(sec)) if self.cp.has_section(sec) else []

    def save(self):
        tmp = self.path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            self.cp.write(f)
        os.replace(tmp, self.path)          # atomic: never a half-written file


# ── autostart (XDG: works in XFCE, KDE, GNOME, and with dex in i3) ──────

def _autostart_path(app):
    d = os.path.join(os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config"), "autostart")
    return os.path.join(d, app + ".desktop")


def self_command():
    """How to start this program again: the installed command if there is one,
    else this very script (running from a checkout)."""
    name = os.path.basename(sys.argv[0])
    found = shutil.which(name)
    return found or os.path.abspath(sys.argv[0])


def autostart_get(app):
    p = _autostart_path(app)
    if not os.path.exists(p):
        return False
    try:
        with open(p, encoding="utf-8") as f:
            text = f.read()
        return "Hidden=true" not in text and "X-GNOME-Autostart-enabled=false" not in text
    except OSError:
        return False


def autostart_set(app, name, comment, on):
    p = _autostart_path(app)
    if not on:
        try:
            os.remove(p)
        except FileNotFoundError:
            pass
        return
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as f:
        f.write("[Desktop Entry]\nType=Application\nName=%s\nComment=%s\nExec=%s\nIcon=%s\n"
                "Terminal=false\nX-GNOME-Autostart-enabled=true\nX-GNOME-Autostart-Delay=3\n"
                % (name, comment, shlex.quote(self_command()), app))


# ── single instance over D-Bus ──────────────────────────────────────────

IFACE_XML = """
<node><interface name="%s.Tray">
  <method name="Command"><arg type="s" name="cmd" direction="in"/></method>
</interface></node>""" % DBUS_PREFIX


def single_instance(name, on_command, argv_cmd):
    """Own <prefix>.<name> on the session bus. If another copy owns it, hand
    it `argv_cmd` and return False: the caller exits."""
    conn = Gio.bus_get_sync(Gio.BusType.SESSION, None)
    bus_name = "%s.%s" % (DBUS_PREFIX, name)
    path = "/" + bus_name.replace(".", "/")
    r = conn.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                       "RequestName", GLib.Variant("(su)", (bus_name, 4)),   # DO_NOT_QUEUE
                       GLib.VariantType("(u)"), Gio.DBusCallFlags.NONE, -1, None)
    if r.unpack()[0] not in (1, 4):          # 1 primary owner · 4 already ours
        conn.call_sync(bus_name, path, DBUS_PREFIX + ".Tray", "Command",
                       GLib.Variant("(s)", (argv_cmd,)), None, Gio.DBusCallFlags.NONE, 3000, None)
        return False

    def call(c, sender, p, iface, method, params, inv):
        cmd = params.unpack()[0]
        inv.return_value(None)
        GLib.idle_add(lambda: (on_command(cmd), False)[1])

    node = Gio.DBusNodeInfo.new_for_xml(IFACE_XML)
    conn.register_object(path, node.interfaces[0], call, None, None)
    return True


# ── notifications (org.freedesktop.Notifications, every desktop has it) ──

class Notifier:
    def __init__(self, app_name, icon, on_click=None):
        self.app_name, self.icon, self.on_click = app_name, icon, on_click
        self.conn = Gio.bus_get_sync(Gio.BusType.SESSION, None)
        self.last = 0
        self.conn.signal_subscribe("org.freedesktop.Notifications", "org.freedesktop.Notifications",
                                   "ActionInvoked", "/org/freedesktop/Notifications", None,
                                   Gio.DBusSignalFlags.NONE, self._action)

    def _action(self, conn, sender, path, iface, signal, params):
        nid, key = params.unpack()
        if nid == self.last and self.on_click:
            self.on_click()

    def send(self, title, body, sound=True, urgent=False):
        hints = {"desktop-entry": GLib.Variant("s", self.icon),
                 "urgency": GLib.Variant("y", 2 if urgent else 1)}
        if sound:
            hints["sound-name"] = GLib.Variant("s", "message-new-instant")
        else:
            hints["suppress-sound"] = GLib.Variant("b", True)

        def done(conn, res):
            try:
                self.last = conn.call_finish(res).unpack()[0]
            except GLib.Error:
                pass                        # no notification daemon: nothing to show it on

        self.conn.call("org.freedesktop.Notifications", "/org/freedesktop/Notifications",
                       "org.freedesktop.Notifications", "Notify",
                       GLib.Variant("(susssasa{sv}i)", (self.app_name, 0, self.icon, title, body,
                                                        ["default", "Show"], hints, 15000)),
                       None, Gio.DBusCallFlags.NONE, -1, None, done)


def desktop_name():
    return (os.environ.get("XDG_CURRENT_DESKTOP") or os.environ.get("DESKTOP_SESSION") or "").lower()


def tray_help():
    """What to tell someone whose desktop shows no tray icon."""
    d = desktop_name()
    if "gnome" in d or "ubuntu" in d:
        return ("GNOME needs the “AppIndicator and KStatusNotifierItem Support” extension.\n"
                "Fedora:  sudo dnf install gnome-shell-extension-appindicator\n"
                "Debian:  sudo apt install gnome-shell-extension-appindicator\n"
                "then log out and in, and enable it in the Extensions app.")
    if "xfce" in d:
        return "Add the “Status Tray Plugin” to a panel (Panel → Add New Items)."
    return "This desktop has no StatusNotifierItem tray running."


def open_path(path):
    """Open a file or URL with the user's default application."""
    try:
        Gio.AppInfo.launch_default_for_uri(GLib.filename_to_uri(path, None) if path.startswith("/") else path, None)
    except GLib.Error:
        subprocess.Popen(["xdg-open", path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def install_page(src_candidates, name):
    """Copy an HTML page to ~/.local/share/achma-tray/<name> (fixed path, so
    the page's localStorage survives upgrades) and return that path."""
    for src in src_candidates:
        if os.path.exists(src):
            dst = os.path.join(data_dir(), name)
            with open(src, "rb") as f:
                data = f.read()
            try:
                with open(dst, "rb") as f:
                    same = f.read() == data
            except OSError:
                same = False
            if not same:
                with open(dst, "wb") as f:
                    f.write(data)
            return dst
    return None


def page_candidates(name):
    here = os.path.dirname(os.path.abspath(__file__))
    return [os.path.join(here, "pages", name),                              # installed package
            os.path.join(here, "..", "..", "..", "html file", name)]        # repo checkout


# ── the flyout: a borderless panel that opens beside the tray icon ──────

def is_x11():
    return type(Gdk.Display.get_default()).__name__.startswith("X11")


def pointer_position():
    try:
        seat = Gdk.Display.get_default().get_default_seat()
        _, x, y = seat.get_pointer().get_position()
        return x, y
    except Exception:
        return 0, 0


def place_near(x, y, w, h, gap=8):
    """Top-left corner for a w×h window opened from a tray icon at (x, y),
    on the side away from the panel and inside the monitor's work area."""
    display = Gdk.Display.get_default()
    mon = display.get_monitor_at_point(x, y) or display.get_primary_monitor() or display.get_monitor(0)
    wa = mon.get_workarea()
    if x < wa.x:                                    # panel on the left
        px, py = wa.x + gap, y - h // 2
    elif x >= wa.x + wa.width:                      # right
        px, py = wa.x + wa.width - w - gap, y - h // 2
    elif y >= wa.y + wa.height // 2:                # bottom half → open upwards
        px, py = x - w // 2, y - h - gap
    else:                                           # top half → downwards
        px, py = x - w // 2, y + gap
    px = max(wa.x + gap, min(px, wa.x + wa.width - w - gap))
    py = max(wa.y + gap, min(py, wa.y + wa.height - h - gap))
    return px, py


class Flyout(Gtk.Window):
    """X11 (incl. XWayland): an override-redirect popup with a pointer grab,
    i.e. it behaves like a menu — exact placement, closes on any outside
    click or Esc, works under every window manager including i3.
    Native Wayland: a borderless keep-above window that closes when it loses
    focus; gtk-layer-shell, when installed, pins it to a screen corner."""

    def __init__(self, width, height):
        self.x11 = is_x11()
        super().__init__(type=Gtk.WindowType.POPUP if self.x11 else Gtk.WindowType.TOPLEVEL)
        self.w, self.h = width, height
        self.hidden_at = 0
        self.grabbed = False
        self.set_default_size(width, height)
        self.set_size_request(width, height)
        self.set_resizable(False)
        self.set_app_paintable(True)
        self.add_events(Gdk.EventMask.BUTTON_PRESS_MASK | Gdk.EventMask.BUTTON_RELEASE_MASK |
                        Gdk.EventMask.KEY_PRESS_MASK | Gdk.EventMask.POINTER_MOTION_MASK)
        self.connect("button-press-event", self._press)
        self.connect("key-press-event", self._key)
        self.layer = None
        if not self.x11:
            self.set_decorated(False)
            self.set_keep_above(True)
            self.set_skip_taskbar_hint(True)
            self.set_skip_pager_hint(True)
            self.set_type_hint(Gdk.WindowTypeHint.UTILITY)
            self.connect("focus-out-event", lambda *a: self.close_flyout())
            try:
                gi.require_version("GtkLayerShell", "0.1")
                from gi.repository import GtkLayerShell
                if GtkLayerShell.is_supported():
                    GtkLayerShell.init_for_window(self)
                    GtkLayerShell.set_layer(self, GtkLayerShell.Layer.OVERLAY)
                    GtkLayerShell.set_keyboard_mode(self, GtkLayerShell.KeyboardMode.ON_DEMAND)
                    self.layer = GtkLayerShell
            except (ValueError, ImportError):
                pass

    def toggle(self, x=0, y=0):
        if self.get_visible():
            self.close_flyout()
        elif GLib.get_monotonic_time() - self.hidden_at > 250000:   # the click that just closed it
            self.open_at(x, y)

    def open_at(self, x=0, y=0):
        if self.x11:
            display = Gdk.Display.get_default()
            on_screen = any(_contains(display.get_monitor(i).get_geometry(), x, y)
                            for i in range(display.get_n_monitors()))
            if (x, y) == (0, 0) or not on_screen:
                x, y = pointer_position()
            h = min(self.h, display.get_monitor_at_point(x, y).get_workarea().height - 24)
            self.set_size_request(self.w, h)
            self.resize(self.w, h)
            self.move(*place_near(x, y, self.w, h))
        elif self.layer:
            L = self.layer
            display = Gdk.Display.get_default()
            mon = display.get_primary_monitor() or display.get_monitor(0)
            g = mon.get_geometry()
            bottom = y > g.y + g.height // 2 if (x, y) != (0, 0) else True
            right = x > g.x + g.width // 2 if (x, y) != (0, 0) else True
            for edge, on in ((L.Edge.BOTTOM, bottom), (L.Edge.TOP, not bottom),
                             (L.Edge.RIGHT, right), (L.Edge.LEFT, not right)):
                L.set_anchor(self, edge, on)
                L.set_margin(self, edge, 12 if on else 0)
        self.show_all()
        self.present()
        if self.x11:
            GLib.idle_add(self._grab)

    def _grab(self):
        gw = self.get_window()
        if gw is None:
            return False
        seat = Gdk.Display.get_default().get_default_seat()
        st = seat.grab(gw, Gdk.SeatCapabilities.ALL, True, None, None, None, None)
        self.grabbed = st == Gdk.GrabStatus.SUCCESS
        return False

    def close_flyout(self):
        if not self.get_visible():
            return
        if self.grabbed:
            Gdk.Display.get_default().get_default_seat().ungrab()
            self.grabbed = False
        self.hide()
        self.hidden_at = GLib.get_monotonic_time()

    def _press(self, w, ev):
        # With the grab, a click anywhere on screen lands here; compare in root
        # coordinates so a click inside a child widget is never mistaken for one
        # outside.
        gw = self.get_window()
        if gw is None:
            return False
        _, ox, oy = gw.get_origin()
        a = self.get_allocation()
        if not (ox <= ev.x_root < ox + a.width and oy <= ev.y_root < oy + a.height):
            self.close_flyout()
            return True
        return False

    def _key(self, w, ev):
        if ev.keyval == Gdk.KEY_Escape:
            self.close_flyout()
            return True
        return False


def _contains(r, x, y):
    return r.x <= x < r.x + r.width and r.y <= y < r.y + r.height
