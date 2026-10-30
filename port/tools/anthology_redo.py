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
HOOKED = {'sub_828C4060', 'sub_828C4140',  # (superstar_mods.cpp: the attire lookups)
          'sub_8245F1C0',                   # (managers.cpp: browsing)
          'sub_82734560',                   # (superstar_mods.cpp: the exhibition grid)
          'sub_827374A0'}                   # (match_types.cpp: a match set up)
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
    # (and each id 50 + n is kept when it has a record - the plain lookup sub_828B53B8, his call there - not
    # sub_828C1BD8's record-and-available check)
    ('A9', 'sub_826533B0', [('ea = -928 + ctx.r1.u32;', 'ea = -4352 + ctx.r1.u32;', 1),
                            ('ctx.r4.s64 = 160;', 'ctx.r4.s64 = 3296;', 1),
                            ('ctx.r25.u64 = ctx.r31.u64;', 'ctx.r25.s64 = 206;', 1),
                            ('sub_828C1BD8(ctx, base);', 'sub_828B53B8(ctx, base);', 1),
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


# A11-A18: his changes of an instruction to another kind (a branch turned round or removed, a load
# replaced): (change id, function, [(instruction number in the function, its text in the original, the
# code for his instruction - '' for his nop)]). The number counts the generated code's instruction
# comments from 0, as scratchpad side.py / both.py print them; the text is checked.
POS = [
    # A11: two checks removed (his nop for both branches)
    ('A11', 'sub_8245CB58', [(193, 'bne cr6,0x8245ce88', ''), (196, 'beq cr6,0x8245ce88', '')]),
    # A12: an entry is skipped when its id (r29) is outside 0-49 (his own little function at his 0x828B7540:
    # 1 for 0 <= id < 50), in place of the original's sub_8244C800(object +272, id) == 1
    ('A12', 'sub_8245F1C0', [(190, 'lwz r3,272(r31)', ''),
                             (191, 'bl 0x8244c800', 'ctx.r3.u64 = (ctx.r29.s32 >= 0 && ctx.r29.s32 < 50) ? 1 : 0;'),
                             (193, 'beq cr6,0x8245f4e4', 'if (!ctx.cr6.eq) goto loc_8245F4E4;')]),
    # A13: arenas: "== 3" -> ">= 200" and "!= 3" -> "< 200" (with the word at 0x82DAB600 3 -> 201, anthology.cpp)
    ('A13', 'sub_824B8AB8', [(84, 'cmpwi cr6,r11,3', 'ctx.cr6.compare<int32_t>(ctx.r11.s32, 200, ctx.xer);'),
                             (85, 'beq cr6,0x824b8fc4', 'if (!ctx.cr6.lt) goto loc_824B8FC4;')]),
    ('A13', 'sub_82776980', [(8, 'cmpwi cr6,r11,3', 'ctx.cr6.compare<int32_t>(ctx.r11.s32, 200, ctx.xer);'),
                             (9, 'bne cr6,0x827769b8', 'if (ctx.cr6.lt) goto loc_827769B8;')]),
    # A14: mode "== 2" -> ">= 2", and that way builds mode 1's list (sub_825221F0) - the original's mode 2
    # list is sub_825222E0
    ('A14', 'sub_82522450', [(9, 'beq cr6,0x82522494', 'if (!ctx.cr6.lt) goto loc_82522494;'),
                             (17, 'bl 0x825222e0', 'ctx.lr = 0x82522498;\n\tsub_825221F0(ctx, base);')]),
    # A15: an id of 100 or more takes the 50-59 way (in place of a second, same lookup)
    ('A15', 'sub_825F9E68', [(9, 'bl 0x825740c8', 'ctx.cr6.compare<int32_t>(ctx.r11.s32, 100, ctx.xer);'),
                             (10, 'lwz r11,64(r3)', 'if (!ctx.cr6.lt) goto loc_825F9FA4;')]),
    # A16: a call replaced by 0
    ('A16', 'sub_82734560', [(42, 'bl 0x828b6950', 'ctx.r3.u64 = 0;')]),
    # A17: his jump over the original's 24 special ids (100, 101 ... 279), and the range 50-70 -> 50-999
    ('A17', 'sub_82331BE8', [(i, 'beq cr6,0x82331d5c', '') for i in range(12, 59, 2)] +
                            [(61, 'cmpwi cr6,r3,70', 'ctx.cr6.compare<int32_t>(ctx.r3.s32, 999, ctx.xer);')]),
    # A19: the arena select's NEXT PAGE (the original: the DOWNLOADED ARENAS panel) turns his pages - his page
    # (+9584: 0 = the game's page, then 2-8, back to 0) stepped by his 0x82479300, the tiles filled by his
    # 0x82479400 (AnthArenaPage), the panel's call (sub_824775F0) and its state check (+9588) left out
    ('A19', 'sub_82478B00', [(5, 'lwz r11,9588(r3)', 'ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 9584);'),
                             (8, 'bne cr6,0x82478b80', ''),
                             (12, 'li r11,1', 'ctx.r10.s64 = ctx.r11.u32 == 0 || ctx.r11.u32 > 8 ? 2 : '
                                              'ctx.r11.u32 == 8 ? 0 : ctx.r11.u32 + 1;'),
                             (14, 'stw r11,9588(r31)', 'REX_STORE_U32(ctx.r31.u32 + 9584, ctx.r10.u32);'),
                             (15, 'bl 0x823c8d48', 'AnthArenaPage(ctx, base);'),
                             (20, 'bl 0x824775f0', '')]),
    # A19: the arena under the cursor - on his pages his table's (his 0x82479150: page - 2, row x 5 + column),
    # else the original's with his page in r9 (the panel's state not read)
    ('A19', 'sub_82477598', [(0, 'lwz r9,9632(r3)',
                              'ctx.r9.u64 = REX_LOAD_U32(ctx.r3.u32 + 9628);\n'
                              '\tif (ctx.r9.s32 >= 2 && ctx.r9.u32 <= 8) {\n'
                              '\t\tctx.r3.u64 = svr2011_anth_arena_pages[((ctx.r9.u32 - 2) * 20 + '
                              'REX_LOAD_U32(ctx.r3.u32 + 72) * 5 + REX_LOAD_U32(ctx.r3.u32 + 88)) % 140];\n'
                              '\t\treturn;\n\t}')]),
    # A21: the /ETEX/%04d picture of number + 300 (the original: + 100)
    ('A21', 'sub_827667E0', [(21, 'addi r6,r11,100', 'ctx.r6.s64 = ctx.r11.s64 + 300;')]),
    # A22: an id (+64 of sub_825740C8's object) of 100 or more takes entry 0 (the original: the block skipped)
    ('A22', 'sub_82583988', [(7, 'bge cr6,0x82583c0c', 'if (!ctx.cr6.lt) ctx.r30.s64 = 0;')]),
    # A23: a match set up: a person whose record id is 20-24 skips the record's lookup (sub_828B4528 /
    # sub_828C1BD8) - his jump to the block after it
    ('A23', 'sub_827374A0', [(54, 'lwzx r3,r11,r29',
                              'ctx.r3.u64 = REX_LOAD_U32(ctx.r11.u32 + ctx.r29.u32);\n'
                              '\tif (ctx.r3.u32 - 20 <= 4) goto loc_827376B0;')]),
    # A24: the arena select's flags 11 -> 15 for the match kind sub_82733D18 answers (his Readme: "I enable
    # the select arena for the rumble")
    ('A24', 'sub_8245C2B0', [(36, 'li r30,11', 'ctx.r30.s64 = 15;')]),
]
# A18: two text ids swapped (a plain constant)
CHANGES += [
    ('A18', 'sub_821B0EE8', [('ctx.r3.u64 = ctx.r3.u64 | 34633;', 'ctx.r3.u64 = ctx.r3.u64 | 34637;', 1)]),
    ('A18', 'sub_821B0F18', [('ctx.r3.u64 = ctx.r3.u64 | 34637;', 'ctx.r3.u64 = ctx.r3.u64 | 34633;', 1)]),
]


def redo_at(cid, fn, body, edits):
    lines = body.split('\n')
    com = [i for i, l in enumerate(lines) if l.strip().startswith('// ')]
    label = re.compile(r'\s*loc_[0-9A-F]{8}:')
    for idx, asm, new in sorted(edits, reverse=True):
        if idx >= len(com) or lines[com[idx]].strip() != '// ' + asm:
            got = lines[com[idx]].strip() if idx < len(com) else 'nothing'
            sys.exit('anthology_redo: %s %s: instruction %d is "%s", expected "%s"' % (cid, fn, idx, got, asm))
        i = com[idx]
        j = i + 1
        while j < len(lines) and not lines[j].strip().startswith('// ') and not label.match(lines[j]):
            j += 1
        note = ' '.join(new.split()) if new else 'nop'
        lines[i + 1:j] = (['\t' + new] if new else []) + ['\t// (Yumboy11: %s)' % note]
    return '\n'.join(lines)


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
    want = {fn for _, fn, _ in CHANGES} | {fn for _, fn, *_ in IDS} | {fn for _, fn, _ in POS}
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
             'extern "C" uint32_t svr2011_anth_ids[15];',
             '// (A19) his arena pages\' arena numbers (anthology.cpp)',
             'extern "C" uint8_t svr2011_anth_arena_pages[140];',
             '// (A19) his 0x82479400: the arena select\'s 20 tiles (+3216 from the screen, 320 bytes apart, their',
             '// picture key at +272) for his page - page 2 first keeps the game\'s page\'s keys (160 bytes at +9620,',
             '// -1 at +9624: the DOWNLOADED ARENAS panel\'s ids, unused now), page 0 puts them back; pages 2-8 show',
             '// keys 10-29, 30-49 ... of the arena picture table (+10656: its +64)',
             'static void AnthArenaPage(PPCContext& ctx, uint8_t* base) {',
             '  PPCContext c = ctx;',
             '  const uint32_t s = ctx.r31.u32 - 44, page = REX_LOAD_U32(s + 9628);',
             '  uint32_t off;',
             '  auto show = [&](uint32_t i, uint64_t key) {',
             '    c.r3.u64 = s + 3216 + i * 320;',
             '    c.r4.u64 = key;',
             '    c.lr = 0x82478B40;',
             '    sub_827023A0(c, base);',
             '  };',
             '  if (page == 0) {',
             '    const uint32_t saved = REX_LOAD_U32(s + 9620);',
             '    if (REX_LOAD_U32(s + 9624) != 0xFFFFFFFFu || !saved) return;',
             '    for (uint32_t i = 0; i < 20; ++i) show(i, REX_LOAD_U64(saved + i * 8));',
             '    return;',
             '  }',
             '  if (page == 2) {',
             '    if (REX_LOAD_U32(s + 9624) != 0xFFFFFFFFu) {',
             '      c.r3.u64 = 160;',
             '      c.lr = 0x82478B40;',
             '      sub_8269A8D8(c, base);',
             '      const uint32_t saved = c.r3.u32;',
             '      if (!saved) return;',
             '      REX_STORE_U32(s + 9620, saved);',
             '      REX_STORE_U32(s + 9624, 0xFFFFFFFFu);',
             '      for (uint32_t i = 0; i < 20; ++i) REX_STORE_U64(saved + i * 8, REX_LOAD_U64(s + 3216 + i * 320 + 272));',
             '    }',
             '    off = 80;',
             '  } else {',
             '    off = page * 160 - 240;',
             '  }',
             '  const uint32_t table = REX_LOAD_U32(s + 10656);',
             '  if (!table) return;',
             '  const uint32_t from = REX_LOAD_U32(table + 64) + off;',
             '  for (uint32_t i = 0; i < 20; ++i) show(i, REX_LOAD_U64(from + i * 8));',
             '}', '']
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
    for cid, fn, edits in POS:
        if fn not in found:
            sys.exit('anthology_redo: %s not in %s' % (fn, gen))
        body = redo_at(cid, fn, found[fn], edits)
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
