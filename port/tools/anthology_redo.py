"""SvR Anthology build: Yumboy11's changes that are a constant inside a function
of the game (docs/ANTHOLOGY_CHANGES.md). Each is redone as a hook made from
this build's own recompiled function (generated/default, from the original
disc program) with only those constants changed - written into the build
folder at build time, never into the source tree (the generated code is not
part of the repository).

usage: anthology_redo.py <generated dir> <out .cpp>"""
import os
import re
import sys

# (change id, function, [(code as generated, the same with his value, times)]). A function the port
# hooks already (HOOKED) is made as anth_<function> instead: the port's hook calls that in this build
# (SVR2011_IMP, anthology.h).
HOOKED = {'sub_828C4060', 'sub_828C4140'}  # (superstar_mods.cpp: the attire lookups)
CHANGES = [
    # A3: a table of 80 entries of 40 bytes (count at +3200) -> 129 entries (count at +5160)
    ('A3', 'sub_828C40D0', [('ctx.r3.s64 = 3204;', 'ctx.r3.s64 = 5164;', 1),
                            # (the other allocation path, in 16-byte units: 201 -> 323, room for the 5164)
                            ('ctx.r3.s64 = 201;', 'ctx.r3.s64 = 323;', 1)]),
    ('A3', 'sub_828C3F28', [('ctx.r9.s64 = 80;', 'ctx.r9.s64 = 129;', 1),
                            ('ctx.r3.u32 + 3200', 'ctx.r3.u32 + 5160', 1)]),
    ('A3', 'sub_828C3F70', [('ctx.r3.u32 + 3200', 'ctx.r3.u32 + 5160', 1)]),
    ('A3', 'sub_828C3FF8', [('ctx.r3.u32 + 3200', 'ctx.r3.u32 + 5160', 1)]),
    ('A3', 'sub_828C4060', [('ctx.r3.u32 + 3200', 'ctx.r3.u32 + 5160', 1)]),
    ('A3', 'sub_828C4140', [('ctx.r3.u32 + 3200', 'ctx.r3.u32 + 5160', 1)]),
    # A4: motion entry names /MOVT/M%03d for numbers up to 255 (the original: up to 99)
    ('A4', 'sub_82156918', [('ctx.cr6.compare<int32_t>(ctx.r3.s32, 100, ctx.xer);',
                             'ctx.cr6.compare<int32_t>(ctx.r3.s32, 256, ctx.xer);', 1)]),
    # A5: names with a number in three digits (his: hundreds over the 'M' of "/MENU/M00", then tens, units;
    # the fallback "00" at the first two) - the original writes two digits, so a number from 100 up made
    # names like /MENU/M00I that aren't there and the loader waited for them forever
    ('A5', 'sub_82478168', [('REX_STORE_U8(ctx.r1.u32 + 96, ctx.r7.u8);',
                             'REX_STORE_U8(ctx.r1.u32 + 96, ctx.r7.u8);\n\tAnthDigits(base, ctx.r1.u32 + 94, ctx.r31.u32);', 1),
                            ('REX_STORE_U8(ctx.r1.u32 + 96, ctx.r11.u8);', 'REX_STORE_U8(ctx.r1.u32 + 94, ctx.r11.u8);', 1)]),
    ('A5', 'sub_8271D868', [('ctx.r29.u64 = uint32_t((ctx.r9.s32 && !(ctx.r30.s32 == INT32_MIN',
                             'g_anth_n = ctx.r30.u32;\n\tctx.r29.u64 = uint32_t((ctx.r9.s32 && !(ctx.r30.s32 == INT32_MIN', 1),
                            ('REX_STORE_U8(ctx.r1.u32 + 88, ctx.r6.u8);',
                             'REX_STORE_U8(ctx.r1.u32 + 88, ctx.r6.u8);\n\tAnthDigits(base, ctx.r1.u32 + 86, g_anth_n);', 1),
                            ('REX_STORE_U8(ctx.r1.u32 + 88, ctx.r28.u8);', 'REX_STORE_U8(ctx.r1.u32 + 86, ctx.r28.u8);', 1),
                            ('REX_STORE_U8(ctx.r1.u32 + 104, ctx.r7.u8);',
                             'REX_STORE_U8(ctx.r1.u32 + 104, ctx.r7.u8);\n\tAnthDigits(base, ctx.r1.u32 + 102, g_anth_n);', 1),
                            ('REX_STORE_U8(ctx.r1.u32 + 104, ctx.r28.u8);', 'REX_STORE_U8(ctx.r1.u32 + 102, ctx.r28.u8);', 1)]),
    ('A3', 'sub_828C4400', [('ctx.r3.u32 + 3200', 'ctx.r3.u32 + 5160', 1),
                            ('ctx.r28.u32 + 3200', 'ctx.r28.u32 + 5160', 1)]),
    # A10: an index read from an object (+64) into a table always 0 in his program - with his versions'
    # data the original read past the table (a float as a pointer, crash loading a match)
    ('A10', 'sub_821E9700', [('ctx.r7.u64 = REX_LOAD_U32(ctx.r3.u32 + 64);', 'ctx.r7.u64 = 0;', 1)]),
    # A9: a list on the stack for 206 entries (the original: 10) - his versions' menu ids (A8) go past 10
    ('A9', 'sub_826533B0', [('ea = -928 + ctx.r1.u32;', 'ea = -4352 + ctx.r1.u32;', 1),
                            ('ctx.r4.s64 = 160;', 'ctx.r4.s64 = 3296;', 1),
                            ('ctx.r25.u64 = ctx.r31.u64;', 'ctx.r25.s64 = 206;', 1),
                            ('ctx.r1.s64 = ctx.r1.s64 + 928;', 'ctx.r1.s64 = ctx.r1.s64 + 4352;', 1)]),
]

# A8: the ids his versions change (defaultSVR07.xex ...): each `li` of the register in order -> the slot
# of svr2011_anth_ids (anthology.cpp, read from the chosen version), None = the same in all of his. The
# original's values are checked first.
IDS = [
    ('A8', 'sub_825A6810', 'r4', [1, 0, 2, 17, 12, 16, 15, 14, 4, 6, 18, 7, 8, 9, 10, 11],
     [0, 1, None, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14]),
    ('A8', 'sub_825AAB10', 'r31', [300, 1, 0, 17, 12, 16, 15, 14, 4, 6, 18, 7, 8, 9, 20, 11],
     [None, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, None, 14]),
]


def redo_ids(cid, fn, body, reg, original, slots):
    pat = re.compile(r'ctx\.%s\.s64 = (-?\d+);' % reg)
    found = [int(m.group(1)) for m in pat.finditer(body)]
    if found != original:
        sys.exit('anthology_redo: %s %s: %s ids %s, expected %s' % (cid, fn, reg, found, original))
    it = iter(slots)

    def sub(m):
        k = next(it)
        return m.group(0) if k is None else 'ctx.%s.s64 = svr2011_anth_ids[%d];' % (reg, k)
    return pat.sub(sub, body)


def functions(gen):
    out = {}
    for name in sorted(os.listdir(gen)):
        if not (name.endswith('.cpp') and '_recomp.' in name):
            continue
        text = open(os.path.join(gen, name), encoding='utf-8').read()
        for m in re.finditer(r'^DEFINE_REX_FUNC\((sub_[0-9A-F]{8})\) \{\n(.*?)^\}\n', text, re.S | re.M):
            out[m.group(1)] = m.group(2)
    return out


def main():
    gen, out = sys.argv[1], sys.argv[2]
    want = {fn for _, fn, _ in CHANGES} | {fn for _, fn, *_ in IDS}
    found = {k: v for k, v in functions(gen).items() if k in want}
    lines = ['// Made by tools/anthology_redo.py from generated/default - do not edit.',
             '// Yumboy11\'s changes redone (docs/ANTHOLOGY_CHANGES.md).',
             '#include <rex/hook.h>', '#include <rex/ppc.h>', '#include "generated/default/svr2011_init.h"', '',
             "// (A5) three digits of n at guest address at; the number between two of a function's steps",
             'static void AnthDigits(uint8_t* base, uint32_t at, uint32_t n) {',
             "  REX_STORE_U8(at, uint8_t('0' + n / 100 % 10));",
             "  REX_STORE_U8(at + 1, uint8_t('0' + n / 10 % 10));",
             "  REX_STORE_U8(at + 2, uint8_t('0' + n % 10));",
             '}',
             'static thread_local uint32_t g_anth_n;',
             '// (A8) the chosen version\'s ids (anthology.cpp)',
             'extern "C" uint32_t svr2011_anth_ids[15];', '']
    for cid, fn, subs in CHANGES:
        if fn not in found:
            sys.exit('anthology_redo: %s not in %s' % (fn, gen))
        body = found[fn]
        for old, new, times in subs:
            n = body.count(old)
            if n != times:
                sys.exit('anthology_redo: %s %s: "%s" %d times, expected %d' % (cid, fn, old, n, times))
            body = body.replace(old, new)
        head = 'extern "C" REX_FUNC(anth_%s) {' % fn if fn in HOOKED else 'REX_HOOK_RAW(%s) {' % fn
        lines += ['// %s (Yumboy11\'s change)' % cid, head, body.rstrip('\n'), '}', '']
    for cid, fn, reg, original, slots in IDS:
        if fn not in found:
            sys.exit('anthology_redo: %s not in %s' % (fn, gen))
        body = redo_ids(cid, fn, found[fn], reg, original, slots)
        lines += ['// %s (Yumboy11\'s versions)' % cid, 'REX_HOOK_RAW(%s) {' % fn, body.rstrip('\n'), '}', '']
    text = '\n'.join(lines) + '\n'
    if not os.path.exists(out) or open(out, encoding='utf-8').read() != text:
        open(out, 'w', encoding='utf-8', newline='\n').write(text)


if __name__ == '__main__':
    main()
