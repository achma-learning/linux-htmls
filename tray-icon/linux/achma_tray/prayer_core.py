"""prayer_core.py — prayer times, cities and the spine's geometry. No GTK.

A line-for-line port of start-dock.html §0 and §3 (and of src/prayer-tray.cpp,
which ports the same code for Windows). Morocco method, as AlAdhan method 21
defines it: Fajr 19°, Isha 17°, Asr shadow factor 1, sunrise/sunset 0.833°,
and the Ministry's precaution of +5 min on Dhuhr and Maghrib.

Astronomy: PrayTimes.org (Hamid Zarrabi-Zadeh, LGPL), one pass, rounded to the
minute. Python floats are the same IEEE doubles as JavaScript numbers, so the
output is identical to the page's, minute for minute — tests/check_times.py
proves it for every city and every day of a year.
"""
import datetime
import math

# (id, Arabic, French, lat, lng) — start-dock.html CITIES, verbatim.
# Settings store the id, never the position, so the list can grow.
CITIES = [
    (1, "الرباط", "Rabat", 34.02, -6.84), (7, "القنيطرة", "Kénitra", 34.26, -6.58),
    (14, "طنجة", "Tanger", 35.76, -5.83), (15, "تطوان", "Tétouan", 35.59, -5.36),
    (18, "شفشاون", "Chefchaouen", 35.17, -5.26), (23, "الحسيمة", "Al Hoceïma", 35.25, -3.93),
    (29, "وزان", "Ouezzane", 34.80, -5.58), (31, "وجدة", "Oujda", 34.68, -1.91),
    (39, "الناظور", "Nador", 35.17, -2.93), (35, "كرسيف", "Guercif", 34.23, -3.35),
    (58, "الدار البيضاء", "Casablanca", 33.57, -7.59), (59, "المحمدية", "Mohammedia", 33.69, -7.38),
    (61, "سطات", "Settat", 33.00, -7.62), (65, "برشيد", "Berrechid", 33.27, -7.59),
    (66, "الجديدة", "El Jadida", 33.23, -8.50), (73, "بني ملال", "Béni Mellal", 32.34, -6.35),
    (79, "خريبكة", "Khouribga", 32.88, -6.91), (81, "فاس", "Fès", 34.02, -5.01),
    (89, "تازة", "Taza", 34.21, -4.01), (99, "مكناس", "Meknès", 33.89, -5.55),
    (100, "يفرن", "Ifrane", 33.53, -5.11), (103, "آزرو", "Azrou", 33.44, -5.22),
    (104, "مراكش", "Marrakech", 31.63, -7.98), (106, "الصويرة", "Essaouira", 31.51, -9.77),
    (108, "بنجرير", "Benguerir", 32.23, -7.95), (111, "آسفي", "Safi", 32.30, -9.24),
    (117, "أكادير", "Agadir", 30.43, -9.60), (118, "تارودانت", "Taroudant", 30.47, -8.88),
    (119, "تزنيت", "Tiznit", 29.70, -9.80), (123, "طاطا", "Tata", 29.75, -7.97),
    (128, "الرشيدية", "Errachidia", 31.93, -4.43), (136, "ميدلت", "Midelt", 32.68, -4.73),
    (137, "زاكورة", "Zagora", 30.33, -5.84), (138, "ورزازات", "Ouarzazate", 30.92, -6.90),
    (139, "تنغير", "Tinghir", 31.51, -5.53), (149, "كلميم", "Guelmim", 28.98, -10.06),
    (152, "طانطان", "Tan-Tan", 28.44, -11.10), (156, "العيون", "Laâyoune", 27.13, -13.16),
    (165, "الداخلة", "Dakhla", 23.68, -15.96), (148, "سيدي إفني", "Sidi Ifni", 29.38, -10.17),
    (201, "سلا", "Salé", 34.05, -6.80), (202, "تمارة", "Témara", 33.93, -6.91),
    (203, "العرائش", "Larache", 35.19, -6.16), (204, "القصر الكبير", "Ksar El Kébir", 35.00, -5.90),
    (205, "الخميسات", "Khémisset", 33.82, -6.07), (206, "بركان", "Berkane", 34.92, -2.32),
    (207, "تاوريرت", "Taourirt", 34.41, -2.90), (208, "خنيفرة", "Khénifra", 32.94, -5.67),
    (209, "صفرو", "Sefrou", 33.83, -4.83), (210, "إنزكان", "Inezgane", 30.36, -9.54),
    (211, "قلعة السراغنة", "El Kelâa des Sraghna", 32.05, -7.41), (212, "اليوسفية", "Youssoufia", 32.25, -8.53),
    (213, "الفقيه بن صالح", "Fquih Ben Salah", 32.50, -6.69), (214, "أزيلال", "Azilal", 31.96, -6.57),
    (215, "فكيك", "Figuig", 32.11, -1.23), (216, "السمارة", "Smara", 26.74, -11.67),
    (217, "بوجدور", "Boujdour", 26.13, -14.48),
]
DEFAULT_CITY_ID = 104   # Marrakech


def city_by_id(cid):
    for c in CITIES:
        if c[0] == cid:
            return c
    return None


def cities_sorted():
    import unicodedata

    def key(c):
        return unicodedata.normalize("NFD", c[2]).encode("ascii", "ignore").decode().lower()
    return sorted(CITIES, key=key)


# key, English, Arabic — Shuruq is shown on the spine but never alerted
PRAYERS = [
    ("Fajr", "Fajr", "الفجر"), ("Sunrise", "Shuruq", "الشروق"), ("Dhuhr", "Dhuhr", "الظهر"),
    ("Asr", "Asr", "العصر"), ("Maghrib", "Maghrib", "المغرب"), ("Isha", "Isha", "العشاء"),
]
FAJR, SUNRISE, DHUHR, ASR, MAGHRIB, ISHA = range(6)


def p_en(i, weekday):
    """weekday: Monday=0 … Friday=4 (Python's convention). Dhuhr is Jumuʿa on Friday."""
    return "Jumuʿa" if i == DHUHR and weekday == 4 else PRAYERS[i][1]


def p_ar(i, weekday):
    return "الجمعة" if i == DHUHR and weekday == 4 else PRAYERS[i][2]


# ── §3 PRAYER ─────────────────────────────────────────────────────────────

def _rad(d): return d * math.pi / 180
def _deg(r): return r * 180 / math.pi
def _fix(a, b):
    a = a - b * math.floor(a / b)
    return a + b if a < 0 else a
def _sin(d): return math.sin(_rad(d))
def _cos(d): return math.cos(_rad(d))
def _tan(d): return math.tan(_rad(d))
def _asin(x): return _deg(math.asin(x))
def _atan2(y, x): return _deg(math.atan2(y, x))
def _acot(x): return _deg(math.atan(1 / x))


def _julian(y, m, d):
    if m <= 2:
        y -= 1
        m += 12
    A = math.floor(y / 100)
    B = 2 - A + math.floor(A / 4)
    return math.floor(365.25 * (y + 4716)) + math.floor(30.6001 * (m + 1)) + d + B - 1524.5


def _sun(jd):
    D = jd - 2451545.0
    g = _fix(357.529 + 0.98560028 * D, 360)
    q = _fix(280.459 + 0.98564736 * D, 360)
    L = _fix(q + 1.915 * _sin(g) + 0.020 * _sin(2 * g), 360)
    e = 23.439 - 0.00000036 * D
    RA = _atan2(_cos(e) * _sin(L), _cos(L)) / 15
    return _asin(_sin(e) * _sin(L)), q / 15 - _fix(RA, 24)


def pray_times(y, m, d, lat, lng, tz):
    """m 1–12 · tz hours east of UTC → six minutes-after-midnight (None if the
    sun never reaches that angle, which only happens far from Morocco)."""
    jd = _julian(y, m, d) - lng / (15 * 24)

    def noon(t):
        return _fix(12 - _sun(jd + t)[1], 24)

    def angle(a, t, ccw):
        decl = _sun(jd + t)[0]
        n = noon(t)
        c = (-_sin(a) - _sin(decl) * _sin(lat)) / (_cos(decl) * _cos(lat))
        if c < -1 or c > 1:
            return None
        h = _deg(math.acos(c)) / 15
        return n - h if ccw else n + h

    def asr(t):
        decl = _sun(jd + t)[0]
        return angle(-_acot(1 + _tan(abs(lat - decl))), t, False)

    raw = [angle(19, 5 / 24, True), angle(0.833, 6 / 24, True), noon(12 / 24),
           asr(13 / 24), angle(0.833, 18 / 24, False), angle(17, 18 / 24, False)]
    off = [0, 0, 5, 0, 5, 0]
    out = []
    for i, r in enumerate(raw):
        if r is None:
            out.append(None)
            continue
        v = _fix(r + tz - lng / 15 + off[i] / 60 + 0.5 / 60, 24)
        h = math.floor(v)
        out.append(h * 60 + math.floor((v - h) * 60))
    return out


def tz_hours_at_noon(day):
    """The local UTC offset at noon of `day`: clock changes happen at night.
    The system's own zone data decides — the same clock the times are shown
    against, so a stale zone moves prayers and "now" together."""
    noon = datetime.datetime(day.year, day.month, day.day, 12).astimezone()
    return noon.utcoffset().total_seconds() / 3600


def times_for(day, lat, lng):
    return pray_times(day.year, day.month, day.day, lat, lng, tz_hours_at_noon(day))


def hm(m):
    return "--:--" if m is None else "%02d:%02d" % (m // 60, m % 60)


def fmt_left(mins):
    """'3 h, 32 min' — but under an hour, just '32 min'."""
    h, m = divmod(mins, 60)
    return "%d h, %d min" % (h, m) if h > 0 else "%d min" % m


def next_prayer(today, tomorrow, now_min_int):
    """(index, whole minutes to it, is_tomorrow). Shuruq counts, as in the deck."""
    for i, t in enumerate(today):
        if t is not None and t > now_min_int:
            return i, t - now_min_int, False
    f = tomorrow[FAJR] if tomorrow[FAJR] is not None else (today[FAJR] or 0)
    return FAJR, (1440 - now_min_int) + f, True


# ── §5 SPINE geometry — the rail runs Fajr-20 → Isha+3h, and may pass midnight ──

def spine_range(today):
    if today[FAJR] is None or today[ISHA] is None:
        return 5 * 60, 23 * 60
    return max(0, today[FAJR] - 20), today[ISHA] + 180


def rail_min(m, today):
    a, b = spine_range(today)
    if m >= a:
        return m
    lifted = m + 1440
    if lifted <= b:
        return lifted
    return m if (a - m) <= (lifted - b) else lifted


def pct_of(m, today):
    a, b = spine_range(today)
    return max(0.0, min(1.0, (rail_min(m, today) - a) / (b - a)))


def relax_labels(items, gap):
    """Label collision, relaxed in both directions around the true centre.
    items: list of dicts with 'pct'; sets 'lbl'. The tick never moves."""
    items.sort(key=lambda o: o["pct"])
    for o in items:
        o["lbl"] = o["pct"]
    for i in range(1, len(items)):
        if items[i]["lbl"] - items[i - 1]["lbl"] < gap:
            items[i]["lbl"] = items[i - 1]["lbl"] + gap
    over = items[-1]["lbl"] - 1.0 if items else 0
    if over > 0:
        for o in items:
            o["lbl"] -= over
        for i in range(len(items) - 2, -1, -1):
            if items[i + 1]["lbl"] - items[i]["lbl"] < gap:
                items[i]["lbl"] = items[i + 1]["lbl"] - gap
        if items[0]["lbl"] < 0:
            d = -items[0]["lbl"]
            for o in items:
                o["lbl"] += d
    return items


# ── weather codes (WMO, as Open-Meteo reports them) ──────────────────────

WX_TEXT = {0: "Clear", 1: "Mainly clear", 2: "Partly cloudy", 3: "Overcast", 45: "Fog", 48: "Rime fog",
           51: "Light drizzle", 53: "Drizzle", 55: "Dense drizzle", 56: "Freezing drizzle", 57: "Freezing drizzle",
           61: "Light rain", 63: "Rain", 65: "Heavy rain", 66: "Freezing rain", 67: "Freezing rain",
           71: "Light snow", 73: "Snow", 75: "Heavy snow", 77: "Snow grains", 80: "Showers", 81: "Showers",
           82: "Violent showers", 85: "Snow showers", 86: "Snow showers", 95: "Thunderstorm",
           96: "Storm + hail", 99: "Storm + hail"}


def wx_kind(c):
    if c == 0: return "clear"
    if c == 1: return "mostly"
    if c == 2: return "partly"
    if c == 3: return "cloud"
    if c in (45, 48): return "fog"
    if 51 <= c <= 57: return "drizzle"
    if 61 <= c <= 67 or 80 <= c <= 82: return "rain"
    if 71 <= c <= 77 or c in (85, 86): return "snow"
    if c >= 95: return "storm"
    return "none"
