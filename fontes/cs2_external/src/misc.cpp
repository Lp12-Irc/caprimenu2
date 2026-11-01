#include "misc.hpp"
#include "esp.hpp"

void Misc::reset() {
    restore_flash();
    reset_trigger();
    trigger_state_=TriggerState::Paused;
}
void Misc::reset_trigger() {
    if (tb_fired_) {
        INPUT input{}; input.type = INPUT_MOUSE; input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        input_.send(1, &input, sizeof(input));
    }
    tb_fired_ = false; tb_fire_time_ = tb_release_time_ = 0; tb_target_ = 0;
}
void Misc::restore_flash() {
    if (flash_memory_ && flash_pawn_ && flash_memory_->is_valid())
        flash_memory_->write<float>(flash_pawn_ + offsets::m_flFlashMaxAlpha, original_alpha_);
    flash_memory_ = nullptr; flash_pawn_ = 0;
}
void Misc::update(const EntityCache& cache, const Config& cfg, Memory* mem, int sw, int sh) {
    const auto* local = cache.local();
    if (!local || !local->valid()) { reset(); return; }
    if (cfg.no_flash && mem && mem->has_write()) apply_no_flash(cache, cfg, mem);
    else restore_flash();
    tick_triggerbot(cache, cfg, sw, sh);
}
void Misc::apply_no_flash(const EntityCache& cache, const Config& cfg, Memory* mem) {
    const auto* local = cache.local();
    if (!local || !local->pawn) return;
    if (flash_pawn_ != local->pawn) {
        restore_flash();
        if (!mem->read_into(local->pawn + offsets::m_flFlashMaxAlpha, original_alpha_)) return;
        flash_memory_ = mem; flash_pawn_ = local->pawn;
    }
    mem->write<float>(local->pawn + offsets::m_flFlashMaxAlpha, cfg.no_flash_max);
    if (local->flash > cfg.no_flash_max) {
        mem->write<float>(local->pawn + offsets::m_flFlashOverlayAlpha, cfg.no_flash_max);
        mem->write<float>(local->pawn + offsets::m_flFlashScreenshotAlpha, cfg.no_flash_max);
    }
}
uintptr_t Misc::enemy_under_crosshair(const EntityCache& cache, int sw, int sh) const {
    const auto* local = cache.local();
    if (!local || sw <= 0 || sh <= 0) return 0;
    for (const auto& p : cache.players()) {
        if (!p.is_local && p.valid() && p.team != local->team && p.pawn == local->crosshair_target) return p.pawn;
    }
    return 0;
}
void Misc::tick_triggerbot(const EntityCache& cache, const Config& cfg, int sw, int sh) {
    const auto now = input_.now();
    if (!cfg.triggerbot) { reset_trigger(); trigger_state_=TriggerState::Off; return; }
    if (!(input_.key(cfg.tb_key) & 0x8000)) { reset_trigger(); trigger_state_=TriggerState::WaitingKey; return; }
    if (tb_fired_) {
        if (now < tb_release_time_) return;
        reset_trigger();
    }
    const auto target = enemy_under_crosshair(cache, sw, sh);
    if (!target) { reset_trigger(); trigger_state_=TriggerState::NoTarget; return; }
    if (target != tb_target_) { tb_target_ = target; tb_fire_time_ = now + static_cast<ULONGLONG>(cfg.tb_delay_ms); }
    if (now < tb_fire_time_) { trigger_state_=TriggerState::Delay; return; }
    INPUT input{}; input.type = INPUT_MOUSE; input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    if (input_.send(1, &input, sizeof(input)) == 1) {
        tb_fired_ = true; tb_release_time_ = now + static_cast<ULONGLONG>(cfg.tb_release_ms);
        trigger_state_=TriggerState::Pressed;
    } else trigger_state_=TriggerState::InputRejected;
}
