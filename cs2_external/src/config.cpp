// language: C++20, file: config.cpp, target: Windows 11 x64
// Requires: json.hpp (nlohmann/json single header)
#include "config.hpp"
#include "string_enc.hpp"
#include "json.hpp"
#include <fstream>
#include <vector>

using json = nlohmann::json;

static std::vector<uint8_t> xor_bytes(const std::string& s) {
    std::vector<uint8_t> v(s.size());
    for (size_t i = 0; i < s.size(); ++i)
        v[i] = static_cast<uint8_t>(s[i]) ^ enc::KEY;
    return v;
}

void Config::save(const std::string& path) const {
    json j;
    j["esp"] = esp; j["esp_box"] = esp_box; j["esp_box_style"] = esp_box_style;
    j["esp_box_thick"] = esp_box_thick; j["esp_skeleton"] = esp_skeleton;
    j["esp_skel_thick"] = esp_skel_thick; j["esp_snaplines"] = esp_snaplines;
    j["esp_distance"] = esp_distance; j["esp_healthbar"] = esp_healthbar;
    j["esp_health_num"] = esp_health_num; j["esp_weapon"] = esp_weapon;
    j["esp_utilities"] = esp_utilities; j["esp_vischeck"] = esp_vischeck;
    j["esp_filter"] = esp_filter; j["esp_max_dist"] = esp_max_dist;
    j["col_ct"] = col_ct; j["col_tr"] = col_tr;
    j["col_visible"] = col_visible; j["col_skeleton"] = col_skeleton;
    j["aim"] = aim; j["aim_key"] = aim_key; j["aim_bone"] = aim_bone;
    j["aim_fov"] = aim_fov; j["aim_smooth"] = aim_smooth;
    j["aim_target"] = aim_target; j["aim_ads_only"] = aim_ads_only;
    j["aim_sens"] = aim_sens; j["aim_delay_ms"] = aim_delay_ms;
    j["aim_fov_circle"] = aim_fov_circle;
    j["silent_aim"] = silent_aim; j["sa_fov"] = sa_fov; j["sa_bone"] = sa_bone;
    j["rcs"] = rcs; j["rcs_intensity"] = rcs_intensity;
    j["radar"] = radar; j["radar_x"] = radar_x; j["radar_y"] = radar_y;
    j["radar_sz"] = radar_sz; j["radar_scale"] = radar_scale;
    j["no_flash"] = no_flash; j["no_flash_max"] = no_flash_max;
    j["crosshair"] = crosshair; j["ch_size"] = ch_size;
    j["ch_thick"] = ch_thick; j["ch_gap"] = ch_gap; j["ch_dot"] = ch_dot;
    j["triggerbot"] = triggerbot; j["tb_key"] = tb_key;
    j["tb_delay_ms"] = tb_delay_ms; j["tb_release_ms"] = tb_release_ms;
    j["spectator_list"] = spectator_list; j["bomb_timer"] = bomb_timer;
    j["item_esp"] = item_esp; j["menu_key"] = menu_key;
    j["saved_key"] = std::string(saved_key);

    std::string s = j.dump(2);
    auto enc = xor_bytes(s);
    std::ofstream f(path, std::ios::binary);
    if (f.is_open()) f.write(reinterpret_cast<char*>(enc.data()), enc.size());
}

void Config::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return;
    std::vector<uint8_t> enc((std::istreambuf_iterator<char>(f)), {});

    std::string dec(enc.size(), '\0');
    for (size_t i = 0; i < enc.size(); ++i)
        dec[i] = static_cast<char>(enc[i] ^ enc::KEY);

    try {
        auto j = json::parse(dec);
        #define LOAD(key, field, def) field = j.value(#key, def)
        LOAD(esp, esp, true); LOAD(esp_box, esp_box, true);
        LOAD(esp_box_style, esp_box_style, 1);
        LOAD(esp_box_thick, esp_box_thick, 1.5f);
        LOAD(esp_skeleton, esp_skeleton, true);
        LOAD(esp_skel_thick, esp_skel_thick, 1.2f);
        LOAD(esp_snaplines, esp_snaplines, false);
        LOAD(esp_distance, esp_distance, true);
        LOAD(esp_healthbar, esp_healthbar, true);
        LOAD(esp_health_num, esp_health_num, false);
        LOAD(esp_weapon, esp_weapon, true);
        LOAD(esp_utilities, esp_utilities, true);
        LOAD(esp_vischeck, esp_vischeck, true);
        LOAD(esp_filter, esp_filter, 0);
        LOAD(esp_max_dist, esp_max_dist, 500.f);
        LOAD(col_ct, col_ct, COL(0,180,255));
        LOAD(col_tr, col_tr, COL(255,80,80));
        LOAD(aim, aim, false); LOAD(aim_key, aim_key, VK_RBUTTON);
        LOAD(aim_bone, aim_bone, 0); LOAD(aim_fov, aim_fov, 8.f);
        LOAD(aim_smooth, aim_smooth, 5.f);
        LOAD(aim_target, aim_target, 0);
        LOAD(aim_ads_only, aim_ads_only, true);
        LOAD(aim_sens, aim_sens, 1.0f);
        LOAD(aim_delay_ms, aim_delay_ms, 0.f);
        LOAD(aim_fov_circle, aim_fov_circle, true);
        LOAD(silent_aim, silent_aim, false);
        LOAD(sa_fov, sa_fov, 4.f); LOAD(sa_bone, sa_bone, 0);
        LOAD(rcs, rcs, false); LOAD(rcs_intensity, rcs_intensity, 1.0f);
        LOAD(radar, radar, true);
        LOAD(radar_x, radar_x, 15.f); LOAD(radar_y, radar_y, 75.f);
        LOAD(radar_sz, radar_sz, 160.f); LOAD(radar_scale, radar_scale, 300.f);
        LOAD(no_flash, no_flash, false); LOAD(no_flash_max, no_flash_max, 0.f);
        LOAD(crosshair, crosshair, false);
        LOAD(ch_size, ch_size, 6.f); LOAD(ch_thick, ch_thick, 1.5f);
        LOAD(ch_gap, ch_gap, 2.f); LOAD(ch_dot, ch_dot, true);
        LOAD(triggerbot, triggerbot, false);
        LOAD(tb_key, tb_key, VK_MENU);
        LOAD(tb_delay_ms, tb_delay_ms, 75.f);
        LOAD(spectator_list, spectator_list, true);
        LOAD(bomb_timer, bomb_timer, true);
        LOAD(item_esp, item_esp, false);
        LOAD(menu_key, menu_key, VK_INSERT);
        #undef LOAD
        auto k = j.value("saved_key", std::string{});
        strncpy_s(saved_key, k.c_str(), sizeof(saved_key) - 1);
    } catch (...) {}
}
