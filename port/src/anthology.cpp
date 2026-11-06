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
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <fmt/format.h>

REXCVAR_DEFINE_STRING(anthology_version, "", "Game",
                      "SVR Anthology version: \"\" (Anthology, default.xex), SVR07, SVR08, SVR09, SVR10, SVR11, LOWM or "
                      "DIVAS - Yumboy11's default<version>.xex in his folder, read for its pac list, system sound pack "
                      "and menu ids");

// A8 (Yumboy11's versions): the 15 ids of sub_825A6810 / sub_825AAB10 (tools/anthology_redo.py), the
// original disc's until a version is read.
extern "C" uint32_t svr2011_anth_ids[15] = {1, 0, 17, 12, 16, 15, 14, 4, 6, 18, 7, 8, 9, 10, 11};
// A19 (his arena select pages): the arena numbers of his pages 2-8, 20 tiles each (0: an empty tile), read
// from his program; all 0 until then (his pages then show no arenas).
extern "C" uint8_t svr2011_anth_arena_pages[140] = {};

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
  // (the select screen's "?" tile: the game's random pick, not the port's managers list and M picture)
  rex::cvar::SetFlagByName("managers_tile", "false");
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

void HideOnlineRows(uint8_t* table, uint32_t size) {
  // (MFLO: +4 records, +8 shown, records of 0x74 bytes from +0x28: +0 label, +0x18 group, +0x41 bit 0 hidden)
  if (!kAnthology || !table || size < 0x28 || Be32(table) != 1) return;
  const uint32_t total = Be32(table + 4);
  if (!total || total > 0x1000 || 0x28 + total * 0x74 > size) return;
  uint32_t hidden = 0;
  for (uint32_t i = 0; i < total; ++i) {
    uint8_t* r = table + 0x28 + i * 0x74;
    if (Be32(r + 0x18) == 1 && Be32(r) == 0xA02E && !(r[0x41] & 1)) r[0x41] |= 1, ++hidden;  // (main menu, Xbox LIVE)
  }
  if (!hidden) return;
  SetBe32(table + 8, Be32(table + 8) - hidden);
  REXLOG_INFO("[svr2011] anthology: the main menu's Xbox LIVE row hidden (no online in this build)");
}

namespace {
// Yumboy11's eight programs are one build with a few bytes changed (all 14,856,192 bytes, at the same
// places): A6 the pac list's name, A7 the system sound pack's name, A8 two lists of ids. The chosen one
// is read from his folder (never written) and the same values are put into the original program.
constexpr size_t kHisXexSize = 14856192;
constexpr size_t kHisList = 0x336BB, kHisList2 = 0x336D3, kHisPck = 0x3866C;  // (in his file)
constexpr size_t kHisIds = 0x5AA080, kHisIds2 = 0x5AE3A0;                    // (li r4 / li r31 runs)
constexpr uint32_t kList = 0x820306F3, kList2 = 0x8203070B, kPck = 0x820356AC;  // (the original's)
// A19: his arena pages' table (his 0x82479340, 7 pages x 20 bytes), checked by his code beside it: the
// page step at his 0x82479300 (li r10,2) and the cursor's page read at his 0x82479150 (lwz r9,9628(r3))
constexpr size_t kHisPages = 0x47C340, kHisPageStep = 0x47C300, kHisPageRead = 0x47C150;

// The immediates of `li reg, n` in [from, from + 0x100) of his file.
std::vector<uint32_t> LoadImmediates(const std::vector<uint8_t>& x, size_t from, uint32_t reg) {
  std::vector<uint32_t> out;
  for (size_t o = from; o + 4 <= from + 0x100; o += 4) {
    const uint32_t w = Be32(x.data() + o);
    if ((w >> 26) == 14 && ((w >> 16) & 31) == 0 && ((w >> 21) & 31) == reg) out.push_back(w & 0xFFFF);
  }
  return out;
}

void ApplyVersion(rex::memory::Memory* memory) {
  std::string v = rex::cvar::Query<std::string>("anthology_version");
  if (v.size() > 8 || v.find_first_of("./\\:") != std::string::npos) {
    REXLOG_WARN("[svr2011] anthology: version \"{}\" is not one of Yumboy11's - the Anthology", v);
    v.clear();
  }
  const std::string name = "default" + v + ".xex";
  std::ifstream f(GameDataFolder() / name, std::ios::binary);
  std::vector<uint8_t> x((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  auto text = [&](size_t at, size_t n) { return std::string(reinterpret_cast<const char*>(x.data() + at), n); };
  using namespace std::string_literals;
  if (x.size() != kHisXexSize || text(kHisList - 7, 7) != "D:\\pac\\"s || text(kHisList + 8, 7) != "_4x3.h\0"s ||
      text(kHisList2 - 7, 7) != "D:\\pac\\"s || text(kHisList2 + 8, 3) != ".h\0"s ||
      text(kHisPck + 6, 16) != ".pck\0\0/YAFH/DUPL"s) {
    REXLOG_WARN("[svr2011] anthology: {} in {} is not one of Yumboy11's 1.0 programs - the disc's lists",
                name, GameDataFolder().string());
    return;
  }
  // A8: his first function's 16 `li r4` (the third is always 2), his second's 15 `li r31` to check them
  const std::vector<uint32_t> a = LoadImmediates(x, kHisIds, 4), b = LoadImmediates(x, kHisIds2, 31);
  uint32_t ids[15];
  bool ok = a.size() == 16 && a[2] == 2 && b.size() == 15 && b[13] == 20;
  for (int i = 0, k = 0; ok && i < 16; ++i)
    if (i != 2) ids[k++] = a[i];
  for (int i = 0; ok && i < 15; ++i)
    if (i != 13 && b[i] != ids[i]) ok = false;
  if (!ok) {
    REXLOG_WARN("[svr2011] anthology: {}'s menu ids not where they should be - the disc's lists", name);
    return;
  }
  // (the strings are in the program's read-only data: its page made writable first)
  auto put = [&](uint32_t at, size_t from, size_t n) {
    if (auto* heap = memory->LookupHeap(at))
      heap->Protect(at & ~0xFFFu, 0x1000, rex::memory::kMemoryProtectRead | rex::memory::kMemoryProtectWrite);
    std::memcpy(memory->TranslateVirtual<uint8_t*>(at), x.data() + from, n);
  };
  if (On("A6")) put(kList, kHisList, 8), put(kList2, kHisList2, 8);
  if (On("A7")) put(kPck, kHisPck, 10);
  if (On("A8")) std::memcpy(svr2011_anth_ids, ids, sizeof ids);
  if (On("A19")) {
    if (Be32(x.data() + kHisPageStep) == 0x39400002 && Be32(x.data() + kHisPageRead) == 0x8123259C) {
      std::memcpy(svr2011_anth_arena_pages, x.data() + kHisPages, sizeof svr2011_anth_arena_pages);
      REXLOG_INFO("[svr2011] anthology A19: his arena select pages 2-8 read");
    } else {
      REXLOG_WARN("[svr2011] anthology A19: his arena pages' code not where it should be - his pages stay empty");
    }
  }
  std::string list;
  for (uint32_t id : ids) list += fmt::format(" {}", id);
  REXLOG_INFO("[svr2011] anthology: version {} (Yumboy11's): pac\\{}.h, sound\\{}, menu ids{}", name,
              text(kHisList2, 8), text(kHisPck, 10), list);
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
  if (kAnthology && memory && On("A13")) {
    // A13 (Yumboy11's change, with sub_824B8AB8 / sub_82776980's compares in tools/anthology_redo.py):
    // the word at 0x82DAB600 3 -> 201.
    uint8_t* p = memory->TranslateVirtual<uint8_t*>(0x82DAB600);
    if (Be32(p) == 3) {
      if (auto* heap = memory->LookupHeap(0x82DAB600))
        heap->Protect(0x82DAB000, 0x1000, rex::memory::kMemoryProtectRead | rex::memory::kMemoryProtectWrite);
      SetBe32(p, 201);
      REXLOG_INFO("[svr2011] anthology A13: 0x82DAB600 201 (Yumboy11's value)");
    }
  }
  if (kAnthology && memory) ApplyVersion(memory);  // (A6-A8)
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

