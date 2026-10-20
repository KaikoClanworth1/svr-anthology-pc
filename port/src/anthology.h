// The SvR Anthology build (branch anthology, SVR2011_ANTHOLOGY): the game runs
// SVR Anthology by Yumboy11 - the player's own install, made from their own
// SvR 2011 ISO with Yumboy11's own installer - straight from its folder
// (--game_data_root). Nothing of the Anthology is part of this port, and
// nothing is ever written to its folder.
//
// The build has no mods (no_mods.h is always on), no texture packs and no
// online. The port's changes to string.pac and menu.pac (game_files.cpp) are
// made to copies in UserData\Anthology, and only when the Anthology's file is
// still the original game's (otherwise the Anthology's own is used as it is).
#pragma once

#include <filesystem>

namespace rex::filesystem {
class VirtualFileSystem;
}
namespace rex::memory {
class Memory;
}

namespace svr2011 {

#ifdef SVR2011_ANTHOLOGY
constexpr bool kAnthology = true;
#else
constexpr bool kAnthology = false;
#endif

// Where the disc files are read from: --game_data_root, else beside the program.
const std::filesystem::path& GameDataFolder();
void SetGameDataFolder(const std::filesystem::path& dir);

// The port's text and menu changes for an Anthology install: copies in
// user_data\Anthology, linked over the game's files (GAME:\pac\...).
void PatchAnthologyFiles(rex::filesystem::VirtualFileSystem* vfs, const std::filesystem::path& user_data);

// menu.pac as the game reads it (the patched copy when there is one).
std::filesystem::path MenuPacFile();
void SetAnthologyMenuPac(const std::filesystem::path& file);

// The hooks that let the original disc program read the Anthology's data
// (docs/ANTHOLOGY_CHANGES.md: each one is Yumboy11's change, redone here).
void InstallAnthology(rex::memory::Memory* memory);

// Online, texture packs: off in this build (after the settings are loaded).
void ForceAnthologySettings();

}  // namespace svr2011
