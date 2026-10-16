/* common.h — shared plumbing for the two tray apps.
 *
 * Everything here is plain Win32 that exists on Windows 7 SP1 and later.
 * Anything newer (Shell_NotifyIconGetRect is 7+, DwmSetWindowAttribute corner
 * rounding is 11+, SetProcessDPIAware is Vista+) is looked up at run time with
 * GetProcAddress, so a missing export degrades a feature instead of refusing
 * to start the program.
 *
 * Settings live in %APPDATA%\AchmaTray\<app>.ini — a plain text file the user
 * can read, back up or delete. No registry, except the one optional
 * HKCU\...\Run value that "Start with Windows" writes.
 */
#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601   /* Windows 7 */
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <math.h>

/* ── tiny string helpers ───────────────────────────────────────────── */

static inline void wcopy(wchar_t *dst, size_t cap, const wchar_t *src) {
    if (!cap) return;
    size_t i = 0;
    for (; src && src[i] && i + 1 < cap; i++) dst[i] = src[i];
    dst[i] = 0;
}

/* UTF-8 → UTF-16, into a caller buffer. */
static inline void u8to16(const char *s, wchar_t *out, int cap) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, out, cap);
    if (n <= 0) out[0] = 0;
}

/* ── paths ─────────────────────────────────────────────────────────── */

static inline void ExePath(wchar_t *out, DWORD cap) {
    DWORD n = GetModuleFileNameW(NULL, out, cap);
    if (n == 0 || n >= cap) out[0] = 0;
}

/* %APPDATA%\AchmaTray, created on first use. */
static inline BOOL DataDir(wchar_t *out, size_t cap) {
    wchar_t base[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, base))) {
        /* No profile folder (rare: a locked-down kiosk). Fall back to the exe's folder. */
        ExePath(base, MAX_PATH);
        wchar_t *slash = wcsrchr(base, L'\\');
        if (slash) *slash = 0;
    }
    _snwprintf(out, cap, L"%ls\\AchmaTray", base);
    out[cap - 1] = 0;
    CreateDirectoryW(out, NULL);
    return TRUE;
}

/* ── INI settings ──────────────────────────────────────────────────── */

struct Ini {
    wchar_t path[MAX_PATH];

    void Open(const wchar_t *file) {
        wchar_t dir[MAX_PATH];
        DataDir(dir, MAX_PATH);
        _snwprintf(path, MAX_PATH, L"%ls\\%ls", dir, file);
        path[MAX_PATH - 1] = 0;
        /* Write a UTF-16LE BOM into a brand-new file. The Profile API then
           keeps the whole file in UTF-16, so Arabic city names round-trip
           instead of being squeezed through the ANSI code page. */
        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
            HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                DWORD w; const WORD bom = 0xFEFF;
                WriteFile(h, &bom, 2, &w, NULL);
                CloseHandle(h);
            }
        }
    }
    int GetInt(const wchar_t *sec, const wchar_t *key, int def) const {
        return (int)GetPrivateProfileIntW(sec, key, def, path);
    }
    void GetStr(const wchar_t *sec, const wchar_t *key, const wchar_t *def, wchar_t *out, DWORD cap) const {
        GetPrivateProfileStringW(sec, key, def, out, cap, path);
    }
    double GetDouble(const wchar_t *sec, const wchar_t *key, double def) const {
        wchar_t b[64];
        GetPrivateProfileStringW(sec, key, L"", b, 64, path);
        if (!b[0]) return def;
        wchar_t *end = NULL;
        double v = wcstod(b, &end);
        return (end == b) ? def : v;
    }
    void SetInt(const wchar_t *sec, const wchar_t *key, int v) const {
        wchar_t b[32]; _snwprintf(b, 32, L"%d", v); b[31] = 0;
        WritePrivateProfileStringW(sec, key, b, path);
    }
    void SetStr(const wchar_t *sec, const wchar_t *key, const wchar_t *v) const {
        WritePrivateProfileStringW(sec, key, v, path);
    }
    void SetDouble(const wchar_t *sec, const wchar_t *key, double v) const {
        wchar_t b[64]; _snwprintf(b, 64, L"%.5f", v); b[63] = 0;
        WritePrivateProfileStringW(sec, key, b, path);
    }
};

/* ── Start with Windows (HKCU Run) ─────────────────────────────────── */

#define RUN_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"

static inline BOOL AutostartGet(const wchar_t *name) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS) return FALSE;
    LONG r = RegQueryValueExW(k, name, NULL, NULL, NULL, NULL);
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

static inline void AutostartSet(const wchar_t *name, BOOL on) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) != ERROR_SUCCESS) return;
    if (on) {
        wchar_t exe[MAX_PATH], cmd[MAX_PATH + 4];
        ExePath(exe, MAX_PATH);
        _snwprintf(cmd, MAX_PATH + 4, L"\"%ls\"", exe);
        cmd[MAX_PATH + 3] = 0;
        RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)cmd, (DWORD)((wcslen(cmd) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(k, name);
    }
    RegCloseKey(k);
}

/* ── DPI ───────────────────────────────────────────────────────────── */

/* The manifest already declares the process DPI-aware; this is the belt to
   that pair of braces, for a copy that has lost its manifest. Vista+. */
static inline void EnableDpiAwareness(void) {
    HMODULE u = GetModuleHandleW(L"user32.dll");
    typedef BOOL (WINAPI *Fn)(void);
    Fn f = u ? (Fn)(void *)GetProcAddress(u, "SetProcessDPIAware") : NULL;
    if (f) f();
}

static inline int SystemDpi(void) {
    HDC dc = GetDC(NULL);
    int d = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    return d > 0 ? d : 96;
}

/* ── fonts ─────────────────────────────────────────────────────────── */

static int CALLBACK FontFoundProc(const LOGFONTW *, const TEXTMETRICW *, DWORD, LPARAM lp) {
    *(BOOL *)lp = TRUE;
    return 0;
}

static inline BOOL FontInstalled(const wchar_t *face) {
    LOGFONTW lf = {};
    lf.lfCharSet = DEFAULT_CHARSET;
    wcopy(lf.lfFaceName, LF_FACESIZE, face);
    BOOL found = FALSE;
    HDC dc = GetDC(NULL);
    EnumFontFamiliesExW(dc, &lf, (FONTENUMPROCW)FontFoundProc, (LPARAM)&found, 0);
    ReleaseDC(NULL, dc);
    return found;
}

/* First installed face from a NULL-terminated list; the last entry is the
   fallback even if enumeration says no (GDI's mapper will still find something). */
static inline const wchar_t *PickFont(const wchar_t *const *faces) {
    const wchar_t *last = L"Segoe UI";
    for (int i = 0; faces[i]; i++) {
        if (FontInstalled(faces[i])) return faces[i];
        last = faces[i];
    }
    return last;
}

static inline HFONT MakeFont(const wchar_t *face, double px, int weight, double scale) {
    return CreateFontW(-(int)floor(px * scale + 0.5), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, face);
}

/* The system's own dialog font, for the settings window and menus. */
static inline HFONT MessageFont(void) {
    NONCLIENTMETRICSW m = {};
    m.cbSize = sizeof m;
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof m, &m, 0))
        return CreateFontIndirectW(&m.lfMessageFont);
    return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
}

/* ── HTTP GET (WinINet) ────────────────────────────────────────────────
   WinINet rather than WinHTTP on purpose: WinINet follows the Internet
   Options TLS settings, and IE11 on Windows 7 turns TLS 1.2 on there, while
   WinHTTP on Windows 7 speaks only TLS 1.0 until KB3140245 plus a registry
   edit. The caller still passes a plain-http fallback for a machine that
   cannot do TLS 1.2 at all. Returns a malloc'd, NUL-terminated body. */
static inline char *HttpGet(const wchar_t *url) {
    HINTERNET net = InternetOpenW(L"AchmaTray/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!net) return NULL;
    DWORD to = 10000;
    InternetSetOptionW(net, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof to);
    InternetSetOptionW(net, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof to);
    InternetSetOptionW(net, INTERNET_OPTION_SEND_TIMEOUT, &to, sizeof to);
    char *buf = NULL;
    HINTERNET req = InternetOpenUrlW(net, url, NULL, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_COOKIES, 0);
    if (req) {
        DWORD status = 0, len = sizeof status;
        HttpQueryInfoW(req, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &len, NULL);
        if (status == 200) {
            size_t cap = 16384, used = 0;
            buf = (char *)malloc(cap);
            for (;;) {
                if (cap - used < 4097) {
                    char *nb = (char *)realloc(buf, cap * 2);
                    if (!nb) { free(buf); buf = NULL; break; }
                    buf = nb; cap *= 2;
                }
                DWORD got = 0;
                if (!InternetReadFile(req, buf + used, 4096, &got)) { free(buf); buf = NULL; break; }
                if (got == 0) break;
                used += got;
                if (used > (1u << 20)) { free(buf); buf = NULL; break; } /* 1 MB: not a weather reply */
            }
            if (buf) buf[used] = 0;
        }
        InternetCloseHandle(req);
    }
    InternetCloseHandle(net);
    return buf;
}

/* ── minimal JSON number lookup ────────────────────────────────────────
   Finds "key": inside the object that starts at `obj` and parses the number
   after it. Enough for Open-Meteo's flat "current" block; deliberately not a
   JSON parser. The search stops at the object's closing brace. */
static inline const char *JsonObject(const char *json, const char *name) {
    char pat[64];
    _snprintf(pat, sizeof pat, "\"%s\":{", name);
    pat[sizeof pat - 1] = 0;
    const char *p = strstr(json, pat);
    return p ? p + strlen(pat) - 1 : NULL;
}

static inline BOOL JsonNumber(const char *obj, const char *key, double *out) {
    if (!obj) return FALSE;
    const char *end = strchr(obj, '}');
    char pat[64];
    _snprintf(pat, sizeof pat, "\"%s\":", key);
    pat[sizeof pat - 1] = 0;
    const char *p = obj;
    while ((p = strstr(p, pat)) != NULL) {
        if (end && p > end) return FALSE;
        p += strlen(pat);
        while (*p == ' ') p++;
        if (*p == '[') p++;                 /* first element of a daily array */
        if (*p == '-' || (*p >= '0' && *p <= '9')) { *out = strtod(p, NULL); return TRUE; }
    }
    return FALSE;
}

/* ── single instance ───────────────────────────────────────────────── */

/* A second launch pokes the first one (shows its window) and exits. */
static inline BOOL AlreadyRunning(const wchar_t *mutexName, const wchar_t *wndClass, UINT pokeMsg, WPARAM what = 0) {
    CreateMutexW(NULL, TRUE, mutexName);
    if (GetLastError() != ERROR_ALREADY_EXISTS) return FALSE;
    HWND other = FindWindowW(wndClass, NULL);
    if (other) {
        /* let the running copy bring its window to the front */
        AllowSetForegroundWindow(ASFW_ANY);
        PostMessageW(other, pokeMsg, what, 0);
    }
    return TRUE;
}

/* ── tray icon helpers ─────────────────────────────────────────────── */

static inline void TrayAdd(HWND hwnd, UINT id, UINT cbMsg, HICON icon, const wchar_t *tip) {
    NOTIFYICONDATAW n = {};
    n.cbSize = sizeof n;
    n.hWnd = hwnd; n.uID = id;
    n.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    n.uCallbackMessage = cbMsg;
    n.hIcon = icon;
    wcopy(n.szTip, ARRAYSIZE(n.szTip), tip);
    Shell_NotifyIconW(NIM_ADD, &n);
}

static inline void TrayUpdate(HWND hwnd, UINT id, HICON icon, const wchar_t *tip) {
    NOTIFYICONDATAW n = {};
    n.cbSize = sizeof n;
    n.hWnd = hwnd; n.uID = id;
    n.uFlags = (icon ? NIF_ICON : 0) | (tip ? NIF_TIP : 0);
    n.hIcon = icon;
    if (tip) wcopy(n.szTip, ARRAYSIZE(n.szTip), tip);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

/* Balloon on 7, toast on 10/11 — Windows converts it. */
static inline void TrayBalloon(HWND hwnd, UINT id, const wchar_t *title, const wchar_t *text, BOOL sound) {
    NOTIFYICONDATAW n = {};
    n.cbSize = sizeof n;
    n.hWnd = hwnd; n.uID = id;
    n.uFlags = NIF_INFO;
    n.dwInfoFlags = NIIF_INFO | (sound ? 0 : NIIF_NOSOUND);
    wcopy(n.szInfoTitle, ARRAYSIZE(n.szInfoTitle), title);
    wcopy(n.szInfo, ARRAYSIZE(n.szInfo), text);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

static inline void TrayRemove(HWND hwnd, UINT id) {
    NOTIFYICONDATAW n = {};
    n.cbSize = sizeof n;
    n.hWnd = hwnd; n.uID = id;
    Shell_NotifyIconW(NIM_DELETE, &n);
}

/* Where the icon sits, so a flyout can open next to it. Windows 7+; on
   anything older (or if the shell refuses) fall back to the cursor. */
static inline RECT TrayIconRect(HWND hwnd, UINT id) {
    RECT r = {};
    typedef HRESULT (WINAPI *Fn)(const NOTIFYICONIDENTIFIER *, RECT *);
    HMODULE sh = GetModuleHandleW(L"shell32.dll");
    Fn f = sh ? (Fn)(void *)GetProcAddress(sh, "Shell_NotifyIconGetRect") : NULL;
    if (f) {
        NOTIFYICONIDENTIFIER nid = {};
        nid.cbSize = sizeof nid;
        nid.hWnd = hwnd; nid.uID = id;
        if (SUCCEEDED(f(&nid, &r)) && r.right > r.left) return r;
    }
    POINT pt; GetCursorPos(&pt);
    r.left = pt.x; r.top = pt.y; r.right = pt.x + 1; r.bottom = pt.y + 1;
    return r;
}

/* Place a w×h window beside `anchor`, on the side facing away from the
   taskbar, kept inside that monitor's work area. Works with the taskbar on
   any edge, and on any monitor. */
static inline POINT FlyoutPosition(RECT anchor, int w, int h, int gap) {
    POINT c = { (anchor.left + anchor.right) / 2, (anchor.top + anchor.bottom) / 2 };
    HMONITOR mon = MonitorFromPoint(c, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {}; mi.cbSize = sizeof mi;
    GetMonitorInfoW(mon, &mi);
    RECT wa = mi.rcWork, m = mi.rcMonitor;
    POINT p;
    if (wa.bottom < m.bottom && c.y >= wa.bottom) {        /* taskbar at the bottom */
        p.x = c.x - w / 2; p.y = wa.bottom - h - gap;
    } else if (wa.top > m.top && c.y <= wa.top) {          /* top */
        p.x = c.x - w / 2; p.y = wa.top + gap;
    } else if (wa.left > m.left && c.x <= wa.left) {       /* left */
        p.x = wa.left + gap; p.y = c.y - h / 2;
    } else if (wa.right < m.right && c.x >= wa.right) {    /* right */
        p.x = wa.right - w - gap; p.y = c.y - h / 2;
    } else {                                               /* overflow area, auto-hide, unknown */
        p.x = c.x - w / 2; p.y = c.y - h - gap;
    }
    if (p.x + w > wa.right - gap) p.x = wa.right - gap - w;
    if (p.x < wa.left + gap) p.x = wa.left + gap;
    if (p.y + h > wa.bottom - gap) p.y = wa.bottom - gap - h;
    if (p.y < wa.top + gap) p.y = wa.top + gap;
    return p;
}

/* Windows 11 rounds popups only when asked. Harmless no-op elsewhere. */
static inline void RoundCorners(HWND hwnd) {
    HMODULE d = LoadLibraryW(L"dwmapi.dll");
    if (!d) return;
    typedef HRESULT (WINAPI *Fn)(HWND, DWORD, LPCVOID, DWORD);
    Fn f = (Fn)(void *)GetProcAddress(d, "DwmSetWindowAttribute");
    if (f) {
        DWORD pref = 3;                     /* DWMWCP_ROUNDSMALL */
        f(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &pref, sizeof pref);
    }
    /* dwmapi stays loaded for the life of the process; that is fine. */
}

/* ── embedded resources ────────────────────────────────────────────── */

/* Same bytes already on disk? Then leave the file (and its timestamp) alone. */
static inline BOOL FileEquals(const wchar_t *path, const void *data, DWORD size) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    BOOL same = FALSE;
    LARGE_INTEGER len;
    if (GetFileSizeEx(h, &len) && len.QuadPart == size) {
        void *buf = malloc(size ? size : 1);
        DWORD got = 0;
        if (buf && ReadFile(h, buf, size, &got, NULL) && got == size) same = memcmp(buf, data, size) == 0;
        free(buf);
    }
    CloseHandle(h);
    return same;
}

/* Write an RCDATA resource out to %APPDATA%\AchmaTray\<name> and open it in
   the default browser. Always the same path, so the page's localStorage (its
   origin is the file path) survives updates of the .exe. */
static inline BOOL OpenEmbeddedPage(int resId, const wchar_t *name) {
    HRSRC r = FindResourceW(NULL, MAKEINTRESOURCEW(resId), MAKEINTRESOURCEW(10) /* RT_RCDATA */);
    if (!r) return FALSE;
    HGLOBAL g = LoadResource(NULL, r);
    DWORD size = SizeofResource(NULL, r);
    const void *data = g ? LockResource(g) : NULL;
    if (!data || !size) return FALSE;

    wchar_t dir[MAX_PATH], path[MAX_PATH];
    DataDir(dir, MAX_PATH);
    _snwprintf(path, MAX_PATH, L"%ls\\%ls", dir, name);
    path[MAX_PATH - 1] = 0;

    if (!FileEquals(path, data, size)) {
        HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) return FALSE;
        DWORD w = 0;
        WriteFile(h, data, size, &w, NULL);
        CloseHandle(h);
    }
    return (INT_PTR)ShellExecuteW(NULL, L"open", path, NULL, NULL, SW_SHOWNORMAL) > 32;
}
