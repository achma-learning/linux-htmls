/* ai-hub-tray.cpp — the AI Hub, one click from the notification area.
 *
 * The same assistants as "html file/ai-hub.html", as a native menu:
 *
 *   Left click      → the launcher: favourites first, then every assistant
 *   Ctrl+Alt+A      → your default assistant, from anywhere
 *   Right click     → default assistant, favourites, app-window mode,
 *                     left-click behaviour, hotkey, custom assistants,
 *                     start with Windows
 *
 * "App window" opens the chatbot through a Chromium browser's --app= mode
 * (Edge, Chrome, Brave, Vivaldi): its own window and taskbar button, no tabs
 * or address bar — the Windows twin of bin/ai-open on the Linux side. With
 * no Chromium browser installed it simply opens a tab in the default browser.
 *
 * Your own assistants go in %APPDATA%\AchmaTray\ai-hub-tray.ini:
 *     [custom]
 *     My GPT=https://chatgpt.com/g/g-xxxx
 */
#include "common.h"
#include <objidl.h>
#include <algorithm>
#include <gdiplus.h>
#include <shlwapi.h>
#include "resource.h"

using namespace Gdiplus;

#define APP_NAME   L"AI Hub"
#define RUN_NAME   L"AchmaAIHubTray"
#define WND_CLASS  L"AchmaAIHubTray.Main"
#define TRAY_ID    1
#define WM_TRAY    (WM_APP + 1)
#define WM_POKE    (WM_APP + 9)
#define HOTKEY_ID  1

/* ── the assistants (ai-hub.html SERVICES, same order) ─────────────── */
struct Service { wchar_t id[32], name[64], provider[48], url[512]; COLORREF color; BOOL fav; };

static const struct { const wchar_t *id, *name, *provider, *url; COLORREF color; BOOL fav; } BUILTIN[] = {
 {L"chatgpt",    L"ChatGPT",     L"OpenAI",       L"https://chatgpt.com/",            RGB(0x10,0xa3,0x7f), TRUE },
 {L"claude",     L"Claude",      L"Anthropic",    L"https://claude.ai/new",           RGB(0xd9,0x77,0x57), TRUE },
 {L"gemini",     L"Gemini",      L"Google",       L"https://gemini.google.com/",      RGB(0x42,0x85,0xf4), TRUE },
 {L"deepseek",   L"DeepSeek",    L"DeepSeek",     L"https://chat.deepseek.com/",      RGB(0x4d,0x78,0xff), TRUE },
 {L"perplexity", L"Perplexity",  L"Perplexity",   L"https://www.perplexity.ai/",      RGB(0x20,0xb8,0xcd), TRUE },
 {L"notebooklm", L"NotebookLM",  L"Google",       L"https://notebooklm.google.com/",  RGB(0x5f,0x63,0x68), FALSE},
 {L"grok",       L"Grok",        L"xAI",          L"https://grok.com/",               RGB(0x8a,0x8f,0x9c), FALSE},
 {L"kimi",       L"Kimi",        L"Moonshot AI",  L"https://www.kimi.com/",           RGB(0x69,0x5c,0xff), FALSE},
 {L"mistral",    L"Le Chat",     L"Mistral AI",   L"https://chat.mistral.ai/",        RGB(0xff,0x70,0x00), FALSE},
 {L"qwen",       L"Qwen",        L"Alibaba",      L"https://chat.qwen.ai/",           RGB(0x62,0x57,0xee), FALSE},
 {L"aistudio",   L"AI Studio",   L"Google",       L"https://aistudio.google.com/",    RGB(0x1a,0x73,0xe8), FALSE},
 {L"copilot",    L"Copilot",     L"Microsoft",    L"https://copilot.microsoft.com/",  RGB(0x36,0xa5,0xff), FALSE},
 {L"metaai",     L"Meta AI",     L"Meta",         L"https://www.meta.ai/",            RGB(0x08,0x7c,0xff), FALSE},
 {L"huggingchat",L"HuggingChat", L"Hugging Face", L"https://huggingface.co/chat/",    RGB(0xff,0x9d,0x00), FALSE},
 {L"poe",        L"Poe",         L"Quora",        L"https://poe.com/",                RGB(0x8b,0x5c,0xf6), FALSE},
};
static const int NBUILTIN = (int)(sizeof BUILTIN / sizeof BUILTIN[0]);

#define MAX_SERVICES 64
static Service g_svc[MAX_SERVICES];
static int g_nsvc;
static HBITMAP g_bmp[MAX_SERVICES];

/* command ids */
enum {
    ID_OPEN_DEFAULT = 100, ID_HUBPAGE, ID_APPWIN, ID_HOTKEY, ID_LEFT_LIST, ID_LEFT_DEFAULT,
    ID_CUSTOM, ID_AUTOSTART, ID_EXIT,
    ID_LAUNCH0 = 1000, ID_DEFAULT0 = 2000, ID_FAV0 = 3000,
};

static struct {
    HINSTANCE inst;
    HWND main;
    Ini ini;
    UINT msgTaskbarCreated;
    double s;
    HICON icon;
    wchar_t def[32];        /* default assistant id */
    BOOL appWindow, hotkey, leftOpensDefault;
    BOOL hotkeyOk;
    ULONG_PTR gdip;
} G;

static inline int S(double v) { return (int)floor(v * G.s + 0.5); }

/* ── settings ──────────────────────────────────────────────────────── */

static int FindService(const wchar_t *id) {
    for (int i = 0; i < g_nsvc; i++) if (_wcsicmp(g_svc[i].id, id) == 0) return i;
    return -1;
}

static BOOL InList(const wchar_t *list, const wchar_t *id) {
    wchar_t pat[40]; _snwprintf(pat, 40, L",%ls,", id); pat[39] = 0;
    wchar_t wrapped[1100]; _snwprintf(wrapped, 1100, L",%ls,", list); wrapped[1099] = 0;
    return wcsstr(wrapped, pat) != NULL;
}

static void MakeMenuBitmaps(void);

/* Re-read every time a menu opens, so an edit to the .ini shows up without
   a restart. Cheap: one small file. */
static void LoadSettings(void) {
    Ini &I = G.ini;
    g_nsvc = 0;
    wchar_t favs[1024];
    I.GetStr(L"hub", L"favorites", L"\x01", favs, 1024);
    BOOL haveFavs = favs[0] != 1;
    for (int i = 0; i < NBUILTIN && g_nsvc < MAX_SERVICES; i++) {
        Service &s = g_svc[g_nsvc++];
        wcopy(s.id, 32, BUILTIN[i].id); wcopy(s.name, 64, BUILTIN[i].name);
        wcopy(s.provider, 48, BUILTIN[i].provider); wcopy(s.url, 512, BUILTIN[i].url);
        s.color = BUILTIN[i].color;
        s.fav = haveFavs ? InList(favs, s.id) : BUILTIN[i].fav;
    }
    /* [custom] Name=URL — only http(s) URLs are accepted */
    wchar_t sec[8192];
    DWORD n = GetPrivateProfileSectionW(L"custom", sec, 8192, I.path);
    static const COLORREF palette[] = { RGB(0x7c,0x5c,0xff), RGB(0x00,0xb4,0xff), RGB(0xe8,0xa3,0x3d), RGB(0x4f,0xb0,0x9a), RGB(0xc9,0x6a,0x4e) };
    int k = 0;
    for (wchar_t *p = sec; n && *p && g_nsvc < MAX_SERVICES; p += wcslen(p) + 1) {
        if (*p == L';' || *p == L'#') continue;
        wchar_t *eq = wcschr(p, L'=');
        if (!eq) continue;
        *eq = 0;
        const wchar_t *name = p, *url = eq + 1;
        while (*url == L' ') url++;
        if (_wcsnicmp(url, L"https://", 8) && _wcsnicmp(url, L"http://", 7)) { *eq = L'='; continue; }
        Service &s = g_svc[g_nsvc++];
        _snwprintf(s.id, 32, L"custom%d", k); s.id[31] = 0;
        wcopy(s.name, 64, name); wcopy(s.provider, 48, L"custom"); wcopy(s.url, 512, url);
        /* trim trailing spaces from the name */
        for (int j = (int)wcslen(s.name) - 1; j >= 0 && s.name[j] == L' '; j--) s.name[j] = 0;
        s.color = palette[k % 5];
        s.fav = haveFavs ? InList(favs, s.id) : TRUE;
        *eq = L'=';
        k++;
    }
    I.GetStr(L"hub", L"default", L"claude", G.def, 32);
    if (FindService(G.def) < 0) wcopy(G.def, 32, L"claude");
    G.appWindow = I.GetInt(L"hub", L"app_window", 1);
    G.hotkey = I.GetInt(L"hub", L"hotkey", 1);
    G.leftOpensDefault = I.GetInt(L"hub", L"left_click_opens_default", 0);
    MakeMenuBitmaps();
}

static void SaveFavorites(void) {
    wchar_t list[1024] = L"";
    for (int i = 0; i < g_nsvc; i++) {
        if (!g_svc[i].fav) continue;
        if (wcslen(list) + wcslen(g_svc[i].id) + 2 >= 1024) break;
        if (list[0]) wcscat(list, L",");
        wcscat(list, g_svc[i].id);
    }
    G.ini.SetStr(L"hub", L"favorites", list);
}

/* ── browser: find a Chromium for --app= windows ───────────────────── */

static BOOL AppPath(const wchar_t *exe, wchar_t *out) {
    wchar_t key[200];
    _snwprintf(key, 200, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\%ls", exe); key[199] = 0;
    HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    for (HKEY root : roots) {
        DWORD size = MAX_PATH * sizeof(wchar_t), type = 0;
        if (RegGetValueW(root, key, NULL, RRF_RT_REG_SZ, &type, out, &size) == ERROR_SUCCESS &&
            GetFileAttributesW(out) != INVALID_FILE_ATTRIBUTES)
            return TRUE;
    }
    return FALSE;
}

/* Prefer the default browser if it is a Chromium; else Edge, Chrome, Brave. */
static BOOL FindAppBrowser(wchar_t *out) {
    wchar_t def[MAX_PATH]; DWORD n = MAX_PATH;
    if (SUCCEEDED(AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_EXECUTABLE, L"http", L"open", def, &n))) {
        const wchar_t *base = wcsrchr(def, L'\\'); base = base ? base + 1 : def;
        static const wchar_t *chromium[] = { L"msedge.exe", L"chrome.exe", L"brave.exe", L"vivaldi.exe", L"chromium.exe", NULL };
        for (int i = 0; chromium[i]; i++)
            if (_wcsicmp(base, chromium[i]) == 0 && GetFileAttributesW(def) != INVALID_FILE_ATTRIBUTES) {
                wcopy(out, MAX_PATH, def);
                return TRUE;
            }
    }
    return AppPath(L"msedge.exe", out) || AppPath(L"chrome.exe", out) || AppPath(L"brave.exe", out);
}

static void Launch(int i) {
    if (i < 0 || i >= g_nsvc) return;
    const wchar_t *url = g_svc[i].url;
    if (G.appWindow) {
        wchar_t exe[MAX_PATH], args[600];
        if (FindAppBrowser(exe)) {
            _snwprintf(args, 600, L"--app=\"%ls\"", url); args[599] = 0;
            if ((INT_PTR)ShellExecuteW(NULL, L"open", exe, args, NULL, SW_SHOWNORMAL) > 32) return;
        }
    }
    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
}

/* ── icons ─────────────────────────────────────────────────────────── */

static void RoundRectPath(GraphicsPath &p, float x, float y, float w, float h, float r) {
    p.AddArc(x, y, 2 * r, 2 * r, 180, 90);
    p.AddArc(x + w - 2 * r, y, 2 * r, 2 * r, 270, 90);
    p.AddArc(x + w - 2 * r, y + h - 2 * r, 2 * r, 2 * r, 0, 90);
    p.AddArc(x, y + h - 2 * r, 2 * r, 2 * r, 90, 90);
    p.CloseFigure();
}

/* A 32-bpp premultiplied DIB, the only kind a menu draws with alpha. */
static HBITMAP ToMenuBitmap(Bitmap &src, int size) {
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = size; bi.bmiHeader.biHeight = -size;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    HBITMAP h = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!h) return NULL;
    BitmapData d;
    Rect r(0, 0, size, size);
    if (src.LockBits(&r, ImageLockModeRead, PixelFormat32bppPARGB, &d) == Ok) {
        for (int y = 0; y < size; y++)
            memcpy((BYTE *)bits + y * size * 4, (BYTE *)d.Scan0 + y * d.Stride, size * 4);
        src.UnlockBits(&d);
    }
    return h;
}

/* Brand-coloured tile with the assistant's initial — what the hub page shows. */
static HBITMAP ServiceBitmap(const Service &s, int size) {
    Bitmap b(size, size, PixelFormat32bppPARGB);
    Graphics g(&b);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.Clear(Color(0, 0, 0, 0));
    GraphicsPath p;
    RoundRectPath(p, 0.5f, 0.5f, size - 1.0f, size - 1.0f, size * 0.26f);
    SolidBrush fill(Color(255, GetRValue(s.color), GetGValue(s.color), GetBValue(s.color)));
    g.FillPath(&fill, &p);
    wchar_t ch[2] = { s.name[0], 0 };
    FontFamily fam(L"Segoe UI");
    Font f(fam.IsAvailable() ? L"Segoe UI" : L"Tahoma", size * 0.62f, FontStyleBold, UnitPixel);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    SolidBrush white(Color(255, 255, 255, 255));
    g.DrawString(ch, 1, &f, RectF(0, size * 0.03f, (float)size, (float)size), &sf, &white);
    return ToMenuBitmap(b, size);
}

static void MakeMenuBitmaps(void) {
    for (int i = 0; i < MAX_SERVICES; i++) if (g_bmp[i]) { DeleteObject(g_bmp[i]); g_bmp[i] = NULL; }
    int size = GetSystemMetrics(SM_CYMENUCHECK);
    if (size < 13) size = 13;
    for (int i = 0; i < g_nsvc; i++) g_bmp[i] = ServiceBitmap(g_svc[i], size);
}

/* The tray icon: violet→blue tile with a four-point spark (the hub's ✦). */
static HICON MakeTrayIcon(int size) {
    Bitmap b(size, size, PixelFormat32bppARGB);
    Graphics g(&b);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(0, 0, 0, 0));
    float z = (float)size;
    GraphicsPath p;
    RoundRectPath(p, 0.5f, 0.5f, z - 1, z - 1, z * 0.28f);
    LinearGradientBrush lg(PointF(0, 0), PointF(z, z), Color(255, 0x7c, 0x5c, 0xff), Color(255, 0x4f, 0x8c, 0xff));
    g.FillPath(&lg, &p);
    float c = z / 2, R = z * 0.34f, r = z * 0.085f;
    PointF star[8] = { PointF(c, c - R), PointF(c + r, c - r), PointF(c + R, c), PointF(c + r, c + r),
                       PointF(c, c + R), PointF(c - r, c + r), PointF(c - R, c), PointF(c - r, c - r) };
    SolidBrush w(Color(255, 255, 255, 255));
    g.FillPolygon(&w, star, 8);
    HICON ic = NULL;
    b.GetHICON(&ic);
    return ic;
}

/* ── hotkey ────────────────────────────────────────────────────────── */

static void ApplyHotkey(BOOL announceFailure) {
    UnregisterHotKey(G.main, HOTKEY_ID);
    G.hotkeyOk = FALSE;
    if (!G.hotkey) return;
    /* MOD_NOREPEAT is Windows 7+; holding the keys must not open ten windows */
    G.hotkeyOk = RegisterHotKey(G.main, HOTKEY_ID, MOD_CONTROL | MOD_ALT | 0x4000, 'A');
    if (!G.hotkeyOk && announceFailure)
        TrayBalloon(G.main, TRAY_ID, L"Ctrl+Alt+A is taken",
                    L"Another program already uses Ctrl+Alt+A. The tray menu still works.", FALSE);
}

static void UpdateTip(void) {
    int d = FindService(G.def);
    wchar_t tip[128];
    if (G.hotkey && G.hotkeyOk) _snwprintf(tip, 128, L"AI Hub — click to launch\nCtrl+Alt+A: %ls", d >= 0 ? g_svc[d].name : L"");
    else _snwprintf(tip, 128, L"AI Hub — click to launch");
    tip[127] = 0;
    TrayUpdate(G.main, TRAY_ID, NULL, tip);
}

/* ── menus ─────────────────────────────────────────────────────────── */

static void AddItem(HMENU m, UINT id, const wchar_t *text, int svc, UINT state) {
    MENUITEMINFOW mi = {};
    mi.cbSize = sizeof mi;
    mi.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE | MIIM_FTYPE | (svc >= 0 && g_bmp[svc] ? MIIM_BITMAP : 0);
    mi.fType = MFT_STRING;
    mi.wID = id;
    mi.dwTypeData = (LPWSTR)text;
    mi.fState = state;
    if (svc >= 0) mi.hbmpItem = g_bmp[svc];
    InsertMenuItemW(m, GetMenuItemCount(m), TRUE, &mi);
}

static void AddLabel(HMENU m, const wchar_t *text) {
    AddItem(m, 0, text, -1, MFS_DISABLED);
}

static UINT Track(HMENU m, POINT pt) {
    SetForegroundWindow(G.main);    /* or the menu will not close on an outside click */
    UINT flags = TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY;
    flags |= GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    UINT cmd = (UINT)TrackPopupMenu(m, flags, pt.x, pt.y, 0, G.main, NULL);
    PostMessageW(G.main, WM_NULL, 0, 0);
    return cmd;
}

/* Left click: favourites, then everything else. */
static void ShowLauncher(POINT pt) {
    LoadSettings();
    HMENU m = CreatePopupMenu();
    int d = FindService(G.def);
    BOOL anyFav = FALSE;
    for (int i = 0; i < g_nsvc; i++) {
        if (!g_svc[i].fav) continue;
        AddItem(m, ID_LAUNCH0 + i, g_svc[i].name, i, i == d ? MFS_DEFAULT : 0);
        anyFav = TRUE;
    }
    HMENU all = CreatePopupMenu();
    for (int i = 0; i < g_nsvc; i++) {
        wchar_t t[128]; _snwprintf(t, 128, L"%ls\t%ls", g_svc[i].name, g_svc[i].provider); t[127] = 0;
        AddItem(all, ID_LAUNCH0 + i, t, i, i == d ? MFS_DEFAULT : 0);
    }
    if (anyFav) AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)all, anyFav ? L"All assistants" : L"Assistants");
    AppendMenuW(m, MF_STRING, ID_HUBPAGE, L"Open AI Hub page");
    UINT cmd = Track(m, pt);
    DestroyMenu(m);
    if (cmd) SendMessageW(G.main, WM_COMMAND, cmd, 0);
}

/* Right click: settings. */
static void ShowOptions(POINT pt) {
    LoadSettings();
    int d = FindService(G.def);
    wchar_t exe[MAX_PATH];
    BOOL chromium = FindAppBrowser(exe);

    HMENU defs = CreatePopupMenu(), favs = CreatePopupMenu();
    for (int i = 0; i < g_nsvc; i++) {
        /* no brand tiles here: a tile would sit where the tick goes and hide it */
        AddItem(defs, ID_DEFAULT0 + i, g_svc[i].name, -1, 0);
        AddItem(favs, ID_FAV0 + i, g_svc[i].name, -1, g_svc[i].fav ? MFS_CHECKED : 0);
    }
    /* radio bullets instead of ticks for the one-of-many list */
    if (d >= 0) CheckMenuRadioItem(defs, ID_DEFAULT0, ID_DEFAULT0 + g_nsvc - 1, ID_DEFAULT0 + d, MF_BYCOMMAND);

    HMENU left = CreatePopupMenu();
    AppendMenuW(left, MF_STRING, ID_LEFT_LIST, L"Shows the launcher list");
    wchar_t t[160];
    _snwprintf(t, 160, L"Opens %ls directly", d >= 0 ? g_svc[d].name : L"the default"); t[159] = 0;
    AppendMenuW(left, MF_STRING, ID_LEFT_DEFAULT, t);
    CheckMenuRadioItem(left, ID_LEFT_LIST, ID_LEFT_DEFAULT, G.leftOpensDefault ? ID_LEFT_DEFAULT : ID_LEFT_LIST, MF_BYCOMMAND);

    HMENU m = CreatePopupMenu();
    _snwprintf(t, 160, L"Open %ls%ls", d >= 0 ? g_svc[d].name : L"", G.hotkey && G.hotkeyOk ? L"\tCtrl+Alt+A" : L"");
    t[159] = 0;
    AddItem(m, ID_OPEN_DEFAULT, t, d, MFS_DEFAULT);
    AppendMenuW(m, MF_STRING, ID_HUBPAGE, L"Open AI Hub page");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AddLabel(m, L"Settings");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)defs, L"Default assistant");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)favs, L"Favourites");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)left, L"Left click");
    AppendMenuW(m, MF_STRING | (chromium ? (G.appWindow ? MF_CHECKED : 0) : MF_GRAYED), ID_APPWIN,
                chromium ? L"Open as app window (no tabs)" : L"Open as app window (needs Edge or Chrome)");
    AppendMenuW(m, MF_STRING | (G.hotkey ? MF_CHECKED : 0), ID_HOTKEY,
                G.hotkey && !G.hotkeyOk ? L"Hotkey Ctrl+Alt+A (taken by another app)" : L"Hotkey Ctrl+Alt+A");
    AppendMenuW(m, MF_STRING, ID_CUSTOM, L"Add your own assistants…");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING | (AutostartGet(RUN_NAME) ? MF_CHECKED : 0), ID_AUTOSTART, L"Start with Windows");
    AppendMenuW(m, MF_STRING, ID_EXIT, L"Exit");
    UINT cmd = Track(m, pt);
    DestroyMenu(m);
    if (cmd) SendMessageW(G.main, WM_COMMAND, cmd, 0);
}

/* Open the .ini in Notepad, with a commented example the first time. */
static void EditCustom(void) {
    wchar_t probe[8];
    if (GetPrivateProfileSectionW(L"custom", probe, 8, G.ini.path) == 0) {
        /* written as commented-out lines: delete the leading ; to enable one */
        WritePrivateProfileStringW(L"custom", L"; Claude Projects", L"https://claude.ai/projects", G.ini.path);
        WritePrivateProfileStringW(L"custom", L"; ChatGPT temporary chat", L"https://chatgpt.com/?temporary-chat=true", G.ini.path);
    }
    ShellExecuteW(NULL, L"open", L"notepad.exe", G.ini.path, NULL, SW_SHOWNORMAL);
}

/* ── main window ───────────────────────────────────────────────────── */

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == G.msgTaskbarCreated && m) {
        TrayAdd(h, TRAY_ID, WM_TRAY, G.icon, APP_NAME);
        UpdateTip();
        return 0;
    }
    switch (m) {
    case WM_TRAY:
        switch (LOWORD(lp)) {
        case WM_LBUTTONUP: {
            POINT pt; GetCursorPos(&pt);
            if (G.leftOpensDefault) { LoadSettings(); Launch(FindService(G.def)); }
            else ShowLauncher(pt);
            break;
        }
        case WM_RBUTTONUP: case WM_CONTEXTMENU: {
            POINT pt; GetCursorPos(&pt);
            ShowOptions(pt);
            break;
        }
        }
        return 0;
    case WM_HOTKEY:
        if (wp == HOTKEY_ID) { LoadSettings(); Launch(FindService(G.def)); }
        return 0;
    case WM_POKE: {     /* launched again while running: open the launcher where the mouse is */
        POINT pt; GetCursorPos(&pt);
        ShowLauncher(pt);
        return 0;
    }
    case WM_COMMAND: {
        UINT id = LOWORD(wp);
        if (id >= ID_LAUNCH0 && id < ID_LAUNCH0 + MAX_SERVICES) { Launch(id - ID_LAUNCH0); return 0; }
        if (id >= ID_DEFAULT0 && id < ID_DEFAULT0 + MAX_SERVICES) {
            int i = id - ID_DEFAULT0;
            if (i < g_nsvc) { G.ini.SetStr(L"hub", L"default", g_svc[i].id); wcopy(G.def, 32, g_svc[i].id); UpdateTip(); }
            return 0;
        }
        if (id >= ID_FAV0 && id < ID_FAV0 + MAX_SERVICES) {
            int i = id - ID_FAV0;
            if (i < g_nsvc) { g_svc[i].fav = !g_svc[i].fav; SaveFavorites(); }
            return 0;
        }
        switch (id) {
        case ID_OPEN_DEFAULT: Launch(FindService(G.def)); break;
        case ID_HUBPAGE:
            if (!OpenEmbeddedPage(IDR_PAGE, L"ai-hub.html"))
                MessageBoxW(NULL, L"Could not open the AI Hub page.", APP_NAME, MB_ICONWARNING);
            break;
        case ID_APPWIN: G.appWindow = !G.appWindow; G.ini.SetInt(L"hub", L"app_window", G.appWindow); break;
        case ID_HOTKEY:
            G.hotkey = !G.hotkey; G.ini.SetInt(L"hub", L"hotkey", G.hotkey);
            ApplyHotkey(TRUE); UpdateTip();
            break;
        case ID_LEFT_LIST: G.leftOpensDefault = FALSE; G.ini.SetInt(L"hub", L"left_click_opens_default", 0); break;
        case ID_LEFT_DEFAULT: G.leftOpensDefault = TRUE; G.ini.SetInt(L"hub", L"left_click_opens_default", 1); break;
        case ID_CUSTOM: EditCustom(); break;
        case ID_AUTOSTART: AutostartSet(RUN_NAME, !AutostartGet(RUN_NAME)); break;
        case ID_EXIT: DestroyWindow(h); break;
        }
        return 0;
    }
    case WM_DESTROY:
        UnregisterHotKey(h, HOTKEY_ID);
        TrayRemove(h, TRAY_ID);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int) {
    if (AlreadyRunning(L"Local\\AchmaAIHubTray.Single", WND_CLASS, WM_POKE)) return 0;
    EnableDpiAwareness();
    GdiplusStartupInput gsi;
    GdiplusStartup(&G.gdip, &gsi, NULL);

    G.inst = inst;
    G.s = SystemDpi() / 96.0;
    G.ini.Open(L"ai-hub-tray.ini");
    LoadSettings();

    WNDCLASSEXW wc = { sizeof wc };
    wc.hInstance = inst;
    wc.lpfnWndProc = MainProc;
    wc.lpszClassName = WND_CLASS;
    wc.hIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    RegisterClassExW(&wc);
    G.main = CreateWindowExW(0, WND_CLASS, APP_NAME, 0, 0, 0, 0, 0, NULL, NULL, inst, NULL);
    G.msgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    G.icon = MakeTrayIcon(GetSystemMetrics(SM_CXSMICON));
    TrayAdd(G.main, TRAY_ID, WM_TRAY, G.icon, APP_NAME);
    ApplyHotkey(TRUE);
    UpdateTip();

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (G.icon) DestroyIcon(G.icon);
    for (int i = 0; i < MAX_SERVICES; i++) if (g_bmp[i]) DeleteObject(g_bmp[i]);
    GdiplusShutdown(G.gdip);
    return 0;
}
