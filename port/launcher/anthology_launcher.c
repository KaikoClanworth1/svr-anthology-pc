/* SvR Anthology Launcher (branch anthology): sets up and starts SVR Anthology by Yumboy11 on the
 * SvR 2011 PC recomp.
 *
 * SVR Anthology is Yumboy11's mod for the Xbox 360 version of WWE SmackDown vs. Raw 2011. Nothing of it is in
 * this program or this port: the player uses their own SvR 2011 ISO (or extracted game folder) and
 * Yumboy11's own installer, which makes a new "WWE SmackDown vs Raw Anthology" folder next to itself. This
 * launcher only starts his installer (in its own window, the player answers it) and then runs the port's game
 * (svr2011.exe, the Anthology build: no mods, no online) with --game_data_root=<that folder>. It never moves,
 * changes or writes anything in the installer's folder or the Anthology's folder.
 *
 * Settings: anthology_launcher.ini beside the program. The game's own settings: svr2011.toml beside it.
 *
 * Test aids (no window): --check <anthology folder> <out file>  - the folder check's result
 *                        --command <anthology folder> <out file> - the game's command line
 *                        --installer-check <installer folder> <out file>
 */
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#ifndef PORT_VERSION
#define PORT_VERSION L"dev"
#endif
#define WINDOW_TITLE L"SVR Anthology by Yumboy11 - SvR 2011 PC recomp launcher"
#define GAME_EXE L"svr2011.exe"
#define ANTHOLOGY_GAME L"WWE SmackDown vs Raw Anthology"
#define ANTHOLOGY_SAVES L"WWE SmackDown vs Raw Anthology Saves and DLC"
#define INSTALLER_EXE L"Anthology_Installer.exe"
#define INSTALLER_DATA L"Anthology_Install_data.dat"

enum {
    ID_ISO_EDIT = 100, ID_ISO_FILE, ID_ISO_FOLDER,
    ID_INST_EDIT, ID_INST_BROWSE, ID_INST_STATUS, ID_INST_RUN, ID_INST_NOTE,
    ID_GAME_EDIT, ID_GAME_BROWSE, ID_GAME_STATUS,
    ID_SAVES_OPEN, ID_SAVES_README, ID_OUR_FOLDERS,
    ID_FULLSCREEN, ID_PLAY, ID_CREDITS, ID_README, ID_STATUS
};
#define WM_INSTALLER_DONE (WM_APP + 1)

static HWND s_wnd;
static HFONT s_font, s_bold, s_big, s_title;
static WCHAR s_dir[MAX_PATH];  /* the launcher's folder (the port's files) */
static WCHAR s_ini[MAX_PATH];
static WCHAR s_iso[MAX_PATH], s_inst[MAX_PATH], s_game[MAX_PATH];
static HANDLE s_installer;     /* Yumboy11's installer while it runs */
static HANDLE s_game_proc;

/* ---- files and folders ---- */

static int join(WCHAR *out, const WCHAR *a, const WCHAR *b)
{
    size_t n = wcslen(a);
    return swprintf_s(out, MAX_PATH, n && a[n - 1] == L'\\' ? L"%s%s" : L"%s\\%s", a, b) > 0;
}
static int file_exists(const WCHAR *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
static int dir_exists(const WCHAR *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
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

/* svr2011.toml beside the game: one flat key = value line set (added when missing). */
static void set_toml(const WCHAR *key, const char *value)
{
    WCHAR path[MAX_PATH];
    char k[64], *data = NULL, *out;
    size_t len = 0, cap, o = 0, i = 0;
    int done = 0;
    FILE *f;
    if (!join(path, s_dir, L"svr2011.toml")) return;
    WideCharToMultiByte(CP_UTF8, 0, key, -1, k, sizeof k, NULL, NULL);
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
    cap = len + strlen(k) + strlen(value) + 8;
    out = malloc(cap);
    if (!out) { free(data); return; }
    while (i < len) {
        size_t e = i, kl = strlen(k);
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
static int toml_true(const WCHAR *key)
{
    WCHAR path[MAX_PATH];
    char line[256], k[64];
    int on = 0;
    FILE *f;
    if (!join(path, s_dir, L"svr2011.toml") || _wfopen_s(&f, path, L"rb") || !f) return 0;
    WideCharToMultiByte(CP_UTF8, 0, key, -1, k, sizeof k, NULL, NULL);
    while (fgets(line, sizeof line, f))
        if (!strncmp(line, k, strlen(k)) && (line[strlen(k)] == ' ' || line[strlen(k)] == '='))
            on = strstr(line, "true") != NULL;
    fclose(f);
    return on;
}

/* ---- the window ---- */

static void set_text(int id, const WCHAR *t) { SetDlgItemTextW(s_wnd, id, t); }

static void save_settings(void)
{
    WritePrivateProfileStringW(L"Anthology", L"Iso", s_iso, s_ini);
    WritePrivateProfileStringW(L"Anthology", L"Installer", s_inst, s_ini);
    WritePrivateProfileStringW(L"Anthology", L"GameFolder", s_game, s_ini);
}

static void refresh(void)
{
    WCHAR why[300], t[400];
    int inst_ok = check_installer(s_inst, why, 300), game_ok;
    set_text(ID_ISO_EDIT, s_iso);
    set_text(ID_INST_EDIT, s_inst);
    swprintf_s(t, 400, L"Installer: %s", why);
    set_text(ID_INST_STATUS, t);
    EnableWindow(GetDlgItem(s_wnd, ID_INST_RUN), inst_ok && s_iso[0] && !s_installer);
    set_text(ID_GAME_EDIT, s_game);
    game_ok = check_game(s_game, why, 300);
    swprintf_s(t, 400, L"Anthology folder: %s", why);
    set_text(ID_GAME_STATUS, t);
    EnableWindow(GetDlgItem(s_wnd, ID_PLAY), game_ok && !s_installer);
    {
        WCHAR saves[MAX_PATH], parent[MAX_PATH];
        int there;
        parent_of(parent, s_game);
        there = s_game[0] && join(saves, parent, ANTHOLOGY_SAVES) && dir_exists(saves);
        EnableWindow(GetDlgItem(s_wnd, ID_SAVES_OPEN), there);
        EnableWindow(GetDlgItem(s_wnd, ID_SAVES_README), there);
    }
    EnableWindow(GetDlgItem(s_wnd, ID_README), s_inst[0] && has_file(s_inst, L"Readme.txt"));
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
    WCHAR exe[MAX_PATH], cmd[MAX_PATH * 2 + 8], m[1600], why[300], out[MAX_PATH];
    STARTUPINFOW si = {sizeof si};
    PROCESS_INFORMATION pi;
    if (!check_installer(s_inst, why, 300) || !s_iso[0]) return;
    if (inside(s_inst, s_iso)) {
        MessageBoxW(s_wnd, L"Yumboy11's installer can't be inside the original game's folder. Put the installer "
                           L"folder somewhere else.", WINDOW_TITLE, MB_ICONWARNING);
        return;
    }
    join(out, s_inst, ANTHOLOGY_GAME);
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
    set_text(ID_STATUS, L"Yumboy11's installer is running in its own window...");
    refresh();
}

static void installer_done(DWORD code)
{
    WCHAR out[MAX_PATH], why[300], m[400];
    CloseHandle(s_installer);
    s_installer = NULL;
    if (join(out, s_inst, ANTHOLOGY_GAME) && check_game(out, why, 300)) {
        wcscpy_s(s_game, MAX_PATH, out);
        save_settings();
        set_text(ID_STATUS, L"The Anthology is installed. Set up its saves and DLC (step 4), then press PLAY.");
    } else {
        swprintf_s(m, 400, L"The installer closed (code %lu) and there is no complete Anthology folder yet.", code);
        set_text(ID_STATUS, m);
    }
    refresh();
}

static void open_path(const WCHAR *p) { ShellExecuteW(s_wnd, L"open", p, NULL, NULL, SW_SHOWNORMAL); }

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
    set_toml(L"fullscreen", IsDlgButtonChecked(s_wnd, ID_FULLSCREEN) == BST_CHECKED ? "true" : "false");
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
                L"This launcher and the game program are the SvR 2011 PC recomp (" PORT_VERSION
                L"), made with the permission of Yumboy11, who allows it to be used with his Anthology as "
                L"long as players use their own ISO and his own installer, and he is fully credited.\n\n"
                L"WWE SmackDown vs. Raw 2011 belongs to its owners.",
                L"Credits - SVR Anthology by Yumboy11", MB_ICONINFORMATION);
}

static HWND add(const WCHAR *cls, const WCHAR *text, DWORD style, int x, int y, int w, int h, int id, HFONT f)
{
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, s_wnd, (HMENU)(INT_PTR)id,
                             GetModuleHandleW(NULL), NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE);
    return c;
}

static void build_window(void)
{
    const int L = 20, W = 680;
    int y = 128;
    add(L"STATIC", L"1. Your WWE SmackDown vs. Raw 2011 Xbox 360 disc image (ISO) or extracted game folder (unmodified)",
        0, L, y, W, 18, -1, s_bold);
    y += 22;
    add(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, L, y, W - 210, 24, ID_ISO_EDIT, s_font);
    add(L"BUTTON", L"ISO...", BS_PUSHBUTTON, L + W - 200, y, 95, 24, ID_ISO_FILE, s_font);
    add(L"BUTTON", L"Folder...", BS_PUSHBUTTON, L + W - 100, y, 100, 24, ID_ISO_FOLDER, s_font);
    y += 36;
    add(L"STATIC", L"2. Yumboy11's \"SVR Anthology 1.0 Installer\" folder (the one you downloaded from him)", 0, L, y, W,
        18, -1, s_bold);
    y += 22;
    add(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, L, y, W - 110, 24, ID_INST_EDIT, s_font);
    add(L"BUTTON", L"Browse...", BS_PUSHBUTTON, L + W - 100, y, 100, 24, ID_INST_BROWSE, s_font);
    y += 28;
    add(L"STATIC", L"", 0, L, y, W, 18, ID_INST_STATUS, s_font);
    y += 26;
    add(L"BUTTON", L"3. Run Yumboy11's installer", BS_PUSHBUTTON, L, y, 230, 30, ID_INST_RUN, s_bold);
    add(L"STATIC",
        L"His installer opens in its own window. It makes the Anthology next to itself, in the installer folder "
        L"(about 5 times the size of SvR 2011). Already installed? Choose its folder below.",
        0, L + 240, y - 2, W - 240, 36, ID_INST_NOTE, s_font);
    y += 44;
    add(L"STATIC", L"The \"" ANTHOLOGY_GAME L"\" folder his installer made", 0, L, y, W, 18, -1, s_bold);
    y += 22;
    add(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, L, y, W - 110, 24, ID_GAME_EDIT, s_font);
    add(L"BUTTON", L"Browse...", BS_PUSHBUTTON, L + W - 100, y, 100, 24, ID_GAME_BROWSE, s_font);
    y += 28;
    add(L"STATIC", L"", 0, L, y, W, 18, ID_GAME_STATUS, s_font);
    y += 30;
    add(L"STATIC", L"4. Saves and DLC: follow Yumboy11's readme in his \"Saves and DLC\" folder", 0, L, y, W, 18, -1, s_bold);
    y += 22;
    add(L"BUTTON", L"His readme", BS_PUSHBUTTON, L, y, 130, 26, ID_SAVES_README, s_font);
    add(L"BUTTON", L"His Saves and DLC folder", BS_PUSHBUTTON, L + 140, y, 190, 26, ID_SAVES_OPEN, s_font);
    add(L"BUTTON", L"This game's Saves and DLC folders", BS_PUSHBUTTON, L + 340, y, 240, 26, ID_OUR_FOLDERS, s_font);
    y += 42;
    add(L"BUTTON", L"Full screen", BS_AUTOCHECKBOX, L, y + 8, 140, 22, ID_FULLSCREEN, s_font);
    add(L"BUTTON", L"PLAY", BS_DEFPUSHBUTTON, L + W - 220, y, 220, 40, ID_PLAY, s_big);
    add(L"BUTTON", L"Credits", BS_PUSHBUTTON, L + 160, y + 6, 100, 28, ID_CREDITS, s_font);
    add(L"BUTTON", L"Yumboy11's Readme", BS_PUSHBUTTON, L + 270, y + 6, 160, 28, ID_README, s_font);
    y += 52;
    add(L"STATIC", L"", 0, L, y, W, 36, ID_STATUS, s_font);
    CheckDlgButton(s_wnd, ID_FULLSCREEN, toml_true(L"fullscreen") ? BST_CHECKED : BST_UNCHECKED);
}

/* The banner: the credit first. */
static void paint_banner(HDC dc, RECT rc)
{
    RECT b = {0, 0, rc.right, 116}, r;
    HBRUSH bg = CreateSolidBrush(RGB(18, 18, 22));
    FillRect(dc, &b, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(235, 190, 60));
    SelectObject(dc, s_title);
    r = b; r.left = 20; r.top = 10;
    DrawTextW(dc, L"SVR ANTHOLOGY", -1, &r, DT_LEFT | DT_TOP | DT_SINGLELINE);
    SetTextColor(dc, RGB(255, 255, 255));
    SelectObject(dc, s_big);
    r.top = 60;
    DrawTextW(dc, L"by Yumboy11", -1, &r, DT_LEFT | DT_TOP | DT_SINGLELINE);
    SetTextColor(dc, RGB(170, 170, 180));
    SelectObject(dc, s_font);
    r.top = 92;
    DrawTextW(dc, L"Yumboy11's SVR Anthology 1.0 on the SvR 2011 PC recomp - your own ISO, his own installer. "
                  L"All credit for the Anthology: Yumboy11.", -1, &r, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static LRESULT CALLBACK wnd_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_ISO_FILE: pick_iso(); save_settings(); refresh(); break;
        case ID_ISO_FOLDER:
            if (pick_folder(L"Your extracted SvR 2011 Xbox 360 game folder (with default.xex)", s_iso)) save_settings();
            refresh();
            break;
        case ID_INST_BROWSE:
            if (pick_folder(L"Yumboy11's \"SVR Anthology 1.0 Installer\" folder", s_inst)) save_settings();
            refresh();
            break;
        case ID_INST_RUN: run_installer(); break;
        case ID_GAME_BROWSE:
            if (pick_folder(L"The \"" ANTHOLOGY_GAME L"\" folder", s_game)) save_settings();
            refresh();
            break;
        case ID_SAVES_README: open_saves_readme(); break;
        case ID_SAVES_OPEN: {
            WCHAR parent[MAX_PATH], p[MAX_PATH];
            parent_of(parent, s_game);
            if (join(p, parent, ANTHOLOGY_SAVES)) open_path(p);
            break;
        }
        case ID_OUR_FOLDERS: {
            WCHAR p[MAX_PATH];
            if (join(p, s_dir, L"Saves")) { CreateDirectoryW(p, NULL); open_path(p); }
            if (join(p, s_dir, L"DLC")) { CreateDirectoryW(p, NULL); open_path(p); }
            break;
        }
        case ID_PLAY: play(); break;
        case ID_CREDITS: credits(); break;
        case ID_README: {
            WCHAR p[MAX_PATH];
            if (join(p, s_inst, L"Readme.txt")) open_path(p);
            break;
        }
        }
        return 0;
    case WM_INSTALLER_DONE: installer_done((DWORD)wp); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        RECT rc;
        HDC dc = BeginPaint(w, &ps);
        GetClientRect(w, &rc);
        paint_banner(dc, rc);
        EndPaint(w, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)wp, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
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

static HFONT font(int px, int weight)
{
    return CreateFontW(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
}

/* Test aids: a result into a file (UTF-8), no window. */
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

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR args, int show)
{
    WNDCLASSW wc = {0};
    MSG m;
    int argc = 0;
    WCHAR **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    (void)prev; (void)args;
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
        if (!wcscmp(argv[1], L"--command")) {
            join(exe, s_dir, GAME_EXE);
            game_command(cmd, MAX_PATH * 2 + 40, exe, argv[2]);
            return write_result(argv[3], cmd);
        }
    }
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    InitCommonControls();
    GetPrivateProfileStringW(L"Anthology", L"Iso", L"", s_iso, MAX_PATH, s_ini);
    GetPrivateProfileStringW(L"Anthology", L"Installer", L"", s_inst, MAX_PATH, s_ini);
    GetPrivateProfileStringW(L"Anthology", L"GameFolder", L"", s_game, MAX_PATH, s_ini);
    s_font = font(15, FW_NORMAL);
    s_bold = font(15, FW_SEMIBOLD);
    s_big = font(24, FW_BOLD);
    s_title = font(44, FW_HEAVY);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = L"SvRAnthologyLauncher";
    RegisterClassW(&wc);
    {
        RECT r = {0, 0, 720, 660};
        AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
        s_wnd = CreateWindowW(wc.lpszClassName, WINDOW_TITLE, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    }
    build_window();
    refresh();
    ShowWindow(s_wnd, show);
    UpdateWindow(s_wnd);
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
