// The SvR Anthology build: see anthology.h.
#include "anthology.h"

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>

namespace svr2011 {

namespace {
std::filesystem::path g_game_data;
std::filesystem::path g_menu_pac;  // (the patched copy, PatchAnthologyFiles)
}  // namespace

const std::filesystem::path& GameDataFolder() {
  if (g_game_data.empty()) g_game_data = rex::filesystem::GetExecutableFolder();
  return g_game_data;
}

void SetGameDataFolder(const std::filesystem::path& dir) {
  std::error_code ec;
  g_game_data = std::filesystem::absolute(dir, ec);
  if (ec) g_game_data = dir;
}

void SetAnthologyMenuPac(const std::filesystem::path& file) { g_menu_pac = file; }

std::filesystem::path MenuPacFile() {
  return g_menu_pac.empty() ? GameDataFolder() / "pac" / "menu" / "menu.pac" : g_menu_pac;
}

void ForceAnthologySettings() {
  if (!kAnthology) return;
  // (no online: lobbies, Community Creations and the account stay off - the
  // Anthology's content is not the normal game's, so its matches would not
  // stay in step with normal players)
  rex::cvar::SetFlagByName("online_enabled", "false");
  rex::cvar::SetFlagByName("p2p_enabled", "false");
  rex::cvar::SetFlagByName("online_token", "");
  // (no texture packs: they are mods too)
  rex::cvar::SetFlagByName("native_texture_packs", "");
  // (the Anthology's folder: read only)
  rex::cvar::SetFlagByName("allow_game_relative_writes", "false");
  REXLOG_INFO("[svr2011] SvR Anthology build (SVR Anthology by Yumboy11): data {}; no mods, no online",
              GameDataFolder().string());
}

}  // namespace svr2011
