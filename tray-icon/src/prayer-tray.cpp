/* prayer-tray.cpp — the Day Spine, living in the notification area.
 *
 * A port of the spine pane of "html file/start-dock.html" (deck v0.4):
 *   - prayer times computed locally, Morocco method (AlAdhan method 21):
 *     Fajr 19°, Isha 17°, Asr shadow 1, +5 min on Dhuhr and Maghrib;
 *   - the day drawn to scale from Fajr-20 min to Isha+3 h, with the true tick
 *     on the rail and a label that may be nudged off a crowded evening;
 *   - weather from Open-Meteo, the single optional network call;
 *   - a reminder N minutes before each adhan and one at the adhan itself.
 *
 * Left click the icon  → the flyout (city, weather, day spine, next prayer).
 * Right click          → menu: city, settings, weather refresh, autostart.
 * Click the city name in the flyout → settings.
 *
 * The tray icon is itself a tiny spine: the rail, what is lived of the day,
 * and where you are on it. It turns terracotta while an adhan is near.
 */
#include "common.h"
#include <objidl.h>
#include <algorithm>
#include <gdiplus.h>
#include "resource.h"

using namespace Gdiplus;

/* ── palette (start-dock.html :root) ───────────────────────────────── */
#define C_INK    RGB(0x11, 0x14, 0x1B)
#define C_INK2   RGB(0x17, 0x1C, 0x25)
#define C_INK3   RGB(0x1D, 0x24, 0x30)
#define C_LINE   RGB(0x28, 0x30, 0x3D)
#define C_LINE2  RGB(0x33, 0x3D, 0x4D)
#define C_TADEL  RGB(0xE9, 0xE5, 0xDB)
#define C_DIM    RGB(0x81, 0x8C, 0x9E)
#define C_DIMMER RGB(0x5A, 0x64, 0x73)
#define C_SAFF   RGB(0xE8, 0xA3, 0x3D)
#define C_VERD   RGB(0x4F, 0xB0, 0x9A)
#define C_TERRA  RGB(0xC9, 0x6A, 0x4E)
static inline Color GC(COLORREF c, BYTE a = 255) { return Color(a, GetRValue(c), GetGValue(c), GetBValue(c)); }
static inline COLORREF Mix(COLORREF a, COLORREF b, double t) {   /* a over b at opacity t */
    return RGB((int)(GetRValue(a) * t + GetRValue(b) * (1 - t)),
               (int)(GetGValue(a) * t + GetGValue(b) * (1 - t)),
               (int)(GetBValue(a) * t + GetBValue(b) * (1 - t)));
}

/* ── cities (start-dock.html CITIES, verbatim coordinates) ─────────── */
struct City { int id; const wchar_t *ar, *fr; double lat, lng; };
static const City CITIES[] = {
 {1,L"الرباط",L"Rabat",34.02,-6.84},{7,L"القنيطرة",L"Kénitra",34.26,-6.58},
 {14,L"طنجة",L"Tanger",35.76,-5.83},{15,L"تطوان",L"Tétouan",35.59,-5.36},
 {18,L"شفشاون",L"Chefchaouen",35.17,-5.26},{23,L"الحسيمة",L"Al Hoceïma",35.25,-3.93},
 {29,L"وزان",L"Ouezzane",34.80,-5.58},{31,L"وجدة",L"Oujda",34.68,-1.91},
 {39,L"الناظور",L"Nador",35.17,-2.93},{35,L"كرسيف",L"Guercif",34.23,-3.35},
 {58,L"الدار البيضاء",L"Casablanca",33.57,-7.59},{59,L"المحمدية",L"Mohammedia",33.69,-7.38},
 {61,L"سطات",L"Settat",33.00,-7.62},{65,L"برشيد",L"Berrechid",33.27,-7.59},
 {66,L"الجديدة",L"El Jadida",33.23,-8.50},{73,L"بني ملال",L"Béni Mellal",32.34,-6.35},
 {79,L"خريبكة",L"Khouribga",32.88,-6.91},{81,L"فاس",L"Fès",34.02,-5.01},
 {89,L"تازة",L"Taza",34.21,-4.01},{99,L"مكناس",L"Meknès",33.89,-5.55},
 {100,L"يفرن",L"Ifrane",33.53,-5.11},{103,L"آزرو",L"Azrou",33.44,-5.22},
 {104,L"مراكش",L"Marrakech",31.63,-7.98},{106,L"الصويرة",L"Essaouira",31.51,-9.77},
 {108,L"بنجرير",L"Benguerir",32.23,-7.95},{111,L"آسفي",L"Safi",32.30,-9.24},
 {117,L"أكادير",L"Agadir",30.43,-9.60},{118,L"تارودانت",L"Taroudant",30.47,-8.88},
 {119,L"تزنيت",L"Tiznit",29.70,-9.80},{123,L"طاطا",L"Tata",29.75,-7.97},
 {128,L"الرشيدية",L"Errachidia",31.93,-4.43},{136,L"ميدلت",L"Midelt",32.68,-4.73},
 {137,L"زاكورة",L"Zagora",30.33,-5.84},{138,L"ورزازات",L"Ouarzazate",30.92,-6.90},
 {139,L"تنغير",L"Tinghir",31.51,-5.53},{149,L"كلميم",L"Guelmim",28.98,-10.06},
 {152,L"طانطان",L"Tan-Tan",28.44,-11.10},{156,L"العيون",L"Laâyoune",27.13,-13.16},
 {165,L"الداخلة",L"Dakhla",23.68,-15.96},{148,L"سيدي إفني",L"Sidi Ifni",29.38,-10.17},
 {201,L"سلا",L"Salé",34.05,-6.80},{202,L"تمارة",L"Témara",33.93,-6.91},
 {203,L"العرائش",L"Larache",35.19,-6.16},{204,L"القصر الكبير",L"Ksar El Kébir",35.00,-5.90},
 {205,L"الخميسات",L"Khémisset",33.82,-6.07},{206,L"بركان",L"Berkane",34.92,-2.32},
 {207,L"تاوريرت",L"Taourirt",34.41,-2.90},{208,L"خنيفرة",L"Khénifra",32.94,-5.67},
 {209,L"صفرو",L"Sefrou",33.83,-4.83},{210,L"إنزكان",L"Inezgane",30.36,-9.54},
 {211,L"قلعة السراغنة",L"El Kelâa des Sraghna",32.05,-7.41},{212,L"اليوسفية",L"Youssoufia",32.25,-8.53},
 {213,L"الفقيه بن صالح",L"Fquih Ben Salah",32.50,-6.69},{214,L"أزيلال",L"Azilal",31.96,-6.57},
 {215,L"فكيك",L"Figuig",32.11,-1.23},{216,L"السمارة",L"Smara",26.74,-11.67},
 {217,L"بوجدور",L"Boujdour",26.13,-14.48},
};
static const int NCITIES = (int)(sizeof CITIES / sizeof CITIES[0]);
static const int DEFAULT_CITY_ID = 104;   /* Marrakech */
static int g_sorted[sizeof CITIES / sizeof CITIES[0]];   /* display order: alphabetical */

struct Prayer { const wchar_t *key, *en, *ar; };
static const Prayer PRAYERS[6] = {
 {L"Fajr",L"Fajr",L"الفجر"},{L"Sunrise",L"Shuruq",L"الشروق"},{L"Dhuhr",L"Dhuhr",L"الظهر"},
 {L"Asr",L"Asr",L"العصر"},{L"Maghrib",L"Maghrib",L"المغرب"},{L"Isha",L"Isha",L"العشاء"},
};
enum { P_FAJR, P_SUNRISE, P_DHUHR, P_ASR, P_MAGHRIB, P_ISHA };

/* The name a prayer goes by today: Dhuhr is Jumuʿa on a Friday. */
static const wchar_t *PEn(int i, int dow) { return (i == P_DHUHR && dow == 5) ? L"Jumu‘a" : PRAYERS[i].en; }
static const wchar_t *PAr(int i, int dow) { return (i == P_DHUHR && dow == 5) ? L"الجمعة" : PRAYERS[i].ar; }

/* ── weather codes (WMO, as Open-Meteo reports them) ───────────────── */
enum Wx { WX_NONE, WX_CLEAR, WX_MOSTLY, WX_PARTLY, WX_CLOUD, WX_FOG, WX_DRIZZLE, WX_RAIN, WX_SNOW, WX_STORM };
static Wx WxKind(int c) {
    if (c == 0) return WX_CLEAR;
    if (c == 1) return WX_MOSTLY;
    if (c == 2) return WX_PARTLY;
    if (c == 3) return WX_CLOUD;
    if (c == 45 || c == 48) return WX_FOG;
    if (c >= 51 && c <= 57) return WX_DRIZZLE;
    if ((c >= 61 && c <= 67) || (c >= 80 && c <= 82)) return WX_RAIN;
    if ((c >= 71 && c <= 77) || c == 85 || c == 86) return WX_SNOW;
    if (c >= 95) return WX_STORM;
    return WX_NONE;
}
static const wchar_t *WxText(int c) {
    switch (c) {
    case 0: return L"Clear"; case 1: return L"Mainly clear"; case 2: return L"Partly cloudy"; case 3: return L"Overcast";
    case 45: return L"Fog"; case 48: return L"Rime fog";
    case 51: return L"Light drizzle"; case 53: return L"Drizzle"; case 55: return L"Dense drizzle";
    case 56: case 57: return L"Freezing drizzle";
    case 61: return L"Light rain"; case 63: return L"Rain"; case 65: return L"Heavy rain";
    case 66: case 67: return L"Freezing rain";
    case 71: return L"Light snow"; case 73: return L"Snow"; case 75: return L"Heavy snow"; case 77: return L"Snow grains";
    case 80: case 81: return L"Showers"; case 82: return L"Violent showers"; case 85: case 86: return L"Snow showers";
    case 95: return L"Thunderstorm"; case 96: case 99: return L"Storm + hail";
    }
    return L"";
}

/* ── state ─────────────────────────────────────────────────────────── */
#define APP_NAME     L"Prayer Spine"
#define RUN_NAME     L"AchmaPrayerTray"
#define WND_CLASS    L"AchmaPrayerTray.Main"
#define FLY_CLASS    L"AchmaPrayerTray.Flyout"
#define SET_CLASS    L"AchmaPrayerTray.Settings"
#define TRAY_ID      1
#define WM_TRAY      (WM_APP + 1)
#define WM_WEATHER   (WM_APP + 2)
#define WM_POKE      (WM_APP + 9)
#define T_TICK       1
#define T_WEATHER    2
#define T_PULSE      3

enum { ID_SHOW = 100, ID_SETTINGS, ID_REFRESH, ID_AUTOSTART, ID_DOCK, ID_EXIT, ID_CITY0 = 1000 };

struct Location { wchar_t name[64], ar[64]; double lat, lng; BOOL custom; int cityId; };

struct Weather {
    BOOL have;          /* a reading exists (live or cached) */
    double temp, feels, rh, wind, tmax, tmin;
    int code, isDay;
    long long at;       /* unix seconds of the reading */
    wchar_t key[96];    /* which location it belongs to */
};

static struct {
    HINSTANCE inst;
    HWND main, fly, settings;
    Ini ini;
    UINT msgTaskbarCreated;
    double s;                           /* DPI scale, 1.0 = 96 dpi */

    Location loc;
    int pre;                            /* minutes of warning before each adhan; 0 = off */
    BOOL notifyAdhan, sound, wx;

    int today[6], tomorrow[6];          /* minutes after local midnight, -1 = undefined */
    int dayStamp;                       /* yyyymmdd the times were computed for */

    Weather w;
    enum { NET_IDLE, NET_WAIT, NET_OK, NET_OFF } net;
    volatile LONG fetching;
    int fetchGen;

    HICON trayIcon;
    int lastIconMinute;
    DWORD flyHiddenAt;

    const wchar_t *mono, *arab;
    HFONT fCity, fTemp, fLbl, fTime, fName, fNameB, fFoot, fAr, fLeft;
    ULONG_PTR gdipToken;
} G;

static inline int S(double v) { return (int)floor(v * G.s + 0.5); }

/* ── time helpers ──────────────────────────────────────────────────── */

static int NowMinuteInt(void) { SYSTEMTIME t; GetLocalTime(&t); return t.wHour * 60 + t.wMinute; }
static double NowMinute(void) {
    SYSTEMTIME t; GetLocalTime(&t);
    return t.wHour * 60 + t.wMinute + t.wSecond / 60.0;
}
static int Today(SYSTEMTIME *out) {
    SYSTEMTIME t; GetLocalTime(&t);
    if (out) *out = t;
    return t.wYear * 10000 + t.wMonth * 100 + t.wDay;
}
static long long UnixNow(void) {
    FILETIME f; GetSystemTimeAsFileTime(&f);
    ULARGE_INTEGER u; u.LowPart = f.dwLowDateTime; u.HighPart = f.dwHighDateTime;
    return (long long)(u.QuadPart / 10000000ULL) - 11644473600LL;
}

/* Offset of local time from UTC, in hours, at local noon of the given day.
   The offset at noon is the day's offset: clock changes happen at night.
   Windows' own zone data decides it — the same clock the times are shown
   against, so a stale zone moves prayers and "now" together. */
static double TzHoursAtNoon(int y, int m, int d) {
    SYSTEMTIME local = {}, utc = {};
    local.wYear = (WORD)y; local.wMonth = (WORD)m; local.wDay = (WORD)d; local.wHour = 12;
    if (!TzSpecificLocalTimeToSystemTime(NULL, &local, &utc)) {
        TIME_ZONE_INFORMATION tz; GetTimeZoneInformation(&tz);
        return -tz.Bias / 60.0;
    }
    FILETIME fl, fu; SystemTimeToFileTime(&local, &fl); SystemTimeToFileTime(&utc, &fu);
    ULARGE_INTEGER a, b;
    a.LowPart = fl.dwLowDateTime; a.HighPart = fl.dwHighDateTime;
    b.LowPart = fu.dwLowDateTime; b.HighPart = fu.dwHighDateTime;
    return ((double)(long long)(a.QuadPart - b.QuadPart)) / 36000000000.0;
}

/* ── §PRAYER — PrayTimes.org astronomy (Hamid Zarrabi-Zadeh, LGPL), one pass,
   exactly as start-dock.html runs it. Checked there against AlAdhan for four
   Moroccan cities over four months: identical to the minute except Asr ±1 on
   about one day in seven. ── */
namespace pt {
static const double PI = 3.14159265358979323846;
static double rad(double d) { return d * PI / 180; }
static double deg(double r) { return r * 180 / PI; }
static double fix(double a, double b) { a = a - b * floor(a / b); return a < 0 ? a + b : a; }
static double dsin(double d) { return sin(rad(d)); }
static double dcos(double d) { return cos(rad(d)); }
static double dtan(double d) { return tan(rad(d)); }
static double dasin(double x) { return deg(asin(x)); }
static double dacos(double x) { return deg(acos(x)); }
static double datan2(double y, double x) { return deg(atan2(y, x)); }
static double dacot(double x) { return deg(atan(1 / x)); }
static double julian(int y, int m, int d) {
    if (m <= 2) { y -= 1; m += 12; }
    double A = floor(y / 100.0), B = 2 - A + floor(A / 4);
    return floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5;
}
struct Sun { double decl, eqt; };
static Sun sun(double jd) {
    double D = jd - 2451545.0, g = fix(357.529 + 0.98560028 * D, 360), q = fix(280.459 + 0.98564736 * D, 360);
    double L = fix(q + 1.915 * dsin(g) + 0.020 * dsin(2 * g), 360), e = 23.439 - 0.00000036 * D;
    double RA = datan2(dcos(e) * dsin(L), dcos(L)) / 15;
    Sun s; s.decl = dasin(dsin(e) * dsin(L)); s.eqt = q / 15 - fix(RA, 24);
    return s;
}
static double g_jd, g_lat;
static double noon(double t) { return fix(12 - sun(g_jd + t).eqt, 24); }
static double angle(double a, double t, bool ccw) {
    Sun s = sun(g_jd + t);
    double n = noon(t);
    double c = (-dsin(a) - dsin(s.decl) * dsin(g_lat)) / (dcos(s.decl) * dcos(g_lat));
    if (c < -1 || c > 1) return NAN;        /* the sun never reaches that angle today */
    double h = dacos(c) / 15;
    return n + (ccw ? -h : h);
}
static double asr(double t) {
    Sun s = sun(g_jd + t);
    return angle(-dacot(1 + dtan(fabs(g_lat - s.decl))), t, false);
}
/* m 1–12 · tz hours east of UTC · out: minutes after midnight, -1 if undefined */
static void times(int y, int m, int d, double lat, double lng, double tz, int out[6]) {
    g_jd = julian(y, m, d) - lng / (15 * 24);
    g_lat = lat;
    double raw[6] = { angle(19, 5 / 24.0, true), angle(0.833, 6 / 24.0, true), noon(12 / 24.0),
                      asr(13 / 24.0), angle(0.833, 18 / 24.0, false), angle(17, 18 / 24.0, false) };
    const double off[6] = { 0, 0, 5, 0, 5, 0 };
    for (int i = 0; i < 6; i++) {
        if (isnan(raw[i])) { out[i] = -1; continue; }
        double v = fix(raw[i] + tz - lng / 15 + off[i] / 60 + 0.5 / 60, 24);
        int h = (int)floor(v);
        out[i] = h * 60 + (int)floor((v - h) * 60);
    }
}
} // namespace pt

static void ComputePrayer(void) {
    SYSTEMTIME t;
    G.dayStamp = Today(&t);
    pt::times(t.wYear, t.wMonth, t.wDay, G.loc.lat, G.loc.lng, TzHoursAtNoon(t.wYear, t.wMonth, t.wDay), G.today);
    /* tomorrow, for the countdown after Isha */
    FILETIME f; SystemTimeToFileTime(&t, &f);
    ULARGE_INTEGER u; u.LowPart = f.dwLowDateTime; u.HighPart = f.dwHighDateTime;
    u.QuadPart += 864000000000ULL;
    f.dwLowDateTime = u.LowPart; f.dwHighDateTime = u.HighPart;
    SYSTEMTIME n; FileTimeToSystemTime(&f, &n);
    pt::times(n.wYear, n.wMonth, n.wDay, G.loc.lat, G.loc.lng, TzHoursAtNoon(n.wYear, n.wMonth, n.wDay), G.tomorrow);
}

/* Next prayer (Shuruq included, as the deck does) and whole minutes to it. */
static void NextPrayer(int *idx, int *mins, BOOL *isTomorrow) {
    int nm = NowMinuteInt();
    for (int i = 0; i < 6; i++) {
        if (G.today[i] > nm) { *idx = i; *mins = G.today[i] - nm; *isTomorrow = FALSE; return; }
    }
    int f = G.tomorrow[P_FAJR] >= 0 ? G.tomorrow[P_FAJR] : G.today[P_FAJR];
    *idx = P_FAJR; *mins = (1440 - nm) + (f >= 0 ? f : 0); *isTomorrow = TRUE;
}

static void FmtHM(int m, wchar_t *out, size_t cap) {
    if (m < 0) _snwprintf(out, cap, L"--:--");
    else _snwprintf(out, cap, L"%02d:%02d", m / 60, m % 60);
    out[cap - 1] = 0;
}
/* "3 h, 32 min" — but under an hour, just "32 min" */
static void FmtLeft(int mins, wchar_t *out, size_t cap) {
    int h = mins / 60, m = mins % 60;
    if (h > 0) _snwprintf(out, cap, L"%d h, %d min", h, m);
    else _snwprintf(out, cap, L"%d min", m);
    out[cap - 1] = 0;
}

/* ── §SPINE geometry (start-dock.html spineRange / railMin / pctOf) ──
   The rail runs Fajr-20min → Isha+3h, and on a late-Isha evening that end is
   past midnight, so the range may exceed 1440 and minutes are lifted into it. */
static void SpineRange(double *a, double *b) {
    if (G.today[P_FAJR] < 0 || G.today[P_ISHA] < 0) { *a = 5 * 60; *b = 23 * 60; return; }
    *a = G.today[P_FAJR] - 20 > 0 ? G.today[P_FAJR] - 20 : 0;
    *b = G.today[P_ISHA] + 180;
}
static double RailMin(double m) {
    double a, b; SpineRange(&a, &b);
    if (m >= a) return m;
    double lifted = m + 1440;
    if (lifted <= b) return lifted;
    return (a - m) <= (lifted - b) ? m : lifted;
}
static double PctOf(double m) {
    double a, b; SpineRange(&a, &b);
    double p = (RailMin(m) - a) / (b - a);
    return p < 0 ? 0 : p > 1 ? 1 : p;
}

/* ── settings ──────────────────────────────────────────────────────── */

static const City *CityById(int id) {
    for (int i = 0; i < NCITIES; i++) if (CITIES[i].id == id) return &CITIES[i];
    return NULL;
}

static void LoadSettings(void) {
    const Ini &I = G.ini;
    G.loc.custom = I.GetInt(L"location", L"custom", 0);
    G.loc.cityId = I.GetInt(L"location", L"city_id", DEFAULT_CITY_ID);
    const City *c = CityById(G.loc.cityId);
    if (!c) { c = CityById(DEFAULT_CITY_ID); G.loc.cityId = DEFAULT_CITY_ID; }
    if (G.loc.custom) {
        I.GetStr(L"location", L"name", L"My location", G.loc.name, 64);
        G.loc.ar[0] = 0;
        G.loc.lat = I.GetDouble(L"location", L"lat", c->lat);
        G.loc.lng = I.GetDouble(L"location", L"lng", c->lng);
        if (G.loc.lat < -90 || G.loc.lat > 90 || G.loc.lng < -180 || G.loc.lng > 180) G.loc.custom = FALSE;
    }
    if (!G.loc.custom) {
        wcopy(G.loc.name, 64, c->fr); wcopy(G.loc.ar, 64, c->ar);
        G.loc.lat = c->lat; G.loc.lng = c->lng;
    }
    G.pre = I.GetInt(L"alerts", L"remind_before", 10);
    if (G.pre < 0 || G.pre > 60) G.pre = 10;
    G.notifyAdhan = I.GetInt(L"alerts", L"at_adhan", 1);
    G.sound = I.GetInt(L"alerts", L"sound", 1);
    G.wx = I.GetInt(L"weather", L"enabled", 1);
}

static void SaveSettings(void) {
    const Ini &I = G.ini;
    I.SetInt(L"location", L"custom", G.loc.custom);
    I.SetInt(L"location", L"city_id", G.loc.cityId);
    if (G.loc.custom) {
        I.SetStr(L"location", L"name", G.loc.name);
        I.SetDouble(L"location", L"lat", G.loc.lat);
        I.SetDouble(L"location", L"lng", G.loc.lng);
    }
    I.SetInt(L"alerts", L"remind_before", G.pre);
    I.SetInt(L"alerts", L"at_adhan", G.notifyAdhan);
    I.SetInt(L"alerts", L"sound", G.sound);
    I.SetInt(L"weather", L"enabled", G.wx);
}

static void LocKey(wchar_t *out, size_t cap) {
    _snwprintf(out, cap, L"%.2f,%.2f", G.loc.lat, G.loc.lng);
    out[cap - 1] = 0;
}

/* ── §WEATHER — Open-Meteo, on a worker thread ─────────────────────── */

struct FetchJob { double lat, lng; int gen; };

static DWORD WINAPI WeatherThread(LPVOID p) {
    FetchJob *job = (FetchJob *)p;
    wchar_t q[512];
    _snwprintf(q, 512,
        L"api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
        L"&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m,is_day"
        L"&daily=temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=1",
        job->lat, job->lng);
    q[511] = 0;
    wchar_t url[600];
    _snwprintf(url, 600, L"https://%ls", q); url[599] = 0;
    char *body = HttpGet(url);
    if (!body) {   /* no TLS 1.2 on this machine (stock Windows 7 without IE11), or offline */
        _snwprintf(url, 600, L"http://%ls", q); url[599] = 0;
        body = HttpGet(url);
    }
    Weather *w = (Weather *)calloc(1, sizeof(Weather));
    if (body && w) {
        const char *cur = JsonObject(body, "current");
        const char *day = JsonObject(body, "daily");
        double code = -1, isDay = 1;
        if (cur && JsonNumber(cur, "temperature_2m", &w->temp) && JsonNumber(cur, "weather_code", &code)) {
            w->have = TRUE;
            w->code = (int)code;
            if (!JsonNumber(cur, "apparent_temperature", &w->feels)) w->feels = w->temp;
            JsonNumber(cur, "relative_humidity_2m", &w->rh);
            JsonNumber(cur, "wind_speed_10m", &w->wind);
            JsonNumber(cur, "is_day", &isDay);
            w->isDay = isDay != 0;
            w->tmax = w->tmin = NAN;
            if (day) { JsonNumber(day, "temperature_2m_max", &w->tmax); JsonNumber(day, "temperature_2m_min", &w->tmin); }
            w->at = UnixNow();
        }
    }
    free(body);
    if (w && !w->have) { free(w); w = NULL; }
    if (!PostMessageW(G.main, WM_WEATHER, (WPARAM)job->gen, (LPARAM)w)) free(w);
    free(job);
    return 0;
}

static void SaveWeatherCache(void) {
    const Ini &I = G.ini;
    wchar_t b[64];
    I.SetStr(L"weather", L"key", G.w.key);
    _snwprintf(b, 64, L"%lld", G.w.at); b[63] = 0; I.SetStr(L"weather", L"at", b);
    I.SetDouble(L"weather", L"temp", G.w.temp);
    I.SetDouble(L"weather", L"feels", G.w.feels);
    I.SetDouble(L"weather", L"rh", G.w.rh);
    I.SetDouble(L"weather", L"wind", G.w.wind);
    I.SetDouble(L"weather", L"tmax", isnan(G.w.tmax) ? -999 : G.w.tmax);
    I.SetDouble(L"weather", L"tmin", isnan(G.w.tmin) ? -999 : G.w.tmin);
    I.SetInt(L"weather", L"code", G.w.code);
    I.SetInt(L"weather", L"is_day", G.w.isDay);
}

/* The last reading for THIS location, so the flyout is never blank offline. */
static void LoadWeatherCache(void) {
    const Ini &I = G.ini;
    wchar_t key[96], want[96], at[32];
    I.GetStr(L"weather", L"key", L"", key, 96);
    LocKey(want, 96);
    memset(&G.w, 0, sizeof G.w);
    if (wcscmp(key, want) != 0) return;
    I.GetStr(L"weather", L"at", L"0", at, 32);
    G.w.at = _wtoi64(at);
    if (G.w.at <= 0) return;
    G.w.have = TRUE;
    wcopy(G.w.key, 96, key);
    G.w.temp = I.GetDouble(L"weather", L"temp", 0);
    G.w.feels = I.GetDouble(L"weather", L"feels", G.w.temp);
    G.w.rh = I.GetDouble(L"weather", L"rh", 0);
    G.w.wind = I.GetDouble(L"weather", L"wind", 0);
    G.w.tmax = I.GetDouble(L"weather", L"tmax", -999); if (G.w.tmax < -900) G.w.tmax = NAN;
    G.w.tmin = I.GetDouble(L"weather", L"tmin", -999); if (G.w.tmin < -900) G.w.tmin = NAN;
    G.w.code = I.GetInt(L"weather", L"code", 0);
    G.w.isDay = I.GetInt(L"weather", L"is_day", 1);
}

static void FetchWeather(BOOL force) {
    if (!G.wx) { G.net = G.NET_IDLE; return; }
    if (!force && G.w.have && UnixNow() - G.w.at < 30 * 60) { G.net = G.NET_OK; return; }
    if (InterlockedCompareExchange(&G.fetching, 1, 0) != 0) return;
    FetchJob *job = (FetchJob *)malloc(sizeof(FetchJob));
    job->lat = G.loc.lat; job->lng = G.loc.lng; job->gen = G.fetchGen;
    G.net = G.NET_WAIT;
    HANDLE th = CreateThread(NULL, 0, WeatherThread, job, 0, NULL);
    if (th) CloseHandle(th);
    else { free(job); InterlockedExchange(&G.fetching, 0); G.net = G.NET_OFF; }
}

/* ── drawing: weather glyph (vector, so it looks the same on 7 and 11) ── */

static void FillCloud(Graphics &g, float cx, float cy, float u, const Color &c) {
    SolidBrush b(c);
    g.FillEllipse(&b, cx - 0.42f * u, cy - 0.05f * u, 0.46f * u, 0.40f * u);
    g.FillEllipse(&b, cx - 0.20f * u, cy - 0.30f * u, 0.52f * u, 0.56f * u);
    g.FillEllipse(&b, cx + 0.08f * u, cy - 0.08f * u, 0.40f * u, 0.42f * u);
    g.FillRectangle(&b, cx - 0.22f * u, cy + 0.06f * u, 0.52f * u, 0.29f * u);
}

static void DrawSun(Graphics &g, float cx, float cy, float u, COLORREF bg) {
    Pen ray(GC(C_SAFF), (std::max)(1.0f, u * 0.08f));
    ray.SetStartCap(LineCapRound); ray.SetEndCap(LineCapRound);
    for (int i = 0; i < 8; i++) {
        float a = (float)(i * pt::PI / 4);
        g.DrawLine(&ray, cx + cosf(a) * u * 0.30f, cy + sinf(a) * u * 0.30f, cx + cosf(a) * u * 0.44f, cy + sinf(a) * u * 0.44f);
    }
    SolidBrush b(GC(C_SAFF));
    g.FillEllipse(&b, cx - 0.21f * u, cy - 0.21f * u, 0.42f * u, 0.42f * u);
    (void)bg;
}

static void DrawMoon(Graphics &g, float cx, float cy, float u, COLORREF bg) {
    SolidBrush m(Color(255, 0xF0, 0xDC, 0x9A)), cut(GC(bg));
    g.FillEllipse(&m, cx - 0.30f * u, cy - 0.30f * u, 0.60f * u, 0.60f * u);
    g.FillEllipse(&cut, cx - 0.14f * u, cy - 0.40f * u, 0.56f * u, 0.56f * u);
}

/* size = box edge in px, centred on (cx, cy). bg is what it sits on. */
static void DrawWeather(Graphics &g, float cx, float cy, float u, int code, BOOL day, COLORREF bg) {
    Wx k = WxKind(code);
    Color cloud(255, 0xD9, 0xDC, 0xE3), dark(255, 0x9A, 0xA3, 0xB2);
    switch (k) {
    case WX_CLEAR:
        if (day) DrawSun(g, cx, cy, u, bg); else DrawMoon(g, cx, cy, u, bg);
        break;
    case WX_MOSTLY: case WX_PARTLY:
        if (day) DrawSun(g, cx - 0.16f * u, cy - 0.14f * u, u * (k == WX_MOSTLY ? 0.95f : 0.8f), bg);
        else DrawMoon(g, cx - 0.14f * u, cy - 0.14f * u, u * 0.85f, bg);
        FillCloud(g, cx + 0.10f * u, cy + 0.12f * u, u * (k == WX_MOSTLY ? 0.55f : 0.75f), cloud);
        break;
    case WX_CLOUD:
        FillCloud(g, cx, cy, u, cloud);
        break;
    case WX_FOG: {
        Pen p(cloud, (std::max)(1.0f, u * 0.09f)); p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound);
        for (int i = 0; i < 3; i++) {
            float y = cy - 0.22f * u + i * 0.22f * u, o = (i % 2) ? 0.08f * u : 0;
            g.DrawLine(&p, cx - 0.38f * u + o, y, cx + 0.38f * u - (0.08f * u - o), y);
        }
        break;
    }
    case WX_DRIZZLE: case WX_RAIN: case WX_STORM: {
        FillCloud(g, cx, cy - 0.12f * u, u * 0.9f, k == WX_STORM ? dark : cloud);
        if (k == WX_STORM) {
            SolidBrush bolt(GC(C_SAFF));
            PointF pts[] = { PointF(cx + 0.02f * u, cy + 0.05f * u), PointF(cx - 0.14f * u, cy + 0.30f * u),
                             PointF(cx - 0.01f * u, cy + 0.30f * u), PointF(cx - 0.08f * u, cy + 0.48f * u),
                             PointF(cx + 0.16f * u, cy + 0.20f * u), PointF(cx + 0.03f * u, cy + 0.20f * u),
                             PointF(cx + 0.12f * u, cy + 0.05f * u) };
            g.FillPolygon(&bolt, pts, 7);
        } else {
            Pen p(Color(255, 0x6F, 0xA8, 0xF0), (std::max)(1.0f, u * (k == WX_RAIN ? 0.08f : 0.06f)));
            p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound);
            for (int i = -1; i <= 1; i++) {
                float x = cx + i * 0.2f * u;
                g.DrawLine(&p, x, cy + 0.16f * u, x - 0.06f * u, cy + (k == WX_RAIN ? 0.40f : 0.30f) * u);
            }
        }
        break;
    }
    case WX_SNOW: {
        FillCloud(g, cx, cy - 0.12f * u, u * 0.9f, cloud);
        SolidBrush b(Color(255, 0xFF, 0xFF, 0xFF));
        float r = (std::max)(1.0f, 0.06f * u);
        for (int i = -1; i <= 1; i++) g.FillEllipse(&b, cx + i * 0.2f * u - r, cy + 0.3f * u - r + (i == 0 ? 0.08f * u : 0), 2 * r, 2 * r);
        break;
    }
    default: {
        SolidBrush b(GC(C_DIM));
        g.FillEllipse(&b, cx - 0.06f * u, cy - 0.06f * u, 0.12f * u, 0.12f * u);
    }
    }
}

/* ── drawing: the flyout ───────────────────────────────────────────── */

static void Txt(HDC dc, HFONT f, COLORREF c, int x, int ycentre, const wchar_t *s, int spacing = 0) {
    SelectObject(dc, f);
    SetTextColor(dc, c);
    SetTextCharacterExtra(dc, spacing);
    RECT r = { x, ycentre - 40, x + 2000, ycentre + 40 };
    DrawTextW(dc, s, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_LEFT);
    SetTextCharacterExtra(dc, 0);
}
static void TxtRight(HDC dc, HFONT f, COLORREF c, int right, int ycentre, const wchar_t *s) {
    SelectObject(dc, f);
    SetTextColor(dc, c);
    RECT r = { right - 2000, ycentre - 40, right, ycentre + 40 };
    DrawTextW(dc, s, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_RIGHT);
}
static int TxtW(HDC dc, HFONT f, const wchar_t *s, int spacing = 0) {
    SelectObject(dc, f);
    SetTextCharacterExtra(dc, spacing);
    SIZE z = {}; GetTextExtentPoint32W(dc, s, (int)wcslen(s), &z);
    SetTextCharacterExtra(dc, 0);
    return z.cx;
}
static void Fill(HDC dc, int x, int y, int w, int h, COLORREF c) {
    RECT r = { x, y, x + w, y + h };
    SetBkColor(dc, c);
    ExtTextOutW(dc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);   /* fastest solid fill there is */
}

/* The header is clickable (opens settings); remember where it is. */
static RECT g_headerHit;

static void PaintFlyout(HDC dc, int W, int H) {
    SetBkMode(dc, TRANSPARENT);
    Fill(dc, 0, 0, W, H, C_INK2);

    /* §BAR */
    const int barH = S(32);
    Fill(dc, 0, 0, W, barH, C_INK3);
    Fill(dc, 0, barH, W, 1, C_LINE);
    g_headerHit.left = 0; g_headerHit.top = 0; g_headerHit.right = W; g_headerHit.bottom = barH;

    SYSTEMTIME st; GetLocalTime(&st);
    const int dow = st.wDayOfWeek;
    int x = S(12), cy = barH / 2;
    COLORREF netc = G.net == G.NET_OK ? C_VERD : G.net == G.NET_OFF ? C_TERRA : G.net == G.NET_WAIT ? C_SAFF : C_LINE2;
    {
        Graphics g(dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        SolidBrush b(GC(netc));
        float d = 6 * (float)G.s;
        g.FillEllipse(&b, (float)x, cy - d / 2, d, d);
    }
    x += S(6) + S(9);
    Txt(dc, G.fCity, C_TADEL, x, cy, G.loc.name);
    x += TxtW(dc, G.fCity, G.loc.name) + S(9);
    if (G.wx) {
        wchar_t t[16];
        if (G.w.have) _snwprintf(t, 16, L"%d°", (int)floor(G.w.temp + 0.5)); else wcopy(t, 16, L"--°");
        t[15] = 0;
        Txt(dc, G.fTemp, C_DIM, x, cy, t);
        x += TxtW(dc, G.fTemp, t) + S(9);
        if (G.w.have) {
            Graphics g(dc);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            float u = 16 * (float)G.s;
            DrawWeather(g, x + u / 2, (float)cy, u, G.w.code, G.w.isDay, C_INK3);
        }
    }

    /* §SPINE */
    int y = barH + 1 + S(9);
    const int lblH = S(12);
    Txt(dc, G.fLbl, C_DIMMER, S(11), y + lblH / 2, L"DAY SPINE", S(1.6));
    y += lblH + S(7);

    /* footer heights, bottom-up */
    const int footPad = S(9), footLblH = S(12), nameH = S(15), leftH = S(23), bottomPad = S(8);
    const int footTop = H - bottomPad - leftH - S(2) - nameH - S(4) - footLblH - footPad - 1;
    const int railTop = y + S(6), railBot = footTop - S(7) - S(6);
    const int railH = railBot - railTop;
    const int rx = S(11);                       /* rail box left */
    const int trackX = rx + S(47);
    const int railRight = W - S(11);

    Fill(dc, trackX, railTop, S(2), railH, C_LINE2);

    const double nowM = NowMinute();
    const double nowPct = PctOf(nowM);
    const int nowY = railTop + (int)(nowPct * railH + 0.5);

    {   /* elapsed: verdigris → saffron at .5 */
        Graphics g(dc);
        if (nowY > railTop + 1) {
            LinearGradientBrush lg(Point(0, railTop - 1), Point(0, nowY + 1), GC(C_VERD, 128), GC(C_SAFF, 128));
            g.FillRectangle(&lg, trackX, railTop, S(2), nowY - railTop);
        }
        /* the now line, saffron at .3, drawn under the labels as the DOM does */
        SolidBrush ln(GC(C_SAFF, 77));
        g.FillRectangle(&ln, rx + S(39) + S(12), nowY, (railRight + S(5)) - (rx + S(51)), (std::max)(1, S(1)));
    }

    int nidx = 0, nmins = 0; BOOL tomorrow = FALSE;
    NextPrayer(&nidx, &nmins, &tomorrow);

    /* label collision — relaxed in both directions around the true centre;
       the TICK stays at the true time, only the label moves */
    struct Item { int i, t; double pct, lbl; } it[6];
    int n = 0;
    for (int i = 0; i < 6; i++) if (G.today[i] >= 0) { it[n].i = i; it[n].t = G.today[i]; it[n].pct = it[n].lbl = PctOf(G.today[i]); n++; }
    for (int a = 1; a < n; a++) for (int b = a; b > 0 && it[b].pct < it[b - 1].pct; b--) { Item t = it[b]; it[b] = it[b - 1]; it[b - 1] = t; }
    const double GAP = railH > 0 ? (double)S(13) / railH : 0.024;
    for (int a = 1; a < n; a++) if (it[a].lbl - it[a - 1].lbl < GAP) it[a].lbl = it[a - 1].lbl + GAP;
    double over = n ? it[n - 1].lbl - 1.0 : 0;
    if (over > 0) {
        for (int a = 0; a < n; a++) it[a].lbl -= over;
        for (int a = n - 2; a >= 0; a--) if (it[a + 1].lbl - it[a].lbl < GAP) it[a].lbl = it[a + 1].lbl - GAP;
        if (n && it[0].lbl < 0) { double d = -it[0].lbl; for (int a = 0; a < n; a++) it[a].lbl += d; }
    }

    const COLORREF tickPass = Mix(C_LINE2, C_INK2, 0.45);
    for (int a = 0; a < n; a++) {
        BOOL pass = it[a].t <= nowM, next = it[a].i == nidx;
        int ty = railTop + (int)(it[a].pct * railH + 0.5);
        int ly = railTop + (int)(it[a].lbl * railH + 0.5);
        /* tick, true position */
        if (next) Fill(dc, rx + S(43), ty - S(1), S(10), S(2), C_TERRA);
        else Fill(dc, rx + S(43), ty, S(10), (std::max)(1, S(1)), pass ? tickPass : C_LINE2);
        /* label row */
        wchar_t hm[8]; FmtHM(it[a].t, hm, 8);
        COLORREF tc = next ? C_TERRA : C_DIMMER;
        COLORREF nc = next ? C_TERRA : pass ? C_DIMMER : C_DIM;
        TxtRight(dc, G.fTime, tc, rx + S(43), ly, hm);
        if (next) Fill(dc, rx + S(43), ly - S(1), S(10), S(2), C_TERRA);
        else Fill(dc, rx + S(43), ly, S(10), (std::max)(1, S(1)), C_LINE2);
        Txt(dc, next ? G.fNameB : G.fName, nc, rx + S(53) + S(6), ly, PEn(it[a].i, dow));
    }

    {   /* now dot, with the deck's 2.6 s pulse */
        Graphics g(dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        float cx = trackX + S(2) / 2.0f, cyy = (float)nowY, r = 3.5f * (float)G.s;
        double ph = (GetTickCount() % 2600) / 2600.0;
        if (ph < 0.7) {
            float spread = (float)(ph / 0.7) * 7 * (float)G.s;
            BYTE al = (BYTE)(140 * (1 - ph / 0.7));
            SolidBrush halo(GC(C_SAFF, al));
            g.FillEllipse(&halo, cx - r - spread, cyy - r - spread, 2 * (r + spread), 2 * (r + spread));
        }
        SolidBrush dot(GC(C_SAFF));
        g.FillEllipse(&dot, cx - r, cyy - r, 2 * r, 2 * r);
    }

    /* §FOOT */
    Fill(dc, 0, footTop, W, 1, C_LINE);
    y = footTop + 1 + footPad;
    Txt(dc, G.fLbl, C_DIMMER, S(11), y + footLblH / 2, L"NEXT PRAYER", S(1.6));
    y += footLblH + S(4);
    {
        wchar_t nm[64];
        _snwprintf(nm, 64, L"%ls · ", PEn(nidx, tomorrow ? (dow + 1) % 7 : dow)); nm[63] = 0;
        Txt(dc, G.fFoot, C_DIM, S(11), y + nameH / 2, nm);
        int nx = S(11) + TxtW(dc, G.fFoot, nm);
        Txt(dc, G.fAr, C_TERRA, nx, y + nameH / 2, PAr(nidx, tomorrow ? (dow + 1) % 7 : dow));
    }
    y += nameH + S(2);
    wchar_t left[32]; FmtLeft(nmins, left, 32);
    Txt(dc, G.fLeft, C_TERRA, S(11), y + leftH / 2, left);

    /* hairline frame */
    HBRUSH fr = CreateSolidBrush(C_LINE2);
    RECT all = { 0, 0, W, H };
    FrameRect(dc, &all, fr);
    DeleteObject(fr);
}

/* ── drawing: the tray icon, a spine in 16 pixels ──────────────────── */

static BOOL AdhanNear(void) {
    if (G.pre <= 0) return FALSE;
    int nm = NowMinuteInt();
    for (int i = 0; i < 6; i++) {
        if (i == P_SUNRISE || G.today[i] < 0) continue;
        int d = G.today[i] - nm;
        if (d >= 0 && d <= G.pre) return TRUE;
    }
    return FALSE;
}

static HICON MakeTrayIcon(int size) {
    Bitmap bmp(size, size, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.Clear(Color(0, 0, 0, 0));
    float z = (float)size, u = z / 16.0f;

    BOOL near_ = AdhanNear();
    /* tile */
    GraphicsPath tile;
    float r = 3.2f * u, m = 0.5f * u;
    tile.AddArc(m, m, 2 * r, 2 * r, 180, 90);
    tile.AddArc(z - m - 2 * r, m, 2 * r, 2 * r, 270, 90);
    tile.AddArc(z - m - 2 * r, z - m - 2 * r, 2 * r, 2 * r, 0, 90);
    tile.AddArc(m, z - m - 2 * r, 2 * r, 2 * r, 90, 90);
    tile.CloseFigure();
    SolidBrush bg(GC(C_INK3));
    g.FillPath(&bg, &tile);
    Pen edge(near_ ? GC(C_TERRA) : GC(C_LINE2), (std::max)(1.0f, u * (near_ ? 1.3f : 1.0f)));
    g.DrawPath(&edge, &tile);

    /* rail */
    float top = 2.6f * u, bot = z - 2.6f * u, rx = 5.2f * u, rw = (std::max)(1.0f, 1.6f * u);
    SolidBrush track(GC(C_LINE2));
    g.FillRectangle(&track, rx, top, rw, bot - top);
    double p = PctOf(NowMinute());
    float ny = top + (float)p * (bot - top);
    if (ny > top + 0.5f) {
        LinearGradientBrush lg(PointF(0, top - 1), PointF(0, ny + 1), GC(C_VERD), GC(C_SAFF));
        g.FillRectangle(&lg, rx, top, rw, ny - top);
    }
    /* prayer ticks */
    int nidx, nmins; BOOL tom;
    NextPrayer(&nidx, &nmins, &tom);
    g.SetSmoothingMode(SmoothingModeNone);
    for (int i = 0; i < 6; i++) {
        if (G.today[i] < 0) continue;
        float ty = top + (float)PctOf(G.today[i]) * (bot - top);
        BOOL next = i == nidx && !tom;
        SolidBrush tb(next ? GC(C_TERRA) : GC(C_DIM, G.today[i] <= NowMinute() ? 110 : 220));
        float len = next ? 7.0f * u : 4.5f * u;
        g.FillRectangle(&tb, rx + rw + 1.0f * u, ty - (next ? u : u / 2), len, next ? 2 * u : u);
    }
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    SolidBrush dot(GC(C_SAFF));
    float dr = 2.2f * u;
    g.FillEllipse(&dot, rx + rw / 2 - dr, ny - dr, 2 * dr, 2 * dr);

    HICON ic = NULL;
    bmp.GetHICON(&ic);
    return ic;
}

/* ── tray: icon + tooltip refresh ──────────────────────────────────── */

static void RefreshTray(BOOL force) {
    int nm = NowMinuteInt();
    if (!force && nm == G.lastIconMinute) return;
    G.lastIconMinute = nm;

    HICON old = G.trayIcon;
    G.trayIcon = MakeTrayIcon(GetSystemMetrics(SM_CXSMICON));

    int nidx, nmins; BOOL tom;
    NextPrayer(&nidx, &nmins, &tom);
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t hm[8], left[32], tip[128], wx[48] = L"";
    FmtHM(tom ? G.tomorrow[P_FAJR] : G.today[nidx], hm, 8);
    FmtLeft(nmins, left, 32);
    if (G.wx && G.w.have) { _snwprintf(wx, 48, L" · %d° %ls", (int)floor(G.w.temp + 0.5), WxText(G.w.code)); wx[47] = 0; }
    _snwprintf(tip, 128, L"%ls%ls\n%ls %ls · in %ls", G.loc.name, wx, PEn(nidx, tom ? (st.wDayOfWeek + 1) % 7 : st.wDayOfWeek), hm, left);
    tip[127] = 0;
    TrayUpdate(G.main, TRAY_ID, G.trayIcon, tip);
    if (old) DestroyIcon(old);
}

/* ── alerts (start-dock.html checkAdhan): a window of 3 minutes, once per
   prayer per day, Shuruq excluded; what fired is written down so a restart
   inside the window does not repeat it. ── */

static void CheckAlerts(void) {
    if (!G.notifyAdhan && G.pre <= 0) return;
    SYSTEMTIME st; GetLocalTime(&st);
    int nm = st.wHour * 60 + st.wMinute;
    wchar_t day[16], seenDay[16], seen[512];
    _snwprintf(day, 16, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay); day[15] = 0;
    G.ini.GetStr(L"alerts_seen", L"day", L"", seenDay, 16);
    G.ini.GetStr(L"alerts_seen", L"ids", L"", seen, 512);
    if (wcscmp(day, seenDay) != 0) seen[0] = 0;

    const int WIN = 3;
    for (int i = 0; i < 6; i++) {
        if (i == P_SUNRISE) continue;
        int t = G.today[i];
        if (t < 0) continue;
        const wchar_t *kind = NULL;
        if (G.notifyAdhan && nm >= t && nm < t + WIN) kind = L"adhan";
        else if (G.pre > 0 && nm >= t - G.pre && nm < (t < t - G.pre + WIN ? t : t - G.pre + WIN)) kind = L"pre";
        if (!kind) continue;
        wchar_t id[32]; _snwprintf(id, 32, L"|%ls:%ls|", PRAYERS[i].key, kind); id[31] = 0;
        if (wcsstr(seen, id)) continue;

        wchar_t hm[8], title[64], text[256];
        FmtHM(t, hm, 8);
        if (kind[0] == L'a') {
            _snwprintf(title, 64, L"حان وقت صلاة %ls", PAr(i, st.wDayOfWeek));
            _snwprintf(text, 256, L"%ls · %ls · %ls", PEn(i, st.wDayOfWeek), hm, G.loc.name);
        } else {
            _snwprintf(title, 64, L"%ls in %d min", PEn(i, st.wDayOfWeek), t - nm);
            _snwprintf(text, 256, L"اقترب وقت صلاة %ls · %ls · %ls", PAr(i, st.wDayOfWeek), hm, G.loc.name);
        }
        title[63] = 0; text[255] = 0;
        TrayBalloon(G.main, TRAY_ID, title, text, G.sound);

        size_t len = wcslen(seen);
        if (len + wcslen(id) + 1 < 512) wcscat(seen, id);
        G.ini.SetStr(L"alerts_seen", L"day", day);
        G.ini.SetStr(L"alerts_seen", L"ids", seen);
        break;      /* one balloon at a time; the next tick picks up anything else */
    }
}

/* ── fonts ─────────────────────────────────────────────────────────── */

static void MakeFonts(void) {
    static const wchar_t *mono[] = { L"JetBrains Mono", L"IBM Plex Mono", L"Cascadia Mono", L"Consolas",
                                     L"Lucida Console", L"Courier New", NULL };
    static const wchar_t *arab[] = { L"Noto Naskh Arabic", L"Amiri", L"Scheherazade New", L"Segoe UI",
                                     L"Tahoma", NULL };
    G.mono = PickFont(mono);
    G.arab = PickFont(arab);
    double s = G.s;
    G.fCity  = MakeFont(G.mono, 11.5, FW_SEMIBOLD, s);
    G.fTemp  = MakeFont(G.mono, 11.5, FW_NORMAL, s);
    G.fLbl   = MakeFont(G.mono, 9, FW_MEDIUM, s);
    G.fTime  = MakeFont(G.mono, 9.5, FW_NORMAL, s);
    G.fName  = MakeFont(G.mono, 10, FW_NORMAL, s);
    G.fNameB = MakeFont(G.mono, 10, FW_SEMIBOLD, s);
    G.fFoot  = MakeFont(G.mono, 11, FW_NORMAL, s);
    G.fAr    = MakeFont(G.arab, 12, FW_NORMAL, s);
    G.fLeft  = MakeFont(G.mono, 17, FW_NORMAL, s);
}

/* ── flyout window ─────────────────────────────────────────────────── */

static void ShowSettings(void);
static void ShowMenu(POINT pt);

static void FlyoutSize(int *w, int *h) { *w = S(204); *h = S(600); }

static void ShowFlyout(void) {
    if (!G.fly) return;
    if (IsWindowVisible(G.fly)) { SetForegroundWindow(G.fly); return; }
    int w, h; FlyoutSize(&w, &h);
    RECT anchor = TrayIconRect(G.main, TRAY_ID);
    /* never taller than the screen it opens on */
    POINT c = { (anchor.left + anchor.right) / 2, (anchor.top + anchor.bottom) / 2 };
    MONITORINFO mi = {}; mi.cbSize = sizeof mi;
    GetMonitorInfoW(MonitorFromPoint(c, MONITOR_DEFAULTTONEAREST), &mi);
    int maxH = (mi.rcWork.bottom - mi.rcWork.top) - S(24);
    if (h > maxH) h = maxH;
    POINT p = FlyoutPosition(anchor, w, h, S(12));
    SetWindowPos(G.fly, HWND_TOPMOST, p.x, p.y, w, h, SWP_SHOWWINDOW);
    SetForegroundWindow(G.fly);
    SetTimer(G.fly, T_PULSE, 40, NULL);
}

static void HideFlyout(void) {
    if (G.fly && IsWindowVisible(G.fly)) {
        KillTimer(G.fly, T_PULSE);
        ShowWindow(G.fly, SW_HIDE);
        G.flyHiddenAt = GetTickCount();
    }
}

static LRESULT CALLBACK FlyProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        int W = rc.right, H = rc.bottom;
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, W, H);
        HGDIOBJ old = SelectObject(mem, bmp);
        PaintFlyout(mem, W, H);
        BitBlt(dc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_TIMER:
        if (wp == T_PULSE) InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) HideFlyout();
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) HideFlyout();
        else if (wp == 'R') { FetchWeather(TRUE); InvalidateRect(h, NULL, FALSE); }
        else if (wp == 'S' || wp == 'C') { HideFlyout(); ShowSettings(); }
        return 0;
    case WM_SETCURSOR: {
        POINT pt; GetCursorPos(&pt); ScreenToClient(h, &pt);
        SetCursor(LoadCursor(NULL, PtInRect(&g_headerHit, pt) ? IDC_HAND : IDC_ARROW));
        return TRUE;
    }
    case WM_LBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (PtInRect(&g_headerHit, pt)) { HideFlyout(); ShowSettings(); }
        return 0;
    }
    case WM_RBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ClientToScreen(h, &pt);
        HideFlyout();
        ShowMenu(pt);
        return 0;
    }
    }
    return DefWindowProcW(h, m, wp, lp);
}

/* ── settings window ───────────────────────────────────────────────── */

enum { IDC_CITY = 300, IDC_NAME, IDC_LAT, IDC_LNG, IDC_PRE, IDC_ADHAN, IDC_SOUND, IDC_WX, IDC_AUTO, IDC_OK, IDC_CANCEL };
static HFONT g_uiFont;

static HWND Ctl(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id, DWORD ex = 0) {
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h),
                             parent, (HMENU)(INT_PTR)id, G.inst, NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
    return c;
}

static void SettingsSyncCustom(HWND h) {
    HWND combo = GetDlgItem(h, IDC_CITY);
    int sel = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
    BOOL custom = sel == NCITIES;
    EnableWindow(GetDlgItem(h, IDC_NAME), custom);
    EnableWindow(GetDlgItem(h, IDC_LAT), custom);
    EnableWindow(GetDlgItem(h, IDC_LNG), custom);
    if (!custom && sel >= 0 && sel < NCITIES) {
        const City &c = CITIES[g_sorted[sel]];
        wchar_t b[32];
        SetDlgItemTextW(h, IDC_NAME, c.fr);
        _snwprintf(b, 32, L"%.2f", c.lat); b[31] = 0; SetDlgItemTextW(h, IDC_LAT, b);
        _snwprintf(b, 32, L"%.2f", c.lng); b[31] = 0; SetDlgItemTextW(h, IDC_LNG, b);
    }
}

static BOOL SettingsApply(HWND h) {
    int sel = (int)SendDlgItemMessageW(h, IDC_CITY, CB_GETCURSEL, 0, 0);
    Location loc = G.loc;
    if (sel == NCITIES) {
        wchar_t name[64], la[32], lo[32], *e1, *e2;
        GetDlgItemTextW(h, IDC_NAME, name, 64);
        GetDlgItemTextW(h, IDC_LAT, la, 32);
        GetDlgItemTextW(h, IDC_LNG, lo, 32);
        for (wchar_t *p = la; *p; p++) if (*p == L',') *p = L'.';   /* accept a decimal comma */
        for (wchar_t *p = lo; *p; p++) if (*p == L',') *p = L'.';
        double lat = wcstod(la, &e1), lng = wcstod(lo, &e2);
        if (e1 == la || e2 == lo || lat < -66 || lat > 66 || lng < -180 || lng > 180) {
            MessageBoxW(h, L"Latitude must be between -66 and 66 and longitude between -180 and 180.\n"
                           L"(Beyond ±66° the sun may not reach the Fajr/Isha angles at all.)",
                        APP_NAME, MB_ICONWARNING);
            return FALSE;
        }
        if (!name[0]) wcopy(name, 64, L"My location");
        loc.custom = TRUE; loc.lat = lat; loc.lng = lng;
        wcopy(loc.name, 64, name); loc.ar[0] = 0;
    } else if (sel >= 0) {
        const City &c = CITIES[g_sorted[sel]];
        loc.custom = FALSE; loc.cityId = c.id; loc.lat = c.lat; loc.lng = c.lng;
        wcopy(loc.name, 64, c.fr); wcopy(loc.ar, 64, c.ar);
    }
    BOOL ok; int pre = (int)GetDlgItemInt(h, IDC_PRE, &ok, FALSE);
    if (!ok || pre > 60) pre = G.pre;

    BOOL moved = loc.lat != G.loc.lat || loc.lng != G.loc.lng;
    G.loc = loc;
    G.pre = pre;
    G.notifyAdhan = IsDlgButtonChecked(h, IDC_ADHAN) == BST_CHECKED;
    G.sound = IsDlgButtonChecked(h, IDC_SOUND) == BST_CHECKED;
    BOOL wx = IsDlgButtonChecked(h, IDC_WX) == BST_CHECKED;
    BOOL wxTurnedOn = wx && !G.wx;
    G.wx = wx;
    AutostartSet(RUN_NAME, IsDlgButtonChecked(h, IDC_AUTO) == BST_CHECKED);
    SaveSettings();

    ComputePrayer();
    if (moved) { G.fetchGen++; LoadWeatherCache(); }
    if (moved || wxTurnedOn) FetchWeather(TRUE);
    if (!G.wx) G.net = G.NET_IDLE;
    RefreshTray(TRUE);
    return TRUE;
}

static LRESULT CALLBACK SetProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_CREATE: {
        Ctl(h, L"STATIC", L"City", SS_LEFT, 16, 19, 110, 18, -1);
        HWND combo = Ctl(h, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 130, 15, 254, 320, IDC_CITY);
        int cur = -1;
        for (int i = 0; i < NCITIES; i++) {
            const City &c = CITIES[g_sorted[i]];
            wchar_t b[128]; _snwprintf(b, 128, L"%ls  —  %ls", c.fr, c.ar); b[127] = 0;
            SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)b);
            if (!G.loc.custom && c.id == G.loc.cityId) cur = i;
        }
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"Custom location…");
        SendMessageW(combo, CB_SETCURSEL, G.loc.custom ? NCITIES : cur, 0);

        Ctl(h, L"STATIC", L"Name", SS_LEFT, 16, 53, 110, 18, -1);
        Ctl(h, L"EDIT", G.loc.name, ES_AUTOHSCROLL | WS_TABSTOP, 130, 50, 254, 23, IDC_NAME, WS_EX_CLIENTEDGE);
        Ctl(h, L"STATIC", L"Latitude", SS_LEFT, 16, 85, 110, 18, -1);
        wchar_t b[32];
        _snwprintf(b, 32, L"%.4f", G.loc.lat); b[31] = 0;
        Ctl(h, L"EDIT", b, ES_AUTOHSCROLL | WS_TABSTOP, 130, 82, 90, 23, IDC_LAT, WS_EX_CLIENTEDGE);
        Ctl(h, L"STATIC", L"Longitude", SS_LEFT, 232, 85, 64, 18, -1);
        _snwprintf(b, 32, L"%.4f", G.loc.lng); b[31] = 0;
        Ctl(h, L"EDIT", b, ES_AUTOHSCROLL | WS_TABSTOP, 300, 82, 84, 23, IDC_LNG, WS_EX_CLIENTEDGE);
        Ctl(h, L"STATIC", L"Times use the Moroccan Ministry method (Fajr 19°, Isha 17°, +5 min "
                          L"on Dhuhr and Maghrib) and this PC's clock and time zone.",
            SS_LEFT, 16, 114, 368, 34, -1);

        Ctl(h, L"STATIC", L"Remind before adhan", SS_LEFT, 16, 160, 130, 18, -1);
        _snwprintf(b, 32, L"%d", G.pre); b[31] = 0;
        Ctl(h, L"EDIT", b, ES_NUMBER | WS_TABSTOP, 150, 157, 44, 23, IDC_PRE, WS_EX_CLIENTEDGE);
        Ctl(h, L"STATIC", L"minutes  (0 = off)", SS_LEFT, 202, 160, 180, 18, -1);
        SendDlgItemMessageW(h, IDC_PRE, EM_SETLIMITTEXT, 2, 0);

        Ctl(h, L"BUTTON", L"Notify at the adhan itself", BS_AUTOCHECKBOX | WS_TABSTOP, 16, 190, 368, 20, IDC_ADHAN);
        Ctl(h, L"BUTTON", L"Play the notification sound", BS_AUTOCHECKBOX | WS_TABSTOP, 16, 214, 368, 20, IDC_SOUND);
        Ctl(h, L"BUTTON", L"Live weather (Open-Meteo, one request every 30 min)", BS_AUTOCHECKBOX | WS_TABSTOP, 16, 238, 368, 20, IDC_WX);
        Ctl(h, L"BUTTON", L"Start with Windows", BS_AUTOCHECKBOX | WS_TABSTOP, 16, 262, 368, 20, IDC_AUTO);
        CheckDlgButton(h, IDC_ADHAN, G.notifyAdhan ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_SOUND, G.sound ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_WX, G.wx ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(h, IDC_AUTO, AutostartGet(RUN_NAME) ? BST_CHECKED : BST_UNCHECKED);

        Ctl(h, L"BUTTON", L"OK", BS_DEFPUSHBUTTON | WS_TABSTOP, 212, 302, 84, 28, IDC_OK);
        Ctl(h, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, 300, 302, 84, 28, IDC_CANCEL);
        SettingsSyncCustom(h);
        SetFocus(combo);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_CITY:
            if (HIWORD(wp) == CBN_SELCHANGE) SettingsSyncCustom(h);
            return 0;
        case IDOK: case IDC_OK:
            if (SettingsApply(h)) DestroyWindow(h);
            return 0;
        case IDCANCEL: case IDC_CANCEL:
            DestroyWindow(h);
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)wp, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        G.settings = NULL;
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

static void ShowSettings(void) {
    if (G.settings) { ShowWindow(G.settings, SW_SHOWNORMAL); SetForegroundWindow(G.settings); return; }
    RECT r = { 0, 0, S(400), S(346) };
    DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, ex = WS_EX_CONTROLPARENT | WS_EX_DLGMODALFRAME;
    AdjustWindowRectEx(&r, style, FALSE, ex);
    int w = r.right - r.left, h = r.bottom - r.top;
    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    G.settings = CreateWindowExW(ex, SET_CLASS, APP_NAME L" — Settings", style,
                                 wa.left + (wa.right - wa.left - w) / 2, wa.top + (wa.bottom - wa.top - h) / 2,
                                 w, h, NULL, NULL, G.inst, NULL);
    ShowWindow(G.settings, SW_SHOWNORMAL);
    SetForegroundWindow(G.settings);
}

/* ── context menu ──────────────────────────────────────────────────── */

static void ShowMenu(POINT pt) {
    HMENU cities = CreatePopupMenu();
    for (int i = 0; i < NCITIES; i++) {
        const City &c = CITIES[g_sorted[i]];
        UINT f = MF_STRING;
        if (!G.loc.custom && c.id == G.loc.cityId) f |= MF_CHECKED;
        if (i && i % 20 == 0) f |= MF_MENUBARBREAK;    /* three short columns, not one tall list */
        AppendMenuW(cities, f, ID_CITY0 + g_sorted[i], c.fr);
    }
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_SHOW, L"Show day spine");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)cities, L"City");
    AppendMenuW(m, MF_STRING, ID_SETTINGS, L"Settings…");
    AppendMenuW(m, MF_STRING | (G.wx ? 0 : MF_GRAYED), ID_REFRESH, L"Refresh weather");
    AppendMenuW(m, MF_STRING, ID_DOCK, L"Open Start Dock page");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING | (AutostartGet(RUN_NAME) ? MF_CHECKED : 0), ID_AUTOSTART, L"Start with Windows");
    AppendMenuW(m, MF_STRING, ID_EXIT, L"Exit");
    SetMenuDefaultItem(m, ID_SHOW, FALSE);

    SetForegroundWindow(G.main);    /* or the menu will not close on an outside click */
    UINT flags = TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY;
    flags |= GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    UINT cmd = (UINT)TrackPopupMenu(m, flags, pt.x, pt.y, 0, G.main, NULL);
    PostMessageW(G.main, WM_NULL, 0, 0);
    DestroyMenu(m);     /* destroys the submenu too */
    if (cmd) SendMessageW(G.main, WM_COMMAND, cmd, 0);
}

static void SelectCity(int idx) {
    if (idx < 0 || idx >= NCITIES) return;
    const City &c = CITIES[idx];
    G.loc.custom = FALSE; G.loc.cityId = c.id; G.loc.lat = c.lat; G.loc.lng = c.lng;
    wcopy(G.loc.name, 64, c.fr); wcopy(G.loc.ar, 64, c.ar);
    SaveSettings();
    ComputePrayer();
    G.fetchGen++;
    LoadWeatherCache();
    FetchWeather(TRUE);
    RefreshTray(TRUE);
}

/* ── main window (hidden; owns the tray icon) ──────────────────────── */

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == G.msgTaskbarCreated && m) {    /* Explorer restarted: put the icon back */
        TrayAdd(h, TRAY_ID, WM_TRAY, G.trayIcon, APP_NAME);
        RefreshTray(TRUE);
        return 0;
    }
    switch (m) {
    case WM_TRAY:
        switch (LOWORD(lp)) {
        case WM_LBUTTONUP:
            if (IsWindowVisible(G.fly)) HideFlyout();
            else if (GetTickCount() - G.flyHiddenAt > 250) ShowFlyout();   /* the click that just closed it */
            break;
        case WM_RBUTTONUP: case WM_CONTEXTMENU: {
            POINT pt; GetCursorPos(&pt);
            ShowMenu(pt);
            break;
        }
        case NIN_BALLOONUSERCLICK:
            ShowFlyout();
            break;
        }
        return 0;
    case WM_POKE:
        if (wp == 1) ShowSettings(); else ShowFlyout();
        return 0;
    case WM_TIMER:
        if (wp == T_TICK) {
            if (Today(NULL) != G.dayStamp) { ComputePrayer(); RefreshTray(TRUE); }
            RefreshTray(FALSE);
            CheckAlerts();
            if (IsWindowVisible(G.fly)) InvalidateRect(G.fly, NULL, FALSE);
        } else if (wp == T_WEATHER) {
            FetchWeather(FALSE);
        }
        return 0;
    case WM_WEATHER: {
        Weather *w = (Weather *)lp;
        InterlockedExchange(&G.fetching, 0);
        if ((int)wp != G.fetchGen) { free(w); FetchWeather(TRUE); return 0; }   /* city changed meanwhile */
        if (w) {
            G.w = *w;
            LocKey(G.w.key, 96);
            SaveWeatherCache();
            G.net = G.NET_OK;
            free(w);
            SetTimer(h, T_WEATHER, 30 * 60 * 1000, NULL);
        } else {
            G.net = G.NET_OFF;      /* keep showing the last reading */
            SetTimer(h, T_WEATHER, 5 * 60 * 1000, NULL);
        }
        RefreshTray(TRUE);
        if (IsWindowVisible(G.fly)) InvalidateRect(G.fly, NULL, FALSE);
        return 0;
    }
    case WM_TIMECHANGE:
        ComputePrayer();
        RefreshTray(TRUE);
        return 0;
    case WM_POWERBROADCAST:
        if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) {
            ComputePrayer();
            RefreshTray(TRUE);
            FetchWeather(FALSE);
        }
        return TRUE;
    case WM_COMMAND: {
        UINT id = LOWORD(wp);
        if (id >= ID_CITY0 && id < ID_CITY0 + (UINT)NCITIES) { SelectCity((int)(id - ID_CITY0)); return 0; }
        switch (id) {
        case ID_SHOW: ShowFlyout(); break;
        case ID_SETTINGS: ShowSettings(); break;
        case ID_REFRESH: FetchWeather(TRUE); RefreshTray(TRUE); break;
        case ID_DOCK:
            if (!OpenEmbeddedPage(IDR_PAGE, L"start-dock.html"))
                MessageBoxW(NULL, L"Could not open the Start Dock page.", APP_NAME, MB_ICONWARNING);
            break;
        case ID_AUTOSTART: AutostartSet(RUN_NAME, !AutostartGet(RUN_NAME)); break;
        case ID_EXIT: DestroyWindow(h); break;
        }
        return 0;
    }
    case WM_DESTROY:
        TrayRemove(h, TRAY_ID);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

static int CmpCity(const void *a, const void *b) {
    return CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE | NORM_IGNORENONSPACE,
                          CITIES[*(const int *)a].fr, -1, CITIES[*(const int *)b].fr, -1) - 2;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR cmd, int) {
    /* --show opens the flyout, --settings the settings window: handy as a
       pinned shortcut. A second launch hands either to the running copy. */
    BOOL showNow = cmd && (wcsstr(cmd, L"--show") || wcsstr(cmd, L"/show"));
    BOOL settingsNow = cmd && (wcsstr(cmd, L"--settings") || wcsstr(cmd, L"/settings"));
    if (AlreadyRunning(L"Local\\AchmaPrayerTray.Single", WND_CLASS, WM_POKE, settingsNow ? 1 : 0)) return 0;

    EnableDpiAwareness();
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    GdiplusStartupInput gsi;
    GdiplusStartup(&G.gdipToken, &gsi, NULL);

    G.inst = inst;
    G.s = SystemDpi() / 96.0;
    G.ini.Open(L"prayer-tray.ini");
    for (int i = 0; i < NCITIES; i++) g_sorted[i] = i;
    qsort(g_sorted, NCITIES, sizeof(int), CmpCity);
    LoadSettings();
    LoadWeatherCache();
    MakeFonts();
    g_uiFont = MessageFont();
    G.lastIconMinute = -1;

    HICON big = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    HICON small = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);

    WNDCLASSEXW wc = { sizeof wc };
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = big; wc.hIconSm = small;

    wc.lpfnWndProc = MainProc; wc.lpszClassName = WND_CLASS;
    RegisterClassExW(&wc);

    wc.lpfnWndProc = FlyProc; wc.lpszClassName = FLY_CLASS;
    wc.style = CS_DROPSHADOW;
    wc.hbrBackground = NULL;
    RegisterClassExW(&wc);

    wc.lpfnWndProc = SetProc; wc.lpszClassName = SET_CLASS;
    wc.style = 0;
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    RegisterClassExW(&wc);

    G.main = CreateWindowExW(0, WND_CLASS, APP_NAME, 0, 0, 0, 0, 0, NULL, NULL, inst, NULL);
    G.fly = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, FLY_CLASS, APP_NAME, WS_POPUP,
                            0, 0, S(204), S(600), G.main, NULL, inst, NULL);
    RoundCorners(G.fly);
    G.msgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    ComputePrayer();
    G.trayIcon = MakeTrayIcon(GetSystemMetrics(SM_CXSMICON));
    TrayAdd(G.main, TRAY_ID, WM_TRAY, G.trayIcon, APP_NAME);
    RefreshTray(TRUE);
    G.net = G.wx ? (G.w.have ? G.NET_OK : G.NET_WAIT) : G.NET_IDLE;
    FetchWeather(FALSE);

    SetTimer(G.main, T_TICK, 10 * 1000, NULL);
    SetTimer(G.main, T_WEATHER, 30 * 60 * 1000, NULL);
    CheckAlerts();
    if (settingsNow) ShowSettings();
    else if (showNow) ShowFlyout();

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (G.settings && IsDialogMessageW(G.settings, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (G.trayIcon) DestroyIcon(G.trayIcon);
    GdiplusShutdown(G.gdipToken);
    return 0;
}
