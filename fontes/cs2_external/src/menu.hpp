// language: C++20, file: menu.hpp, target: Windows 11 x64, ImGui
#pragma once
#include "config.hpp"
#include "auth.hpp"
#include <string>

class Menu {
public:
    static void apply_theme();
    void render(Config& cfg, const AuthInfo& auth_info, bool& running);
    void radar_controls(Config& cfg, int sw, int sh);
    bool want_capture_keyboard() const;
    bool want_capture_mouse()    const;

private:
    void tab_esp    (Config& cfg);
    void tab_aimbot (Config& cfg);
    void tab_misc   (Config& cfg);
    void tab_config (Config& cfg);
    void tab_info   (Config& cfg, const AuthInfo& ai);
    void color_edit (const char* label, ImU32& col);

    int active_tab_ = 0;
    std::string status_;
    char profile_name_[64] = "default";
};
