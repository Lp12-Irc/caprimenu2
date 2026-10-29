// language: C++20, file: misc.cpp, target: Windows 11 x64
#include "misc.hpp"
#include "esp.hpp"  // for w2s

void Misc::update(const EntityCache& cache, const Config& cfg, Memory* mem) {
    if (cfg.no_flash && mem && mem->has_write()) apply_no_flash(cache, cfg, mem);
    tick_triggerbot(cache, cfg);
}

void Misc::apply_no_flash(const EntityCache& cache, const Config& cfg, Memory* mem) {
    // Writes m_flFlashAlpha — requires VM_WRITE handle (same detection note as silent aim)
    const auto* local = cache.local();
    if (!local || !local->pawn) return;
    if (local->flash > cfg.no_flash_max)
        mem->write<float>(local->pawn + offsets::m_flFlashAlpha, cfg.no_flash_max);
}

bool Misc::enemy_under_crosshair(const EntityCache& cache, int sw, int sh) const {
    const auto* local = cache.local();
    if (!local) return false;

    Esp esp;
    const auto& vm = cache.view_matrix();
    float cx = sw * 0.5f, cy = sh * 0.5f;
    constexpr float THRESHOLD = 8.f; // px tolerance

    for (const auto& p : cache.players()) {
        if (p.is_local || !p.valid() || p.team == local->team) continue;
        // Check all bone positions
        for (int bi = bones::HEAD; bi <= bones::CHEST_UP; ++bi) {
            Scr s = esp.w2s(p.bone[bi].pos(), vm, sw, sh);
            if (!s.vis) continue;
            float dx = s.x - cx, dy = s.y - cy;
            if (sqrtf(dx*dx + dy*dy) < THRESHOLD) return true;
        }
    }
    return false;
}

void Misc::tick_triggerbot(const EntityCache& cache, const Config& cfg) {
    if (!cfg.triggerbot) return;
    if (!(GetAsyncKeyState(cfg.tb_key) & 0x8000)) { tb_fired_ = false; return; }

    // Get screen dims from overlay — passed via cfg or global; use 1920x1080 fallback
    // In main loop, pass sw/sh explicitly
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);

    bool hit = enemy_under_crosshair(cache, sw, sh);
    if (hit && !tb_fired_) {
        DWORD64 now = GetTickCount64();
        if (tb_fire_time_ == 0) {
            tb_fire_time_ = now + static_cast<DWORD64>(cfg.tb_delay_ms);
        } else if (now >= tb_fire_time_) {
            INPUT in{ .type = INPUT_MOUSE, .mi = { .dwFlags = MOUSEEVENTF_LEFTDOWN }};
            SendInput(1, &in, sizeof(INPUT));
            in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            Sleep(static_cast<DWORD>(cfg.tb_release_ms));
            SendInput(1, &in, sizeof(INPUT));
            tb_fired_     = true;
            tb_fire_time_ = 0;
        }
    } else if (!hit) {
        tb_fire_time_ = 0;
        tb_fired_     = false;
    }
}
