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
    ('A3', 'sub_828C4400', [('ctx.r3.u32 + 3200', 'ctx.r3.u32 + 5160', 1),
                            ('ctx.r28.u32 + 3200', 'ctx.r28.u32 + 5160', 1)]),
]


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
    want = {fn for _, fn, _ in CHANGES}
    found = {k: v for k, v in functions(gen).items() if k in want}
    lines = ['// Made by tools/anthology_redo.py from generated/default - do not edit.',
             '// Yumboy11\'s changes redone (docs/ANTHOLOGY_CHANGES.md).',
             '#include <rex/hook.h>', '#include <rex/ppc.h>', '#include "generated/default/svr2011_init.h"', '']
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
    text = '\n'.join(lines) + '\n'
    if not os.path.exists(out) or open(out, encoding='utf-8').read() != text:
        open(out, 'w', encoding='utf-8', newline='\n').write(text)


if __name__ == '__main__':
    main()
