/* SvR Anthology launcher: the original disc's default.xex from the player's own ISO (or extracted folder),
 * for the game's Original\default.xex (anthology.h: this recompiled game runs the original program, the
 * Anthology's data). The XDVDFS reader is the normal launcher's (svr2011_launcher.c), copied as it is. */
#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "anthology_disc.h"

/* ── the disc image: XDVDFS ────────────────────────────────────────────── */

typedef struct {
    WCHAR    path[MAX_PATH];     /* relative, backslashes */
    uint32_t sector, size;
    int      is_dir;
} DiscFile;

typedef struct {
    HANDLE    f;
    uint64_t  base;              /* partition offset in the image */
    DiscFile *files;             /* directories and files, in table order */
    int       count, cap, nfiles;
    uint64_t  total;             /* bytes in files */
} Disc;

static int disc_read(Disc *d, uint64_t off, void *buf, DWORD n)
{
    LARGE_INTEGER li;
    DWORD got = 0;
    li.QuadPart = (LONGLONG)(d->base + off);
    return SetFilePointerEx(d->f, li, NULL, FILE_BEGIN) && ReadFile(d->f, buf, n, &got, NULL) && got == n;
}

static int disc_add(Disc *d, const WCHAR *path, uint32_t sector, uint32_t size, int is_dir)
{
    if (d->count == d->cap) {
        int cap = d->cap ? d->cap * 2 : 256;
        DiscFile *n = (DiscFile *)realloc(d->files, sizeof(DiscFile) * (size_t)cap);
        if (!n)
            return 0;
        d->files = n;
        d->cap = cap;
    }
    wcscpy_s(d->files[d->count].path, MAX_PATH, path);
    d->files[d->count].sector = sector;
    d->files[d->count].size = size;
    d->files[d->count].is_dir = is_dir;
    d->count++;
    if (!is_dir) {
        d->nfiles++;
        d->total += size;
    }
    return 1;
}

/* One directory table, entry by entry in the order they are stored:
 *   u16 left, u16 right, u32 sector, u32 size, u8 attributes, u8 name length, name
 * 4-byte aligned; 0xFFFF where an entry would start pads to the next sector. */
static int disc_dir(Disc *d, uint32_t sector, uint32_t size, const WCHAR *prefix, int depth)
{
    uint8_t *buf;
    uint32_t off = 0;
    int ok = 1;
    if (depth > 32 || size > (16u << 20) || d->count > 1000000)
        return 0;
    buf = (uint8_t *)malloc(size);
    if (!buf)
        return 0;
    if (!disc_read(d, (uint64_t)sector * 2048, buf, size)) {
        free(buf);
        return 0;
    }
    while (ok && off + 14 <= size) {
        const uint8_t *e = buf + off;
        uint16_t left = (uint16_t)(e[0] | e[1] << 8);
        uint32_t sec, sz;
        uint8_t attr = e[12], nlen = e[13];
        WCHAR name[256], path[MAX_PATH];
        int i;
        if (left == 0xFFFF || nlen == 0) {          /* padding to the next sector */
            off = (off / 2048 + 1) * 2048;
            continue;
        }
        if (off + 14 + nlen > size) {
            ok = 0;
            break;
        }
        memcpy(&sec, e + 4, 4);
        memcpy(&sz, e + 8, 4);
        for (i = 0; i < nlen; i++)
            name[i] = (WCHAR)e[14 + i];
        name[nlen] = 0;
        if (wcschr(name, L'\\') || wcschr(name, L'/') || wcschr(name, L':') || !wcscmp(name, L"..")
                || !wcscmp(name, L".")) {
            ok = 0;                                 /* unsafe or broken */
            break;
        }
        if (wcslen(prefix) + 1 + nlen >= MAX_PATH) {
            ok = 0;
            break;
        }
        swprintf_s(path, MAX_PATH, prefix[0] ? L"%s\\%s" : L"%s%s", prefix, name);
        if (attr & 0x10) {
            ok = disc_add(d, path, sec, sz, 1) && (!sz || disc_dir(d, sec, sz, path, depth + 1));
        } else {
            ok = disc_add(d, path, sec, sz, 0);
        }
        off = (off + 14 + nlen + 3) & ~3u;
    }
    free(buf);
    return ok;
}

static void disc_close(Disc *d)
{
    if (d->f && d->f != INVALID_HANDLE_VALUE)
        CloseHandle(d->f);
    free(d->files);
    memset(d, 0, sizeof *d);
}

/* Open an image and list its files; on failure err explains why. */
static int disc_open(Disc *d, const WCHAR *image, WCHAR *err, size_t errn)
{
    static const uint64_t bases[] = { 0, 0x18300000ull, 0xFD90000ull, 0x2080000ull };
    uint8_t vd[2048];
    int i;
    memset(d, 0, sizeof *d);
    d->f = CreateFileW(image, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (d->f == INVALID_HANDLE_VALUE) {
        swprintf_s(err, errn, L"The disc image could not be opened (error %lu).", GetLastError());
        return 0;
    }
    for (i = 0; i < 4; i++) {
        d->base = bases[i];
        if (disc_read(d, 32 * 2048, vd, sizeof vd) && !memcmp(vd, "MICROSOFT*XBOX*MEDIA", 20)) {
            uint32_t root, rsize;
            memcpy(&root, vd + 20, 4);
            memcpy(&rsize, vd + 24, 4);
            if (!disc_dir(d, root, rsize, L"", 0)) {
                swprintf_s(err, errn, L"The disc image is damaged: its file list could not be read.");
                return 0;
            }
            return 1;
        }
    }
    swprintf_s(err, errn, L"This is not an Xbox 360 disc image (no XDVDFS file system was found in it).");
    return 0;
}

static DiscFile *disc_find(Disc *d, const WCHAR *path)
{
    int i;
    for (i = 0; i < d->count; i++)
        if (!d->files[i].is_dir && !_wcsicmp(d->files[i].path, path))
            return &d->files[i];
    return NULL;
}

/* The original disc's default.xex (what this port is recompiled from). */
#define XEX_SIZE 15020032u
static const uint8_t kXexSha256[32] = {
    0x6a, 0xea, 0xd4, 0xcb, 0x29, 0xca, 0xa1, 0x1f, 0x24, 0x5d, 0x37, 0xb3, 0x62, 0x77, 0xb5, 0xab,
    0x57, 0x77, 0x2c, 0xff, 0xa2, 0xcf, 0x8c, 0x45, 0x9d, 0x32, 0x95, 0xa3, 0xb2, 0xd3, 0xeb, 0xc1};

static int sha256(const uint8_t *data, size_t n, uint8_t out[32])
{
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE h = NULL;
    int ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0 &&
             BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0) == 0 && BCryptHashData(h, (PUCHAR)data, (ULONG)n, 0) == 0 &&
             BCryptFinishHash(h, out, 32, 0) == 0;
    if (h) BCryptDestroyHash(h);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

int anth_xex_ok(const WCHAR *file)
{
    HANDLE f = CreateFileW(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    uint8_t *buf, hash[32];
    DWORD got = 0;
    int ok = 0;
    if (f == INVALID_HANDLE_VALUE) return 0;
    if (GetFileSize(f, NULL) == XEX_SIZE && (buf = (uint8_t *)malloc(XEX_SIZE)) != NULL) {
        ok = ReadFile(f, buf, XEX_SIZE, &got, NULL) && got == XEX_SIZE && sha256(buf, XEX_SIZE, hash) &&
             !memcmp(hash, kXexSha256, 32);
        free(buf);
    }
    CloseHandle(f);
    return ok;
}

int anth_copy_original_xex(const WCHAR *source, const WCHAR *dest_dir, WCHAR *err, size_t errn)
{
    WCHAR out[MAX_PATH], tmp[MAX_PATH];
    uint8_t *buf = NULL, hash[32];
    DWORD attr = GetFileAttributesW(source), put = 0;
    HANDLE h;
    int ok = 0;
    if (attr == INVALID_FILE_ATTRIBUTES) {
        swprintf_s(err, errn, L"Your SvR 2011 ISO or folder is not there: %s", source);
        return 0;
    }
    if (!(buf = (uint8_t *)malloc(XEX_SIZE))) {
        swprintf_s(err, errn, L"Out of memory.");
        return 0;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        WCHAR p[MAX_PATH];
        DWORD got = 0;
        swprintf_s(p, MAX_PATH, L"%s\\default.xex", source);
        h = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        ok = h != INVALID_HANDLE_VALUE && GetFileSize(h, NULL) == XEX_SIZE && ReadFile(h, buf, XEX_SIZE, &got, NULL) &&
             got == XEX_SIZE;
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
        if (!ok) swprintf_s(err, errn, L"The folder has no default.xex of SvR 2011's original disc: %s", source);
    } else {
        Disc d;
        DiscFile *x;
        if (disc_open(&d, source, err, errn)) {
            x = disc_find(&d, L"default.xex");
            ok = x && x->size == XEX_SIZE && disc_read(&d, (uint64_t)x->sector * 2048, buf, XEX_SIZE);
            if (!ok) swprintf_s(err, errn, L"The disc image has no default.xex of SvR 2011's original disc.");
        }
        disc_close(&d);
    }
    if (ok && !(ok = sha256(buf, XEX_SIZE, hash) && !memcmp(hash, kXexSha256, 32)))
        swprintf_s(err, errn, L"This default.xex is not the original SvR 2011 disc's (it is changed, or another "
                              L"version). Use your own unmodified SvR 2011 ISO.");
    if (ok) {
        swprintf_s(out, MAX_PATH, L"%s\\Original", dest_dir);
        CreateDirectoryW(out, NULL);
        swprintf_s(out, MAX_PATH, L"%s\\Original\\default.xex", dest_dir);
        swprintf_s(tmp, MAX_PATH, L"%s\\Original\\default.xex.part", dest_dir);
        h = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        ok = h != INVALID_HANDLE_VALUE && WriteFile(h, buf, XEX_SIZE, &put, NULL) && put == XEX_SIZE;
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
        ok = ok && MoveFileExW(tmp, out, MOVEFILE_REPLACE_EXISTING);
        if (!ok) {
            DeleteFileW(tmp);
            swprintf_s(err, errn, L"Could not write %s (Windows error %lu).", out, GetLastError());
        }
    }
    free(buf);
    return ok;
}
