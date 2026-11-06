/* SVR Anthology launcher - Report a problem (anth_report.c): the player's answers, the newest logs, crash reports
   and settings, and any pictures go to <server>/api/reports as app "anthology". The dashboard keeps them on its own
   SvR Anthology page. The receipts go to UserData\reports.json, so My reports (reports_tab.c) can follow them. */
#pragma once

#include <windows.h>

typedef HWND (*anth_add_fn)(int page, const WCHAR *cls, const WCHAR *text, DWORD style, int x, int y, int w, int h,
                            int id);
typedef void (*anth_server_fn)(WCHAR *out, int n);

#define WM_APP_ANTH_REPORT (WM_APP + 22)

/* The page's controls (ids id_base .. id_base + 19). dir: the folder holding logs\, crashes\ and svr2011.toml. */
void anth_report_build(HWND wnd, anth_add_fn add, int page, int id_base, const WCHAR *dir, const WCHAR *version,
                       anth_server_fn server);
int anth_report_command(int id, int code);
/* The send's answer (WM_APP_ANTH_REPORT); 1 when it was its. */
int anth_report_message(UINT m, WPARAM wp, LPARAM lp);

/* Test aid, no window: answers file (UTF-8 lines: kind, how often, what, before, contact), and the result in out_text.
   1 when the server took it. */
int anth_report_send_file(const WCHAR *dir, const WCHAR *version, const WCHAR *server, const WCHAR *answers,
                          WCHAR *out_text, int out_n);
