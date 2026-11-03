#include "misc.hpp"
#include "head_gate.hpp"
#include <algorithm>

void Misc::reset() {
    restore_flash();
    reset_trigger();
    trigger_state_=TriggerState::Paused;
}
void Misc::suspend(const Config& cfg) {
    restore_flash();
    if(!cfg.triggerbot || !(input_.key(cfg.tb_key)&0x8000)){reset_trigger();trigger_state_=cfg.triggerbot?TriggerState::WaitingKey:TriggerState::Off;return;}
    if(cfg.tb_head_only){reset_trigger();trigger_state_=TriggerState::WaitingData;return;}
    if(tb_fired_ && input_.now()>=tb_release_time_) {
        INPUT input{};input.type=INPUT_MOUSE;input.mi.dwFlags=MOUSEEVENTF_LEFTUP;
        input_.send(1,&input,sizeof(input));tb_fired_=false;
        tb_next_press_time_=input_.now()+static_cast<ULONGLONG>(cfg.tb_interval_ms);
    }
    trigger_state_=TriggerState::WaitingData;
}
void Misc::reset_trigger() {
    if (tb_fired_) {
        INPUT input{}; input.type = INPUT_MOUSE; input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        input_.send(1, &input, sizeof(input));
    }
    tb_fired_ = false; tb_fire_time_ = tb_release_time_ = 0; tb_target_ = 0; tb_next_press_time_=0;
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
    int team=0;
    if (!local || sw <= 0 || sh <= 0 || !cache.current_local_team(team)) return 0;
    uintptr_t target=0;if(!cache.current_crosshair_target(target))return 0;
    for (const auto& p : cache.players()) {
        if (!p.is_local && p.valid() && p.team != team && p.pawn == target && cache.current_enemy(p,team)) return p.pawn;
    }
    return 0;
}
bool Misc::head_aligned(const EntityCache& cache, uintptr_t target, const Config& cfg, TriggerState& reason) const {
    reason = TriggerState::WaitingData;
    for (const auto& player : cache.players()) {
        if (player.pawn != target) continue;
        PlayerEnt live;
        Matrix4x4 camera{};
        if (!cache.refresh_pose(player, live) || !cache.current_view_matrix(camera)) return false;
        const auto result = head_gate::test(live, camera, cfg.tb_head_margin);
        if (result == head_gate::Result::Outside) reason = TriggerState::WaitingHead;
        return result == head_gate::Result::Inside;
    }
    return false;
}
void Misc::tick_triggerbot(const EntityCache& cache, const Config& cfg, int sw, int sh) {
    const auto now = input_.now();
    if (!cfg.triggerbot) { reset_trigger(); trigger_state_=TriggerState::Off; return; }
    if (!(input_.key(cfg.tb_key) & 0x8000)) { reset_trigger(); trigger_state_=TriggerState::WaitingKey; return; }
    if (tb_fired_) {
        TriggerState reason = TriggerState::NoTarget;
        const auto target = cfg.tb_head_only ? enemy_under_crosshair(cache, sw, sh) : tb_target_;
        const bool still_aligned = !cfg.tb_head_only || (target == tb_target_ && head_aligned(cache, target, cfg, reason));
        if (now < tb_release_time_ && still_aligned) return;
        reset_trigger();tb_next_press_time_=now+static_cast<ULONGLONG>(cfg.tb_interval_ms);
        trigger_state_=TriggerState::Release;return;
    }
    if(now<tb_next_press_time_){trigger_state_=TriggerState::Release;return;}
    const auto target = enemy_under_crosshair(cache, sw, sh);
    if (!target) { reset_trigger(); trigger_state_=TriggerState::NoTarget; return; }
    if (cfg.tb_head_only) {
        TriggerState reason;
        if (!head_aligned(cache, target, cfg, reason)) { reset_trigger(); trigger_state_=reason; return; }
    }
    if (target != tb_target_) { tb_target_ = target; tb_fire_time_ = now + static_cast<ULONGLONG>(cfg.tb_delay_ms); }
    if (now < tb_fire_time_) { trigger_state_=TriggerState::Delay; return; }
    INPUT input{}; input.type = INPUT_MOUSE; input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    if (input_.send(1, &input, sizeof(input)) == 1) {
        tb_fired_ = true; tb_release_time_ = now + static_cast<ULONGLONG>(cfg.tb_release_ms);
        trigger_state_=TriggerState::Pressed;
    } else trigger_state_=TriggerState::InputRejected;
}
