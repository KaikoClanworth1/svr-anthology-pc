# SVR Anthology by Yumboy11 — on the SmackDown vs. Raw 2011 PC recomp

**SVR Anthology is Yumboy11's mod** for the Xbox 360 version of *WWE SmackDown vs. Raw 2011*. All credit for
the Anthology goes to **Yumboy11** and the people he thanks in his Readme: Brienj, Red Rooster, ryokudou,
littlebase631, theviper12, xtremebrett, tekken57 (X-Packer, X-Rey), ERI619, eatrawmeat391 (Pac Editor Tool),
jakeypearce, Mark/Rampage, Justyn, Sean O Connor, Wrestling Modders INC, RipZz (Sound Factory Tool), Obs Kenobs,
WatchTheSmoke, and everyone who sent textures, suggestions and feedback.

SVR Anthology is freeware: never buy or sell it.

This repository is the **Anthology version of the [SvR 2011 PC recomp](https://github.com/KaikoClanworth1/wwe-svr2011-pc)
by KaikoClanworth**:
a launcher and the recompiled game program that run the Anthology **you install yourself**.

> [!IMPORTANT]
> **Nothing of SVR Anthology is in this repository or its releases**, and no game either: no disc image, XEX,
> game data, Anthology files, or any part of Yumboy11's installer. You need:
> - your own, unmodified *WWE SmackDown vs. Raw 2011* Xbox 360 ISO (or extracted game folder), and
> - Yumboy11's own **SVR Anthology 1.0 Installer**, downloaded from him.
>
> Yumboy11 allows his Anthology to be used with this recomp on exactly those terms: your own ISO, his own
> installer, and full credit to him.

## How it works

1. **SvR Anthology Launcher** starts Yumboy11's installer in its own window (you answer it there). It makes the
   Anthology next to itself, as it always does; the launcher never moves or changes it.
2. The launcher copies his saves and DLC packages into its own folder, and the original program (`default.xex`)
   out of your ISO.
3. **PLAY** runs the recompiled game on the Anthology's folder, read only.

The recomp can only run the original disc's program, while the Anthology ships a program of its own (a later
build of the game with Yumboy11's changes). Where his program does something his data needs, this version does
the same thing in the original program, with the smallest hook it can. Each of those changes is his, and each one
is listed and credited in [port/docs/ANTHOLOGY_CHANGES.md](port/docs/ANTHOLOGY_CHANGES.md).

This version has **no mods, no Mod Maker and no online play**: it runs the Anthology as Yumboy11 made it.

## Install

Download the `SvR-Anthology-PC-v<version>.zip` release, extract it into a folder of its own (for example
`C:\Games\SvR Anthology PC`), start **SvR Anthology Launcher.exe** and follow its steps. The release's
*Read Me.txt* has the details.

## Building

As the main port ([its README](https://github.com/KaikoClanworth1/wwe-svr2011-pc)), with the CMake option
`SVR2011_ANTHOLOGY` (on in this repository). Targets: `svr2011` and `svr2011_anthology_launcher`;
`port/tools/package_anthology.ps1` makes the release zip.

> [!NOTE]
> The PC recomp (the launcher and the recompiled game program) is by **KaikoClanworth**, made with
> [Claude Code](https://claude.com/claude-code). *WWE SmackDown vs. Raw 2011* belongs to its owners; SVR Anthology belongs to Yumboy11.
