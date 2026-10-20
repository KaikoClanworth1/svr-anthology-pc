/* SvR Anthology launcher: the original disc's default.xex (anthology_disc.c). */
#pragma once
#include <windows.h>

/* 1 if `file` is the original SvR 2011 disc's default.xex (size + SHA-256). */
int anth_xex_ok(const WCHAR *file);
/* Copies default.xex out of `source` (an .iso or an extracted game folder) to <dest_dir>\Original\default.xex,
 * after checking it is the original disc's. 0 + err when not. */
int anth_copy_original_xex(const WCHAR *source, const WCHAR *dest_dir, WCHAR *err, size_t errn);
