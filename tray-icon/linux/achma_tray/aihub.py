"""aihub.py — AI Hub for Linux: the chatbot launcher in the system tray.

Same behaviour as ai-hub-tray.exe:
  left click   → launcher: type to filter, ↑↓ ↵, favourites first
  right click  → every assistant, plus settings (default assistant,
                 favourites, left-click behaviour, own window, shortcut,
                 your own assistants, start at login)

A global hotkey cannot be grabbed portably on Linux (Wayland forbids it), so
Ctrl+Alt+A is registered with the desktop instead — GNOME and XFCE
automatically, KDE/i3/others with a copy-paste instruction — and the desktop
runs:
    ai-hub-tray --open-default

Opening a chatbot follows ai-hub/bin/ai-open: Chromium-family browsers get
--app=<url> (own window, and a WM_CLASS instance i3 rules can match),
Firefox gets --class=AIHub --new-window.
"""
import io
import math
import os
import shlex
import shutil
import subprocess
import sys

from . import common  # sets GDK_BACKEND before Gtk loads
from .common import Config, Flyout, Notifier
from .sni import Item, Separator, StatusNotifierItem, argb_pixmap

import cairo  # noqa: E402
from gi.repository import Gdk, Gio, GLib, Gtk, Pango, PangoCairo  # noqa: E402

APP = "achma-ai-hub-tray"
NAME = "AI Hub"
VERSION = "1.0.0"
CMD_OPEN_DEFAULT = "--open-default"

# ai-hub.html SERVICES, same order: id, name, provider, url, colour, favourite
BUILTIN = [
    ("chatgpt", "ChatGPT", "OpenAI", "https://chatgpt.com/", "#10a37f", True),
    ("claude", "Claude", "Anthropic", "https://claude.ai/new", "#d97757", True),
    ("gemini", "Gemini", "Google", "https://gemini.google.com/", "#4285f4", True),
    ("deepseek", "DeepSeek", "DeepSeek", "https://chat.deepseek.com/", "#4d78ff", True),
    ("perplexity", "Perplexity", "Perplexity", "https://www.perplexity.ai/", "#20b8cd", True),
    ("notebooklm", "NotebookLM", "Google", "https://notebooklm.google.com/", "#5f6368", False),
    ("grok", "Grok", "xAI", "https://grok.com/", "#8a8f9c", False),
    ("kimi", "Kimi", "Moonshot AI", "https://www.kimi.com/", "#695cff", False),
    ("mistral", "Le Chat", "Mistral AI", "https://chat.mistral.ai/", "#ff7000", False),
    ("qwen", "Qwen", "Alibaba", "https://chat.qwen.ai/", "#6257ee", False),
    ("aistudio", "AI Studio", "Google", "https://aistudio.google.com/", "#1a73e8", False),
    ("copilot", "Copilot", "Microsoft", "https://copilot.microsoft.com/", "#36a5ff", False),
    ("metaai", "Meta AI", "Meta", "https://www.meta.ai/", "#087cff", False),
    ("huggingchat", "HuggingChat", "Hugging Face", "https://huggingface.co/chat/", "#ff9d00", False),
    ("poe", "Poe", "Quora", "https://poe.com/", "#8b5cf6", False),
]
CUSTOM_COLOURS = ["#7c5cff", "#00b4ff", "#e8a33d", "#4fb09a", "#c96a4e"]


def rgb(h):
    return tuple(int(h[i:i + 2], 16) / 255 for i in (1, 3, 5))


def round_rect(cr, x, y, w, h, r):
    cr.new_sub_path()
    cr.arc(x + w - r, y + r, r, -math.pi / 2, 0)
    cr.arc(x + w - r, y + h - r, r, 0, math.pi / 2)
    cr.arc(x + r, y + h - r, r, math.pi / 2, math.pi)
    cr.arc(x + r, y + r, r, math.pi, 3 * math.pi / 2)
    cr.close_path()


def draw_tile(cr, size, colour, letter):
    """Brand-coloured tile with the initial — what the hub page shows."""
    round_rect(cr, 0, 0, size, size, size * 0.26)
    cr.set_source_rgb(*rgb(colour))
    cr.fill()
    lo = PangoCairo.create_layout(cr)
    fd = Pango.FontDescription.from_string("Sans Bold")
    fd.set_absolute_size(size * 0.58 * Pango.SCALE)
    lo.set_font_description(fd)
    lo.set_text(letter, -1)
    _ink, logical = lo.get_pixel_extents()
    cr.set_source_rgb(1, 1, 1)
    cr.move_to((size - logical.width) / 2 - logical.x, (size - logical.height) / 2 - logical.y)
    PangoCairo.show_layout(cr, lo)


def tile_png(colour, letter, size=16):
    s = cairo.ImageSurface(cairo.FORMAT_ARGB32, size, size)
    draw_tile(cairo.Context(s), size, colour, letter)
    b = io.BytesIO()
    s.write_to_png(b)
    return b.getvalue()


def hub_icon(z):
    """Violet→blue tile with the hub's four-point spark."""
    s = cairo.ImageSurface(cairo.FORMAT_ARGB32, z, z)
    cr = cairo.Context(s)
    round_rect(cr, 0.5, 0.5, z - 1, z - 1, z * 0.28)
    g = cairo.LinearGradient(0, 0, z, z)
    g.add_color_stop_rgb(0, *rgb("#7c5cff"))
    g.add_color_stop_rgb(1, *rgb("#4f8cff"))
    cr.set_source(g)
    cr.fill()
    c, R, r = z / 2, z * 0.34, z * 0.085
    pts = [(c, c - R), (c + r, c - r), (c + R, c), (c + r, c + r), (c, c + R), (c - r, c + r), (c - R, c), (c - r, c - r)]
    cr.move_to(*pts[0])
    for p in pts[1:]:
        cr.line_to(*p)
    cr.close_path()
    cr.set_source_rgb(1, 1, 1)
    cr.fill()
    return s


# ── opening a URL (ai-hub/bin/ai-open, in Python) ───────────────────────

CHROMIUM = ("chrome", "chromium", "brave", "msedge", "microsoft-edge", "vivaldi", "opera", "thorium", "yandex")
FIREFOX = ("firefox", "librewolf", "waterfox", "floorp", "zen")


def browser_command():
    """The default browser's command line, field codes removed. Handles
    native packages and Flatpaks alike (their Exec runs `flatpak run …`)."""
    app = Gio.AppInfo.get_default_for_uri_scheme("https") or Gio.AppInfo.get_default_for_type("text/html", False)
    if not app or not app.get_commandline():
        return None, ""
    try:
        argv = shlex.split(app.get_commandline())
    except ValueError:
        return None, ""
    argv = [a for a in argv if not (a.startswith("%") and len(a) == 2) and a not in ("@@u", "@@", "@@U")]
    return argv, (app.get_id() or "").lower() + " " + " ".join(argv).lower()


def find_chromium():
    for exe in ("google-chrome", "google-chrome-stable", "chromium", "chromium-browser", "brave-browser",
                "microsoft-edge", "microsoft-edge-stable", "vivaldi-stable", "vivaldi"):
        p = shutil.which(exe)
        if p:
            return [p]
    return None


def open_url(url, own_window):
    if own_window:
        argv, ident = browser_command()
        if argv and any(k in ident for k in CHROMIUM):
            return _spawn(argv + ["--app=" + url])
        if argv and any(k in ident for k in FIREFOX):
            return _spawn(argv + ["--class=AIHub", "--new-window", url])
        other = find_chromium()            # default browser has no app mode; another one might
        if other:
            return _spawn(other + ["--app=" + url])
    try:
        Gio.AppInfo.launch_default_for_uri(url, None)
    except GLib.Error:
        _spawn(["xdg-open", url])


def _spawn(argv):
    try:
        subprocess.Popen(argv, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                         start_new_session=True)
        return True
    except OSError:
        return False


# ── keyboard shortcut, registered with the desktop ──────────────────────

def setup_shortcut():
    """Bind Ctrl+Alt+A to `ai-hub-tray --open-default`. Returns (done, message)."""
    cmd = "%s %s" % (shlex.quote(common.self_command()), CMD_OPEN_DEFAULT)
    d = common.desktop_name()
    if any(k in d for k in ("gnome", "ubuntu", "unity", "pop", "budgie")):
        src = Gio.SettingsSchemaSource.get_default()
        base_id = "org.gnome.settings-daemon.plugins.media-keys"
        if src and src.lookup(base_id, True) and src.lookup(base_id + ".custom-keybinding", True):
            path = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/achma-ai-hub/"
            base = Gio.Settings.new(base_id)
            paths = list(base.get_strv("custom-keybindings"))
            if path not in paths:
                base.set_strv("custom-keybindings", paths + [path])
            kb = Gio.Settings.new_with_path(base_id + ".custom-keybinding", path)
            kb.set_string("name", "AI Hub — open default assistant")
            kb.set_string("command", cmd)
            kb.set_string("binding", "<Control><Alt>a")
            Gio.Settings.sync()
            return True, "Ctrl+Alt+A now opens your default assistant (GNOME Settings → Keyboard → Custom Shortcuts)."
    if "xfce" in d and shutil.which("xfconf-query"):
        r = subprocess.run(["xfconf-query", "-c", "xfce4-keyboard-shortcuts", "-p", "/commands/custom/<Primary><Alt>a",
                            "-n", "-t", "string", "-s", cmd], capture_output=True)
        if r.returncode == 0:
            return True, "Ctrl+Alt+A now opens your default assistant (Settings → Keyboard → Application Shortcuts)."
    if "kde" in d or "plasma" in d:
        return False, ("In KDE: System Settings → Keyboard → Shortcuts → Add New → Command or Script,\n"
                       "command:  %s\nthen press Ctrl+Alt+A as its shortcut." % cmd)
    return False, ("Bind Ctrl+Alt+A to this command in your desktop's keyboard settings:\n  %s\n\n"
                   "i3 / sway:\n  bindsym Control+Mod1+a exec --no-startup-id %s" % (cmd, cmd))


# ── the app ─────────────────────────────────────────────────────────────

CSS = b"""
#aihub { background: #11141b; color: #e9e5db; border: 1px solid #333d4d; }
#aihub .brand { font-weight: bold; font-size: 15px; }
#aihub .dim { color: #818c9e; font-size: 11px; }
#aihub .section { color: #5a6473; font-size: 9px; letter-spacing: 2px; font-weight: bold; padding: 8px 12px 2px; }
#aihub entry { background: #1d2430; color: #e9e5db; border: 1px solid #333d4d; border-radius: 8px;
               box-shadow: none; caret-color: #e8a33d; min-height: 30px; }
#aihub entry:focus { border-color: #7c5cff; }
#aihub list, #aihub scrolledwindow, #aihub viewport { background: transparent; }
#aihub row { padding: 5px 10px; border-radius: 8px; margin: 0 6px; background: transparent; color: #e9e5db; }
#aihub row:selected, #aihub row:hover { background: rgba(124, 92, 255, 0.18); }
#aihub .footer button { color: #818c9e; background: none; border: none; box-shadow: none; padding: 2px 6px; }
#aihub .footer button:hover { color: #e9e5db; }
"""


class AIHubTray:
    def __init__(self):
        self.cfg = Config("ai-hub-tray.ini")
        self.load()
        self.notifier = Notifier(NAME, APP)
        self.fly = None
        self.sni = StatusNotifierItem(APP, NAME, self.build_menu, on_activate=self.on_activate,
                                      on_secondary=self.open_default)
        self.sni.on_host_missing = lambda: self.notifier.send("No system tray found", common.tray_help(), sound=False)
        self.sni.set_icon(pixmaps=[argb_pixmap(hub_icon(z)) for z in (16, 22, 24, 32, 48, 64)])
        self.update_tooltip()

    # settings — re-read on every menu/launcher open so .ini edits apply at once
    def load(self):
        c = self.cfg
        c.reload()
        favs = c.get("hub", "favorites", None)
        favset = set(f.strip() for f in favs.split(",")) if favs is not None else None
        self.services = []
        for sid, name, prov, url, col, fav in BUILTIN:
            self.services.append({"id": sid, "name": name, "provider": prov, "url": url, "color": col,
                                  "fav": (sid in favset) if favset is not None else fav})
        k = 0
        for name, url in c.section("custom"):
            name, url = name.strip(), url.strip()
            if name.startswith((";", "#")) or not url.lower().startswith(("https://", "http://")):
                continue
            sid = "custom%d" % k
            self.services.append({"id": sid, "name": name, "provider": "custom", "url": url,
                                  "color": CUSTOM_COLOURS[k % 5], "fav": (sid in favset) if favset is not None else True})
            k += 1
        self.default = c.get("hub", "default", "claude")
        if not self.find(self.default):
            self.default = "claude"
        self.own_window = c.get_int("hub", "app_window", 1) == 1
        self.left_default = c.get_int("hub", "left_click_opens_default", 0) == 1
        self.icons = {s["id"]: tile_png(s["color"], s["name"][:1].upper()) for s in self.services}

    def find(self, sid):
        return next((s for s in self.services if s["id"] == sid), None)

    def save(self, key, value):
        self.cfg.set("hub", key, value)
        self.cfg.save()

    def update_tooltip(self):
        d = self.find(self.default)
        self.sni.set_tooltip(NAME, "Click to launch · %s on the shortcut" % (d["name"] if d else ""))

    # actions
    def launch(self, s):
        if s:
            open_url(s["url"], self.own_window)

    def open_default(self):
        self.load()
        self.launch(self.find(self.default))

    def on_activate(self, x, y):
        self.load()
        if self.left_default:
            self.launch(self.find(self.default))
            return
        if self.fly is None:
            self.fly = Launcher(self)
        if self.fly.get_visible():
            self.fly.close_flyout()
        else:
            self.fly.populate()
            self.fly.toggle(x, y)
            self.fly.entry.grab_focus()

    def on_command(self, cmd):
        if cmd == "open-default":
            self.open_default()
        else:
            self.on_activate(0, 0)

    def open_hub_page(self):
        p = common.install_page(common.page_candidates("ai-hub.html"), "ai-hub.html")
        if p:
            common.open_path(p)

    def toggle_fav(self, sid):
        s = self.find(sid)
        s["fav"] = not s["fav"]
        self.save("favorites", ",".join(x["id"] for x in self.services if x["fav"]))

    def set_default(self, sid):
        self.default = sid
        self.save("default", sid)
        self.update_tooltip()

    def shortcut(self):
        done, msg = setup_shortcut()
        if done:
            self.notifier.send("Keyboard shortcut set", msg, sound=False)
        else:
            dlg = Gtk.MessageDialog(message_type=Gtk.MessageType.INFO, buttons=Gtk.ButtonsType.OK,
                                    text="Keyboard shortcut Ctrl+Alt+A")
            dlg.format_secondary_text(msg)
            dlg.set_title(NAME)
            for w in dlg.get_message_area().get_children():
                if isinstance(w, Gtk.Label):
                    w.set_selectable(True)
            dlg.run()
            dlg.destroy()

    def edit_custom(self):
        if not self.cfg.cp.has_section("custom"):
            self.cfg.save()
            with open(self.cfg.path, "a", encoding="utf-8") as f:
                f.write("[custom]\n; one assistant per line — delete the leading ; to enable one\n"
                        "; Claude Projects = https://claude.ai/projects\n"
                        "; ChatGPT temporary chat = https://chatgpt.com/?temporary-chat=true\n")
        common.open_path(self.cfg.path)

    def build_menu(self):
        self.load()
        d = self.find(self.default)
        favs = [s for s in self.services if s["fav"]]
        launch = [Item(s["name"], action=(lambda s=s: self.launch(s)), icon_png=self.icons[s["id"]]) for s in favs]
        every = [Item("%s — %s" % (s["name"], s["provider"]), action=(lambda s=s: self.launch(s)),
                      icon_png=self.icons[s["id"]]) for s in self.services]
        defaults = [Item(s["name"], action=(lambda i=s["id"]: self.set_default(i)), toggle="radio",
                         checked=s["id"] == self.default) for s in self.services]
        fav_menu = [Item(s["name"], action=(lambda i=s["id"]: self.toggle_fav(i)), toggle="checkmark",
                         checked=s["fav"]) for s in self.services]
        left = [Item("Shows the launcher", toggle="radio", checked=not self.left_default,
                     action=lambda: self.save("left_click_opens_default", 0)),
                Item("Opens %s directly" % (d["name"] if d else "the default"), toggle="radio", checked=self.left_default,
                     action=lambda: self.save("left_click_opens_default", 1))]
        auto = common.autostart_get(APP)
        return ([Item("Open %s" % (d["name"] if d else ""), action=self.open_default,
                      icon_png=self.icons.get(self.default))]
                + [Separator()] + launch
                + [Item("All assistants", children=every), Item("Open AI Hub page", action=self.open_hub_page),
                   Separator(),
                   Item("Settings", enabled=False),
                   Item("Default assistant", children=defaults),
                   Item("Favourites", children=fav_menu),
                   Item("Left click", children=left),
                   Item("Open in its own window (no tabs)", toggle="checkmark", checked=self.own_window,
                        action=lambda: self.save("app_window", 0 if self.own_window else 1)),
                   Item("Keyboard shortcut Ctrl+Alt+A…", action=self.shortcut),
                   Item("Add your own assistants…", action=self.edit_custom),
                   Separator(),
                   Item("Start at login", toggle="checkmark", checked=auto,
                        action=lambda: common.autostart_set(APP, NAME, "Quick launcher for AI chatbots", not auto)),
                   Item("Quit", action=Gtk.main_quit)])


class Launcher(Flyout):
    """The left-click flyout: a search box over the assistants."""

    def __init__(self, app):
        super().__init__(300, 440)
        self.app = app
        self.set_app_paintable(False)       # unlike the spine, this one is plain widgets: let GTK paint
        self.set_name("aihub")
        prov = Gtk.CssProvider()
        prov.load_from_data(CSS)
        Gtk.StyleContext.add_provider_for_screen(Gdk.Screen.get_default(), prov,
                                                 Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self.add(box)

        head = Gtk.Box(spacing=9, margin_start=12, margin_end=12, margin_top=12, margin_bottom=8)
        logo = Gtk.DrawingArea()
        logo.set_size_request(24, 24)
        logo.connect("draw", lambda w, cr: (cr.set_source_surface(hub_icon(24), 0, 0), cr.paint()))
        brand = Gtk.Label(label="AI Hub", xalign=0)
        brand.get_style_context().add_class("brand")
        self.hint = Gtk.Label(xalign=1, hexpand=True)
        self.hint.get_style_context().add_class("dim")
        head.pack_start(logo, False, False, 0)
        head.pack_start(brand, False, False, 0)
        head.pack_start(self.hint, True, True, 0)
        box.pack_start(head, False, False, 0)

        self.entry = Gtk.SearchEntry(placeholder_text="Search assistants…", margin_start=12, margin_end=12,
                                     margin_bottom=6)
        self.entry.connect("search-changed", lambda *a: self.refilter())
        self.entry.connect("activate", lambda *a: self.launch_selected())
        self.entry.connect("key-press-event", self.entry_key)
        box.pack_start(self.entry, False, False, 0)

        self.list = Gtk.ListBox()
        self.list.set_selection_mode(Gtk.SelectionMode.BROWSE)
        self.list.set_filter_func(self.filter)
        self.list.set_header_func(self.header)
        self.list.connect("row-activated", lambda lb, row: self.launch_row(row))
        sw = Gtk.ScrolledWindow(hscrollbar_policy=Gtk.PolicyType.NEVER, vexpand=True)
        sw.add(self.list)
        box.pack_start(sw, True, True, 0)

        foot = Gtk.Box(spacing=4, margin=6)
        foot.get_style_context().add_class("footer")
        page = Gtk.Button(label="Open AI Hub page")
        page.connect("clicked", lambda *a: (self.close_flyout(), self.app.open_hub_page()))
        foot.pack_start(page, False, False, 0)
        tip = Gtk.Label(label="right-click icon: settings", xalign=1, hexpand=True)
        tip.get_style_context().add_class("dim")
        foot.pack_end(tip, False, False, 6)
        box.pack_start(foot, False, False, 0)

    def populate(self):
        for r in self.list.get_children():
            self.list.remove(r)
        ordered = [s for s in self.app.services if s["fav"]] + [s for s in self.app.services if not s["fav"]]
        for s in ordered:
            row = Gtk.ListBoxRow()
            row.service = s
            h = Gtk.Box(spacing=10)
            da = Gtk.DrawingArea()
            da.set_size_request(22, 22)
            da.connect("draw", lambda w, cr, s=s: draw_tile(cr, 22, s["color"], s["name"][:1].upper()))
            name = Gtk.Label(label=s["name"], xalign=0)
            prov = Gtk.Label(label=s["provider"] + ("  ★" if s["id"] == self.app.default else ""), xalign=1, hexpand=True)
            prov.get_style_context().add_class("dim")
            h.pack_start(da, False, False, 0)
            h.pack_start(name, False, False, 0)
            h.pack_start(prov, True, True, 0)
            row.add(h)
            self.list.add(row)
        d = self.app.find(self.app.default)
        self.hint.set_text("★ %s" % d["name"] if d else "")
        self.entry.set_text("")
        self.list.show_all()
        self.refilter()

    def filter(self, row):
        q = self.entry.get_text().strip().lower()
        s = row.service
        return not q or q in s["name"].lower() or q in s["provider"].lower() or q in s["id"]

    def header(self, row, before):
        if self.entry.get_text().strip():
            row.set_header(None)
            return
        fav = row.service["fav"]
        if before is None or before.service["fav"] != fav:
            lb = Gtk.Label(label="FAVOURITES" if fav else "ALL ASSISTANTS", xalign=0)
            lb.get_style_context().add_class("section")
            row.set_header(lb)
        else:
            row.set_header(None)

    def visible_rows(self):
        return [r for r in self.list.get_children() if self.filter(r)]

    def refilter(self):
        self.list.invalidate_filter()
        self.list.invalidate_headers()
        rows = self.visible_rows()
        if rows:
            self.list.select_row(rows[0])

    def entry_key(self, w, ev):
        rows = self.visible_rows()
        sel = self.list.get_selected_row()
        if ev.keyval in (Gdk.KEY_Down, Gdk.KEY_Up) and rows:
            i = rows.index(sel) if sel in rows else -1
            i = min(len(rows) - 1, i + 1) if ev.keyval == Gdk.KEY_Down else max(0, i - 1)
            self.list.select_row(rows[i])
            rows[i].grab_focus()
            self.entry.grab_focus_without_selecting()
            return True
        return False

    def launch_selected(self):
        row = self.list.get_selected_row()
        if row is None or not self.filter(row):
            rows = self.visible_rows()
            row = rows[0] if rows else None
        if row:
            self.launch_row(row)

    def launch_row(self, row):
        self.close_flyout()
        self.app.launch(row.service)


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if "--version" in argv:
        print(NAME, VERSION)
        return 0
    cmd = "open-default" if CMD_OPEN_DEFAULT in argv else "launcher"
    holder = {}
    if not common.single_instance("AIHubTray", lambda c: holder["app"].on_command(c), cmd):
        return 0
    Gtk.Window.set_default_icon_name(APP)
    app = AIHubTray()
    holder["app"] = app
    if CMD_OPEN_DEFAULT in argv:            # first launch was the shortcut itself: open, then stay in the tray
        GLib.idle_add(lambda: (app.open_default(), False)[1])
    elif "--launcher" in argv:
        GLib.timeout_add(300, lambda: (app.on_activate(0, 0), False)[1])
    Gtk.main()
    return 0


if __name__ == "__main__":
    sys.exit(main())
