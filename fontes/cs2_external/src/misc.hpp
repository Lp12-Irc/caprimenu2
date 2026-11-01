// language: C++20, file: misc.hpp, target: Windows 11 x64
#pragma once
#include "entities.hpp"
#include "config.hpp"
#include "memory.hpp"
#include "input.hpp"

class Misc {
public:
    enum class TriggerState { Off, Paused, WaitingKey, NoTarget, Delay, Pressed, InputRejected };
    TriggerState trigger_state() const { return trigger_state_; }
    static const char* trigger_text(TriggerState state) {
        switch (state) {
            case TriggerState::Off: return "Desligado";
            case TriggerState::Paused: return "Pausado: feche o menu e volte ao jogo";
            case TriggerState::WaitingKey: return "Aguardando tecla";
            case TriggerState::NoTarget: return "Aguardando inimigo sob a mira";
            case TriggerState::Delay: return "Aguardando atraso configurado";
            case TriggerState::Pressed: return "Disparando";
            default: return "Windows recusou a entrada de mouse";
        }
    }
    explicit Misc(InputApi input = {}) : input_(input) {}
    void update(const EntityCache& cache, const Config& cfg, Memory* mem, int sw, int sh);
    void reset();
    // Triggerbot state machine (delay needs wall clock)
    void tick_triggerbot(const EntityCache& cache, const Config& cfg, int sw, int sh);

private:
    uintptr_t enemy_under_crosshair(const EntityCache& cache, int sw, int sh) const;
    void apply_no_flash(const EntityCache& cache, const Config& cfg, Memory* mem);

    DWORD64 tb_fire_time_  = 0;
    bool    tb_fired_      = false;
    DWORD64 tb_release_time_ = 0;
    uintptr_t tb_target_ = 0;
    Memory* flash_memory_ = nullptr;
    uintptr_t flash_pawn_ = 0;
    float original_alpha_ = 255.f;
    void restore_flash();
    void reset_trigger();
    InputApi input_;
    TriggerState trigger_state_ = TriggerState::Off;
};
