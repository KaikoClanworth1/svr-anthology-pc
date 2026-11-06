/* SVR Anthology launcher - Report a problem (see anth_report.h). */

#include "anth_report.h"

#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "report.h"
#include "report_receipts.h"
#include "report_send.h"
#include "report_urls.h"

enum { F_INTRO, F_KIND_L, F_KIND, F_OFTEN_L, F_OFTEN, F_WHAT_L, F_WHAT, F_BEFORE_L, F_BEFORE, F_CONTACT_L, F_CONTACT,
       F_PICS, F_PICS_L, F_PICS_CLEAR, F_SAVE, F_SEND, F_STATUS, F_COUNT };

static const WCHAR *const k_kinds[] = {
    L"The game crashed (closed by itself)", L"The game froze or hung", L"Graphics look wrong", L"Sound or music",
    L"Controller or keyboard", L"Saves or created content", L"DLC or unlocks", L"Installing or Setup",
    L"The launcher", L"The phone install", L"Something else" };
static const WCHAR *const k_often[] = { L"Every time", L"Sometimes", L"It happened once" };

/* What one send needs: its thread owns this copy. */
typedef struct {
    WCHAR dir[MAX_PATH], version[40], server[300];
    char kind[120], often[40], what[6000], before[6000], contact[200];
    WCHAR pics[REPORT_MAX_IMAGES][MAX_PATH];
    int n_pics, with_save, ok;
    ReportResult r;
    WCHAR zip[MAX_PATH];
} Send;

static HWND s_wnd, s_c[F_COUNT];
static int s_base, s_busy, s_npics;
static WCHAR s_dir[MAX_PATH], s_version[40], s_pics[REPORT_MAX_IMAGES][MAX_PATH];
static anth_server_fn s_server;

static void utf8(const WCHAR *in, char *out, int n)
{
    if (!WideCharToMultiByte(CP_UTF8, 0, in, -1, out, n, NULL, NULL)) out[0] = 0;
    out[n - 1] = 0;
}

static void text_of(int i, char *out, int n)
{
    WCHAR *w = (WCHAR *)calloc((size_t)n, sizeof(WCHAR));
    out[0] = 0;
    if (!w) return;
    GetWindowTextW(s_c[i], w, n);
    utf8(w, out, n);
    free(w);
}

/* The zip, the report, the receipt. Blocks. */
static void do_send(Send *s)
{
    char ver[40], sig[320] = "", desc[13000], *json;
    int logs = 0, crashes = 0, i;
    ReportUpload u;
    utf8(s->version, ver, sizeof ver);
    snprintf(desc, sizeof desc,
             "SVR Anthology PC %s\r\nProblem: %s\r\nHappens: %s\r\n\r\nWhat happened:\r\n%s\r\n\r\nJust before:\r\n%s\r\n",
             ver, s->kind, s->often, s->what, s->before[0] ? s->before : "(not said)");
    ZeroMemory(&s->r, sizeof s->r);
    if (!report_make_match(s->dir, s->version, desc, NULL, s->with_save, s->zip, MAX_PATH, &logs, &crashes)) {
        wcscpy_s(s->r.why, 300, L"The report could not be written (is the game folder read-only?).");
        return;
    }
    report_signature(s->dir, sig, sizeof sig);
    json = report_json("anthology", ver, s->kind, s->often, s->what, s->before, s->contact, sig, "");
    if (!json) {
        wcscpy_s(s->r.why, 300, L"Out of memory.");
        return;
    }
    ZeroMemory(&u, sizeof u);
    u.json = json;
    u.zip = s->zip;
    for (i = 0; i < s->n_pics; i++) u.images[i] = s->pics[i];
    u.n_images = s->n_pics;
    s->ok = report_send(s->server, &u, &s->r);
    free(json);
    if (s->ok) receipt_add(s->dir, "anthology", s->r.id, s->r.secret, s->kind, s->what);
}

static DWORD WINAPI send_thread(void *arg)
{
    Send *s = (Send *)arg;
    do_send(s);
    PostMessageW(s_wnd, WM_APP_ANTH_REPORT, 0, (LPARAM)s);
    return 0;
}

static void pics_text(void)
{
    WCHAR t[200] = L"No pictures";
    if (s_npics) swprintf_s(t, 200, L"%d picture%s", s_npics, s_npics == 1 ? L"" : L"s");
    SetWindowTextW(s_c[F_PICS_L], t);
    EnableWindow(s_c[F_PICS_CLEAR], s_npics > 0);
    EnableWindow(s_c[F_PICS], s_npics < REPORT_MAX_IMAGES);
}

static void add_pictures(void)
{
    static WCHAR buf[8192];
    OPENFILENAMEW of;
    ZeroMemory(&of, sizeof of);
    buf[0] = 0;
    of.lStructSize = sizeof of;
    of.hwndOwner = s_wnd;
    of.lpstrFilter = L"Pictures (PNG, JPEG, WebP)\0*.png;*.jpg;*.jpeg;*.webp\0";
    of.lpstrFile = buf;
    of.nMaxFile = 8192;
    of.lpstrTitle = L"Pictures for the report";
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&of)) {
        const WCHAR *p = buf + wcslen(buf) + 1;
        WCHAR why[300], full[MAX_PATH];
        if (!*p) p = NULL;   /* (one file: buf is its path; many: the folder, then the names) */
        do {
            if (p) swprintf_s(full, MAX_PATH, L"%s\\%s", buf, p);
            else wcscpy_s(full, MAX_PATH, buf);
            if (s_npics >= REPORT_MAX_IMAGES) break;
            if (report_image_ok(full, why, 300)) wcscpy_s(s_pics[s_npics++], MAX_PATH, full);
            else SetWindowTextW(s_c[F_STATUS], why);
            if (p) p += wcslen(p) + 1;
        } while (p && *p);
    }
    pics_text();
}

static void start_send(void)
{
    Send *s;
    HANDLE t;
    int i;
    const char *p;
    if (s_busy) return;
    s = (Send *)calloc(1, sizeof *s);
    if (!s) return;
    wcscpy_s(s->dir, MAX_PATH, s_dir);
    wcscpy_s(s->version, 40, s_version);
    s_server(s->server, 300);
    text_of(F_KIND, s->kind, sizeof s->kind);
    text_of(F_OFTEN, s->often, sizeof s->often);
    text_of(F_WHAT, s->what, sizeof s->what);
    text_of(F_BEFORE, s->before, sizeof s->before);
    text_of(F_CONTACT, s->contact, sizeof s->contact);
    p = s->what;
    while (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t') p++;
    if (!*p || report_text_has_url(s->what) || report_text_has_url(s->before) || report_text_has_url(s->contact)) {
        SetWindowTextW(s_c[F_STATUS], !*p ? L"Please say what happened first." : REPORT_NO_URLS_W);
        free(s);
        return;
    }
    for (i = 0; i < s_npics; i++) wcscpy_s(s->pics[i], MAX_PATH, s_pics[i]);
    s->n_pics = s_npics;
    s->with_save = IsDlgButtonChecked(s_wnd, s_base + F_SAVE) == BST_CHECKED;
    s_busy = 1;
    EnableWindow(s_c[F_SEND], FALSE);
    SetWindowTextW(s_c[F_STATUS], L"Sending...");
    t = CreateThread(NULL, 0, send_thread, s, 0, NULL);
    if (t) {
        CloseHandle(t);
    } else {
        s_busy = 0;
        EnableWindow(s_c[F_SEND], TRUE);
        SetWindowTextW(s_c[F_STATUS], L"The report could not be started.");
        free(s);
    }
}

void anth_report_build(HWND wnd, anth_add_fn add, int page, int id_base, const WCHAR *dir, const WCHAR *version,
                       anth_server_fn server)
{
    int i;
#define C(i, cls, text, style, x, y, w, h) (s_c[i] = add(page, cls, text, style, x, y, w, h, id_base + (i)))
    s_wnd = wnd;
    s_base = id_base;
    s_server = server;
    wcscpy_s(s_dir, MAX_PATH, dir);
    wcscpy_s(s_version, 40, version);
    C(F_INTRO, L"Static", L"Tell us what went wrong. The report also sends your newest logs, crash reports and settings, "
                          L"without passwords. Yumboy11's game files are never sent. Please report problems with this "
                          L"PC version here, not to Yumboy11.", SS_LEFT | SS_NOPREFIX, 16, 14, 690, 46);
    C(F_KIND_L, L"Static", L"What kind of problem", SS_LEFT, 16, 72, 160, 20);
    C(F_KIND, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 180, 68, 320, 300);
    for (i = 0; i < (int)(sizeof k_kinds / sizeof *k_kinds); i++)
        SendMessageW(s_c[F_KIND], CB_ADDSTRING, 0, (LPARAM)k_kinds[i]);
    SendMessageW(s_c[F_KIND], CB_SETCURSEL, 0, 0);
    C(F_OFTEN_L, L"Static", L"How often", SS_LEFT, 16, 106, 160, 20);
    C(F_OFTEN, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 180, 102, 320, 200);
    for (i = 0; i < (int)(sizeof k_often / sizeof *k_often); i++)
        SendMessageW(s_c[F_OFTEN], CB_ADDSTRING, 0, (LPARAM)k_often[i]);
    SendMessageW(s_c[F_OFTEN], CB_SETCURSEL, 0, 0);
    C(F_WHAT_L, L"Static", L"What happened?", SS_LEFT, 16, 138, 400, 20);
    C(F_WHAT, L"Edit", L"", ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_BORDER | WS_TABSTOP, 16, 160,
      692, 116);
    C(F_BEFORE_L, L"Static", L"What were you doing just before? (optional)", SS_LEFT, 16, 284, 400, 20);
    C(F_BEFORE, L"Edit", L"", ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_BORDER | WS_TABSTOP, 16, 306,
      692, 76);
    C(F_CONTACT_L, L"Static", L"Contact (optional)", SS_LEFT, 16, 396, 160, 20);
    C(F_CONTACT, L"Edit", L"", ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 180, 392, 320, 26);
    C(F_PICS, L"Button", L"Add pictures...", BS_PUSHBUTTON | WS_TABSTOP, 16, 430, 150, 32);
    C(F_PICS_L, L"Static", L"No pictures", SS_LEFT | SS_NOPREFIX, 180, 436, 120, 20);
    C(F_PICS_CLEAR, L"Button", L"Clear", BS_PUSHBUTTON | WS_TABSTOP, 304, 430, 80, 32);
    C(F_SAVE, L"Button", L"Include my main save (not Created Superstars, Paint Tool, logos or replays)",
      BS_AUTOCHECKBOX | WS_TABSTOP, 16, 472, 600, 24);
    C(F_SEND, L"Button", L"Send report", BS_DEFPUSHBUTTON | WS_TABSTOP, 16, 508, 190, 44);
    C(F_STATUS, L"Static", L"", SS_LEFT | SS_NOPREFIX, 222, 508, 486, 60);
#undef C
    pics_text();
}

int anth_report_command(int id, int code)
{
    const int k = id - s_base;
    if (k < 0 || k >= F_COUNT) return 0;
    if (code == BN_CLICKED) {
        if (k == F_SEND) start_send();
        else if (k == F_PICS) add_pictures();
        else if (k == F_PICS_CLEAR) { s_npics = 0; pics_text(); }
    }
    return 1;
}

int anth_report_message(UINT m, WPARAM wp, LPARAM lp)
{
    Send *s = (Send *)lp;
    WCHAR t[600], id[64];
    (void)wp;
    if (m != WM_APP_ANTH_REPORT) return 0;
    s_busy = 0;
    EnableWindow(s_c[F_SEND], TRUE);
    if (s->ok) {
        MultiByteToWideChar(CP_UTF8, 0, s->r.id, -1, id, 64);
        swprintf_s(t, 600, L"Your report reached us. Thank you! Report id: %s. My reports shows what becomes of it.", id);
        SetWindowTextW(s_c[F_WHAT], L"");
        SetWindowTextW(s_c[F_BEFORE], L"");
        s_npics = 0;
        pics_text();
    } else {
        swprintf_s(t, 600, L"Not sent: %s", s->r.why[0] ? s->r.why : L"no answer from the server.");
    }
    SetWindowTextW(s_c[F_STATUS], t);
    free(s);
    return 1;
}

int anth_report_send_file(const WCHAR *dir, const WCHAR *version, const WCHAR *server, const WCHAR *answers,
                          WCHAR *out_text, int out_n)
{
    FILE *f;
    Send *s = (Send *)calloc(1, sizeof *s);
    char *lines[5];
    int i, ok;
    if (!s) return 0;
    wcscpy_s(s->dir, MAX_PATH, dir);
    wcscpy_s(s->version, 40, version);
    wcscpy_s(s->server, 300, server);
    lines[0] = s->kind, lines[1] = s->often, lines[2] = s->what, lines[3] = s->before, lines[4] = s->contact;
    if (_wfopen_s(&f, answers, L"rb") || !f) {
        wcscpy_s(out_text, (size_t)out_n, L"no answers file");
        free(s);
        return 0;
    }
    for (i = 0; i < 5; i++) {
        const int cap = i == 0 ? 120 : i == 1 ? 40 : i == 4 ? 200 : 6000;
        if (fgets(lines[i], cap, f)) lines[i][strcspn(lines[i], "\r\n")] = 0;
    }
    fclose(f);
    do_send(s);
    ok = s->ok;
    if (ok) {
        WCHAR id[64];
        MultiByteToWideChar(CP_UTF8, 0, s->r.id, -1, id, 64);
        swprintf_s(out_text, (size_t)out_n, L"ok %s", id);
    } else {
        swprintf_s(out_text, (size_t)out_n, L"failed: %s", s->r.why);
    }
    free(s);
    return ok;
}
