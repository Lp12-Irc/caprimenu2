// language: C++20 / file: config.hpp / runtime: MSVC / target: Windows x64
// Persisted as XOR-obfuscated JSON for compatibility with existing profiles.
#pragma once
#include <cstdint>
#include <string>
#include <Windows.h>

// ImU32 = uint32_t ABGR (ImGui format)
using ImU32 = uint32_t;
constexpr ImU32 COL(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return (ImU32(a) << 24) | (ImU32(b) << 16) | (ImU32(g) << 8) | r;
}

struct Config {
    // ── ESP ──────────────────────────────────────────────────────────────
    bool  esp             = true;
    bool  esp_box         = true;
    int   esp_box_style   = 1;   // 0=outline 1=corner 2=filled
    float esp_box_thick   = 1.5f;
    bool  esp_skeleton    = true;
    float esp_skel_thick  = 1.2f;
    bool  esp_snaplines   = false;
    bool  esp_distance    = true;
    bool  esp_healthbar   = true;
    bool  esp_health_num  = false;
    bool  esp_weapon      = true;
    bool  esp_utilities   = true;
    bool  esp_vischeck    = true;
    int   esp_filter      = 0;   // 0=enemies 1=allies 2=all
    float esp_max_dist    = 500.f; // meters

    ImU32 col_ct          = COL(0,  180, 255);
    ImU32 col_tr          = COL(255, 80,  80);
    ImU32 col_visible     = COL(0,  255,  80);
    ImU32 col_skeleton    = COL(255,255, 255, 180);
    ImU32 col_snapline    = COL(255,255,   0, 120);

    // ── Aimbot ───────────────────────────────────────────────────────────
    bool  aim             = false;
    int   aim_key         = VK_RBUTTON;
    int   aim_bone        = 0;    // 0=head 1=neck 2=chest
    float aim_fov         = 8.f;
    float aim_smooth      = 2.f;
    int   aim_target      = 0;    // 0=min-fov 1=min-dist
    bool  aim_ads_only    = false;
    float aim_sens        = 1.0f; // match in-game sens
    float aim_delay_ms    = 0.f;
    bool  aim_fov_circle  = true;

    bool  silent_aim      = false;
    float sa_fov          = 4.f;
    int   sa_bone         = 0;

    bool  rcs             = false;
    float rcs_intensity   = 1.0f;

    // ── Misc ─────────────────────────────────────────────────────────────
    bool  radar           = true;
    float radar_x         = 15.f;   // % of screen width
    float radar_y         = 75.f;   // % of screen height
    float radar_sz        = 320.f;  // px
    float radar_scale     = 300.f;  // world units per pixel
    float radar_zoom      = 1.f;
    bool radar_names      = true;

    bool  no_flash        = false;
    float no_flash_max    = 0.f;    // max flash alpha allowed (0=no flash)

    bool  crosshair       = false;
    ImU32 ch_col          = COL(0, 255, 80, 200);
    float ch_size         = 6.f;
    float ch_thick        = 1.5f;
    float ch_gap          = 2.f;
    bool  ch_dot          = true;

    bool  triggerbot      = false;
    int   tb_key          = VK_MENU;
    float tb_delay_ms     = 0.f;
    float tb_release_ms   = 30.f;

    bool  spectator_list  = true;
    bool  bomb_timer      = true;
    bool  item_esp        = false;

    // ── Menu ─────────────────────────────────────────────────────────────
    int   menu_key        = VK_PRIOR;
    bool  menu_open       = false;
    std::string game_status = "Aguardando CS2";
    bool game_ready = false;
    bool write_available = false;
    std::string trigger_status = "Desligado";

    // ── Auth ─────────────────────────────────────────────────────────────
    char  saved_key[256]  = {};

    bool save(const std::string& path, std::string* error = nullptr) const;
    bool load(const std::string& path, std::string* error = nullptr);
};
