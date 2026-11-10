/* SvR Anthology Launcher (branch anthology): sets up and starts SVR Anthology by Yumboy11 on the
 * SvR 2011 PC recomp, in the look of the port's own launcher - Play, Settings and Setup only.
 *
 * SVR Anthology is Yumboy11's mod for the Xbox 360 version of WWE SmackDown vs. Raw 2011. Nothing of it is in
 * this program or this port: the player uses their own SvR 2011 ISO (or extracted game folder) and
 * Yumboy11's own installer, which makes a new "WWE SmackDown vs Raw Anthology" folder next to itself. This
 * launcher only starts his installer (in its own window, the player answers it) and then runs the port's game
 * (svr2011.exe, the Anthology build: no mods, no online) with --game_data_root=<that folder>. It never moves,
 * changes or writes anything in the installer's folder or the Anthology's folder (its pictures are only read:
 * the Play page shows his cover art from his install).
 *
 * Settings: anthology_launcher.ini beside the program. The game's own settings: svr2011.toml beside it (the
 * same keys and values as the port's launcher writes).
 *
 * Test aids (no window): --check <anthology folder> <out file>  - the folder check's result
 *                        --command <anthology folder> <out file> - the game's command line
 *                        --installer-check <installer folder> <out file>
 *                        --original-xex <ISO or game folder> <dest folder> <out file>
 *                            (the original default.xex into <dest>\Original, hash-checked)
 *                        --install-saves <his Saves and DLC folder> <dest folder> <out file>
 *                            (his saves into <dest>\Saves, his DLC packages into <dest>\DLC)
 *                        --capture <play|settings|setup> <file.bmp>
 *                            (the window drawn off-screen, never shown or activated, saved as a BMP)
 *                        --capture update <file.bmp>
 *                            (the Play page with the update button, nothing asked of GitHub)
 *
 * Updates: at the window's start one look at this program's GitHub releases; a newer one puts an Update button in
 * the sidebar. The button starts SvR Anthology Updater.exe (updater_main.c) and closes: it replaces this
 * program's own files only, from this program's own release zip - never anything of Yumboy11's.
 */
#define COBJMACROS
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <math.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <uxtheme.h>
#include <wchar.h>

#include "anth_report.h"
#include "anthology_disc.h"
#include "apk_package.h"
#include "reports_tab.h"
#include "stfs.h"
#include "ui_theme.h"
#include "unzip.h"
#include "updater.h"

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#ifndef PORT_VERSION
#define PORT_VERSION L"dev"
#endif
#define WINDOW_TITLE L"SVR Anthology by Yumboy11 - SvR 2011 PC recomp by KaikoClanworth - Version " PORT_VERSION
#define GAME_EXE L"svr2011.exe"
#define ANTHOLOGY_GAME L"WWE SmackDown vs Raw Anthology"
#define ANTHOLOGY_SAVES L"WWE SmackDown vs Raw Anthology Saves and DLC"
#define INSTALLER_EXE L"Anthology_Installer.exe"
#define INSTALLER_DATA L"Anthology_Install_data.dat"

/* The look: the port's launcher's (svr2011_launcher.c) - a near-black sidebar, dark pages with lighter cards,
   red for the main action. */
#define C_SIDEBAR       RGB(14, 15, 19)
#define C_SIDEBAR_HOVER RGB(28, 30, 36)
#define C_SIDEBAR_SEL   RGB(56, 20, 27)
#define C_ACCENT        RGB(214, 22, 44)
#define C_ACCENT_HOT    RGB(234, 42, 62)
#define C_ACCENT_DOWN   RGB(170, 14, 34)
#define C_PAGE          RGB(20, 22, 27)
#define C_CARD          RGB(27, 30, 36)
#define C_CARD_LINE     RGB(46, 50, 58)
#define C_FIELD         RGB(16, 18, 22)
#define C_TEXT          RGB(236, 237, 241)
#define C_MUTED         RGB(150, 153, 163)
#define C_GOOD          RGB(52, 199, 89)
#define C_WARN          RGB(240, 160, 40)

#define SB_W     196   /* the sidebar */
#define X0       224   /* the pages' left edge (96-DPI units, from the window's) */
#define DY       64    /* the pages' top */
#define CLIENT_W 976
#define CLIENT_H 690
#define NAV_TOP  86
#define NAV_H    42

enum { PAGE_PLAY, PAGE_SETTINGS, PAGE_SETUP, PAGE_PHONE, PAGE_REPORT, PAGE_MY_REPORTS, PAGE_COUNT, PAGE_ALL = -1 };
#define ID_REPORT_BASE 1300    /* Report a problem's controls (anth_report.c) */
#define ID_MY_REPORTS_BASE 1400 /* My reports' (reports_tab.c) */
#define REPORT_SERVER L"https://sho-ti.me/svr"

enum {
    ID_ISO_EDIT = 100, ID_ISO_FILE, ID_ISO_FOLDER,
    ID_INST_EDIT, ID_INST_BROWSE, ID_INST_STATUS, ID_INST_RUN, ID_INST_NOTE,
    ID_GAME_EDIT, ID_GAME_BROWSE, ID_GAME_STATUS,
    ID_SAVES_OPEN, ID_SAVES_README, ID_OUR_FOLDERS,
    ID_PLAY, ID_CREDITS, ID_README, ID_STATUS, ID_SETUP_STATUS, ID_GO_SETUP, ID_CLOSE_ON_START,
    ID_SKIP_INTROS, ID_SKIP_TRAINING, ID_VERSION,
    /* Settings */
    ID_WINDOWED, ID_FULLSCREEN, ID_FULLSCREEN_EX, ID_RESOLUTION, ID_FRAMERATE, ID_RENDER_RES, ID_UPSCALE, ID_VSYNC,
    ID_SHOWFPS, ID_MSAA, ID_RENDERER, ID_PREPARE, ID_TEXTURES, ID_INPUT, ID_AUDIO, ID_MUTE, ID_PRIORITY, ID_LANGUAGE,
    ID_UNLOCK_ALL, ID_MATCH_CAMERA, ID_DEFAULTS, ID_SAVE, ID_SETTINGS_STATUS,
    /* Phone */
    ID_PHONE_INSTALL, ID_PHONE_CANCEL, ID_PHONE_PROGRESS, ID_PHONE_STATUS, ID_PHONE_FOLDERS,
    ID_UPDATE
};
#define WM_INSTALLER_DONE (WM_APP + 1)
#define WM_PHONE (WM_APP + 2)        /* wParam: progress 0-1000, PHONE_OK / PHONE_FAILED; lParam: a text */
#define PHONE_OK 2000
#define PHONE_FAILED 2001
#define WM_UPDATE_FOUND (WM_APP + 3)  /* lParam: the UpdateInfo of a newer release */
#define PAGE_NONE (-2)                /* (a control shown on its own terms: the update button) */
#define UPDATER_EXE L"SvR Anthology Updater.exe"

static HWND s_wnd;
static HINSTANCE s_hinst;
static HICON s_icon;
static int s_dpi = 96, s_page = PAGE_PLAY;
static HFONT s_font, s_big, s_title, s_huge, s_icons, s_nav, s_small, s_side;
static HBRUSH s_page_brush, s_card_brush, s_field_brush;
static WCHAR s_dir[MAX_PATH];  /* the launcher's folder (the port's files) */
static WCHAR s_ini[MAX_PATH];
static WCHAR s_iso[MAX_PATH], s_inst[MAX_PATH], s_game[MAX_PATH];
static HANDLE s_installer;     /* Yumboy11's installer while it runs */
static HANDLE s_game_proc;
static int s_ready;            /* the Anthology is set up: PLAY can start it */
static UpdateInfo s_update;    /* a newer release (version empty: none known) */

static int S(int v) { return MulDiv(v, s_dpi, 96); }

/* ---- files and folders ---- */

static int has_file(const WCHAR *dir, const WCHAR *name)
{
    WCHAR p[MAX_PATH];
    return join(p, dir, name) && file_exists(p);
}
static void parent_of(WCHAR *out, const WCHAR *path)
{
    WCHAR *s;
    wcscpy_s(out, MAX_PATH, path);
    s = wcsrchr(out, L'\\');
    if (s) *s = 0;
}
/* a is b or inside it (case-insensitive) */
static int inside(const WCHAR *a, const WCHAR *b)
{
    size_t n = wcslen(b);
    while (n && b[n - 1] == L'\\') n--;
    return n && !_wcsnicmp(a, b, n) && (a[n] == 0 || a[n] == L'\\');
}

/* The Anthology's game folder: Yumboy11's installer's output (an Xbox 360 game folder). 0 + why when not. */
static int check_game(const WCHAR *dir, WCHAR *why, size_t cap)
{
    WCHAR p[MAX_PATH];
    if (!dir[0]) { wcscpy_s(why, cap, L"not chosen yet"); return 0; }
    if (!dir_exists(dir)) { wcscpy_s(why, cap, L"the folder is not there"); return 0; }
    if (!has_file(dir, L"default.xex")) {
        wcscpy_s(why, cap, L"no default.xex: choose the \"" ANTHOLOGY_GAME L"\" folder his installer made");
        return 0;
    }
    if (!join(p, dir, L"pac") || !dir_exists(p)) { wcscpy_s(why, cap, L"no pac folder: the install is not complete"); return 0; }
    if (inside(dir, s_dir) || inside(s_dir, dir)) {
        wcscpy_s(why, cap, L"keep the Anthology and this launcher in separate folders");
        return 0;
    }
    wcscpy_s(why, cap, L"ready");
    return 1;
}

/* Yumboy11's installer folder: his exe and data (and his bat) side by side. */
static int check_installer(const WCHAR *dir, WCHAR *why, size_t cap)
{
    if (!dir[0]) { wcscpy_s(why, cap, L"not chosen yet"); return 0; }
    if (!has_file(dir, INSTALLER_EXE)) { wcscpy_s(why, cap, L"no " INSTALLER_EXE L" in this folder"); return 0; }
    if (!has_file(dir, INSTALLER_DATA)) { wcscpy_s(why, cap, L"no " INSTALLER_DATA L" in this folder (download it fully)"); return 0; }
    wcscpy_s(why, cap, L"found Yumboy11's installer");
    return 1;
}

/* The game's command line (--game_data_root: the Anthology's folder, read only). */
static void game_command(WCHAR *cmd, size_t cap, const WCHAR *exe, const WCHAR *game)
{
    WCHAR root[MAX_PATH];
    size_t n;
    wcscpy_s(root, MAX_PATH, game);
    n = wcslen(root);
    while (n && root[n - 1] == L'\\') root[--n] = 0;  /* (a trailing \ would escape the quote) */
    swprintf_s(cmd, cap, L"\"%s\" \"--game_data_root=%s\"", exe, root);
}

/* ---- svr2011.toml: the game's settings ---- */

/* One flat key = value line set (added when missing). */
static void set_toml(const char *k, const char *value)
{
    WCHAR path[MAX_PATH];
    char *data = NULL, *out;
    size_t len = 0, cap, o = 0, i = 0, kl = strlen(k);
    int done = 0;
    FILE *f;
    if (!join(path, s_dir, L"svr2011.toml")) return;
    if (!_wfopen_s(&f, path, L"rb") && f) {
        fseek(f, 0, SEEK_END);
        len = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        data = malloc(len + 1);
        if (!data) { fclose(f); return; }
        len = fread(data, 1, len, f);
        data[len] = 0;
        fclose(f);
    }
    cap = len + kl + strlen(value) + 8;
    out = malloc(cap);
    if (!out) { free(data); return; }
    while (i < len) {
        size_t e = i;
        while (e < len && data[e] != '\n') e++;
        if (!done && e - i >= kl && !strncmp(data + i, k, kl) && (data[i + kl] == ' ' || data[i + kl] == '=')) {
            o += (size_t)sprintf_s(out + o, cap - o, "%s = %s\n", k, value);
            done = 1;
        } else {
            memcpy(out + o, data + i, e - i + (e < len));
            o += e - i + (e < len);
        }
        i = e + 1;
    }
    if (!done) o += (size_t)sprintf_s(out + o, cap - o, "%s%s = %s\n", o && out[o - 1] != '\n' ? "\n" : "", k, value);
    if (!_wfopen_s(&f, path, L"wb") && f) {
        fwrite(out, 1, o, f);
        fclose(f);
    }
    free(out);
    free(data);
}
/* A key's value (quotes off) into val; 0 when the file or the key isn't there. */
static int get_toml(const char *k, char *val, size_t cap)
{
    WCHAR path[MAX_PATH];
    char line[512];
    size_t kl = strlen(k);
    int found = 0;
    FILE *f;
    if (!join(path, s_dir, L"svr2011.toml") || _wfopen_s(&f, path, L"rb") || !f) return 0;
    while (fgets(line, sizeof line, f)) {
        char *v, *e;
        if (strncmp(line, k, kl) || (line[kl] != ' ' && line[kl] != '=')) continue;
        v = strchr(line, '=');
        if (!v) continue;
        for (++v; *v == ' ' || *v == '"'; ++v) {}
        for (e = v + strlen(v); e > v && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '"'); --e) {}
        *e = 0;
        strcpy_s(val, cap, v);
        found = 1;
    }
    fclose(f);
    return found;
}
static int toml_int(const char *k, int def)
{
    char v[64];
    return get_toml(k, v, sizeof v) ? atoi(v) : def;
}
static int toml_bool(const char *k, int def)
{
    char v[64];
    return get_toml(k, v, sizeof v) ? !strcmp(v, "true") : def;
}

/* Yumboy11's versions: his default<key>.xex in the Anthology's folder (the game's anthology_version). */
static const struct { const char *key; const WCHAR *label; } k_versions[] = {
    { "", L"SVR Anthology" }, { "SVR07", L"SmackDown vs. Raw 2007" }, { "SVR08", L"SmackDown vs. Raw 2008" },
    { "SVR09", L"SmackDown vs. Raw 2009" }, { "SVR10", L"SmackDown vs. Raw 2010" },
    { "SVR11", L"SmackDown vs. Raw 2011" }, { "LOWM", L"Legends of WrestleMania" }, { "DIVAS", L"Divas" },
};
#define N_VERSIONS ((int)(sizeof k_versions / sizeof k_versions[0]))

/* The settings' choices: as the port's launcher's (svr2011_launcher.c k_res, k_render, k_languages). */
static const struct { int w, h, scale; const WCHAR *label; } k_res[] = {
    { 1280,  720, 1, L"1280 \x00D7 720  (the console's)" },
    { 1600,  900, 2, L"1600 \x00D7 900" },
    { 1920, 1080, 2, L"1920 \x00D7 1080" },
    { 2560, 1440, 2, L"2560 \x00D7 1440" },
    { 3840, 2160, 3, L"3840 \x00D7 2160  (4K)" },
};
#define N_RES ((int)(sizeof k_res / sizeof k_res[0]))
static const struct { int max_scale; double res; int height; const WCHAR *label; } k_render[] = {
    { 4, 1.0, 0, L"Auto (follows the window)" }, { 1, 0.5, 0, L"360p" }, { 1, 2.0 / 3.0, 0, L"480p" },
    { 1, 1.0, 0, L"720p (Xbox 360)" }, { 2, 1.0, 900, L"900p" }, { 2, 1.0, 1080, L"1080p" },
    { 2, 1.0, 0, L"1440p" }, { 3, 1.0, 0, L"2160p (4K)" },
};
#define N_RENDER ((int)(sizeof k_render / sizeof k_render[0]))
static const struct { const WCHAR *label; int id; } k_languages[] = {
    { L"English", 1 }, { L"Fran\x00E7" L"ais (French)", 4 }, { L"Deutsch (German)", 3 },
    { L"Espa\x00F1" L"ol (Spanish)", 5 }, { L"Italiano (Italian)", 6 },
};
#define N_LANGUAGES ((int)(sizeof k_languages / sizeof k_languages[0]))
static const WCHAR *const k_upscale[] = { L"Upscaling: off", L"FSR 1 Ultra Quality", L"FSR 1 Quality",
                                          L"FSR 1 Balanced", L"FSR 1 Performance", L"FSR 2 Ultra Quality (exp.)",
                                          L"FSR 2 Quality (exp.)", L"FSR 2 Balanced (exp.)",
                                          L"FSR 2 Performance (exp.)" };

/* As the game's GraphicsPage::Load picks the row from the three keys. */
static int render_index(int max_scale, double res, int height)
{
    int i, best = 0;
    if (height > 0) {
        for (i = 0; i < N_RENDER; i++)
            if (k_render[i].height > 0 &&
                (k_render[best].height == 0 || abs(k_render[i].height - height) < abs(k_render[best].height - height)))
                best = i;
    } else if (res > 0.0 && res < 0.99) {
        for (i = 0; i < N_RENDER; i++)
            if (k_render[i].res < 0.99 && fabs(k_render[i].res - res) < fabs(k_render[best].res - res))
                best = i;
    } else {
        for (i = 0; i < N_RENDER; i++)
            if (k_render[i].max_scale == max_scale && k_render[i].res >= 0.99 && k_render[i].height == 0)
                best = i;
    }
    return best;
}

/* ---- the window's parts ---- */

/* Every control, with its page and its layout in 96-DPI units (a DPI change lays them out again). */
typedef struct { HWND h; int page, x, y, w, hh, card, big; } Placed;
static Placed s_placed[192];
static int s_nplaced;
/* The cards: rounded panels the window draws behind a page's controls. */
typedef struct { int page, x, y, w, h; const WCHAR *title; } Card;
static Card s_cards[24];
static int s_ncards;

static HWND ctl(int id) { return GetDlgItem(s_wnd, id); }
static void set_text(int id, const WCHAR *t) { SetWindowTextW(ctl(id), t); }
static int checked(int id) { return IsDlgButtonChecked(s_wnd, id) == BST_CHECKED; }
static void check(int id, int on) { CheckDlgButton(s_wnd, id, on ? BST_CHECKED : BST_UNCHECKED); }
static int combo_sel(int id) { return (int)SendMessageW(ctl(id), CB_GETCURSEL, 0, 0); }
static void combo_set(int id, int i) { SendMessageW(ctl(id), CB_SETCURSEL, (WPARAM)i, 0); }
static void combo_add(HWND h, const WCHAR *t) { SendMessageW(h, CB_ADDSTRING, 0, (LPARAM)t); }

static void card(int page, int x, int y, int w, int h, const WCHAR *title)
{
    if (s_ncards < (int)(sizeof s_cards / sizeof s_cards[0]))
        s_cards[s_ncards++] = (Card){ page, x, y, w, h, title };
}

static int in_card(int page, int x, int y)
{
    int i;
    for (i = 0; i < s_ncards; i++)
        if ((s_cards[i].page == page) && x >= s_cards[i].x && x < s_cards[i].x + s_cards[i].w && y >= s_cards[i].y &&
            y < s_cards[i].y + s_cards[i].h)
            return 1;
    return 0;
}

/* A control on a page (x, y from the page's top-left, in 96-DPI units). */
static HWND add(int page, const WCHAR *cls, const WCHAR *text, DWORD style, int x, int y, int w, int h, int id)
{
    const int button = !_wcsicmp(cls, L"Button");
    const DWORD kind = style & BS_TYPEMASK;
    const int push = button && (kind == BS_PUSHBUTTON || kind == BS_DEFPUSHBUTTON);
    HWND c;
    if (push) style = (style & ~(DWORD)BS_TYPEMASK) | BS_OWNERDRAW;
    c = CreateWindowExW(0, cls, text, WS_CHILD | style, S(X0 + x), S(DY + y), S(w), S(h), s_wnd,
                        (HMENU)(INT_PTR)id, s_hinst, NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)s_font, TRUE);
    if (button && !push)
        SetWindowTheme(c, L"", L"");   /* (check boxes and radios: the text colour from WM_CTLCOLORSTATIC) */
    else if (!_wcsicmp(cls, L"Edit") || !_wcsicmp(cls, L"ComboBox"))
        SetWindowTheme(c, L"DarkMode_CFD", NULL);
    if (s_nplaced < (int)(sizeof s_placed / sizeof s_placed[0]))
        s_placed[s_nplaced++] = (Placed){ c, page, x, y, w, h, in_card(page, x, y), push && kind == BS_DEFPUSHBUTTON };
    return c;
}

static Placed *placed(HWND h)
{
    int i;
    for (i = 0; i < s_nplaced; i++)
        if (s_placed[i].h == h) return &s_placed[i];
    return NULL;
}

/* ---- the fonts ---- */

static int CALLBACK font_found(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM lp)
{
    (void)lf; (void)tm; (void)type;
    *(int *)lp = 1;
    return 0;
}

/* The icon font: Segoe Fluent Icons (Windows 11), else Segoe MDL2 Assets. */
static const WCHAR *icon_face(void)
{
    static const WCHAR *face;
    if (!face) {
        LOGFONTW lf;
        int found = 0;
        HDC dc = GetDC(NULL);
        ZeroMemory(&lf, sizeof lf);
        lf.lfCharSet = DEFAULT_CHARSET;
        wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Segoe Fluent Icons");
        EnumFontFamiliesExW(dc, &lf, font_found, (LPARAM)&found, 0);
        ReleaseDC(NULL, dc);
        face = found ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets";
    }
    return face;
}

static void make_fonts(void)
{
    NONCLIENTMETRICSW ncm;
    HFONT *all[] = { &s_font, &s_big, &s_title, &s_huge, &s_icons, &s_nav, &s_small, &s_side };
    int i;
    for (i = 0; i < (int)(sizeof all / sizeof all[0]); i++)
        if (*all[i]) { DeleteObject(*all[i]); *all[i] = NULL; }
    ncm.cbSize = sizeof ncm;
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0, (UINT)s_dpi);
    s_font = CreateFontIndirectW(&ncm.lfMessageFont);
    ncm.lfMessageFont.lfWeight = FW_SEMIBOLD;
    ncm.lfMessageFont.lfHeight = -MulDiv(13, s_dpi, 72);
    s_big = CreateFontIndirectW(&ncm.lfMessageFont);
    ncm.lfMessageFont.lfWeight = FW_BOLD;
    ncm.lfMessageFont.lfHeight = -MulDiv(17, s_dpi, 72);
    s_title = CreateFontIndirectW(&ncm.lfMessageFont);
    ncm.lfMessageFont.lfHeight = -MulDiv(26, s_dpi, 72);
    s_huge = CreateFontIndirectW(&ncm.lfMessageFont);
    ncm.lfMessageFont.lfHeight = -MulDiv(11, s_dpi, 72);   /* (the sidebar's "SVR Anthology") */
    s_side = CreateFontIndirectW(&ncm.lfMessageFont);
    ncm.lfMessageFont.lfWeight = FW_SEMIBOLD;
    ncm.lfMessageFont.lfHeight = -MulDiv(10, s_dpi, 72);
    s_nav = CreateFontIndirectW(&ncm.lfMessageFont);
    ncm.lfMessageFont.lfWeight = FW_NORMAL;
    ncm.lfMessageFont.lfHeight = -MulDiv(9, s_dpi, 72);
    s_small = CreateFontIndirectW(&ncm.lfMessageFont);
    s_icons = CreateFontW(-MulDiv(16, s_dpi, 96), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                          CLEARTYPE_QUALITY, 0, icon_face());
}

/* Every control at the DPI's size and place, with its font. */
static void relayout(void)
{
    int i;
    for (i = 0; i < s_nplaced; i++) {
        const Placed *p = &s_placed[i];
        SetWindowPos(p->h, NULL, S(X0 + p->x), S(DY + p->y), S(p->w), S(p->hh), SWP_NOZORDER | SWP_NOACTIVATE);
        SendMessageW(p->h, WM_SETFONT, (WPARAM)s_font, TRUE);
    }
    InvalidateRect(s_wnd, NULL, TRUE);
}

/* ---- the settings ---- */

static void settings_load(void)
{
    char v[64];
    const int fs = toml_bool("fullscreen", 0), ex = toml_bool("fullscreen_exclusive", 0);
    const int w = toml_int("window_width", 1280);
    int i, ri, ui, li = 0, perf = toml_int("perf_overlay", -1), aa = toml_int("native_aa", 0);
    check(ID_WINDOWED, !fs);
    check(ID_FULLSCREEN, fs && !ex);
    check(ID_FULLSCREEN_EX, fs && ex);
    for (i = 0; i < N_RES; i++)
        if (k_res[i].w == w) combo_set(ID_RESOLUTION, i);
    if (combo_sel(ID_RESOLUTION) < 0) combo_set(ID_RESOLUTION, 0);
    combo_set(ID_FRAMERATE, toml_int("frame_rate", 60) == 30 ? 0 : 1);
    {
        double rs = 1.0;
        if (get_toml("native_render_scale", v, sizeof v)) rs = atof(v);
        ri = render_index(toml_int("native_max_scale", 4), rs, toml_int("native_scene_height", 0));
        combo_set(ID_RENDER_RES, ri);
    }
    ui = 0;
    if (get_toml("native_upscale", v, sizeof v)) {
        const int q = toml_int("native_upscale_quality", 1);
        if (!strcmp(v, "fsr1")) ui = 1 + (q >= 0 && q <= 3 ? q : 1);
        else if (!strcmp(v, "fsr2")) ui = 5 + (q >= 0 && q <= 3 ? q : 1);
    }
    combo_set(ID_UPSCALE, ui);
    check(ID_VSYNC, toml_bool("vsync", 1));
    if (perf < 0) perf = toml_bool("show_fps", 1) ? 1 : 0;
    combo_set(ID_SHOWFPS, perf >= 0 && perf <= 2 ? perf : 1);
    if (!aa) aa = toml_bool("native_2x_msaa", 0) ? 2 : 1;
    combo_set(ID_MSAA, aa >= 1 && aa <= 4 ? aa - 1 : 0);
    combo_set(ID_RENDERER, get_toml("gpu_backend", v, sizeof v) && !_stricmp(v, "vulkan") ? 1
                           : get_toml("gpu_backend", v, sizeof v) && !_stricmp(v, "d3d11") ? 2 : 0);
    check(ID_PREPARE, toml_bool("native_prepare_pipelines", 1));
    combo_set(ID_TEXTURES, get_toml("native_texture_quality", v, sizeof v) && !strcmp(v, "medium") ? 1
                           : get_toml("native_texture_quality", v, sizeof v) && !strcmp(v, "low") ? 2 : 0);
    combo_set(ID_INPUT, get_toml("input_backend", v, sizeof v) && !_stricmp(v, "sdl") ? 1 : 0);
    combo_set(ID_AUDIO, get_toml("audio_backend", v, sizeof v) && !_stricmp(v, "sdl") ? 1 : 0);
    check(ID_MUTE, toml_bool("audio_mute", 0));
    check(ID_PRIORITY, toml_int("process_priority", 0) >= 1);
    {
        const int id = toml_int("user_language", 1);
        for (i = 0; i < N_LANGUAGES; i++)
            if (k_languages[i].id == id) li = i;
        combo_set(ID_LANGUAGE, li);
    }
    check(ID_UNLOCK_ALL, toml_bool("unlock_everything", 1));
    {
        const int ci = toml_int("match_camera", 0);
        combo_set(ID_MATCH_CAMERA, ci >= 0 && ci <= 5 ? ci : 0);
    }
    check(ID_SKIP_INTROS, toml_bool("skip_intros", 0));
    check(ID_SKIP_TRAINING, toml_bool("skip_training", 0));
}

/* The same keys and values as the port's launcher (settings_save) - for what this version has. */
static void settings_save(void)
{
    char v[64];
    int i = combo_sel(ID_RESOLUTION), ri = combo_sel(ID_RENDER_RES), ui = combo_sel(ID_UPSCALE);
    if (i < 0 || i >= N_RES) i = 0;
    if (ri < 0 || ri >= N_RENDER) ri = 0;
    set_toml("gpu_plugin", "\"xenos\"");
    set_toml("native_renderer", "\"main\"");
    set_toml("resolution", "\"720p\"");
    sprintf_s(v, sizeof v, "%d", k_res[i].scale); set_toml("resolution_scale", v);
    sprintf_s(v, sizeof v, "%d", k_res[i].w); set_toml("window_width", v);
    sprintf_s(v, sizeof v, "%d", k_res[i].h); set_toml("window_height", v);
    set_toml("fullscreen", checked(ID_WINDOWED) ? "false" : "true");
    set_toml("fullscreen_exclusive", checked(ID_FULLSCREEN_EX) ? "true" : "false");
    set_toml("frame_rate", combo_sel(ID_FRAMERATE) == 0 ? "30" : "60");
    sprintf_s(v, sizeof v, "%d", k_render[ri].max_scale); set_toml("native_max_scale", v);
    sprintf_s(v, sizeof v, "%.4f", k_render[ri].res); set_toml("native_render_scale", v);
    sprintf_s(v, sizeof v, "%d", k_render[ri].height); set_toml("native_scene_height", v);
    set_toml("native_upscale", ui >= 1 && ui <= 4 ? "\"fsr1\"" : ui >= 5 && ui <= 8 ? "\"fsr2\"" : "\"off\"");
    sprintf_s(v, sizeof v, "%d", ui >= 1 && ui <= 8 ? (ui - 1) % 4 : 1); set_toml("native_upscale_quality", v);
    set_toml("vsync", checked(ID_VSYNC) ? "true" : "false");
    {
        const int pi = combo_sel(ID_SHOWFPS);
        set_toml("show_fps", pi > 0 ? "true" : "false");
        sprintf_s(v, sizeof v, "%d", pi >= 0 && pi <= 2 ? pi : 1); set_toml("perf_overlay", v);
    }
    {
        const int ai = combo_sel(ID_MSAA), level = ai >= 0 && ai < 4 ? ai + 1 : 1;
        set_toml("native_2x_msaa", level >= 2 ? "true" : "false");
        sprintf_s(v, sizeof v, "%d", level); set_toml("native_aa", v);
    }
    set_toml("gpu_backend", combo_sel(ID_RENDERER) == 1 ? "\"vulkan\"" : combo_sel(ID_RENDERER) == 2 ? "\"d3d11\"" : "\"any\"");
    set_toml("native_prepare_pipelines", checked(ID_PREPARE) ? "true" : "false");
    set_toml("native_texture_quality", combo_sel(ID_TEXTURES) == 1 ? "\"medium\"" : combo_sel(ID_TEXTURES) == 2 ? "\"low\"" : "\"high\"");
    set_toml("input_backend", combo_sel(ID_INPUT) == 1 ? "\"sdl\"" : "\"xinput\"");
    set_toml("audio_backend", combo_sel(ID_AUDIO) == 1 ? "\"sdl\"" : "\"xaudio2\"");
    set_toml("audio_mute", checked(ID_MUTE) ? "true" : "false");
    set_toml("process_priority", checked(ID_PRIORITY) ? "1" : "0");
    {
        const int li = combo_sel(ID_LANGUAGE);
        sprintf_s(v, sizeof v, "%d", k_languages[li >= 0 && li < N_LANGUAGES ? li : 0].id); set_toml("user_language", v);
    }
    set_toml("unlock_everything", checked(ID_UNLOCK_ALL) ? "true" : "false");
    {
        const int ci = combo_sel(ID_MATCH_CAMERA);
        sprintf_s(v, sizeof v, "%d", ci >= 0 && ci <= 5 ? ci : 0); set_toml("match_camera", v);
    }
    set_toml("online_enabled", "false");   /* (no online in this version) */
}

static void settings_defaults(void)
{
    check(ID_WINDOWED, 1); check(ID_FULLSCREEN, 0); check(ID_FULLSCREEN_EX, 0);
    combo_set(ID_RESOLUTION, 0); combo_set(ID_FRAMERATE, 1); combo_set(ID_RENDER_RES, 0); combo_set(ID_UPSCALE, 0);
    check(ID_VSYNC, 1); combo_set(ID_SHOWFPS, 1); combo_set(ID_MSAA, 0); combo_set(ID_RENDERER, 0);
    check(ID_PREPARE, 1); combo_set(ID_TEXTURES, 0); combo_set(ID_INPUT, 0); combo_set(ID_AUDIO, 0);
    check(ID_MUTE, 0); check(ID_PRIORITY, 0); combo_set(ID_LANGUAGE, 0); check(ID_UNLOCK_ALL, 1);
    combo_set(ID_MATCH_CAMERA, 0);
}

/* ---- Setup ---- */

static void save_paths(void)
{
    WritePrivateProfileStringW(L"Anthology", L"Iso", s_iso, s_ini);
    WritePrivateProfileStringW(L"Anthology", L"Installer", s_inst, s_ini);
    WritePrivateProfileStringW(L"Anthology", L"GameFolder", s_game, s_ini);
}

static void refresh(void)
{
    WCHAR why[300], t[400], saves[MAX_PATH], parent[MAX_PATH];
    int inst_ok = check_installer(s_inst, why, 300), there;
    set_text(ID_ISO_EDIT, s_iso);
    set_text(ID_INST_EDIT, s_inst);
    swprintf_s(t, 400, L"Installer: %s", why);
    set_text(ID_INST_STATUS, t);
    EnableWindow(ctl(ID_INST_RUN), inst_ok && s_iso[0] && !s_installer);
    set_text(ID_GAME_EDIT, s_game);
    s_ready = check_game(s_game, why, 300);
    swprintf_s(t, 400, L"Anthology folder: %s", why);
    set_text(ID_GAME_STATUS, t);
    EnableWindow(ctl(ID_PLAY), s_ready && !s_installer);
    parent_of(parent, s_game);
    there = s_game[0] && join(saves, parent, ANTHOLOGY_SAVES) && dir_exists(saves);
    EnableWindow(ctl(ID_SAVES_OPEN), there);
    EnableWindow(ctl(ID_SAVES_README), there);
    EnableWindow(ctl(ID_README), s_inst[0] && has_file(s_inst, L"Readme.txt"));
    if (s_ready)
        swprintf_s(t, 400, L"Ready: %s", s_game);
    else
        wcscpy_s(t, 400, L"SVR Anthology isn't set up yet: open Setup (your ISO, Yumboy11's installer, his saves and DLC).");
    set_text(ID_STATUS, t);
    ShowWindow(ctl(ID_GO_SETUP), s_page == PAGE_PLAY && !s_ready ? SW_SHOWNOACTIVATE : SW_HIDE);
    ShowWindow(ctl(ID_UPDATE), s_update.version[0] ? SW_SHOWNOACTIVATE : SW_HIDE);
    InvalidateRect(s_wnd, NULL, FALSE);
}

static int pick_folder(const WCHAR *title, WCHAR *out)
{
    BROWSEINFOW bi = {0};
    PIDLIST_ABSOLUTE id;
    WCHAR p[MAX_PATH];
    bi.hwndOwner = s_wnd;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    id = SHBrowseForFolderW(&bi);
    if (!id) return 0;
    if (!SHGetPathFromIDListW(id, p)) { CoTaskMemFree(id); return 0; }
    CoTaskMemFree(id);
    wcscpy_s(out, MAX_PATH, p);
    return 1;
}

static void pick_iso(void)
{
    OPENFILENAMEW of = {0};
    WCHAR p[MAX_PATH] = L"";
    of.lStructSize = sizeof of;
    of.hwndOwner = s_wnd;
    of.lpstrFilter = L"Xbox 360 disc image (*.iso)\0*.iso\0All files\0*.*\0";
    of.lpstrFile = p;
    of.nMaxFile = MAX_PATH;
    of.lpstrTitle = L"Your WWE SmackDown vs. Raw 2011 Xbox 360 ISO";
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&of)) wcscpy_s(s_iso, MAX_PATH, p);
}

static DWORD WINAPI wait_installer(LPVOID h)
{
    DWORD code = 1;
    WaitForSingleObject((HANDLE)h, INFINITE);
    GetExitCodeProcess((HANDLE)h, &code);
    PostMessageW(s_wnd, WM_INSTALLER_DONE, code, 0);
    return 0;
}

/* Yumboy11's installer, as his bat starts it: his exe with the ISO (or folder), in its own console window where
 * the player answers it. Its output goes next to it, in his folder; nothing here moves it. */
static void run_installer(void)
{
    WCHAR exe[MAX_PATH], cmd[MAX_PATH * 2 + 8], m[1600], why[300];
    STARTUPINFOW si = {sizeof si};
    PROCESS_INFORMATION pi;
    if (!check_installer(s_inst, why, 300) || !s_iso[0]) return;
    if (inside(s_inst, s_iso)) {
        MessageBoxW(s_wnd, L"Yumboy11's installer can't be inside the original game's folder. Put the installer "
                           L"folder somewhere else.", WINDOW_TITLE, MB_ICONWARNING);
        return;
    }
    swprintf_s(m, 1600,
               L"This starts Yumboy11's own SVR Anthology installer in its own window. Answer its questions there.\n\n"
               L"It checks your original game (%s), then creates two NEW folders next to itself, in:\n%s\n\n"
               L"  " ANTHOLOGY_GAME L"\n  " ANTHOLOGY_SAVES L"\n\n"
               L"It tells you how much free space that drive needs (about 5 times the size of SvR 2011). "
               L"Your ISO is not changed, and this launcher does not move or change anything there: if you want "
               L"the Anthology on another drive, move the whole installer folder there first.\n\nStart it now?",
               s_iso, s_inst);
    if (MessageBoxW(s_wnd, m, WINDOW_TITLE, MB_YESNO | MB_ICONINFORMATION) != IDYES) return;
    if (!join(exe, s_inst, INSTALLER_EXE)) return;
    swprintf_s(cmd, MAX_PATH * 2 + 8, L"\"%s\" \"%s\"", exe, s_iso);
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, s_inst, &si, &pi)) {
        swprintf_s(m, 1600, L"Could not start %s (Windows error %lu).", exe, GetLastError());
        MessageBoxW(s_wnd, m, WINDOW_TITLE, MB_ICONERROR);
        return;
    }
    CloseHandle(pi.hThread);
    s_installer = pi.hProcess;
    CloseHandle(CreateThread(NULL, 0, wait_installer, pi.hProcess, 0, NULL));
    set_text(ID_SETUP_STATUS, L"Yumboy11's installer is running in its own window...");
    refresh();
}

/* ---- Phone ---- */

static volatile LONG s_phone_cancel;
static int s_phone_busy;
static WCHAR s_adb[MAX_PATH];

static WCHAR *wdup(const WCHAR *s)
{
    size_t n = wcslen(s) + 1;
    WCHAR *d = (WCHAR *)malloc(n * sizeof(WCHAR));
    if (d) memcpy(d, s, n * sizeof(WCHAR));
    return d;
}

static void phone_progress(int permille, const WCHAR *msg)
{
    PostMessageW(s_wnd, WM_PHONE, (WPARAM)permille, msg ? (LPARAM)wdup(msg) : 0);
}

/* (--adb-install: the console) */
static void phone_console(int permille, const WCHAR *msg)
{
    (void)permille;
    if (msg) wprintf(L"%ls\n", msg);
}

/* The app beside the launcher: Android\SvR Anthology.apk. */
static int phone_apk(WCHAR *out)
{
    return join(out, s_dir, L"Android\\SvR Anthology.apk") && file_exists(out);
}

/* adb: beside the launcher, on PATH or in the Android SDK - else Google's platform tools, downloaded beside the
   launcher (platform-tools). */
static int phone_adb(apk_progress_fn progress, WCHAR *err, int errn)
{
    WCHAR tmp[MAX_PATH], dir[MAX_PATH];
    if (adb_find(s_dir, s_adb, MAX_PATH)) return 1;
    progress(0, L"Downloading Google's Android platform tools (adb)\x2026");
    if (!GetTempPathW(MAX_PATH, dir) || !join(tmp, dir, L"svr-anthology-platform-tools.zip")) return 0;
    if (!update_download(ADB_TOOLS_URL, tmp, NULL, 0, &s_phone_cancel, err, (size_t)errn)) return 0;
    if (!unzip_file(tmp, s_dir)) {
        DeleteFileW(tmp);
        swprintf_s(err, errn, L"Could not unpack the platform tools into %s.", s_dir);
        return 0;
    }
    DeleteFileW(tmp);
    if (!adb_find(s_dir, s_adb, MAX_PATH)) {
        swprintf_s(err, errn, L"The platform tools download has no adb.exe.");
        return 0;
    }
    return 1;
}

static int phone_install(apk_progress_fn progress, WCHAR *err, int errn)
{
    WCHAR apk[MAX_PATH], m[MAX_PATH], why[300];
    if (!check_game(s_game, why, 300)) {
        swprintf_s(err, errn, L"Set SVR Anthology up on this PC first (Setup): %s", why);
        return 0;
    }
    if (!join(m, s_dir, L"Original\\default.xex") || !anth_xex_ok(m)) {
        swprintf_s(err, errn, L"Press PLAY once first: it copies the original program from your ISO.");
        return 0;
    }
    if (!phone_apk(apk)) {
        swprintf_s(err, errn, L"This download has no Android app (Android\\SvR Anthology.apk beside the launcher).");
        return 0;
    }
    return phone_adb(progress, err, errn) &&
           adb_install_anthology(s_adb, s_game, s_dir, apk, &s_phone_cancel, progress, err, errn);
}

static DWORD WINAPI phone_thread(LPVOID unused)
{
    WCHAR err[600];
    int ok;
    (void)unused;
    err[0] = 0;
    ok = phone_install(phone_progress, err, 600);
    PostMessageW(s_wnd, WM_PHONE, ok ? PHONE_OK : PHONE_FAILED, (LPARAM)wdup(err));
    return 0;
}

static void phone_start(void)
{
    HANDLE t;
    if (s_phone_busy) return;
    s_phone_busy = 1;
    s_phone_cancel = 0;
    EnableWindow(ctl(ID_PHONE_INSTALL), FALSE);
    EnableWindow(ctl(ID_PHONE_CANCEL), TRUE);
    SendMessageW(ctl(ID_PHONE_PROGRESS), PBM_SETPOS, 0, 0);
    set_text(ID_PHONE_STATUS, L"Starting\x2026");
    t = CreateThread(NULL, 0, phone_thread, NULL, 0, NULL);
    if (t) CloseHandle(t);
    else s_phone_busy = 0;
}

static void phone_message(WPARAM wp, WCHAR *text)
{
    if (wp == PHONE_OK || wp == PHONE_FAILED) {
        WCHAR m[700];
        s_phone_busy = 0;
        EnableWindow(ctl(ID_PHONE_INSTALL), TRUE);
        EnableWindow(ctl(ID_PHONE_CANCEL), FALSE);
        if (wp == PHONE_OK) {
            SendMessageW(ctl(ID_PHONE_PROGRESS), PBM_SETPOS, 1000, 0);
            swprintf_s(m, 700, L"Done: SVR Anthology is %s on the phone. Open the SVR Anthology app there.",
                       text && !wcscmp(text, L"updated") ? L"updated" : L"installed");
        } else {
            swprintf_s(m, 700, L"%s", text && text[0] ? text : L"It didn't work.");
        }
        set_text(ID_PHONE_STATUS, m);
    } else {
        SendMessageW(ctl(ID_PHONE_PROGRESS), PBM_SETPOS, wp, 0);
        if (text) set_text(ID_PHONE_STATUS, text);
    }
    free(text);
}

static void installer_done(DWORD code)
{
    WCHAR out[MAX_PATH], why[300], m[400];
    CloseHandle(s_installer);
    s_installer = NULL;
    if (join(out, s_inst, ANTHOLOGY_GAME) && check_game(out, why, 300)) {
        wcscpy_s(s_game, MAX_PATH, out);
        save_paths();
        set_text(ID_SETUP_STATUS, L"The Anthology is installed. Set up its saves and DLC (step 4), then PLAY.");
    } else {
        swprintf_s(m, 400, L"The installer closed (code %lu) and there is no complete Anthology folder yet.", code);
        set_text(ID_SETUP_STATUS, m);
    }
    refresh();
}

static void open_path(const WCHAR *p) { ShellExecuteW(s_wnd, L"open", p, NULL, NULL, SW_SHOWNORMAL); }

/* Yumboy11's saves and DLC, from his "Saves and DLC" folder's "XENIA INSTALL ME" (360 packages): the saves
 * (CON, his title 54511A11) become this game's save files in <dest>\Saves, the DLC packages (type 2) are copied
 * to <dest>\DLC (the game installs them at its next start). His folder is only read. */
/* His DLC packages (src: his "XENIA INSTALL ME") into out_dlc; only_missing: the ones not there yet. */
static void install_dlc(const WCHAR *src, const WCHAR *out_dlc, int only_missing, int *dlc, int *skipped)
{
    WCHAR pat[MAX_PATH], from[MAX_PATH], to[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    CreateDirectoryW(out_dlc, NULL);
    if (!join(pat, src, L"*")) return;
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        uint8_t head[0x348];
        DWORD got = 0;
        HANDLE fh;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY || !join(from, src, fd.cFileName) || !join(to, out_dlc, fd.cFileName))
            continue;
        if (only_missing && file_exists(to)) continue;
        fh = CreateFileW(from, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (fh == INVALID_HANDLE_VALUE) continue;
        ReadFile(fh, head, sizeof head, &got, NULL);
        CloseHandle(fh);
        /* (content type 2: marketplace content - his DLC packages; Vol11 and Vol12 are CON, the rest LIVE) */
        if (got < sizeof head || (memcmp(head, "LIVE", 4) && memcmp(head, "PIRS", 4) && memcmp(head, "CON ", 4)) ||
            head[0x344] || head[0x345] || head[0x346] || head[0x347] != 2)
            continue;
        if (CopyFileW(from, to, FALSE)) ++*dlc;
        else ++*skipped;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static int install_saves_dlc(const WCHAR *his, const WCHAR *dest, int *saves, int *dlc, int *skipped)
{
    WCHAR src[MAX_PATH], out_saves[MAX_PATH], out_dlc[MAX_PATH];
    *saves = *dlc = *skipped = 0;
    if (!join(src, his, L"XENIA INSTALL ME") || !dir_exists(src) || !join(out_saves, dest, L"Saves") ||
        !join(out_dlc, dest, L"DLC"))
        return 0;
    *saves = stfs_import_saves(src, out_saves, skipped);
    install_dlc(src, out_dlc, 0, dlc, skipped);
    return 1;
}

/* At start: an install made before 2.1.6.1 left out his CON DLC packages (Vol11, Vol12 - "Some save data
 * cannot be used as Downloadable Content", DLC superstars missing). With DLC installed already, the packages
 * not there yet are added; the saves are left alone. */
static void add_missing_dlc(void)
{
    WCHAR parent[MAX_PATH], his[MAX_PATH], src[MAX_PATH], out_dlc[MAX_PATH];
    int dlc = 0, skipped = 0;
    if (!s_game[0]) return;
    parent_of(parent, s_game);
    if (!join(his, parent, ANTHOLOGY_SAVES) ||
        !join(src, his, L"XENIA INSTALL ME") || !dir_exists(src) || !join(out_dlc, s_dir, L"DLC") || !dir_exists(out_dlc))
        return;
    install_dlc(src, out_dlc, 1, &dlc, &skipped);
}

static void install_saves_dlc_ui(void)
{
    WCHAR parent[MAX_PATH], his[MAX_PATH], save[MAX_PATH], m[700];
    int saves, dlc, skipped;
    parent_of(parent, s_game);
    if (!join(his, parent, ANTHOLOGY_SAVES) || !dir_exists(his)) return;
    if (join(save, s_dir, L"Saves\\SaveData.dat") && file_exists(save) &&
        MessageBoxW(s_wnd, L"This game has a save already (Saves\\SaveData.dat beside this launcher). Replace it with "
                           L"Yumboy11's Anthology save? Your current save is lost.",
                    WINDOW_TITLE, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return;
    if (!install_saves_dlc(his, s_dir, &saves, &dlc, &skipped)) {
        MessageBoxW(s_wnd, L"His \"Saves and DLC\" folder has no \"XENIA INSTALL ME\" folder.", WINDOW_TITLE, MB_ICONWARNING);
        return;
    }
    swprintf_s(m, 700, L"Yumboy11's saves and DLC are set up: %d save file(s), %d DLC package(s) (the game installs "
                       L"them when it starts)%s.",
               saves, dlc, skipped ? L" - some files could not be read" : L"");
    set_text(ID_SETUP_STATUS, m);
}

static void open_saves_readme(void)
{
    WCHAR saves[MAX_PATH], pat[MAX_PATH], p[MAX_PATH], parent[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    parent_of(parent, s_game);
    if (!join(saves, parent, ANTHOLOGY_SAVES) || !join(pat, saves, L"*readme*")) return;
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        open_path(saves);
        return;
    }
    FindClose(h);
    if (join(p, saves, fd.cFileName)) open_path(p);
}

/* ---- Play ---- */

static void play(void)
{
    WCHAR exe[MAX_PATH], cmd[MAX_PATH * 2 + 40], why[300], m[600];
    STARTUPINFOW si = {sizeof si};
    PROCESS_INFORMATION pi;
    if (s_game_proc && WaitForSingleObject(s_game_proc, 0) == WAIT_TIMEOUT) {
        set_text(ID_STATUS, L"The game is already running.");
        return;
    }
    if (!check_game(s_game, why, 300)) return;
    if (!join(exe, s_dir, GAME_EXE) || !file_exists(exe)) {
        MessageBoxW(s_wnd, GAME_EXE L" is missing beside this launcher. Extract the SvR Anthology PC zip again.",
                    WINDOW_TITLE, MB_ICONERROR);
        return;
    }
    if (!has_file(s_dir, L"native_shaders\\shaders.dxil.pak") &&
        MessageBoxW(s_wnd, L"The game's shaders are missing (native_shaders\\shaders.dxil.pak beside this launcher). "
                           L"Extract the zip again into a plain folder such as C:\\Games\\SvR Anthology PC (an antivirus "
                           L"may remove it).\n\nStart the game anyway?",
                    WINDOW_TITLE, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return;
    /* The program the game runs: the original disc's default.xex, from the player's own ISO (Setup), once. */
    if (!join(m, s_dir, L"Original\\default.xex") || !anth_xex_ok(m)) {
        WCHAR err[600];
        if (!s_iso[0]) {
            MessageBoxW(s_wnd, L"Choose your SvR 2011 ISO (or extracted game folder) in Setup first: the game runs "
                               L"the original disc's program from it.", WINDOW_TITLE, MB_ICONINFORMATION);
            return;
        }
        set_text(ID_STATUS, L"Copying the original program (default.xex) from your SvR 2011 ISO...");
        UpdateWindow(s_wnd);
        if (!anth_copy_original_xex(s_iso, s_dir, err, 600)) {
            MessageBoxW(s_wnd, err, WINDOW_TITLE, MB_ICONERROR);
            refresh();
            return;
        }
    }
    {
        /* (the version: Yumboy11's program for it must be in his folder) */
        int v = (int)SendDlgItemMessageW(s_wnd, ID_VERSION, CB_GETCURSEL, 0, 0);
        char q[16];
        WCHAR file[40];
        if (v < 0 || v >= N_VERSIONS) v = 0;
        swprintf_s(file, 40, L"default%hs.xex", k_versions[v].key);
        if (!join(m, s_game, file) || GetFileAttributesW(m) == INVALID_FILE_ATTRIBUTES) {
            swprintf_s(m, 600, L"%s is not in the Anthology's folder, so the %s version can't start. Run "
                               L"Yumboy11's installer again (Setup), or choose another version.", file, k_versions[v].label);
            MessageBoxW(s_wnd, m, WINDOW_TITLE, MB_ICONINFORMATION);
            return;
        }
        sprintf_s(q, sizeof q, "\"%s\"", k_versions[v].key);
        set_toml("anthology_version", q);
    }
    set_toml("skip_intros", checked(ID_SKIP_INTROS) ? "true" : "false");
    set_toml("skip_training", checked(ID_SKIP_TRAINING) ? "true" : "false");
    set_toml("online_enabled", "false");
    WritePrivateProfileStringW(L"Anthology", L"CloseOnStart", checked(ID_CLOSE_ON_START) ? L"1" : L"0", s_ini);
    game_command(cmd, MAX_PATH * 2 + 40, exe, s_game);
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, 0, NULL, s_dir, &si, &pi)) {
        swprintf_s(m, 600, L"Could not start the game (Windows error %lu).", GetLastError());
        MessageBoxW(s_wnd, m, WINDOW_TITLE, MB_ICONERROR);
        return;
    }
    CloseHandle(pi.hThread);
    if (s_game_proc) CloseHandle(s_game_proc);
    s_game_proc = pi.hProcess;
    set_text(ID_STATUS, L"Starting SVR Anthology...");
    if (checked(ID_CLOSE_ON_START)) PostMessageW(s_wnd, WM_CLOSE, 0, 0);
}

static void credits(void)
{
    MessageBoxW(s_wnd,
                L"SVR ANTHOLOGY is made by Yumboy11.\n\n"
                L"It is Yumboy11's mod for the Xbox 360 version of WWE SmackDown vs. Raw 2011, and it is "
                L"freeware: never buy or sell SVR Anthology. This launcher contains nothing of it: you use your own "
                L"SvR 2011 disc image and Yumboy11's own installer, downloaded from him. All credit for the "
                L"Anthology goes to Yumboy11.\n\n"
                L"Yumboy11 thanks, in his Readme: Brienj, Red Rooster, ryokudou, littlebase631, theviper12, "
                L"xtremebrett, tekken57 (X-Packer, X-Rey), ERI619, eatrawmeat391 (Pac Editor Tool), jakeypearce, "
                L"Mark/Rampage, Justyn, Sean O Connor, Wrestling Modders INC, RipZz (Sound Factory Tool), "
                L"Obs Kenobs, WatchTheSmoke, and everyone who sent textures, suggestions and feedback - see his "
                L"Readme.txt (the Readme button) for what each of them did.\n\n"
                L"This launcher and the game program are the SvR 2011 PC recomp by KaikoClanworth (" PORT_VERSION
                L"), made with the permission of Yumboy11, who allows it to be used with his Anthology as "
                L"long as players use their own ISO and his own installer, and he is fully credited.\n\n"
                L"WWE SmackDown vs. Raw 2011 belongs to its owners.",
                L"Credits - SVR Anthology by Yumboy11", MB_ICONINFORMATION);
}

/* ---- updates ---- */

/* One look at this program's releases (nothing downloaded); never in the way when GitHub can't be reached. */
static DWORD WINAPI update_check_thread(LPVOID unused)
{
    static UpdateInfo info;
    WCHAR err[700];
    (void)unused;
    if (update_check(&info, err, 700) && version_newer(info.version, PORT_VERSION))
        PostMessageW(s_wnd, WM_UPDATE_FOUND, 0, (LPARAM)&info);
    return 0;
}

static void update_start(void)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    WCHAR exe[MAX_PATH], cmd[3 * MAX_PATH + 64], ask[500];
    if (s_installer || IsWindowEnabled(ctl(ID_PHONE_CANCEL))) {
        MessageBoxW(s_wnd, L"Wait for Yumboy11's installer or the phone install to finish first.", WINDOW_TITLE, MB_ICONINFORMATION);
        return;
    }
    if (s_game_proc && WaitForSingleObject(s_game_proc, 0) == WAIT_TIMEOUT) {
        MessageBoxW(s_wnd, L"Close the game first, then update.", WINDOW_TITLE, MB_ICONINFORMATION);
        return;
    }
    if (!join(exe, s_dir, UPDATER_EXE) || !has_file(s_dir, UPDATER_EXE)) {
        MessageBoxW(s_wnd, UPDATER_EXE L" is missing beside the launcher: download the newest release from "
                           L"github.com/" UPDATE_REPO L"/releases instead.", WINDOW_TITLE, MB_ICONWARNING);
        return;
    }
    swprintf_s(ask, 500, L"Update from version %s to %s? It downloads about %llu MB from this program's GitHub "
                         L"releases and the launcher starts again after it.\n\nYour saves, settings and Yumboy11's "
                         L"Anthology folder are not touched.\n\nThe Android app isn't updated by this: use the Phone page after it.",
               PORT_VERSION, s_update.version, (unsigned long long)(s_update.size >> 20));
    if (MessageBoxW(s_wnd, ask, WINDOW_TITLE, MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    swprintf_s(cmd, 3 * MAX_PATH + 64, L"\"%s\" --apply \"%s\" \"%s\" --version %s", exe, s_dir, s_dir, s_update.version);
    ZeroMemory(&si, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, 0, NULL, s_dir, &si, &pi)) {
        MessageBoxW(s_wnd, L"Could not start " UPDATER_EXE L".", WINDOW_TITLE, MB_ICONWARNING);
        return;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    DestroyWindow(s_wnd);   /* (the updater waits for this program to close) */
}

/* The updater's own new copy, which it can't put in place while it runs. */
static void update_place_new_updater(void)
{
    WCHAR exe[MAX_PATH], nw[MAX_PATH + 8];
    if (!join(exe, s_dir, UPDATER_EXE)) return;
    swprintf_s(nw, MAX_PATH + 8, L"%s.new", exe);
    if (GetFileAttributesW(nw) != INVALID_FILE_ATTRIBUTES)
        MoveFileExW(nw, exe, MOVEFILE_REPLACE_EXISTING);
}

/* ---- the pages ---- */

static void build_window(void)
{
    HWND h;
    int i;
    /* The sidebar's Update button, above the credit line (hidden until a newer release is known). */
    add(PAGE_NONE, L"Button", L"Update", BS_DEFPUSHBUTTON | WS_TABSTOP, 16 - X0, 516, SB_W - 32, 42, ID_UPDATE);
    /* Play: the Anthology's card (his cover art on the right), the state, the credit. */
    card(PAGE_PLAY, 0, 0, 724, 360, NULL);
    add(PAGE_PLAY, L"Button", L"Play", BS_DEFPUSHBUTTON | WS_TABSTOP, 28, 212, 190, 60, ID_PLAY);
    add(PAGE_PLAY, L"Static", L"Version", SS_LEFT | SS_NOPREFIX, 238, 212, 200, 20, 0);
    h = add(PAGE_PLAY, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 238, 236, 196, 300, ID_VERSION);
    for (i = 0; i < N_VERSIONS; i++) SendMessageW(h, CB_ADDSTRING, 0, (LPARAM)k_versions[i].label);
    add(PAGE_PLAY, L"Button", L"Close on start", BS_AUTOCHECKBOX | WS_TABSTOP, 28, 300, 122, 24, ID_CLOSE_ON_START);
    add(PAGE_PLAY, L"Button", L"Skip intros", BS_AUTOCHECKBOX | WS_TABSTOP, 158, 300, 100, 24, ID_SKIP_INTROS);
    add(PAGE_PLAY, L"Button", L"Skip practice ring", BS_AUTOCHECKBOX | WS_TABSTOP, 266, 300, 150, 24, ID_SKIP_TRAINING);
    card(PAGE_PLAY, 0, 376, 724, 60, NULL);
    add(PAGE_PLAY, L"Static", L"", SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, 52, 396, 480, 22, ID_STATUS);
    add(PAGE_PLAY, L"Button", L"Open Setup", BS_PUSHBUTTON | WS_TABSTOP, 552, 386, 160, 36, ID_GO_SETUP);
    card(PAGE_PLAY, 0, 452, 724, 146, L"Credit");
    add(PAGE_PLAY, L"Static",
        L"SVR Anthology is Yumboy11's mod for WWE SmackDown vs. Raw 2011 - all credit for it goes to Yumboy11 and "
        L"the people he thanks in his Readme. It is freeware: never buy or sell it. Nothing of it is in this "
        L"download: you use your own SvR 2011 ISO and Yumboy11's own installer. The PC recomp (this launcher and "
        L"the game program): KaikoClanworth.",
        SS_LEFT | SS_NOPREFIX, 18, 486, 500, 104, 0);
    add(PAGE_PLAY, L"Button", L"Credits", BS_PUSHBUTTON | WS_TABSTOP, 536, 470, 172, 38, ID_CREDITS);
    add(PAGE_PLAY, L"Button", L"Yumboy11's Readme", BS_PUSHBUTTON | WS_TABSTOP, 536, 518, 172, 38, ID_README);

    /* Settings: Display, Input and audio, Game (the port's launcher's, for what this version has). */
    card(PAGE_SETTINGS, 0, 0, 724, 238, L"Display");
    add(PAGE_SETTINGS, L"Static", L"Mode", SS_LEFT, 16, 40, 120, 20, 0);
    add(PAGE_SETTINGS, L"Button", L"Windowed", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 160, 38, 110, 24, ID_WINDOWED);
    add(PAGE_SETTINGS, L"Button", L"Full screen", BS_AUTORADIOBUTTON, 290, 38, 120, 24, ID_FULLSCREEN);
    add(PAGE_SETTINGS, L"Button", L"Full screen (exclusive)", BS_AUTORADIOBUTTON, 430, 38, 200, 24, ID_FULLSCREEN_EX);
    add(PAGE_SETTINGS, L"Static", L"Window size", SS_LEFT, 16, 76, 120, 20, 0);
    h = add(PAGE_SETTINGS, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP | WS_GROUP, 160, 72, 300, 200, ID_RESOLUTION);
    for (i = 0; i < N_RES; i++) combo_add(h, k_res[i].label);
    h = add(PAGE_SETTINGS, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 476, 72, 232, 200, ID_FRAMERATE);
    combo_add(h, L"30 FPS"); combo_add(h, L"60 FPS");
    add(PAGE_SETTINGS, L"Static", L"Render resolution", SS_LEFT, 16, 110, 140, 20, 0);
    h = add(PAGE_SETTINGS, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 160, 106, 300, 300, ID_RENDER_RES);
    for (i = 0; i < N_RENDER; i++) combo_add(h, k_render[i].label);
    h = add(PAGE_SETTINGS, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 476, 106, 232, 300, ID_UPSCALE);
    for (i = 0; i < (int)(sizeof k_upscale / sizeof k_upscale[0]); i++) combo_add(h, k_upscale[i]);
    add(PAGE_SETTINGS, L"Button", L"VSync (no tearing)", BS_AUTOCHECKBOX | WS_TABSTOP, 16, 140, 200, 24, ID_VSYNC);
    h = add(PAGE_SETTINGS, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 476, 140, 232, 200, ID_SHOWFPS);
    combo_add(h, L"Overlay: off"); combo_add(h, L"Overlay: FPS (F2)"); combo_add(h, L"Overlay: full (F2)");
    add(PAGE_SETTINGS, L"Static", L"Anti-aliasing", SS_LEFT, 16, 176, 120, 20, 0);
    h = add(PAGE_SETTINGS, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 160, 172, 160, 200, ID_MSAA);
    combo_add(h, L"Off"); combo_add(h, L"2x (4 samples)"); combo_add(h, L"3x (9 samples)"); combo_add(h, L"4x (16 samples)");
    add(PAGE_SETTINGS, L"Static", L"Graphics API", SS_LEFT, 340, 176, 120, 20, 0);
    h = add(PAGE_SETTINGS, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 476, 172, 232, 200, ID_RENDERER);
    combo_add(h, L"Automatic (Direct3D 12 recommended)"); combo_add(h, L"Vulkan"); combo_add(h, L"Direct3D 11 (older GPUs)");
    add(PAGE_SETTINGS, L"Button", L"Prepare graphics in the menus (no first-time stutter)", BS_AUTOCHECKBOX | WS_TABSTOP,
        16, 204, 380, 24, ID_PREPARE);
    add(PAGE_SETTINGS, L"Static", L"Textures", SS_LEFT, 400, 208, 70, 20, 0);
    h = add(PAGE_SETTINGS, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 476, 204, 232, 200, ID_TEXTURES);
    combo_add(h, L"High"); combo_add(h, L"Medium (half size)"); combo_add(h, L"Low (quarter size)");

    card(PAGE_SETTINGS, 0, 252, 724, 128, L"Input and audio");
    add(PAGE_SETTINGS, L"Static", L"Controller API", SS_LEFT, 16, 292, 140, 20, 0);
    h = add(PAGE_SETTINGS, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 160, 288, 300, 200, ID_INPUT);
    combo_add(h, L"XInput (Xbox controllers)"); combo_add(h, L"SDL (other gamepads)");
    add(PAGE_SETTINGS, L"Static", L"Audio output", SS_LEFT, 16, 324, 140, 20, 0);
    h = add(PAGE_SETTINGS, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 160, 320, 300, 200, ID_AUDIO);
    combo_add(h, L"XAudio2 (recommended)"); combo_add(h, L"SDL");
    add(PAGE_SETTINGS, L"Button", L"Mute", BS_AUTOCHECKBOX | WS_TABSTOP, 476, 322, 100, 24, ID_MUTE);
    add(PAGE_SETTINGS, L"Button", L"High CPU priority (helps on busy or weak PCs)", BS_AUTOCHECKBOX | WS_TABSTOP,
        16, 350, 400, 24, ID_PRIORITY);

    card(PAGE_SETTINGS, 0, 394, 724, 104, L"Game");
    add(PAGE_SETTINGS, L"Static", L"Language", SS_LEFT, 16, 434, 140, 20, 0);
    h = add(PAGE_SETTINGS, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 160, 430, 300, 200, ID_LANGUAGE);
    for (i = 0; i < N_LANGUAGES; i++) combo_add(h, k_languages[i].label);
    h = add(PAGE_SETTINGS, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 476, 430, 232, 200, ID_MATCH_CAMERA);
    combo_add(h, L"Camera: Normal"); combo_add(h, L"Camera: Far"); combo_add(h, L"Camera: Close");
    combo_add(h, L"Camera: High"); combo_add(h, L"Camera: Low"); combo_add(h, L"Camera: Ramp");
    add(PAGE_SETTINGS, L"Button", L"Everything unlocked from the start (DLC stays)", BS_AUTOCHECKBOX | WS_TABSTOP,
        16, 466, 400, 24, ID_UNLOCK_ALL);

    add(PAGE_SETTINGS, L"Button", L"Restore defaults", BS_PUSHBUTTON | WS_TABSTOP, 0, 514, 170, 38, ID_DEFAULTS);
    add(PAGE_SETTINGS, L"Static", L"", SS_LEFT | SS_NOPREFIX, 186, 524, 330, 22, ID_SETTINGS_STATUS);
    add(PAGE_SETTINGS, L"Button", L"Save", BS_DEFPUSHBUTTON | WS_TABSTOP, 534, 514, 190, 38, ID_SAVE);

    /* Setup: the four steps. */
    card(PAGE_SETUP, 0, 0, 724, 96, L"1.  Your SvR 2011 Xbox 360 disc image (ISO) or extracted game folder");
    add(PAGE_SETUP, L"Edit", L"", WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, 16, 46, 460, 28, ID_ISO_EDIT);
    add(PAGE_SETUP, L"Button", L"ISO...", BS_PUSHBUTTON | WS_TABSTOP, 488, 44, 104, 32, ID_ISO_FILE);
    add(PAGE_SETUP, L"Button", L"Folder...", BS_PUSHBUTTON | WS_TABSTOP, 604, 44, 104, 32, ID_ISO_FOLDER);

    card(PAGE_SETUP, 0, 110, 724, 170, L"2.  Yumboy11's \"SVR Anthology 1.0 Installer\" (downloaded from him)");
    add(PAGE_SETUP, L"Edit", L"", WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, 16, 156, 576, 28, ID_INST_EDIT);
    add(PAGE_SETUP, L"Button", L"Browse...", BS_PUSHBUTTON | WS_TABSTOP, 604, 154, 104, 32, ID_INST_BROWSE);
    add(PAGE_SETUP, L"Static", L"", SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, 16, 192, 690, 20, ID_INST_STATUS);
    add(PAGE_SETUP, L"Button", L"Run Yumboy11's installer", BS_DEFPUSHBUTTON | WS_TABSTOP, 16, 222, 230, 40, ID_INST_RUN);
    add(PAGE_SETUP, L"Static",
        L"It opens in its own window and makes the Anthology next to itself, in the installer folder (about "
        L"5 times the size of SvR 2011). Installed already? Choose its folder in step 3.",
        SS_LEFT | SS_NOPREFIX, 262, 220, 446, 48, ID_INST_NOTE);

    card(PAGE_SETUP, 0, 294, 724, 112, L"3.  The \"" ANTHOLOGY_GAME L"\" folder his installer made");
    add(PAGE_SETUP, L"Edit", L"", WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, 16, 340, 576, 28, ID_GAME_EDIT);
    add(PAGE_SETUP, L"Button", L"Browse...", BS_PUSHBUTTON | WS_TABSTOP, 604, 338, 104, 32, ID_GAME_BROWSE);
    add(PAGE_SETUP, L"Static", L"", SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, 16, 376, 690, 20, ID_GAME_STATUS);

    card(PAGE_SETUP, 0, 420, 724, 96, L"4.  Yumboy11's saves and DLC (from his \"Saves and DLC\" folder)");
    add(PAGE_SETUP, L"Button", L"Install his saves and DLC", BS_PUSHBUTTON | WS_TABSTOP, 16, 464, 230, 36, ID_SAVES_OPEN);
    add(PAGE_SETUP, L"Button", L"His readme", BS_PUSHBUTTON | WS_TABSTOP, 258, 464, 150, 36, ID_SAVES_README);
    add(PAGE_SETUP, L"Button", L"This game's Saves and DLC", BS_PUSHBUTTON | WS_TABSTOP, 420, 464, 288, 36, ID_OUR_FOLDERS);
    add(PAGE_SETUP, L"Static", L"", SS_LEFT | SS_NOPREFIX, 0, 528, 724, 40, ID_SETUP_STATUS);

    /* Phone: the app and the Anthology onto an Android phone over USB (apk_package.c adb_install_anthology). */
    card(PAGE_PHONE, 0, 0, 724, 214, L"Play SVR Anthology on an Android phone");
    add(PAGE_PHONE, L"Static",
        L"1.  Set it up on this PC first (Setup) and press PLAY once: that copies the original program from your ISO.\n"
        L"2.  On the phone: Settings \x2192 About phone \x2192 tap \"Build number\" seven times. Then in Developer "
        L"options turn on USB debugging.\n"
        L"3.  Connect the phone with a USB data cable, unlock it and tap Allow when it asks about this PC.\n"
        L"4.  Install to phone. It installs the SVR Anthology app and copies the Anthology (about 28 GB, so the "
        L"first time takes a while) into the phone's games/SVR Anthology folder. Later, only what changed is "
        L"copied, and the phone's saves stay.",
        SS_LEFT | SS_NOPREFIX, 18, 44, 690, 100, 0);
    add(PAGE_PHONE, L"Button", L"Install to phone", BS_DEFPUSHBUTTON | WS_TABSTOP, 16, 152, 230, 44, ID_PHONE_INSTALL);
    h = add(PAGE_PHONE, L"Button", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, 258, 152, 120, 44, ID_PHONE_CANCEL);
    EnableWindow(h, FALSE);
    h = add(PAGE_PHONE, PROGRESS_CLASSW, L"", 0, 0, 230, 724, 10, ID_PHONE_PROGRESS);
    SendMessageW(h, PBM_SETRANGE32, 0, 1000);
    SetWindowTheme(h, L"", L"");
    SendMessageW(h, PBM_SETBARCOLOR, 0, C_ACCENT);
    SendMessageW(h, PBM_SETBKCOLOR, 0, C_CARD);
    add(PAGE_PHONE, L"Static", L"", SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, 0, 248, 724, 22, ID_PHONE_STATUS);
    card(PAGE_PHONE, 0, 286, 724, 160, L"No USB debugging? Copy by cable instead");
    add(PAGE_PHONE, L"Static",
        L"Install the app (Android\\SvR Anthology.apk in this folder) and open it once. Connect the phone, choose "
        L"File transfer, and copy these into the phone's games\\SVR Anthology folder: the \"" ANTHOLOGY_GAME
        L"\" folder, the \"Original\" folder from this launcher's folder (made when you press Play here), and "
        L"(optional) the \"" ANTHOLOGY_SAVES L"\" folder. The app's Setup tab checks them.",
        SS_LEFT | SS_NOPREFIX, 18, 320, 690, 72, 0);
    add(PAGE_PHONE, L"Button", L"Open the folders to copy", BS_PUSHBUTTON | WS_TABSTOP, 16, 398, 260, 36, ID_PHONE_FOLDERS);
    card(PAGE_PHONE, 0, 462, 724, 110, L"On the phone");
    add(PAGE_PHONE, L"Static",
        L"The app is \"SVR Anthology\" (it sits beside SvR 2011 if you have that). It needs a phone with Vulkan; "
        L"settings, the version and touch controls are in the app. The Anthology stays Yumboy11's: the phone gets "
        L"a copy of your install, nothing else.",
        SS_LEFT | SS_NOPREFIX, 18, 496, 690, 66, 0);
}

/* The reports' server: svr2011.toml's online_server (test folders set theirs), else ours. Online play stays off. */
static void report_server(WCHAR *out, int n)
{
    char v[260];
    if (get_toml("online_server", v, sizeof v) && v[0]) MultiByteToWideChar(CP_UTF8, 0, v, -1, out, n);
    else wcscpy_s(out, (size_t)n, REPORT_SERVER);
}

/* (no accounts in this version) */
static void report_no_token(char *out, size_t n)
{
    if (n) out[0] = 0;
}

static void build_report_pages(void)
{
    card(PAGE_REPORT, 0, 0, 724, 590, NULL);
    anth_report_build(s_wnd, add, PAGE_REPORT, ID_REPORT_BASE, s_dir, PORT_VERSION, report_server);
    reports_build(s_wnd, add, PAGE_MY_REPORTS, ID_MY_REPORTS_BASE, report_server, report_no_token);
    {   /* (reports_tab.c's R_LIST: this launcher's dark colours) */
        HWND list = GetDlgItem(s_wnd, ID_MY_REPORTS_BASE + 2);
        SetWindowTheme(list, L"DarkMode_Explorer", NULL);
        ListView_SetBkColor(list, C_FIELD);
        ListView_SetTextBkColor(list, C_FIELD);
        ListView_SetTextColor(list, RGB(230, 232, 238));
    }
}

static void show_page(int page)
{
    int i;
    s_page = page;
    for (i = 0; i < s_nplaced; i++)
        ShowWindow(s_placed[i].h, s_placed[i].page == page || s_placed[i].page == PAGE_ALL ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (page == PAGE_SETTINGS) settings_load();
    refresh();
    if (page == PAGE_MY_REPORTS) reports_show(s_dir);
    InvalidateRect(s_wnd, NULL, TRUE);
}

/* ---- drawing ---- */

/* His cover art from his install (read only), for the Play card's picture; empty without one. */
static void hero_image(WCHAR *out)
{
    static const WCHAR *const names[] = { L"Anthology_4K_Cover.png", L"Anthology_Cover_2.png", L"Cover.png" };
    WCHAR dir[MAX_PATH];
    int i;
    out[0] = 0;
    if (!s_game[0] || !join(dir, s_game, L"@Alternate_Files\\Boxart and Logos")) return;
    for (i = 0; i < 3; i++)
        if (join(out, dir, names[i]) && file_exists(out)) return;
    out[0] = 0;
}

static RECT page_rect(int x, int y, int w, int h)
{
    RECT r = { S(X0 + x), S(DY + y), S(X0 + x + w), S(DY + y + h) };
    return r;
}

static void paint_window(HDC dc)
{
    static const struct { WCHAR icon; const WCHAR *name; } nav[PAGE_COUNT] = {
        { 0xE768, L"Play" }, { 0xE713, L"Settings" }, { 0xE90F, L"Setup" }, { 0xE8EA, L"Phone" },
        { 0xE7BA, L"Report" }, { 0xE8F2, L"My reports" } };
    static const WCHAR *const titles[PAGE_COUNT] = { NULL, L"Settings", L"Setup", L"Android phone", L"Report a problem", L"My reports" };
    RECT cl, r;
    HBRUSH sb = CreateSolidBrush(C_SIDEBAR);
    WCHAR v[96];
    int k;
    GetClientRect(s_wnd, &cl);
    FillRect(dc, &cl, s_page_brush);
    /* Sidebar: the icon, SVR ANTHOLOGY by Yumboy11, the pages, the version. */
    r = cl;
    r.right = S(SB_W);
    FillRect(dc, &r, sb);
    DeleteObject(sb);
    SetBkMode(dc, TRANSPARENT);
    if (s_icon) DrawIconEx(dc, S(18), S(22), s_icon, S(40), S(40), 0, NULL, DI_NORMAL);
    SelectObject(dc, s_side);
    SetTextColor(dc, RGB(250, 250, 252));
    r.left = S(68); r.top = S(22); r.right = S(SB_W - 6); r.bottom = S(44);
    DrawTextW(dc, L"SVR Anthology", -1, &r, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(dc, s_nav);
    SetTextColor(dc, RGB(208, 20, 44));
    r.top = S(44); r.bottom = S(64);
    DrawTextW(dc, L"by Yumboy11", -1, &r, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    for (k = 0; k < PAGE_COUNT; k++) {
        const int sel = k == s_page;
        RECT it = { 0, S(NAV_TOP + k * NAV_H), S(SB_W), S(NAV_TOP + (k + 1) * NAV_H) };
        WCHAR icon[2] = { nav[k].icon, 0 };
        if (sel) {
            HBRUSH b = CreateSolidBrush(C_SIDEBAR_SEL);
            RECT bar = { 0, it.top, S(4), it.bottom };
            FillRect(dc, &it, b);
            DeleteObject(b);
            b = CreateSolidBrush(C_ACCENT);
            FillRect(dc, &bar, b);
            DeleteObject(b);
        }
        SetTextColor(dc, sel ? C_ACCENT_HOT : RGB(178, 180, 190));
        SelectObject(dc, s_icons);
        r = it;
        r.left += S(26);
        DrawTextW(dc, icon, 1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SetTextColor(dc, sel ? RGB(255, 255, 255) : RGB(214, 216, 224));
        SelectObject(dc, s_nav);
        r.left += S(34);
        DrawTextW(dc, nav[k].name, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    SelectObject(dc, s_font);
    SetTextColor(dc, RGB(150, 152, 162));
    r.left = S(20); r.right = S(SB_W - 10); r.top = cl.bottom - S(56); r.bottom = cl.bottom - S(34);
    DrawTextW(dc, L"PC recomp: KaikoClanworth", -1, &r, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    {
        RECT dot = { S(20), cl.bottom - S(28), S(28), cl.bottom - S(20) };
        theme_round_rect(dc, &dot, S(4), C_GOOD, THEME_NONE, 0);
    }
    swprintf_s(v, 96, L"Launcher v%s", PORT_VERSION);
    r.left = S(36); r.top = cl.bottom - S(34); r.bottom = cl.bottom - S(12);
    DrawTextW(dc, v, -1, &r, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    /* Top right: the version, the credit. */
    SelectObject(dc, s_font);
    SetTextColor(dc, C_MUTED);
    swprintf_s(v, 96, L"SvR Anthology Launcher - Version %s - PC recomp by KaikoClanworth", PORT_VERSION);
    r.left = S(SB_W); r.right = cl.right - S(24); r.top = S(12); r.bottom = S(32);
    DrawTextW(dc, v, -1, &r, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
    SetTextColor(dc, C_ACCENT);
    r.top = S(32); r.bottom = S(52);
    DrawTextW(dc, L"SVR Anthology by Yumboy11 - all credit to Yumboy11", -1, &r, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
    /* The page's title. */
    if (titles[s_page]) {
        SelectObject(dc, s_huge);
        SetTextColor(dc, RGB(250, 250, 252));
        r.left = S(X0); r.top = S(10); r.right = S(X0 + 400); r.bottom = S(DY - 4);
        DrawTextW(dc, titles[s_page], -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    /* The cards. */
    for (k = 0; k < s_ncards; k++) {
        const Card *c = &s_cards[k];
        if (c->page != s_page) continue;
        r = page_rect(c->x, c->y, c->w, c->h);
        theme_round_rect(dc, &r, S(10), C_CARD, C_CARD_LINE, (float)S(1));
        if (c->title) {
            RECT t = r;
            SelectObject(dc, s_big);
            SetTextColor(dc, RGB(244, 245, 248));
            t.left += S(16); t.top += S(10); t.bottom = t.top + S(24);
            DrawTextW(dc, c->title, -1, &t, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        }
    }
    if (s_page == PAGE_PLAY) {
        WCHAR img[MAX_PATH];
        RECT hero = page_rect(0, 0, 724, 360), t;
        hero_image(img);
        if (img[0]) theme_hero(dc, &hero, S(10), img, C_CARD, 0);
        SelectObject(dc, s_huge);
        SetTextColor(dc, RGB(250, 250, 252));
        t = page_rect(28, 96, 440, 48);
        DrawTextW(dc, L"SVR Anthology", -1, &t, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, s_big);
        SetTextColor(dc, C_ACCENT_HOT);
        t = page_rect(28, 146, 440, 26);
        DrawTextW(dc, L"by Yumboy11", -1, &t, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, s_font);
        SetTextColor(dc, C_MUTED);
        t = page_rect(28, 174, 440, 22);
        DrawTextW(dc, L"His SVR Anthology 1.0 on KaikoClanworth's SvR 2011 PC recomp.", -1, &t, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        /* the state's mark */
        SelectObject(dc, s_icons);
        SetTextColor(dc, s_ready ? C_GOOD : C_WARN);
        t = page_rect(18, 376, 30, 60);
        DrawTextW(dc, s_ready ? L"\xE73E" : L"\xE7BA", 1, &t, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}

/* The push buttons, drawn: rounded, red for the main actions (Play with its icon). */
static void draw_button(const DRAWITEMSTRUCT *d)
{
    const Placed *p = placed(d->hwndItem);
    const int primary = p && p->big, disabled = (d->itemState & ODS_DISABLED) != 0, down = (d->itemState & ODS_SELECTED) != 0;
    const int play = GetDlgCtrlID(d->hwndItem) == ID_PLAY;
    RECT r = d->rcItem;
    WCHAR text[128];
    COLORREF fill, line, ink;
    HDC dc = d->hDC;
    if (GetDlgCtrlID(d->hwndItem) == ID_UPDATE) {   /* (it stands on the sidebar) */
        HBRUSH b = CreateSolidBrush(C_SIDEBAR);
        FillRect(dc, &r, b);
        DeleteObject(b);
    } else
        FillRect(dc, &r, p && p->card ? s_card_brush : s_page_brush);
    if (primary) {
        fill = disabled ? RGB(48, 51, 58) : down ? C_ACCENT_DOWN : C_ACCENT;
        line = THEME_NONE;
        ink = disabled ? RGB(122, 126, 136) : RGB(255, 255, 255);
    } else {
        fill = disabled ? RGB(30, 32, 38) : down ? RGB(24, 26, 31) : RGB(34, 37, 44);
        line = disabled ? RGB(44, 47, 54) : RGB(66, 70, 80);
        ink = disabled ? RGB(100, 103, 112) : C_TEXT;
    }
    theme_round_rect(dc, &r, S(play ? 8 : 6), fill, line, (float)S(1));
    GetWindowTextW(d->hwndItem, text, 128);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, ink);
    if (down) OffsetRect(&r, 0, S(1));
    if (play) {
        WCHAR icon[2] = { 0xE768, 0 };
        SIZE ts, is;
        RECT ir = r, tr = r;
        int x0;
        SelectObject(dc, s_huge);
        GetTextExtentPoint32W(dc, text, (int)wcslen(text), &ts);
        SelectObject(dc, s_icons);
        GetTextExtentPoint32W(dc, icon, 1, &is);
        x0 = (r.left + r.right - (is.cx + S(10) + ts.cx)) / 2;
        ir.left = x0;
        ir.right = x0 + is.cx;
        DrawTextW(dc, icon, 1, &ir, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, s_huge);
        tr.left = x0 + is.cx + S(10);
        DrawTextW(dc, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        return;
    }
    SelectObject(dc, s_font);
    DrawTextW(dc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

static LRESULT CALLBACK wnd_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_NOTIFY:
        if (reports_notify((const NMHDR *)lp)) return 0;
        break;
    case WM_APP_ANTH_REPORT: anth_report_message(msg, wp, lp); return 0;
    case WM_APP_REPORTS: reports_message(msg, wp, lp); return 0;
    case WM_COMMAND:
        if (anth_report_command(LOWORD(wp), HIWORD(wp)) || reports_command(LOWORD(wp), HIWORD(wp))) return 0;
        switch (LOWORD(wp)) {
        case ID_ISO_FILE: pick_iso(); save_paths(); refresh(); break;
        case ID_ISO_FOLDER:
            if (pick_folder(L"Your extracted SvR 2011 Xbox 360 game folder (with default.xex)", s_iso)) save_paths();
            refresh();
            break;
        case ID_INST_BROWSE:
            if (pick_folder(L"Yumboy11's \"SVR Anthology 1.0 Installer\" folder", s_inst)) {
                /* (installed already: his game folder beside his installer) */
                WCHAR out[MAX_PATH], why[300];
                if (!s_game[0] && join(out, s_inst, ANTHOLOGY_GAME) && check_game(out, why, 300))
                    wcscpy_s(s_game, MAX_PATH, out);
                save_paths();
            }
            refresh();
            break;
        case ID_INST_RUN: run_installer(); break;
        case ID_GAME_BROWSE:
            if (pick_folder(L"The \"" ANTHOLOGY_GAME L"\" folder", s_game)) save_paths();
            refresh();
            break;
        case ID_SAVES_README: open_saves_readme(); break;
        case ID_SAVES_OPEN: install_saves_dlc_ui(); break;
        case ID_OUR_FOLDERS: {
            WCHAR p[MAX_PATH];
            if (join(p, s_dir, L"Saves")) { CreateDirectoryW(p, NULL); open_path(p); }
            if (join(p, s_dir, L"DLC")) { CreateDirectoryW(p, NULL); open_path(p); }
            break;
        }
        case ID_PLAY: play(); break;
        case ID_UPDATE: update_start(); break;
        case ID_CREDITS: credits(); break;
        case ID_README: {
            WCHAR p[MAX_PATH];
            if (join(p, s_inst, L"Readme.txt")) open_path(p);
            break;
        }
        case ID_GO_SETUP: show_page(PAGE_SETUP); break;
        case ID_PHONE_INSTALL: phone_start(); break;
        case ID_PHONE_CANCEL: InterlockedExchange(&s_phone_cancel, 1); break;
        case ID_PHONE_FOLDERS: {   /* (Explorer at his install's folder and at this launcher's, for copying by cable) */
            WCHAR p[MAX_PATH];
            parent_of(p, s_game);
            if (s_game[0] && dir_exists(p)) open_path(p);
            open_path(s_dir);
            break;
        }
        case ID_SAVE:
            settings_save();
            set_text(ID_SETTINGS_STATUS, L"Saved - the game uses them at its next start.");
            break;
        case ID_DEFAULTS:
            settings_defaults();
            set_text(ID_SETTINGS_STATUS, L"Defaults chosen - Save to keep them.");
            break;
        }
        return 0;
    case WM_INSTALLER_DONE: installer_done((DWORD)wp); return 0;
    case WM_PHONE: phone_message(wp, (WCHAR *)lp); return 0;
    case WM_UPDATE_FOUND: {
        WCHAR t[64];
        s_update = *(const UpdateInfo *)lp;
        swprintf_s(t, 64, L"Update to %s", s_update.version);
        set_text(ID_UPDATE, t);
        refresh();
        return 0;
    }
    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        if (x < S(SB_W) && y >= S(NAV_TOP) && y < S(NAV_TOP + PAGE_COUNT * NAV_H))
            show_page((y - S(NAV_TOP)) / S(NAV_H));
        return 0;
    }
    case WM_SETCURSOR: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(w, &pt);
        if (LOWORD(lp) == HTCLIENT && pt.x < S(SB_W) && pt.y >= S(NAV_TOP) && pt.y < S(NAV_TOP + PAGE_COUNT * NAV_H)) {
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;
        }
        break;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        RECT rc;
        HDC dc = BeginPaint(w, &ps), mem;
        HBITMAP bmp;
        GetClientRect(w, &rc);
        mem = CreateCompatibleDC(dc);
        bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        SelectObject(mem, bmp);
        paint_window(mem);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(w, &ps);
        return 0;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        SetTextColor((HDC)wp, C_TEXT);
        SetBkColor((HDC)wp, C_FIELD);
        return (LRESULT)s_field_brush;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        const Placed *p = placed((HWND)lp);
        const int id = GetDlgCtrlID((HWND)lp);
        WCHAR cn[16];
        if (GetClassNameW((HWND)lp, cn, 16) && !_wcsicmp(cn, L"Edit")) {   /* (read-only fields) */
            SetTextColor((HDC)wp, C_TEXT);
            SetBkColor((HDC)wp, C_FIELD);
            return (LRESULT)s_field_brush;
        }
        SetTextColor((HDC)wp, id == ID_STATUS && s_ready ? C_GOOD : id == ID_INST_NOTE ? C_MUTED : C_TEXT);
        SetBkColor((HDC)wp, p && p->card ? C_CARD : C_PAGE);
        return (LRESULT)(p && p->card ? s_card_brush : s_page_brush);
    }
    case WM_DRAWITEM:
        if (((DRAWITEMSTRUCT *)lp)->CtlType == ODT_BUTTON) {
            draw_button((DRAWITEMSTRUCT *)lp);
            return TRUE;
        }
        break;
    case WM_DPICHANGED: {
        const RECT *r = (const RECT *)lp;
        s_dpi = HIWORD(wp);
        make_fonts();
        SetWindowPos(w, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        relayout();
        return 0;
    }
    case WM_CLOSE:
        if (s_installer && MessageBoxW(w, L"Yumboy11's installer is still running in its window. Close this launcher "
                                          L"anyway? (The installer keeps running.)",
                                       WINDOW_TITLE, MB_YESNO | MB_ICONQUESTION) != IDYES)
            return 0;
        DestroyWindow(w);
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

/* ---- test aids ---- */

/* A result into a file (UTF-8), no window. */
static int write_result(const WCHAR *file, const WCHAR *text)
{
    char u[4 * MAX_PATH * 2];
    FILE *f;
    if (_wfopen_s(&f, file, L"wb") || !f) return 2;
    WideCharToMultiByte(CP_UTF8, 0, text, -1, u, sizeof u, NULL, NULL);
    fputs(u, f);
    fputs("\n", f);
    fclose(f);
    return 0;
}

/* The window as drawn into a BMP (it is never shown: drawn off-screen). */
static void capture_wnd(HWND wnd, const WCHAR *file)
{
    RECT r;
    HDC wdc, mdc;
    HBITMAP bmp;
    BITMAPINFOHEADER bi;
    BITMAPFILEHEADER fh;
    uint8_t *bits;
    int w, h, stride;
    FILE *f;
    GetWindowRect(wnd, &r);
    w = r.right - r.left;
    h = r.bottom - r.top;
    wdc = GetDC(wnd);
    mdc = CreateCompatibleDC(wdc);
    bmp = CreateCompatibleBitmap(wdc, w, h);
    SelectObject(mdc, bmp);
    PrintWindow(wnd, mdc, 2 /* PW_RENDERFULLCONTENT */);
    stride = (w * 3 + 3) & ~3;
    bits = (uint8_t *)malloc((size_t)stride * (size_t)h);
    if (bits) {
        memset(&bi, 0, sizeof bi);
        bi.biSize = sizeof bi; bi.biWidth = w; bi.biHeight = h; bi.biPlanes = 1; bi.biBitCount = 24;
        GetDIBits(mdc, bmp, 0, (UINT)h, bits, (BITMAPINFO *)&bi, DIB_RGB_COLORS);
        memset(&fh, 0, sizeof fh);
        fh.bfType = 0x4D42;
        fh.bfOffBits = sizeof fh + sizeof bi;
        fh.bfSize = fh.bfOffBits + (DWORD)(stride * h);
        if (!_wfopen_s(&f, file, L"wb") && f) {
            fwrite(&fh, sizeof fh, 1, f);
            fwrite(&bi, sizeof bi, 1, f);
            fwrite(bits, (size_t)stride * (size_t)h, 1, f);
            fclose(f);
        }
        free(bits);
    }
    DeleteObject(bmp);
    DeleteDC(mdc);
    ReleaseDC(wnd, wdc);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR args, int show)
{
    WNDCLASSW wc = {0};
    MSG m;
    int argc = 0, capture_page = -1;
    int capture_update = 0;
    const WCHAR *capture_file = NULL;
    WCHAR **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    (void)prev; (void)args;
    s_hinst = inst;
    GetModuleFileNameW(NULL, s_dir, MAX_PATH);
    parent_of(s_dir, s_dir);
    join(s_ini, s_dir, L"anthology_launcher.ini");
    if (argv && argc >= 4) {
        WCHAR why[300], cmd[MAX_PATH * 2 + 40], exe[MAX_PATH];
        if (!wcscmp(argv[1], L"--check")) {
            int ok = check_game(argv[2], why, 300);
            swprintf_s(cmd, MAX_PATH * 2 + 40, L"%s: %s", ok ? L"ok" : L"no", why);
            return write_result(argv[3], cmd) || !ok;
        }
        if (!wcscmp(argv[1], L"--installer-check")) {
            int ok = check_installer(argv[2], why, 300);
            swprintf_s(cmd, MAX_PATH * 2 + 40, L"%s: %s", ok ? L"ok" : L"no", why);
            return write_result(argv[3], cmd) || !ok;
        }
        if (!wcscmp(argv[1], L"--original-xex") && argc >= 5) {
            WCHAR err[600];
            int ok = anth_copy_original_xex(argv[2], argv[3], err, 600);
            return write_result(argv[4], ok ? L"ok" : err) || !ok;
        }
        if (!wcscmp(argv[1], L"--install-saves") && argc >= 5) {
            int saves, dlc, skipped, ok = install_saves_dlc(argv[2], argv[3], &saves, &dlc, &skipped);
            swprintf_s(cmd, MAX_PATH * 2 + 40, L"%s: %d saves, %d dlc, %d skipped", ok ? L"ok" : L"no", saves, dlc, skipped);
            return write_result(argv[4], cmd) || !ok;
        }
        if (!wcscmp(argv[1], L"--command")) {
            join(exe, s_dir, GAME_EXE);
            game_command(cmd, MAX_PATH * 2 + 40, exe, argv[2]);
            return write_result(argv[3], cmd);
        }
        if (!wcscmp(argv[1], L"--report-send") && argc >= 4) {   /* <answers file> <out file>: no window */
            WCHAR server[300], text[500];
            int ok;
            report_server(server, 300);
            ok = anth_report_send_file(s_dir, PORT_VERSION, server, argv[2], text, 500);
            return write_result(argv[3], text) || !ok;
        }
        if (!wcscmp(argv[1], L"--capture")) {
            capture_page = !wcscmp(argv[2], L"settings") ? PAGE_SETTINGS : !wcscmp(argv[2], L"setup") ? PAGE_SETUP
                         : !wcscmp(argv[2], L"report") ? PAGE_REPORT : !wcscmp(argv[2], L"myreports") ? PAGE_MY_REPORTS
                         : !wcscmp(argv[2], L"phone") ? PAGE_PHONE : PAGE_PLAY;
            capture_file = argv[3];
            capture_update = !wcscmp(argv[2], L"update");
        }
    }
    /* Test aid: --adb-install <out file> - Install to phone without the window (the result in the file). */
    if (argv && argc >= 3 && !wcscmp(argv[1], L"--adb-install")) {
        WCHAR err[600] = L"";
        int ok;
        GetPrivateProfileStringW(L"Anthology", L"GameFolder", L"", s_game, MAX_PATH, s_ini);
        ok = phone_install(phone_console, err, 600);
        return write_result(argv[2], ok ? err : err[0] ? err : L"failed") || !ok;
    }
    /* Test aid: --phone-check <out file> - what Install to phone would need, without touching a phone. */
    if (argv && argc >= 3 && !wcscmp(argv[1], L"--phone-check")) {
        WCHAR apk[MAX_PATH], m[MAX_PATH], why[300], r[1200];
        int game, xex, app, adb;
        GetPrivateProfileStringW(L"Anthology", L"GameFolder", L"", s_game, MAX_PATH, s_ini);
        game = check_game(s_game, why, 300);
        xex = join(m, s_dir, L"Original\\default.xex") && anth_xex_ok(m);
        app = phone_apk(apk);
        adb = adb_find(s_dir, s_adb, MAX_PATH);
        swprintf_s(r, 1200, L"game %d (%s)\r\nxex %d\r\napk %d (%s)\r\nadb %d (%s)", game, why, xex, app, apk, adb,
                   adb ? s_adb : L"would download");
        return write_result(argv[2], r) || !(game && xex && app);
    }
    /* (a test aid with too few arguments, or an unknown one: never the window) */
    if (argv && argc >= 2 && argv[1][0] == L'-' && argv[1][1] == L'-' && !capture_file && wcscmp(argv[1], L"--updated"))
        return 2;
    update_place_new_updater();
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    InitCommonControls();
    theme_init();
    GetPrivateProfileStringW(L"Anthology", L"Iso", L"", s_iso, MAX_PATH, s_ini);
    GetPrivateProfileStringW(L"Anthology", L"Installer", L"", s_inst, MAX_PATH, s_ini);
    GetPrivateProfileStringW(L"Anthology", L"GameFolder", L"", s_game, MAX_PATH, s_ini);
    add_missing_dlc();
    s_page_brush = CreateSolidBrush(C_PAGE);
    s_card_brush = CreateSolidBrush(C_CARD);
    s_field_brush = CreateSolidBrush(C_FIELD);
    s_icon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = s_icon;
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"SvRAnthologyLauncher";
    RegisterClassW(&wc);
    s_wnd = CreateWindowW(wc.lpszClassName, WINDOW_TITLE, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                          CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, inst, NULL);
    s_dpi = (int)GetDpiForWindow(s_wnd);
    {
        RECT r = { 0, 0, S(CLIENT_W), S(CLIENT_H) };
        BOOL dark = TRUE;
        AdjustWindowRectExForDpi(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, 0, (UINT)s_dpi);
        SetWindowPos(s_wnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        DwmSetWindowAttribute(s_wnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof dark);
    }
    make_fonts();
    build_window();
    build_report_pages();
    check(ID_CLOSE_ON_START, GetPrivateProfileIntW(L"Anthology", L"CloseOnStart", 0, s_ini));
    {
        char v[16] = "";
        int pick = 0, j;
        get_toml("anthology_version", v, sizeof v);
        for (j = 0; j < N_VERSIONS; j++)
            if (!_stricmp(v, k_versions[j].key)) pick = j;
        SendDlgItemMessageW(s_wnd, ID_VERSION, CB_SETCURSEL, (WPARAM)pick, 0);
    }
    settings_load();
    /* (first start, or not set up: Setup) */
    {
        WCHAR why[300];
        show_page(capture_page >= 0 ? capture_page : check_game(s_game, why, 300) ? PAGE_PLAY : PAGE_SETUP);
    }
    if (capture_update) {   /* (test aid: as if GitHub had a newer release, nothing asked of it) */
        static UpdateInfo fake;
        wcscpy_s(fake.version, 32, L"9.9.9");
        wnd_proc(s_wnd, WM_UPDATE_FOUND, 0, (LPARAM)&fake);
    }
    if (capture_file) {
        /* Drawn off-screen, never activated (tests run beside the user's work). */
        SetWindowPos(s_wnd, NULL, -8000, -8000, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(s_wnd, SW_SHOWNOACTIVATE);
        UpdateWindow(s_wnd);
        if (capture_page == PAGE_MY_REPORTS) {   /* (the server's answer in the picture) */
            const ULONGLONG until = GetTickCount64() + 2500;
            MSG m;
            while (GetTickCount64() < until) {
                while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
                Sleep(20);
            }
            UpdateWindow(s_wnd);
        }
        capture_wnd(s_wnd, capture_file);
        DestroyWindow(s_wnd);
        return 0;
    }
    ShowWindow(s_wnd, SW_SHOWNORMAL);
    UpdateWindow(s_wnd);
    CloseHandle(CreateThread(NULL, 0, update_check_thread, NULL, 0, NULL));
    /* The credit first: on the first start, who made the Anthology. */
    if (!GetPrivateProfileIntW(L"Anthology", L"CreditsShown", 0, s_ini)) {
        credits();
        WritePrivateProfileStringW(L"Anthology", L"CreditsShown", L"1", s_ini);
    }
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(s_wnd, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    return 0;
}
