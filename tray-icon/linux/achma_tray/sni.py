"""sni.py — a tray icon over D-Bus: StatusNotifierItem + DBusMenu.

This is the modern Linux tray protocol (the "KDE/freedesktop" one). It is
implemented here directly on Gio's D-Bus rather than through libappindicator /
libayatana-appindicator, for two reasons:

  * those libraries cannot receive a plain left click (they always open the
    menu), and the prayer flyout needs exactly that click;
  * their GObject typelibs have different names on Debian and Fedora, which
    would split the packaging. Gio is in every GTK install.

Who draws it:
  KDE Plasma      natively
  XFCE            the panel's "Status Tray" plugin (xfce4-panel >= 4.16)
  GNOME           the "AppIndicator and KStatusNotifierItem Support" extension
                  (preinstalled on Ubuntu; one package on Fedora/Debian)
  i3/sway/others  any SNI host: xfce4-panel, waybar, polybar+snixembed, ...

Spec: https://www.freedesktop.org/wiki/Specifications/StatusNotifierItem/
DBusMenu: libdbusmenu's com.canonical.dbusmenu.xml (version 3).
"""
import os
import sys

from gi.repository import Gio, GLib

SNI_IFACE = "org.kde.StatusNotifierItem"
SNI_PATH = "/StatusNotifierItem"
MENU_IFACE = "com.canonical.dbusmenu"
MENU_PATH = "/MenuBar"
WATCHER = "org.kde.StatusNotifierWatcher"

SNI_XML = """
<node><interface name="org.kde.StatusNotifierItem">
  <property name="Category" type="s" access="read"/>
  <property name="Id" type="s" access="read"/>
  <property name="Title" type="s" access="read"/>
  <property name="Status" type="s" access="read"/>
  <property name="WindowId" type="i" access="read"/>
  <property name="IconThemePath" type="s" access="read"/>
  <property name="IconName" type="s" access="read"/>
  <property name="IconPixmap" type="a(iiay)" access="read"/>
  <property name="OverlayIconName" type="s" access="read"/>
  <property name="OverlayIconPixmap" type="a(iiay)" access="read"/>
  <property name="AttentionIconName" type="s" access="read"/>
  <property name="AttentionIconPixmap" type="a(iiay)" access="read"/>
  <property name="AttentionMovieName" type="s" access="read"/>
  <property name="ToolTip" type="(sa(iiay)ss)" access="read"/>
  <property name="ItemIsMenu" type="b" access="read"/>
  <property name="Menu" type="o" access="read"/>
  <method name="ContextMenu"><arg name="x" type="i" direction="in"/><arg name="y" type="i" direction="in"/></method>
  <method name="Activate"><arg name="x" type="i" direction="in"/><arg name="y" type="i" direction="in"/></method>
  <method name="SecondaryActivate"><arg name="x" type="i" direction="in"/><arg name="y" type="i" direction="in"/></method>
  <method name="Scroll"><arg name="delta" type="i" direction="in"/><arg name="orientation" type="s" direction="in"/></method>
  <signal name="NewTitle"/>
  <signal name="NewIcon"/>
  <signal name="NewAttentionIcon"/>
  <signal name="NewOverlayIcon"/>
  <signal name="NewToolTip"/>
  <signal name="NewStatus"><arg name="status" type="s"/></signal>
</interface></node>
"""

MENU_XML = """
<node><interface name="com.canonical.dbusmenu">
  <property name="Version" type="u" access="read"/>
  <property name="TextDirection" type="s" access="read"/>
  <property name="Status" type="s" access="read"/>
  <property name="IconThemePath" type="as" access="read"/>
  <method name="GetLayout">
    <arg type="i" name="parentId" direction="in"/>
    <arg type="i" name="recursionDepth" direction="in"/>
    <arg type="as" name="propertyNames" direction="in"/>
    <arg type="u" name="revision" direction="out"/>
    <arg type="(ia{sv}av)" name="layout" direction="out"/>
  </method>
  <method name="GetGroupProperties">
    <arg type="ai" name="ids" direction="in"/>
    <arg type="as" name="propertyNames" direction="in"/>
    <arg type="a(ia{sv})" name="properties" direction="out"/>
  </method>
  <method name="GetProperty">
    <arg type="i" name="id" direction="in"/>
    <arg type="s" name="name" direction="in"/>
    <arg type="v" name="value" direction="out"/>
  </method>
  <method name="Event">
    <arg type="i" name="id" direction="in"/>
    <arg type="s" name="eventId" direction="in"/>
    <arg type="v" name="data" direction="in"/>
    <arg type="u" name="timestamp" direction="in"/>
  </method>
  <method name="EventGroup">
    <arg type="a(isvu)" name="events" direction="in"/>
    <arg type="ai" name="idErrors" direction="out"/>
  </method>
  <method name="AboutToShow">
    <arg type="i" name="id" direction="in"/>
    <arg type="b" name="needUpdate" direction="out"/>
  </method>
  <method name="AboutToShowGroup">
    <arg type="ai" name="ids" direction="in"/>
    <arg type="ai" name="updatesNeeded" direction="out"/>
    <arg type="ai" name="idErrors" direction="out"/>
  </method>
  <signal name="ItemsPropertiesUpdated">
    <arg type="a(ia{sv})" name="updatedProps"/>
    <arg type="a(ias)" name="removedProps"/>
  </signal>
  <signal name="LayoutUpdated">
    <arg type="u" name="revision"/>
    <arg type="i" name="parent"/>
  </signal>
  <signal name="ItemActivationRequested">
    <arg type="i" name="id"/>
    <arg type="u" name="timestamp"/>
  </signal>
</interface></node>
"""


# ── menu model ──────────────────────────────────────────────────────────

class Item:
    """One menu entry. `children` makes it a submenu; `action` runs on click."""

    def __init__(self, label="", action=None, children=None, enabled=True, visible=True,
                 toggle=None, checked=False, icon_png=None, separator=False, bold=False):
        self.label = label
        self.action = action
        self.children = children
        self.enabled = enabled
        self.visible = visible
        self.toggle = toggle          # None, "checkmark" or "radio"
        self.checked = checked
        self.icon_png = icon_png      # bytes of a PNG, or None
        self.separator = separator
        self.bold = bold
        self.id = 0

    def props(self):
        p = {}
        if self.separator:
            p["type"] = GLib.Variant("s", "separator")
            return p
        # dbusmenu labels use _ as the mnemonic marker; a literal one is doubled
        p["label"] = GLib.Variant("s", self.label.replace("_", "__"))
        if not self.enabled:
            p["enabled"] = GLib.Variant("b", False)
        if not self.visible:
            p["visible"] = GLib.Variant("b", False)
        if self.toggle:
            p["toggle-type"] = GLib.Variant("s", self.toggle)
            p["toggle-state"] = GLib.Variant("i", 1 if self.checked else 0)
        if self.icon_png:
            p["icon-data"] = GLib.Variant("ay", self.icon_png)
        if self.children is not None:
            p["children-display"] = GLib.Variant("s", "submenu")
        return p


def Separator():
    return Item(separator=True)


class DBusMenu:
    """Serves a tree of Items. `build` is called whenever a host is about to
    show the menu, so the menu always reflects current settings."""

    def __init__(self, conn, build):
        self.conn = conn
        self.build = build
        self.revision = 1
        self.items = {}
        self.root = Item(children=[])
        self._signature = None
        self.rebuild(emit=False)
        node = Gio.DBusNodeInfo.new_for_xml(MENU_XML)
        conn.register_object(MENU_PATH, node.interfaces[0], self._call, self._get, None)

    def rebuild(self, emit=True):
        """Re-run build(); return True (and tell hosts) if anything changed."""
        root = Item(children=self.build())
        items = {}
        counter = [0]

        def number(it):
            it.id = counter[0]
            items[it.id] = it
            counter[0] += 1
            for c in it.children or []:
                number(c)

        number(root)
        sig = self._sig(root)
        changed = sig != self._signature
        self.root, self.items = root, items
        if changed:
            self._signature = sig
            self.revision += 1
            if emit:
                self.conn.emit_signal(None, MENU_PATH, MENU_IFACE, "LayoutUpdated",
                                      GLib.Variant("(ui)", (self.revision, 0)))
        return changed

    def _sig(self, it):
        return (it.label, it.enabled, it.visible, it.toggle, it.checked, it.separator,
                len(it.icon_png or b""), tuple(self._sig(c) for c in it.children or ()) if it.children is not None else None)

    def _layout(self, it, depth):
        kids = []
        if it.children and depth != 0:
            kids = [self._layout(c, depth - 1) for c in it.children]
        return GLib.Variant("(ia{sv}av)", (it.id, it.props(), kids))

    def _call(self, conn, sender, path, iface, method, params, inv):
        try:
            if method == "GetLayout":
                parent, depth, _names = params.unpack()
                it = self.items.get(parent)
                if it is None:
                    inv.return_dbus_error("com.canonical.dbusmenu.Error.UnknownId", "no such item")
                    return
                # GLib.Variant can't nest a ready-made variant in a tuple literal,
                # so the layout is built as its own variant and spliced in.
                lay = self._layout(it, depth)
                inv.return_value(GLib.Variant.new_tuple(GLib.Variant("u", self.revision), lay))
            elif method == "GetGroupProperties":
                ids, _names = params.unpack()
                ids = ids or list(self.items)
                out = [(i, self.items[i].props()) for i in ids if i in self.items]
                inv.return_value(GLib.Variant("(a(ia{sv}))", (out,)))
            elif method == "GetProperty":
                i, name = params.unpack()
                p = self.items.get(i)
                v = p.props().get(name) if p else None
                if v is None:
                    inv.return_dbus_error("com.canonical.dbusmenu.Error.UnknownProperty", name)
                else:
                    inv.return_value(GLib.Variant.new_tuple(GLib.Variant("v", v)))
            elif method == "Event":
                i, ev, _data, _ts = params.unpack()
                self._event(i, ev)
                inv.return_value(None)
            elif method == "EventGroup":
                events = params.unpack()[0]
                bad = []
                for i, ev, _d, _t in events:
                    if i in self.items:
                        self._event(i, ev)
                    else:
                        bad.append(i)
                inv.return_value(GLib.Variant("(ai)", (bad,)))
            elif method == "AboutToShow":
                (i,) = params.unpack()
                changed = self.rebuild() if i == 0 else False
                inv.return_value(GLib.Variant("(b)", (changed,)))
            elif method == "AboutToShowGroup":
                ids = params.unpack()[0]
                changed = self.rebuild() if 0 in ids else False
                inv.return_value(GLib.Variant("(aiai)", ([0] if changed else [], [])))
            else:
                inv.return_dbus_error("org.freedesktop.DBus.Error.UnknownMethod", method)
        except Exception as e:      # never leave a host hanging on a reply
            inv.return_dbus_error("org.freedesktop.DBus.Error.Failed", str(e))

    def _event(self, i, ev):
        it = self.items.get(i)
        if ev == "clicked" and it and it.action and it.enabled:
            # after the reply, so a slow action never stalls the host's menu
            GLib.idle_add(lambda: (it.action(), False)[1])

    def _get(self, conn, sender, path, iface, prop):
        return {
            "Version": GLib.Variant("u", 3),
            "TextDirection": GLib.Variant("s", "ltr"),
            "Status": GLib.Variant("s", "normal"),
            "IconThemePath": GLib.Variant("as", []),
        }.get(prop)


# ── the item ────────────────────────────────────────────────────────────

def argb_pixmap(surface):
    """cairo ARGB32 (native-endian, premultiplied) → SNI pixmap tuple
    (w, h, bytes): ARGB, network byte order, straight alpha."""
    surface.flush()
    w, h, stride = surface.get_width(), surface.get_height(), surface.get_stride()
    src = bytes(surface.get_data())
    out = bytearray(w * h * 4)
    little = sys.byteorder == "little"
    o = 0
    for y in range(h):
        row = y * stride
        for x in range(w):
            i = row + x * 4
            if little:
                b, g, r, a = src[i], src[i + 1], src[i + 2], src[i + 3]
            else:
                a, r, g, b = src[i], src[i + 1], src[i + 2], src[i + 3]
            if 0 < a < 255:
                r, g, b = min(255, r * 255 // a), min(255, g * 255 // a), min(255, b * 255 // a)
            out[o], out[o + 1], out[o + 2], out[o + 3] = a, r, g, b
            o += 4
    return (w, h, bytes(out))


class StatusNotifierItem:
    """One tray icon. Call set_icon()/set_tooltip() whenever they change.

    on_activate(x, y)  left click (x, y are the host's screen coordinates,
                       or 0, 0 when it does not know them)
    on_secondary()     middle click
    """

    def __init__(self, app_id, title, build_menu, on_activate=None, on_secondary=None,
                 item_is_menu=False, category="ApplicationStatus"):
        self.conn = Gio.bus_get_sync(Gio.BusType.SESSION, None)
        self.app_id, self.title, self.category = app_id, title, category
        self.on_activate, self.on_secondary = on_activate, on_secondary
        self.item_is_menu = item_is_menu
        self.pixmaps = []
        self.icon_name = ""
        self.tooltip = ("", "")
        self.host_seen = False
        self.on_host_missing = None
        self.menu = DBusMenu(self.conn, build_menu)
        node = Gio.DBusNodeInfo.new_for_xml(SNI_XML)
        self.conn.register_object(SNI_PATH, node.interfaces[0], self._call, self._get, None)
        # the well-known name hosts look for; the spec's "-<pid>-<n>" form
        self.bus_name = "org.kde.StatusNotifierItem-%d-1" % os.getpid()
        Gio.bus_own_name_on_connection(self.conn, self.bus_name, Gio.BusNameOwnerFlags.NONE, None, None)
        # (re-)register every time a watcher appears: panel restarts, late
        # session start, GNOME extension switched on after login...
        Gio.bus_watch_name_on_connection(self.conn, WATCHER, Gio.BusNameWatcherFlags.NONE,
                                         self._watcher_up, self._watcher_down)
        GLib.timeout_add_seconds(10, self._check_host)

    # host side
    def _watcher_up(self, conn, name, owner):
        self.host_seen = True
        conn.call(WATCHER, "/StatusNotifierWatcher", WATCHER, "RegisterStatusNotifierItem",
                  GLib.Variant("(s)", (self.bus_name,)), None, Gio.DBusCallFlags.NONE, -1, None, None)

    def _watcher_down(self, conn, name):
        pass

    def _check_host(self):
        if not self.host_seen and self.on_host_missing:
            self.on_host_missing()
        return False

    # updates
    def set_icon(self, pixmaps=None, icon_name=None):
        if pixmaps is not None:
            self.pixmaps = pixmaps
        if icon_name is not None:
            self.icon_name = icon_name
        self._signal("NewIcon")

    def set_tooltip(self, title, body=""):
        if (title, body) != self.tooltip:
            self.tooltip = (title, body)
            self._signal("NewToolTip")

    def refresh_menu(self):
        self.menu.rebuild()

    def _signal(self, name):
        self.conn.emit_signal(None, SNI_PATH, SNI_IFACE, name, None)

    # D-Bus
    def _get(self, conn, sender, path, iface, prop):
        pm = GLib.Variant("a(iiay)", self.pixmaps)
        return {
            "Category": GLib.Variant("s", self.category),
            "Id": GLib.Variant("s", self.app_id),
            "Title": GLib.Variant("s", self.title),
            "Status": GLib.Variant("s", "Active"),
            "WindowId": GLib.Variant("i", 0),
            "IconThemePath": GLib.Variant("s", ""),
            "IconName": GLib.Variant("s", self.icon_name),
            "IconPixmap": pm,
            "OverlayIconName": GLib.Variant("s", ""),
            "OverlayIconPixmap": GLib.Variant("a(iiay)", []),
            "AttentionIconName": GLib.Variant("s", ""),
            "AttentionIconPixmap": GLib.Variant("a(iiay)", []),
            "AttentionMovieName": GLib.Variant("s", ""),
            "ToolTip": GLib.Variant("(sa(iiay)ss)", ("", [], self.tooltip[0], self.tooltip[1])),
            "ItemIsMenu": GLib.Variant("b", self.item_is_menu),
            "Menu": GLib.Variant("o", MENU_PATH),
        }.get(prop)

    def _call(self, conn, sender, path, iface, method, params, inv):
        args = params.unpack()
        inv.return_value(None)
        if method == "Activate" and self.on_activate:
            GLib.idle_add(lambda: (self.on_activate(args[0], args[1]), False)[1])
        elif method == "SecondaryActivate" and self.on_secondary:
            GLib.idle_add(lambda: (self.on_secondary(), False)[1])
