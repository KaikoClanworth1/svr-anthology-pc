// The SvR Anthology build: see anthology.h.
#include "anthology.h"

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>
#include <rex/system/xmemory.h>

#include "generated/default/svr2011_init.h"
#include "thread_cpu.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include <fmt/format.h>

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

namespace {
rex::memory::Memory* g_memory = nullptr;

// Test aid: SVR2011_ANTHOLOGY_SKIP=A1,A2... - those changes left out.
bool On(const char* id) {
  const char* v = std::getenv("SVR2011_ANTHOLOGY_SKIP");
  return !v || !std::strstr(v, id);
}

uint32_t Be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
void SetBe32(uint8_t* p, uint32_t v) {
  p[0] = uint8_t(v >> 24); p[1] = uint8_t(v >> 16); p[2] = uint8_t(v >> 8); p[3] = uint8_t(v);
}
}  // namespace

void InstallAnthology(rex::memory::Memory* memory) {
  g_memory = memory;
  if (kAnthology && memory && On("A2")) {
    // A2 (Yumboy11's change): the game's memory table at 0x82DAC2FC ({size, id}
    // pairs): part 2 0x20 -> 0x40.
    uint8_t* p = memory->TranslateVirtual<uint8_t*>(0x82DAC304);
    if (Be32(p) == 0x20 && Be32(p + 4) == 2) {
      SetBe32(p, 0x40);
      REXLOG_INFO("[svr2011] anthology A2: memory table part 2 0x40 (Yumboy11's size)");
    }
  }
  // Test aid: SVR2011_ANTHOLOGY_STACKS=<seconds> - every guest thread's place in the log then.
  if (const char* v = std::getenv("SVR2011_ANTHOLOGY_STACKS"); v && *v) {
    const int s = std::atoi(v);
    std::thread([s] {
      std::this_thread::sleep_for(std::chrono::seconds(s));
      LogGuestStacks();
    }).detach();
  }
}

}  // namespace svr2011

#ifdef SVR2011_ANTHOLOGY
// A1 (Yumboy11's change): the file system's directory memory 0x2C000 -> 0x4B000
// bytes, for the Anthology's 379 pacs (the disc's 248 fit in the original; more
// than about 372 overran it). sub_82578848 sets {512, 128, size, buffer, ...} up
// and passes it here; the buffer is made anew at the larger size (the smaller one
// it allocated stays unused).
REX_EXTERN(__imp__sub_826A2A10);
REX_HOOK_RAW(sub_826A2A10) {
  using namespace svr2011;
  if (g_memory && ctx.r3.u32 && On("A1")) {
    uint8_t* p = g_memory->TranslateVirtual<uint8_t*>(ctx.r3.u32);
    if (Be32(p + 8) == 0x2C000) {
      if (const uint32_t buf = g_memory->SystemHeapAlloc(0x4B000)) {
        SetBe32(p + 8, 0x4B000);
        SetBe32(p + 12, buf);
        REXLOG_INFO("[svr2011] anthology A1: file system directory memory 0x4B000 (Yumboy11's size)");
      }
    }
  }
  __imp__sub_826A2A10(ctx, base);
}
// Test aid: SVR2011_ANTHOLOGY_TRACE=1 - sub_825C6DC8's arguments once (r4 +0..15, r5) and rule record 0
// (*0x82EDE954 + 0, 32 bytes).
REX_EXTERN(__imp__sub_825C6DC8);
REX_HOOK_RAW(sub_825C6DC8) {
  using namespace svr2011;
  static const bool trace = std::getenv("SVR2011_ANTHOLOGY_TRACE") != nullptr;
  static int told = 0;
  if (trace && g_memory && told < 6 && ctx.r4.u32) {
    ++told;
    const uint8_t* a = g_memory->TranslateVirtual<uint8_t*>(ctx.r4.u32);
    const uint32_t rules = Be32(g_memory->TranslateVirtual<uint8_t*>(0x82EDE954));
    const uint8_t* r = g_memory->TranslateVirtual<uint8_t*>(rules);
    std::string x, y;
    for (int i = 0; i < 16; ++i) x += fmt::format(" {:02X}", a[i]);
    for (int i = 0; i < 32; ++i) y += fmt::format(" {:02X}", r[i]);
    REXLOG_INFO("[svr2011] anthology trace: sub_825C6DC8 r3 {:08X} r4 {:08X}:{} r5 {} lr {:08X}; rules {:08X}:{}", ctx.r3.u32,
                ctx.r4.u32, x, ctx.r5.u32, uint32_t(ctx.lr), rules, y);
  }
  __imp__sub_825C6DC8(ctx, base);
}
#endif

