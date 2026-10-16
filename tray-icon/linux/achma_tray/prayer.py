"""prayer.py — Prayer Spine for Linux: the day spine in the system tray.

Same behaviour as prayer-tray.exe:
  left click   → flyout: city · weather · day spine · next prayer
  right click  → menu: city, settings, weather refresh, Start Dock page, autostart
  click the city name in the flyout (or press S) → settings; R refreshes weather
  notifications N minutes before each adhan and at the adhan itself

    prayer-tray [--show | --settings]
"""
import datetime
import json
import math
import sys
import threading
import time
import urllib.request

from . import common  # sets GDK_BACKEND before Gtk loads
from .common import Config, Flyout, Notifier
from . import prayer_core as pc
from .sni import Item, Separator, StatusNotifierItem, argb_pixmap

import cairo  # noqa: E402
from gi.repository import GLib, Gtk, Pango, PangoCairo  # noqa: E402

APP = "achma-prayer-tray"
NAME = "Prayer Spine"
VERSION = "1.0.0"

# palette — start-dock.html :root
def _hex(s): return tuple(int(s[i:i + 2], 16) / 255 for i in (1, 3, 5))
INK2, INK3 = _hex("#171C25"), _hex("#1D2430")
LINE, LINE2 = _hex("#28303D"), _hex("#333D4D")
TADEL, DIM, DIMMER = _hex("#E9E5DB"), _hex("#818C9E"), _hex("#5A6473")
SAFF, VERD, TERRA = _hex("#E8A33D"), _hex("#4FB09A"), _hex("#C96A4E")

MONO = "JetBrains Mono, IBM Plex Mono, Cascadia Mono, Fira Code, DejaVu Sans Mono, Liberation Mono, monospace"
ARAB = "Noto Naskh Arabic, Amiri, Scheherazade New, Noto Sans Arabic, DejaVu Sans, sans"


def mix(a, b, t):
    return tuple(a[i] * t + b[i] * (1 - t) for i in range(3))


# ── drawing helpers ─────────────────────────────────────────────────────

_fonts = {}


def font(family, px, bold=False):
    k = (family, px, bold)
    if k not in _fonts:
        d = Pango.FontDescription()
        d.set_family(family)
        d.set_absolute_size(px * Pango.SCALE)
        d.set_weight(Pango.Weight.SEMIBOLD if bold else Pango.Weight.NORMAL)
        _fonts[k] = d
    return _fonts[k]


def layout(cr, s, fd, spacing=0.0):
    lo = PangoCairo.create_layout(cr)
    lo.set_font_description(fd)
    lo.set_text(s, -1)
    if spacing:
        al = Pango.AttrList()
        al.insert(Pango.attr_letter_spacing_new(int(spacing * Pango.SCALE)))
        lo.set_attributes(al)
    return lo


def text(cr, s, fd, color, x, yc, align="left", spacing=0.0):
    """Draw s with its ink centred on yc; returns its width."""
    lo = layout(cr, s, fd, spacing)
    w, h = lo.get_pixel_size()
    if align == "right":
        x -= w
    cr.set_source_rgb(*color)
    cr.move_to(x, yc - h / 2)
    PangoCairo.show_layout(cr, lo)
    return w


def text_w(cr, s, fd):
    return layout(cr, s, fd).get_pixel_size()[0]


def rect(cr, x, y, w, h, color, alpha=1.0):
    cr.set_source_rgba(*color, alpha)
    cr.rectangle(x, y, w, h)
    cr.fill()


def circle(cr, cx, cy, r, color, alpha=1.0):
    cr.set_source_rgba(*color, alpha)
    cr.arc(cx, cy, r, 0, 2 * math.pi)
    cr.fill()


def cloud(cr, cx, cy, u, color):
    cr.set_source_rgb(*color)
    for ex, ey, ew, eh in ((-0.42, -0.05, 0.46, 0.40), (-0.20, -0.30, 0.52, 0.56), (0.08, -0.08, 0.40, 0.42)):
        cr.save()
        cr.translate(cx + (ex + ew / 2) * u, cy + (ey + eh / 2) * u)
        cr.scale(ew * u / 2, eh * u / 2)
        cr.arc(0, 0, 1, 0, 2 * math.pi)
        cr.restore()
        cr.fill()
    cr.rectangle(cx - 0.22 * u, cy + 0.06 * u, 0.52 * u, 0.29 * u)
    cr.fill()


def sun(cr, cx, cy, u):
    cr.set_source_rgb(*SAFF)
    cr.set_line_width(max(1, u * 0.08))
    cr.set_line_cap(cairo.LINE_CAP_ROUND)
    for i in range(8):
        a = i * math.pi / 4
        cr.move_to(cx + math.cos(a) * u * 0.30, cy + math.sin(a) * u * 0.30)
        cr.line_to(cx + math.cos(a) * u * 0.44, cy + math.sin(a) * u * 0.44)
    cr.stroke()
    circle(cr, cx, cy, 0.21 * u, SAFF)


def moon(cr, cx, cy, u, bg):
    circle(cr, cx, cy, 0.30 * u, (0.94, 0.86, 0.60))
    circle(cr, cx + 0.14 * u, cy - 0.12 * u, 0.28 * u, bg)


def weather_glyph(cr, cx, cy, u, code, day, bg):
    """Vector weather icon — no emoji font needed, same on every desktop."""
    k = pc.wx_kind(code)
    light, dark = (0.85, 0.86, 0.89), (0.60, 0.64, 0.70)
    if k == "clear":
        sun(cr, cx, cy, u) if day else moon(cr, cx, cy, u, bg)
    elif k in ("mostly", "partly"):
        if day:
            sun(cr, cx - 0.16 * u, cy - 0.14 * u, u * (0.95 if k == "mostly" else 0.8))
        else:
            moon(cr, cx - 0.14 * u, cy - 0.14 * u, u * 0.85, bg)
        cloud(cr, cx + 0.10 * u, cy + 0.12 * u, u * (0.55 if k == "mostly" else 0.75), light)
    elif k == "cloud":
        cloud(cr, cx, cy, u, light)
    elif k == "fog":
        cr.set_source_rgb(*light)
        cr.set_line_width(max(1, u * 0.09))
        cr.set_line_cap(cairo.LINE_CAP_ROUND)
        for i in range(3):
            y = cy - 0.22 * u + i * 0.22 * u
            o = 0.08 * u if i % 2 else 0
            cr.move_to(cx - 0.38 * u + o, y)
            cr.line_to(cx + 0.38 * u - (0.08 * u - o), y)
        cr.stroke()
    elif k in ("drizzle", "rain", "storm"):
        cloud(cr, cx, cy - 0.12 * u, u * 0.9, dark if k == "storm" else light)
        if k == "storm":
            cr.set_source_rgb(*SAFF)
            pts = [(0.02, 0.05), (-0.14, 0.30), (-0.01, 0.30), (-0.08, 0.48), (0.16, 0.20), (0.03, 0.20), (0.12, 0.05)]
            cr.move_to(cx + pts[0][0] * u, cy + pts[0][1] * u)
            for px, py in pts[1:]:
                cr.line_to(cx + px * u, cy + py * u)
            cr.close_path()
            cr.fill()
        else:
            cr.set_source_rgb(0.44, 0.66, 0.94)
            cr.set_line_width(max(1, u * (0.08 if k == "rain" else 0.06)))
            cr.set_line_cap(cairo.LINE_CAP_ROUND)
            for i in (-1, 0, 1):
                x = cx + i * 0.2 * u
                cr.move_to(x, cy + 0.16 * u)
                cr.line_to(x - 0.06 * u, cy + (0.40 if k == "rain" else 0.30) * u)
            cr.stroke()
    elif k == "snow":
        cloud(cr, cx, cy - 0.12 * u, u * 0.9, light)
        for i in (-1, 0, 1):
            circle(cr, cx + i * 0.2 * u, cy + 0.3 * u + (0.08 * u if i == 0 else 0), max(1, 0.06 * u), (1, 1, 1))
    else:
        circle(cr, cx, cy, 0.06 * u, DIM)


def round_rect(cr, x, y, w, h, r):
    cr.new_sub_path()
    cr.arc(x + w - r, y + r, r, -math.pi / 2, 0)
    cr.arc(x + w - r, y + h - r, r, 0, math.pi / 2)
    cr.arc(x + r, y + h - r, r, math.pi / 2, math.pi)
    cr.arc(x + r, y + r, r, math.pi, 3 * math.pi / 2)
    cr.close_path()


# ── the app ─────────────────────────────────────────────────────────────

class PrayerTray:
    W, H = 204, 600

    def __init__(self):
        self.cfg = Config("prayer-tray.ini")
        self.load_settings()
        self.weather = None
        self.net = "idle"
        self.fetching = False
        self.fetch_gen = 0
        self.last_attempt = 0.0
        self.last_ok = True
        self.header_hit = (0, 0, 0, 0)
        self.settings_win = None
        self.last_minute = None
        self.load_weather_cache()
        self.compute()

        self.fly = Flyout(self.W, self.H)
        self.area = Gtk.DrawingArea()
        self.area.connect("draw", self.on_draw)
        self.area.add_events(0)
        self.fly.add(self.area)
        self.fly.connect("button-release-event", self.on_fly_click)
        self.fly.connect("key-press-event", self.on_fly_key)
        self.pulse_id = None

        self.notifier = Notifier(NAME, APP, on_click=self.show_flyout)
        self.sni = StatusNotifierItem(APP, NAME, self.build_menu, on_activate=self.on_activate,
                                      on_secondary=lambda: self.show_settings())
        self.sni.on_host_missing = lambda: self.notifier.send("No system tray found", common.tray_help(), sound=False)

        self.refresh_tray(force=True)
        self.net = "ok" if (self.wx and self.weather) else ("wait" if self.wx else "idle")
        self.fetch_weather(force=False)
        GLib.timeout_add_seconds(10, self.tick)
        self.check_alerts()

    # settings
    def load_settings(self):
        c = self.cfg
        self.custom = c.get_int("location", "custom", 0) == 1
        self.city_id = c.get_int("location", "city_id", pc.DEFAULT_CITY_ID)
        city = pc.city_by_id(self.city_id) or pc.city_by_id(pc.DEFAULT_CITY_ID)
        self.city_id = city[0]
        if self.custom:
            self.name = c.get("location", "name", "My location") or "My location"
            self.lat = c.get_float("location", "lat", city[3])
            self.lng = c.get_float("location", "lng", city[4])
            if not (-66 <= self.lat <= 66 and -180 <= self.lng <= 180):
                self.custom = False
        if not self.custom:
            self.name, self.lat, self.lng = city[2], city[3], city[4]
        self.pre = c.get_int("alerts", "remind_before", 10)
        if not 0 <= self.pre <= 60:
            self.pre = 10
        self.notify_adhan = c.get_int("alerts", "at_adhan", 1) == 1
        self.sound = c.get_int("alerts", "sound", 1) == 1
        self.wx = c.get_int("weather", "enabled", 1) == 1

    def save_settings(self):
        c = self.cfg
        c.set("location", "custom", int(self.custom))
        c.set("location", "city_id", self.city_id)
        if self.custom:
            c.set("location", "name", self.name)
            c.set("location", "lat", "%.5f" % self.lat)
            c.set("location", "lng", "%.5f" % self.lng)
        c.set("alerts", "remind_before", self.pre)
        c.set("alerts", "at_adhan", int(self.notify_adhan))
        c.set("alerts", "sound", int(self.sound))
        c.set("weather", "enabled", int(self.wx))
        c.save()

    def loc_key(self):
        return "%.2f,%.2f" % (self.lat, self.lng)

    # prayer
    def compute(self):
        d = datetime.date.today()
        self.day = d
        self.today = pc.times_for(d, self.lat, self.lng)
        self.tomorrow = pc.times_for(d + datetime.timedelta(days=1), self.lat, self.lng)

    @staticmethod
    def now_min():
        n = datetime.datetime.now()
        return n.hour * 60 + n.minute + n.second / 60

    @staticmethod
    def now_min_int():
        n = datetime.datetime.now()
        return n.hour * 60 + n.minute

    def next(self):
        return pc.next_prayer(self.today, self.tomorrow, self.now_min_int())

    def adhan_near(self):
        if self.pre <= 0:
            return False
        nm = self.now_min_int()
        return any(t is not None and i != pc.SUNRISE and 0 <= t - nm <= self.pre for i, t in enumerate(self.today))

    # weather
    def fetch_weather(self, force):
        if not self.wx:
            self.net = "idle"
            return
        fresh = self.weather and time.time() - self.weather.get("at", 0) < 30 * 60
        if (not force and fresh) or self.fetching:
            if fresh:
                self.net = "ok"
            return
        self.fetching = True
        self.last_attempt = time.time()
        self.net = "wait"
        gen, lat, lng = self.fetch_gen, self.lat, self.lng

        def work():
            res = None
            url = ("https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
                   "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,"
                   "wind_speed_10m,is_day&daily=temperature_2m_max,temperature_2m_min"
                   "&timezone=auto&forecast_days=1" % (lat, lng))
            try:
                req = urllib.request.Request(url, headers={"User-Agent": "AchmaTray/" + VERSION})
                with urllib.request.urlopen(req, timeout=10) as r:
                    d = json.loads(r.read(1 << 20).decode("utf-8"))
                cur = d["current"]
                daily = d.get("daily") or {}
                res = {"temp": float(cur["temperature_2m"]), "code": int(cur["weather_code"]),
                       "feels": float(cur.get("apparent_temperature", cur["temperature_2m"])),
                       "rh": cur.get("relative_humidity_2m"), "wind": cur.get("wind_speed_10m"),
                       "is_day": int(cur.get("is_day", 1)),
                       "tmax": (daily.get("temperature_2m_max") or [None])[0],
                       "tmin": (daily.get("temperature_2m_min") or [None])[0],
                       "at": time.time()}
            except Exception:
                res = None              # offline, DNS, captive portal, odd payload: keep the last reading
            GLib.idle_add(self.weather_done, gen, res)

        threading.Thread(target=work, daemon=True).start()

    def weather_done(self, gen, res):
        self.fetching = False
        if gen != self.fetch_gen:       # city changed while this was in flight
            self.fetch_weather(force=True)
            return False
        if res:
            self.weather = res
            self.last_ok = True
            self.net = "ok"
            c = self.cfg
            c.set("weather", "key", self.loc_key())
            for k in ("temp", "code", "feels", "rh", "wind", "is_day", "tmax", "tmin", "at"):
                c.set("weather", k, "" if res[k] is None else res[k])
            c.save()
        else:
            self.last_ok = False
            self.net = "off"
        self.refresh_tray(force=True)
        self.area.queue_draw()
        return False

    def load_weather_cache(self):
        self.weather = None
        c = self.cfg
        if c.get("weather", "key") != self.loc_key():
            return
        try:
            w = {k: float(c.get("weather", k)) for k in ("temp", "feels", "at")}
            w["code"] = int(float(c.get("weather", "code")))
            w["is_day"] = int(float(c.get("weather", "is_day", "1")))
            self.weather = w
        except ValueError:
            pass

    # tray icon: a spine in a few pixels
    def icon_surface(self, z):
        s = cairo.ImageSurface(cairo.FORMAT_ARGB32, z, z)
        cr = cairo.Context(s)
        u = z / 16
        near = self.adhan_near()
        round_rect(cr, 0.5 * u, 0.5 * u, z - u, z - u, 3.2 * u)
        cr.set_source_rgb(*INK3)
        cr.fill_preserve()
        cr.set_source_rgb(*(TERRA if near else LINE2))
        cr.set_line_width(max(1, u * (1.3 if near else 1.0)))
        cr.stroke()
        top, bot, rx, rw = 2.6 * u, z - 2.6 * u, 5.2 * u, max(1, 1.6 * u)
        rect(cr, rx, top, rw, bot - top, LINE2)
        ny = top + pc.pct_of(self.now_min(), self.today) * (bot - top)
        if ny > top + 0.5:
            g = cairo.LinearGradient(0, top, 0, ny)
            g.add_color_stop_rgb(0, *VERD)
            g.add_color_stop_rgb(1, *SAFF)
            cr.set_source(g)
            cr.rectangle(rx, top, rw, ny - top)
            cr.fill()
        idx, _m, tom = self.next()
        nm = self.now_min()
        for i, t in enumerate(self.today):
            if t is None:
                continue
            ty = top + pc.pct_of(t, self.today) * (bot - top)
            nxt = i == idx and not tom
            ln, th = (7 * u, 2 * u) if nxt else (4.5 * u, u)
            rect(cr, rx + rw + u, round(ty - th / 2), ln, max(1, round(th)), TERRA if nxt else DIM,
                 1 if nxt else (0.45 if t <= nm else 0.85))
        circle(cr, rx + rw / 2, ny, 2.2 * u, SAFF)
        return s

    def refresh_tray(self, force=False):
        m = self.now_min_int()
        if not force and m == self.last_minute:
            return
        self.last_minute = m
        self.sni.set_icon(pixmaps=[argb_pixmap(self.icon_surface(z)) for z in (16, 22, 24, 32, 48, 64)])
        idx, mins, tom = self.next()
        wd = datetime.date.today().weekday()
        when = pc.hm(self.tomorrow[pc.FAJR] if tom else self.today[idx])
        title = self.name
        if self.wx and self.weather:
            title += " · %d° %s" % (round(self.weather["temp"]), pc.WX_TEXT.get(self.weather["code"], ""))
        self.sni.set_tooltip(title, "%s %s · in %s" % (pc.p_en(idx, (wd + 1) % 7 if tom else wd), when, pc.fmt_left(mins)))

    # flyout
    def on_activate(self, x, y):
        self.fly.toggle(x, y)
        self._pulse_sync()

    def show_flyout(self):
        if not self.fly.get_visible():
            self.fly.open_at(0, 0)
        self._pulse_sync()

    def _pulse_sync(self):
        if self.fly.get_visible() and self.pulse_id is None:
            self.pulse_id = GLib.timeout_add(40, self._pulse)

    def _pulse(self):
        if not self.fly.get_visible():
            self.pulse_id = None
            return False
        self.area.queue_draw()
        return True

    def on_fly_click(self, w, ev):
        x0, y0, x1, y1 = self.header_hit
        if ev.button == 1 and x0 <= ev.x < x1 and y0 <= ev.y < y1:
            self.fly.close_flyout()
            self.show_settings()
        return False

    def on_fly_key(self, w, ev):
        k = Gdk_keyname(ev.keyval)
        if k in ("r", "R"):
            self.fetch_weather(force=True)
            self.area.queue_draw()
        elif k in ("s", "S", "c", "C"):
            self.fly.close_flyout()
            self.show_settings()
        return False

    def on_draw(self, widget, cr):
        a = widget.get_allocation()
        self.paint(cr, a.width, a.height)
        return True

    def paint(self, cr, W, H):
        rect(cr, 0, 0, W, H, INK2)
        barH = 32
        rect(cr, 0, 0, W, barH, INK3)
        rect(cr, 0, barH, W, 1, LINE)
        self.header_hit = (0, 0, W, barH)
        wd = datetime.date.today().weekday()
        x, cy = 12, barH / 2
        netc = {"ok": VERD, "off": TERRA, "wait": SAFF}.get(self.net, LINE2)
        circle(cr, x + 3, cy, 3, netc)
        x += 6 + 9
        x += text(cr, self.name, font(MONO, 11.5, True), TADEL, x, cy) + 9
        if self.wx:
            t = "%d°" % round(self.weather["temp"]) if self.weather else "--°"
            x += text(cr, t, font(MONO, 11.5), DIM, x, cy) + 9
            if self.weather:
                weather_glyph(cr, x + 8, cy, 16, self.weather["code"], self.weather.get("is_day", 1), INK3)

        y = barH + 1 + 9
        text(cr, "DAY SPINE", font(MONO, 9, True), DIMMER, 11, y + 6, spacing=1.6)
        y += 12 + 7

        foot_top = H - 8 - 23 - 2 - 15 - 4 - 12 - 9 - 1
        rail_top, rail_bot = y + 6, foot_top - 7 - 6
        railH = rail_bot - rail_top
        rx, track_x, rail_right = 11, 11 + 47, W - 11
        rect(cr, track_x, rail_top, 2, railH, LINE2)

        nm = self.now_min()
        now_y = rail_top + pc.pct_of(nm, self.today) * railH
        if now_y > rail_top + 1:
            g = cairo.LinearGradient(0, rail_top, 0, now_y)
            g.add_color_stop_rgba(0, *VERD, 0.5)
            g.add_color_stop_rgba(1, *SAFF, 0.5)
            cr.set_source(g)
            cr.rectangle(track_x, rail_top, 2, now_y - rail_top)
            cr.fill()
        rect(cr, rx + 51, round(now_y), (rail_right + 5) - (rx + 51), 1, SAFF, 0.3)

        idx, mins, tom = self.next()
        items = [{"i": i, "t": t, "pct": pc.pct_of(t, self.today)} for i, t in enumerate(self.today) if t is not None]
        pc.relax_labels(items, 13 / railH if railH > 0 else 0.024)
        tick_pass = mix(LINE2, INK2, 0.45)
        for o in items:
            past, nxt = o["t"] <= nm, o["i"] == idx
            ty = round(rail_top + o["pct"] * railH)
            ly = round(rail_top + o["lbl"] * railH)
            if nxt:
                rect(cr, rx + 43, ty - 1, 10, 2, TERRA)
            else:
                rect(cr, rx + 43, ty, 10, 1, tick_pass if past else LINE2)
            text(cr, pc.hm(o["t"]), font(MONO, 9.5), TERRA if nxt else DIMMER, rx + 43, ly, align="right")
            if nxt:
                rect(cr, rx + 43, ly - 1, 10, 2, TERRA)
            else:
                rect(cr, rx + 43, ly, 10, 1, LINE2)
            text(cr, pc.p_en(o["i"], wd), font(MONO, 10, nxt), TERRA if nxt else (DIMMER if past else DIM), rx + 59, ly)

        # the now dot, with the deck's 2.6 s pulse
        cx = track_x + 1
        ph = (time.monotonic() % 2.6) / 2.6
        if ph < 0.7:
            spread = ph / 0.7 * 7
            circle(cr, cx, now_y, 3.5 + spread, SAFF, 0.55 * (1 - ph / 0.7))
        circle(cr, cx, now_y, 3.5, SAFF)

        rect(cr, 0, foot_top, W, 1, LINE)
        y = foot_top + 1 + 9
        text(cr, "NEXT PRAYER", font(MONO, 9, True), DIMMER, 11, y + 6, spacing=1.6)
        y += 12 + 4
        nwd = (wd + 1) % 7 if tom else wd
        lead = "%s · " % pc.p_en(idx, nwd)
        w = text(cr, lead, font(MONO, 11), DIM, 11, y + 7.5)
        text(cr, pc.p_ar(idx, nwd), font(ARAB, 12), TERRA, 11 + w, y + 7.5)
        y += 15 + 2
        text(cr, pc.fmt_left(mins), font(MONO, 17), TERRA, 11, y + 11.5)

        cr.set_source_rgb(*LINE2)
        cr.set_line_width(1)
        cr.rectangle(0.5, 0.5, W - 1, H - 1)
        cr.stroke()

    # alerts (start-dock.html checkAdhan): 3-minute window, once per prayer per day, Shuruq excluded
    def check_alerts(self):
        if not self.notify_adhan and self.pre <= 0:
            return
        now = datetime.datetime.now()
        nm = now.hour * 60 + now.minute
        day = now.strftime("%Y-%m-%d")
        seen = self.cfg.get("alerts_seen", "ids") if self.cfg.get("alerts_seen", "day") == day else ""
        WIN = 3
        for i, t in enumerate(self.today):
            if i == pc.SUNRISE or t is None:
                continue
            kind = None
            if self.notify_adhan and t <= nm < t + WIN:
                kind = "adhan"
            elif self.pre > 0 and t - self.pre <= nm < min(t, t - self.pre + WIN):
                kind = "pre"
            if not kind:
                continue
            tag = "|%s:%s|" % (pc.PRAYERS[i][0], kind)
            if tag in seen:
                continue
            wd = now.weekday()
            if kind == "adhan":
                self.notifier.send("حان وقت صلاة " + pc.p_ar(i, wd),
                                   "%s · %s · %s" % (pc.p_en(i, wd), pc.hm(t), self.name), self.sound, urgent=True)
            else:
                self.notifier.send("%s in %d min" % (pc.p_en(i, wd), t - nm),
                                   "اقترب وقت صلاة %s · %s · %s" % (pc.p_ar(i, wd), pc.hm(t), self.name), self.sound)
            self.cfg.set("alerts_seen", "day", day)
            self.cfg.set("alerts_seen", "ids", seen + tag)
            self.cfg.save()
            break

    def tick(self):
        if datetime.date.today() != self.day:
            self.compute()
            self.refresh_tray(force=True)
        self.refresh_tray()
        self.check_alerts()
        # also catches resume from suspend: the reading is simply old by then
        if self.wx and not self.fetching:
            wait = 30 * 60 if self.last_ok else 5 * 60
            if time.time() - self.last_attempt > wait:
                self.fetch_weather(force=False)
        if self.fly.get_visible() and self.pulse_id is None:
            self.area.queue_draw()
        return True

    # city
    def select_city(self, cid):
        c = pc.city_by_id(cid)
        if not c:
            return
        self.custom, self.city_id, self.name, self.lat, self.lng = False, c[0], c[2], c[3], c[4]
        self.location_changed()

    def location_changed(self):
        self.save_settings()
        self.compute()
        self.fetch_gen += 1
        self.load_weather_cache()
        self.fetch_weather(force=True)
        self.refresh_tray(force=True)
        self.sni.refresh_menu()
        self.area.queue_draw()

    # menu
    def build_menu(self):
        cities = [Item(c[2], action=(lambda cid=c[0]: self.select_city(cid)), toggle="radio",
                       checked=(not self.custom and c[0] == self.city_id)) for c in pc.cities_sorted()]
        auto = common.autostart_get(APP)
        return [
            Item("Show day spine", action=self.show_flyout),
            Item("City", children=cities),
            Item("Settings…", action=self.show_settings),
            Item("Refresh weather", action=lambda: self.fetch_weather(force=True), enabled=self.wx),
            Item("Open Start Dock page", action=self.open_dock),
            Separator(),
            Item("Start at login", toggle="checkmark", checked=auto,
                 action=lambda: common.autostart_set(APP, NAME, "Prayer times and the day spine", not auto)),
            Item("Quit", action=Gtk.main_quit),
        ]

    def open_dock(self):
        p = common.install_page(common.page_candidates("start-dock.html"), "start-dock.html")
        if p:
            common.open_path(p)

    # settings window
    def show_settings(self):
        if self.settings_win:
            self.settings_win.present()
            return
        SettingsWindow(self)

    def on_command(self, cmd):
        if cmd == "settings":
            self.show_settings()
        else:
            self.show_flyout()


def Gdk_keyname(v):
    from gi.repository import Gdk
    return Gdk.keyval_name(v) or ""


class SettingsWindow(Gtk.Window):
    def __init__(self, app):
        super().__init__(title=NAME + " — Settings")
        self.app = app
        app.settings_win = self
        self.set_border_width(16)
        self.set_resizable(False)
        self.set_position(Gtk.WindowPosition.CENTER)
        self.set_icon_name(APP)
        self.connect("destroy", self._gone)

        g = Gtk.Grid(column_spacing=12, row_spacing=10)
        self.add(g)

        def label(t, row, col=0):
            lb = Gtk.Label(label=t, xalign=0)
            g.attach(lb, col, row, 1, 1)
            return lb

        self.sorted = pc.cities_sorted()
        label("City", 0)
        self.combo = Gtk.ComboBoxText()
        for c in self.sorted:
            self.combo.append_text("%s  —  %s" % (c[2], c[1]))
        self.combo.append_text("Custom location…")
        if app.custom:
            self.combo.set_active(len(self.sorted))
        else:
            self.combo.set_active(next((i for i, c in enumerate(self.sorted) if c[0] == app.city_id), 0))
        self.combo.connect("changed", lambda *a: self.sync())
        g.attach(self.combo, 1, 0, 3, 1)

        label("Name", 1)
        self.e_name = Gtk.Entry(text=app.name, hexpand=True)
        g.attach(self.e_name, 1, 1, 3, 1)
        label("Latitude", 2)
        self.e_lat = Gtk.Entry(text="%.4f" % app.lat, width_chars=10)
        g.attach(self.e_lat, 1, 2, 1, 1)
        label("Longitude", 2, 2)
        self.e_lng = Gtk.Entry(text="%.4f" % app.lng, width_chars=10)
        g.attach(self.e_lng, 3, 2, 1, 1)
        note = Gtk.Label(xalign=0, wrap=True, max_width_chars=52)
        note.set_markup("<small>Times use the Moroccan Ministry method (Fajr 19°, Isha 17°, +5 min on "
                        "Dhuhr and Maghrib) and this computer's clock and time zone.</small>")
        note.get_style_context().add_class("dim-label")
        g.attach(note, 0, 3, 4, 1)

        label("Remind before adhan", 4)
        self.spin = Gtk.SpinButton.new_with_range(0, 60, 1)
        self.spin.set_value(app.pre)
        g.attach(self.spin, 1, 4, 1, 1)
        label("minutes  (0 = off)", 4, 2).set_hexpand(True)

        self.c_adhan = Gtk.CheckButton(label="Notify at the adhan itself", active=app.notify_adhan)
        self.c_sound = Gtk.CheckButton(label="Play the notification sound", active=app.sound)
        self.c_wx = Gtk.CheckButton(label="Live weather (Open-Meteo, one request every 30 min)", active=app.wx)
        self.c_auto = Gtk.CheckButton(label="Start at login", active=common.autostart_get(APP))
        for r, w in enumerate((self.c_adhan, self.c_sound, self.c_wx, self.c_auto), start=5):
            g.attach(w, 0, r, 4, 1)

        box = Gtk.Box(spacing=8, halign=Gtk.Align.END, margin_top=8)
        cancel = Gtk.Button(label="Cancel")
        ok = Gtk.Button(label="OK")
        ok.get_style_context().add_class("suggested-action")
        ok.set_can_default(True)
        cancel.connect("clicked", lambda *a: self.destroy())
        ok.connect("clicked", lambda *a: self.apply())
        box.pack_start(cancel, False, False, 0)
        box.pack_start(ok, False, False, 0)
        g.attach(box, 0, 9, 4, 1)
        self.set_default(ok)
        for e in (self.e_name, self.e_lat, self.e_lng):
            e.set_activates_default(True)
        self.connect("key-press-event", lambda w, ev: ev.keyval == 0xff1b and (self.destroy() or True))
        self.sync()
        self.show_all()
        self.present()

    def _gone(self, *a):
        self.app.settings_win = None

    def sync(self):
        i = self.combo.get_active()
        custom = i == len(self.sorted)
        for e in (self.e_name, self.e_lat, self.e_lng):
            e.set_sensitive(custom)
        if not custom and i >= 0:
            c = self.sorted[i]
            self.e_name.set_text(c[2])
            self.e_lat.set_text("%.2f" % c[3])
            self.e_lng.set_text("%.2f" % c[4])

    def apply(self):
        app = self.app
        i = self.combo.get_active()
        moved_from = (app.lat, app.lng)
        if i == len(self.sorted):
            try:
                lat = float(self.e_lat.get_text().replace(",", "."))
                lng = float(self.e_lng.get_text().replace(",", "."))
                if not (-66 <= lat <= 66 and -180 <= lng <= 180):
                    raise ValueError
            except ValueError:
                d = Gtk.MessageDialog(transient_for=self, modal=True, message_type=Gtk.MessageType.WARNING,
                                      buttons=Gtk.ButtonsType.OK,
                                      text="Latitude must be between -66 and 66 and longitude between -180 and 180.")
                d.format_secondary_text("Beyond ±66° the sun may not reach the Fajr/Isha angles at all.")
                d.run()
                d.destroy()
                return
            app.custom, app.lat, app.lng = True, lat, lng
            app.name = self.e_name.get_text().strip() or "My location"
        elif i >= 0:
            c = self.sorted[i]
            app.custom, app.city_id, app.name, app.lat, app.lng = False, c[0], c[2], c[3], c[4]
        app.pre = int(self.spin.get_value())
        app.notify_adhan = self.c_adhan.get_active()
        app.sound = self.c_sound.get_active()
        wx_on = self.c_wx.get_active() and not app.wx
        app.wx = self.c_wx.get_active()
        common.autostart_set(APP, NAME, "Prayer times and the day spine", self.c_auto.get_active())
        if (app.lat, app.lng) != moved_from:
            app.location_changed()
        else:
            app.save_settings()
            if wx_on:
                app.fetch_weather(force=True)
            if not app.wx:
                app.net = "idle"
            app.refresh_tray(force=True)
            app.sni.refresh_menu()
            app.area.queue_draw()
        self.destroy()


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if "--version" in argv:
        print(NAME, VERSION)
        return 0
    cmd = "settings" if "--settings" in argv else ("show" if "--show" in argv else "show")
    holder = {}
    if not common.single_instance("PrayerTray", lambda c: holder["app"].on_command(c), cmd):
        return 0
    Gtk.Window.set_default_icon_name(APP)
    app = PrayerTray()
    holder["app"] = app
    if "--settings" in argv:
        GLib.idle_add(lambda: (app.show_settings(), False)[1])
    elif "--show" in argv:
        GLib.timeout_add(300, lambda: (app.show_flyout(), False)[1])
    Gtk.main()
    return 0


if __name__ == "__main__":
    sys.exit(main())
