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
| A9 | sub_826533B0: a list on the stack for 206 entries (the original: 10) - its frame 928 → 4352 bytes, the clear 160 → 3296, the loop count 206; and each id (from 50) is kept when it has a record (the plain lookup sub_828B53B8) instead of the original's record-and-available check (sub_828C1BD8) | the function made at build time with those changes (tools/anthology_redo.py) | (found with A10) |
| A10 | sub_821E9700: an index read from an object (`lwz r7,64(r3)`) is always 0 (`li r7,0`) | the function made at build time with that line (tools/anthology_redo.py) | SvR 2007-2010, LOWM and Divas crashed loading a match: the original read past a table (a float as a pointer) |
| A11 | sub_8245CB58: two checks removed (both branches to 0x8245CE88 are `nop` in his) | the function made at build time without them | a scan of instructions that change kind, not just a number (scratchpad opscan.py) |
| A12 | sub_8245F1C0: an entry is skipped when its id is outside 0-49 - his own small function (1 for 0 ≤ id < 50) in place of the original's sub_8244C800(object, id) | the same test written into the function at build time | (scan); a tag match crashed until the call was seen to go to his code (callscan.py) |
| A13 | Arenas: sub_824B8AB8 `== 3` → `>= 200`, sub_82776980 `!= 3` → `< 200`, and the word at 0x82DAB600 3 → 201 | the two functions at build time; the word set at start-up (anthology.cpp) | (scan) |
| A14 | sub_82522450: mode `== 2` → `>= 2`, and that way builds mode 1's list (sub_825221F0) instead of the original's mode 2 list (sub_825222E0) | the function at build time | (scan, callscan.py) |
| A15 | sub_825F9E68: an id of 100 or more takes the 50-59 way | the function at build time | (scan) |
| A16 | sub_82734560: a call (sub_828B6950) replaced by 0 | the function at build time | (scan) |
| A17 | sub_82331BE8: his jump over the original's 24 special ids (100, 101 ... 279), and the range 50-70 → 50-999 | the 24 branches left out and the compare changed at build time | (scan) |
| A18 | sub_821B0EE8 / sub_821B0F18: two text ids swapped (0x8749 / 0x874D) | the two constants at build time | (scan) |
| A19 | The arena select's NEXT PAGE: his own code (his 0x82479150-0x82479560, in place of the DOWNLOADED ARENAS panel). sub_82478B00 [82478FF0]: his page (+9584: 0 = the game's page, then 2-8, then 0 again) stepped by his 0x82479300; his 0x82479400 keeps the 20 tiles' pictures the first time (160 bytes at +9620, -1 at +9624 from the screen) and fills the tiles for the page from the arena picture table (its +64: keys 10-29, 30-49 ...), putting the game's back on page 0; the panel's call (sub_824775F0) gone. sub_82477598 [82477A38 -> his 0x82479150]: on his pages the arena under the cursor is from his table at his 0x82479340 (7 pages x 20 numbers, row x 5 + column) | both functions at build time (tools/anthology_redo.py, his 0x82479400 as AnthArenaPage); his table read from his chosen program at start-up, his code beside it checked (anthology.cpp) | players: NEXT PAGE should go to pages of all the modded arenas, not the 3-arena panel |

The build also hides the main menu's Xbox LIVE row (no online in this version: anthology.cpp
HideOnlineRows), which is the port's choice, not a change of his.

Port-side fix that came with A2 (not a change of his): the native renderer took the game's D3D device to
be at a fixed address (0xAD977E80); the larger heap moves it (0xAD96FE80). The device is now taken from
the game's own device calls (native_renderer.cpp NoteDevice).

## Not redone (yet)

Changes found in his program that the port does not do, because nothing has needed them so far. Each
waits for a test that shows it matters:

- The "Entrance FPS Switcher" programs in his @Alternate_Files: not needed (the port has its own 60 fps
  entrances setting).
- Calls his program sends to other functions or to his own code are only partly checked: callscan.py
  checks the redone functions; across the whole program most such differences are the later build's
  (the compiler folds the same small functions differently).

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
