# SVR Anthology by Yumboy11: what this port redoes for it

SVR Anthology is **Yumboy11's** mod for the Xbox 360 version of WWE SmackDown vs. Raw 2011. All credit
for it, and for every change listed here, goes to Yumboy11 (and the people he thanks in his Readme).

The SvR Anthology build of this port (branch `anthology`) runs the player's own Anthology install, made
from their own ISO with Yumboy11's own installer. Nothing of the Anthology is shipped, copied or
re-created: no data, no pacs, no part of his installer. The port reads his files from his folder, as they
are.

His Anthology comes with its own `default.xex`, a later build of the game (title 54511A11, v1.1). This
recompiled port can only run the original disc's program (title 5451085D, v1). Where his program does
something the Anthology's data needs, the port does the same thing in the original program, with the
smallest hook it can. Each change below is his. The port only redoes it so his data can be read, and
lists it so it can be shown to him. Addresses are the original disc program's (his in brackets).

| # | What Yumboy11's program changes | How the port redoes it | Found by |
|---|---|---|---|
| A1 | The file system's directory memory: 0x2C000 → 0x4B000 bytes (sub_82578848 [sub_82578F00]: `lis 2 / ori 0xC000` → `lis 4 / ori 0xB000`), for his 379 pacs (the disc has 248) | hook on sub_826A2A10: the set-up record's size and buffer replaced (a 0x4B000 buffer) | crash mounting pac ~373 (pool free list overwritten) |
| A2 | The game's memory table at 0x82DAC2F8 ({size, id} pairs), part 2: 0x20 → 0x40 (the word at 0x82DAC304) | the table word set at start-up | start-up hang: the loader waited on a full heap |
| A3 | The superstars' attire table (global 0x82EDEC3C): 80 → 129 entries of 40 bytes, its count +3200 → +5160, the allocation 3204 → 5164 (sub_828C40D0, the constructor sub_828C3F28, and sub_828C3F70 / 3FF8 / 4060 / 4140 / 4400) | the seven functions made at build time from this build's own recompiled code with those constants changed (tools/anthology_redo.py → build folder; the generated code is never in the repository). His second allocation path (201 × 16 bytes) is made 323 × 16 so it also has room | crash after the title: the table's overflow overwrote the match rules (a menu read ids as rule categories) |
| A4 | Motion entry names `/MOVT/M%03d` for numbers up to 255 (sub_82156918: `cmpwi r3,100` → `256`) | the function made at build time with that constant (tools/anthology_redo.py) | found with A5 (the same loads) |
| A5 | Names with a number in three digits: the hundreds over the "M" of a `/MENU/M00`-style name, then tens and units, and the fallback "00" at the first two places (sub_82478168, sub_8271D868) | after each of the original's two-digit writes, his three digits are written (a small helper in the build-time file, tools/anthology_redo.py) | match loading hung: the loader waited forever for `/MENU/M00I`, a name the original's two digits made for a number over 99 |
| A6 | His versions' pac lists: each of his eight programs (default.xex = SVR Anthology, defaultSVR07 / SVR08 / SVR09 / SVR10 / SVR11 / LOWM / DIVAS.xex) reads its own list, `pac\pliANTHO.h`, `pac\pliSVR07.h` ... (`_4x3.h` too) | the version chosen in the launcher (`anthology_version`): its name read from his program in his folder and put in the original's two strings (0x820306F3, 0x8203070B) at start-up | his folder: eight programs that differ in about 40 bytes |
| A7 | His versions' system sound pack: `System.pck`, `SSVR07.pck` ... `SyLOWM.pck`, `SDIVAS.pck` | the same: the name from his program into the original's string 0x820356AC | (with A6) |
| A8 | His versions' menu ids: 15 numbers in sub_825A6810 and sub_825AAB10 (the original disc's are SVR11's) | the two functions made at build time with the 15 from a table (tools/anthology_redo.py), filled from his chosen program at start-up | (with A6) |
| A9 | sub_826533B0: a list on the stack for 206 entries (the original: 10) - its frame 928 → 4352 bytes, the clear 160 → 3296, the loop count 206 | the function made at build time with those four changes (tools/anthology_redo.py) | (found with A10) |
| A10 | sub_821E9700: an index read from an object (`lwz r7,64(r3)`) is always 0 (`li r7,0`) | the function made at build time with that line (tools/anthology_redo.py) | SvR 2007-2010, LOWM and Divas crashed loading a match: the original read past a table (a float as a pointer) |

The build also hides the main menu's Xbox LIVE row (no online in this version: anthology.cpp
HideOnlineRows), which is the port's choice, not a change of his.

Port-side fix that came with A2 (not a change of his): the native renderer took the game's D3D device to
be at a fixed address (0xAD977E80); the larger heap moves it (0xAD96FE80). The device is now taken from
the game's own device calls (native_renderer.cpp NoteDevice).

## Not redone (yet)

Changes found in his program that the port does not do, because nothing has needed them so far. Each
waits for a test that shows it matters:

- sub_824B8AB8, sub_82776980: a compare 3 → 200; the value at 0x82DAB600 3 → 201 (the same feature).
- Other code changes of his (an instruction of another kind, not just a number): sub_8245CB58 (two
  branches removed), sub_8245F1C0 (`lwz r3,272(r31)` → `mr r3,r29`, a condition turned round),
  sub_82522450 (`beq` → `bge`), sub_825F9E68 (a limit of 100 added), sub_82734560 (a call replaced by
  `li r3,0`), sub_82478B00 (a call of his own code).
- The "Entrance FPS Switcher" programs in his @Alternate_Files (30 fps / 60 fps entrances): not compared yet.
- sub_82331BE8, sub_82478C60: code of his own (a compare chain rewritten).
- Two text ids swapped (0x8749 / 0x874D, sub_821B0EE8 / sub_821B0F18).

Many other differences come from the later build itself (compiler output: other registers, other
addresses). They aren't his changes, and the port leaves them alone.

## How they were found (for the next ones)

Dump both programs' images (`SVR2011_DUMP_IMAGE`), pair their functions by address-masked fingerprints
(39,054 each), then list the paired functions' changed constants (`li`, `lis`/`ori`, compares) and the
non-address words that differ in `.data`. Then run the Anthology hidden, and go one crash or hang at a
time. Test aids in this build:

- `SVR2011_ANTHOLOGY_XEX=<folder>`: the original `default.xex` from there.
- `SVR2011_ANTHOLOGY_SKIP=A1,A2`: those changes left out (A6, A7, A8 too).
- `anthology_version = "SVR07"` in svr2011.toml (the launcher's Version): which of his programs is read.
- `SVR2011_ANTHOLOGY_STACKS=<s>`: every guest thread's stack in the log.
- `SVR2011_ANTHOLOGY_EXTRA=<folder>`: mounted as `GAME:\anthx` (a test pac list there, with `SVR2011_TEST_PLIST=anthx`).
