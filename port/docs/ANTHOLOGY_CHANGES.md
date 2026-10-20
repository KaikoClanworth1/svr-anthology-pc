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

Port-side fix that came with A2 (not a change of his): the native renderer took the game's D3D device to
be at a fixed address (0xAD977E80); the larger heap moves it (0xAD96FE80). The device is now taken from
the game's own device calls (native_renderer.cpp NoteDevice).

## Not redone (yet)

Changes found in his program that the port does not do, because nothing has needed them so far. Each
waits for a test that shows it matters:

- sub_82156918: a number limit 100 → 256 (`cmpwi r3,100` → `256`), probably the arena numbers.
- sub_824B8AB8, sub_82776980: a compare 3 → 200; the value at 0x82DAB600 3 → 201 (the same feature).
- sub_826533B0: a table on the stack grown 160 → 3296 bytes, its loop fixed to 206 entries.
- sub_825A6868… / sub_825AAB7C…: two lists of ids remapped (0x11 → 0xBF, 0x0C → 0x80, ...).
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
- `SVR2011_ANTHOLOGY_SKIP=A1,A2`: those changes left out.
- `SVR2011_ANTHOLOGY_STACKS=<s>`: every guest thread's stack in the log.
- `SVR2011_ANTHOLOGY_EXTRA=<folder>`: mounted as `GAME:\anthx` (a test pac list there, with `SVR2011_TEST_PLIST=anthx`).
