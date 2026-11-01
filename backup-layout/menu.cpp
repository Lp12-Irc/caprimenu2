// language: C++20, file: menu.cpp, target: Windows 11 x64, ImGui
#include "menu.hpp"
#include "brand.hpp"
#include "imgui.h"
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
namespace fs = std::filesystem;

static const char* BONE_NAMES[] = {"Cabeca","Pescoco","Peito"};
static const char* FILTER_NAMES[] = {"Inimigos","Aliados","Todos"};
static const char* BOX_STYLES[]   = {"Contorno","Cantos","Preenchida"};
static const char* TARGET_MODES[] = {"Min FOV","Min Distance"};

static std::string key_name(int key) {
    switch (key) {
        case VK_LBUTTON: return "Mouse esquerdo";
        case VK_RBUTTON: return "Mouse direito";
        case VK_MBUTTON: return "Mouse central";
        case VK_XBUTTON1: return "Mouse lateral 1";
        case VK_XBUTTON2: return "Mouse lateral 2";
        case VK_PRIOR: return "PgUp";
    }
    UINT scan = MapVirtualKeyW(key, MAPVK_VK_TO_VSC_EX);
    LONG parameter = (scan & 0xFF) << 16;
    if (scan & 0xFF00) parameter |= 1 << 24;
    wchar_t name[64]{}; char utf8[192]{};
    if (!GetKeyNameTextW(parameter, name, 64)) return "Tecla " + std::to_string(key);
    WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8, sizeof(utf8), nullptr, nullptr);
    return utf8;
}
static void key_control(const char* label, int& key) {
    if (!ImGui::BeginCombo(label, key_name(key).c_str())) return;
    for (int candidate = 1; candidate < 255; ++candidate) {
        if (candidate > VK_XBUTTON2 && !MapVirtualKeyW(candidate, MAPVK_VK_TO_VSC)) continue;
        ImGui::PushID(candidate);
        if (ImGui::Selectable(key_name(candidate).c_str(), candidate == key)) key = candidate;
        if (candidate == key) ImGui::SetItemDefaultFocus();
        ImGui::PopID();
    }
    ImGui::EndCombo();
}

// Compact color picker: inline button showing the color, opens picker on click
void Menu::color_edit(const char* label, ImU32& col) {
    float fc[4] = {
        float(col & 0xFF) / 255.f,
        float((col >> 8)  & 0xFF) / 255.f,
        float((col >> 16) & 0xFF) / 255.f,
        float((col >> 24) & 0xFF) / 255.f
    };
    if (ImGui::ColorEdit4(label, fc, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar)) {
        col = IM_COL32(uint8_t(fc[0]*255), uint8_t(fc[1]*255),
                       uint8_t(fc[2]*255), uint8_t(fc[3]*255));
    }
}

void Menu::apply_theme() {
    ImGui::StyleColorsDark();
    auto& st = ImGui::GetStyle();
    st.WindowPadding = {22, 20}; st.FramePadding = {10, 7};
    st.ItemSpacing = {12, 10}; st.ItemInnerSpacing = {8, 6};
    st.WindowRounding = 14; st.ChildRounding = 10; st.FrameRounding = 6;
    st.PopupRounding = 8; st.ScrollbarRounding = 8; st.GrabRounding = 5;
    st.WindowBorderSize = 1; st.ChildBorderSize = 1; st.FrameBorderSize = 0;
    auto* c = st.Colors;
    c[ImGuiCol_WindowBg] = {0.035f, 0.043f, 0.055f, 1};
    c[ImGuiCol_ChildBg] = {0.048f, 0.055f, 0.068f, 1};
    c[ImGuiCol_PopupBg] = {0.075f, 0.09f, 0.12f, 1};
    c[ImGuiCol_Border] = {0.19f, 0.21f, 0.26f, 1};
    c[ImGuiCol_Text] = {0.91f, 0.94f, 0.97f, 1};
    c[ImGuiCol_TextDisabled] = {0.47f, 0.54f, 0.63f, 1};
    c[ImGuiCol_FrameBg] = {0.10f, 0.13f, 0.17f, 1};
    c[ImGuiCol_FrameBgHovered] = {0.16f, 0.21f, 0.27f, 1};
    c[ImGuiCol_FrameBgActive] = {0.18f, 0.27f, 0.31f, 1};
    c[ImGuiCol_Button] = {0.20f, 0.055f, 0.12f, 1};
    c[ImGuiCol_ButtonHovered] = {0.55f, 0.025f, 0.25f, 1};
    c[ImGuiCol_ButtonActive] = {0.85f, 0.015f, 0.33f, 1};
    c[ImGuiCol_CheckMark] = c[ImGuiCol_SliderGrab] = {1.0f, 0.0f, 0.36f, 1};
    c[ImGuiCol_SliderGrabActive] = {1.0f, 0.35f, 0.62f, 1};
    c[ImGuiCol_Header] = {0.23f, 0.035f, 0.12f, 1};
    c[ImGuiCol_HeaderHovered] = {0.38f, 0.035f, 0.18f, 1};
    c[ImGuiCol_HeaderActive] = {0.55f, 0.02f, 0.24f, 1};
    c[ImGuiCol_Separator] = {0.19f, 0.21f, 0.26f, 1};
}

void Menu::render(Config& cfg, const AuthInfo& auth_info, bool& running) {
    auto& io = ImGui::GetIO();
    const ImVec2 size{(std::min)(930.f, io.DisplaySize.x - 24.f),
                      (std::min)(650.f, io.DisplaySize.y - 24.f)};
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos({io.DisplaySize.x * .5f, io.DisplaySize.y * .5f},
                            ImGuiCond_FirstUseEver, {.5f,.5f});
    if (!ImGui::Begin("Capri", nullptr, ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {
        ImGui::End(); return;
    }
    Brand::avatar(42);
    ImGui::SameLine();
    ImGui::TextColored({1.f,0.f,.36f,1}, "CAPRI");
    ImGui::SameLine(); ImGui::TextDisabled("     CS2 / PAINEL");
    ImGui::SameLine(ImGui::GetWindowWidth() - 120);
    if (ImGui::SmallButton("Ocultar")) cfg.menu_open = false;
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::BeginChild("navigation", {164, -40}, true);
    ImGui::TextDisabled("NAVEGACAO"); ImGui::Spacing();
    const char* labels[] = {"01   Visuais", "02   Mira", "03   Utilitarios", "04   Perfis", "05   Conta"};
    for (int i = 0; i < 5; ++i) {
        if (ImGui::Selectable(labels[i], active_tab_ == i, 0, {0, 38})) active_tab_ = i;
    }
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::TextDisabled("CAPRI / 1.1");
    ImGui::TextWrapped("Ajustes aplicados ao alterar os controles.");
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("content", {0, -40}, true);
    const char* titles[] = {"Visuais", "Mira e controle", "Utilitarios", "Seus perfis", "Sua conta"};
    ImGui::TextColored({1.f,0.f,.36f,1}, "%s", titles[active_tab_]);
    ImGui::TextDisabled("CONFIGURACOES DO PERFIL ATUAL");
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::PushItemWidth(180);
    switch (active_tab_) {
        case 0: tab_esp(cfg); break;
        case 1: tab_aimbot(cfg); break;
        case 2: tab_misc(cfg); break;
        case 3: tab_config(cfg); break;
        case 4: tab_info(cfg, auth_info); break;
    }
    ImGui::PopItemWidth();
    ImGui::EndChild();
    ImGui::Spacing();
    ImGui::TextDisabled("%s abrir / ocultar | %s", key_name(cfg.menu_key).c_str(), cfg.game_status.c_str());
    ImGui::SameLine(ImGui::GetWindowWidth() - 110);
    if (ImGui::SmallButton("Encerrar")) running = false;
    ImGui::End();
}

void Menu::tab_esp(Config& cfg) {
    ImGui::Checkbox("Ativar ESP", &cfg.esp);
    ImGui::Separator();
    if (!cfg.esp) { ImGui::TextDisabled("Ative o ESP para ajustar a exibicao."); return; }

    ImGui::Columns(2, nullptr, false);

    ImGui::Checkbox("Caixa do jogador", &cfg.esp_box);
    if (cfg.esp_box) {
        ImGui::Combo("Estilo##box", &cfg.esp_box_style, BOX_STYLES, 3);
        ImGui::SliderFloat("Espessura##box", &cfg.esp_box_thick, 0.5f, 4.f);
    }
    ImGui::Checkbox("Esqueleto",     &cfg.esp_skeleton);
    if (cfg.esp_skeleton)
        ImGui::SliderFloat("Espessura##sk", &cfg.esp_skel_thick, 0.5f, 4.f);
    ImGui::Checkbox("Linhas de referencia",   &cfg.esp_snaplines);
    ImGui::Checkbox("Distancia",     &cfg.esp_distance);
    ImGui::Checkbox("Barra de vida",   &cfg.esp_healthbar);
    if (cfg.esp_healthbar)
        ImGui::Checkbox("Vida numerica", &cfg.esp_health_num);
    ImGui::Checkbox("Arma equipada",  &cfg.esp_weapon);
    ImGui::Checkbox("Inventario de utilitarios", &cfg.esp_utilities);
    ImGui::Checkbox("Cor de jogador avistado", &cfg.esp_vischeck);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Usa o estado de avistamento do jogo; nao representa um teste de paredes.");

    ImGui::NextColumn();

    ImGui::Combo("Jogadores##esp", &cfg.esp_filter, FILTER_NAMES, 3);
    ImGui::SliderFloat("Distancia maxima (m)", &cfg.esp_max_dist, 10.f, 1000.f);
    ImGui::Separator();
    color_edit("Cor dos CTs",      cfg.col_ct);
    color_edit("Cor dos TRs",      cfg.col_tr);
    color_edit("Jogador avistado", cfg.col_visible);
    color_edit("Cor do esqueleto",cfg.col_skeleton);
    color_edit("Cor das linhas",cfg.col_snapline);

    ImGui::Columns(1);
}

void Menu::tab_aimbot(Config& cfg) {
    ImGui::Checkbox("Aimbot", &cfg.aim);
    if (cfg.aim) {
        key_control("Tecla da mira", cfg.aim_key);
        ImGui::Combo("Bone##aim",   &cfg.aim_bone,   BONE_NAMES,   3);
        ImGui::Combo("Target##aim", &cfg.aim_target, TARGET_MODES, 2);
        ImGui::SliderFloat("FOV##aim",    &cfg.aim_fov,    1.f, 45.f);
        ImGui::SliderFloat("Smooth##aim", &cfg.aim_smooth, 1.f, 15.f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("1 = resposta mais rapida; valores maiores suavizam ao longo do tempo.");
        ImGui::SliderFloat("Multiplicador sens.", &cfg.aim_sens, 0.1f, 5.f);
        ImGui::SliderFloat("Delay ms",    &cfg.aim_delay_ms, 0.f, 200.f);
        ImGui::Checkbox("Exigir mouse direito", &cfg.aim_ads_only);
        ImGui::Checkbox("FOV Circle",      &cfg.aim_fov_circle);
    }
    ImGui::Separator();
    ImGui::TextDisabled("Silent Aim: pendente de implementacao");
    ImGui::TextWrapped("Esta versao ainda nao possui um modulo funcional para ativar ou configurar Silent Aim.");
    ImGui::Separator();
    ImGui::Checkbox("Recoil Control (RCS)", &cfg.rcs);
    if (cfg.rcs)
        ImGui::SliderFloat("RCS Intensity", &cfg.rcs_intensity, 0.1f, 2.f);
}

void Menu::tab_misc(Config& cfg) {
    ImGui::Columns(2, nullptr, false);

    ImGui::Checkbox("Radar", &cfg.radar);
    if (cfg.radar) {
        ImGui::SliderFloat("Pos X%%", &cfg.radar_x, 0.f, 90.f);
        ImGui::SliderFloat("Pos Y%%", &cfg.radar_y, 0.f, 90.f);
        ImGui::SliderFloat("Tamanho do mapa", &cfg.radar_sz, 200.f, 600.f);
        ImGui::SliderFloat("Zoom do mapa", &cfg.radar_zoom, 1.f, 4.f);
        ImGui::Checkbox("Nomes no radar", &cfg.radar_names);
        ImGui::TextWrapped("Mapa completo com norte acima. Branco: voce; azul: aliado; vermelho: inimigo.");
    }
    ImGui::Separator();
    ImGui::Checkbox("No Flash", &cfg.no_flash);
    if (cfg.no_flash) {
        ImGui::TextWrapped("%s", cfg.write_available ? "Escrita conectada." : "Escrita indisponivel; confira a permissao do processo.");
        ImGui::SliderFloat("Max Alpha", &cfg.no_flash_max, 0.f, 50.f);
    }
    ImGui::Separator();
    ImGui::Checkbox("Custom Crosshair", &cfg.crosshair);
    if (cfg.crosshair) {
        ImGui::SliderFloat("Size##ch",  &cfg.ch_size,  1.f, 20.f);
        ImGui::SliderFloat("Gap##ch",   &cfg.ch_gap,   0.f, 10.f);
        ImGui::SliderFloat("Thick##ch", &cfg.ch_thick, 0.5f, 4.f);
        ImGui::Checkbox("Dot", &cfg.ch_dot);
        color_edit("CH Color", cfg.ch_col);
    }

    ImGui::NextColumn();

    ImGui::Checkbox("Triggerbot", &cfg.triggerbot);
    if (cfg.triggerbot) {
        key_control("Tecla do disparo", cfg.tb_key);
        ImGui::SliderFloat("Delay ms##tb",   &cfg.tb_delay_ms,   0.f, 300.f);
        ImGui::SliderFloat("Release ms##tb", &cfg.tb_release_ms, 10.f, 100.f);
        ImGui::TextWrapped("Feche o menu e segure %s sobre um inimigo.",key_name(cfg.tb_key).c_str());
        ImGui::TextWrapped("%s",cfg.trigger_status.c_str());
    }
    ImGui::Separator();
    ImGui::Checkbox("Espectadores", &cfg.spectator_list);
    ImGui::Checkbox("Tempo da bomba", &cfg.bomb_timer);
    ImGui::Checkbox("Item ESP", &cfg.item_esp);
    ImGui::Columns(1);
}

void Menu::tab_config(Config& cfg) {
    key_control("Abrir / ocultar menu", cfg.menu_key);
    ImGui::TextWrapped("Salve seus ajustes e alterne entre perfis. A sessao de acesso atual e preservada.");
    ImGui::Spacing();
    ImGui::InputText("Nome do perfil", profile_name_, sizeof(profile_name_));
    std::string name = profile_name_;
    bool valid = !name.empty() && name != "." && name != ".." &&
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ") == std::string::npos &&
        name.back() != ' ';
    ImGui::BeginDisabled(!valid);
    auto save = [&] {
        std::error_code ec; fs::create_directories("configs", ec);
        if (ec) { status_ = "Falha ao criar a pasta de perfis."; return; }
        std::string error;
        status_ = cfg.save("configs/" + name + ".hbc", &error) ? "Perfil salvo." : error;
    };
    if (ImGui::Button("Salvar perfil", {145, 36})) {
        std::error_code ec;
        if (fs::exists("configs/" + name + ".hbc", ec)) ImGui::OpenPopup("Substituir perfil?");
        else save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Carregar", {120, 36})) {
        std::string error, key = cfg.saved_key;
        bool loaded = cfg.load("configs/" + name + ".hbc", &error);
        strcpy_s(cfg.saved_key, key.c_str());
        status_ = loaded ? "Perfil carregado." : error;
    }
    ImGui::EndDisabled();
    if (ImGui::BeginPopupModal("Substituir perfil?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Este perfil ja existe. Substituir os ajustes salvos?");
        if (ImGui::Button("Substituir")) { save(); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (!valid) ImGui::TextWrapped("Use letras, numeros, espacos, traco ou sublinhado.");
    if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::TextDisabled("PERFIS DISPONIVEIS");
    std::error_code ec;
    fs::directory_iterator it("configs", ec), end;
    if (ec) { ImGui::TextDisabled("Nenhum perfil disponivel."); return; }
    for (; it != end; it.increment(ec)) {
        if (ec) { status_ = "Falha ao listar perfis."; break; }
        if (it->path().extension() == ".hbc") {
            std::string stem = it->path().stem().string();
            if (ImGui::Selectable(stem.c_str(), stem == name, 0, {0, 32}))
                strncpy_s(profile_name_, stem.c_str(), _TRUNCATE);
        }
    }
}

void Menu::tab_info(Config& cfg, const AuthInfo& ai) {
    ImGui::Text("Versao:    1.1 / Interface");
    ImGui::Text("HWID:      %s", ai.hwid.c_str());
    ImGui::Text("Expires:   %s", ai.expires_at.empty() ? "â€”" : ai.expires_at.c_str());

    const char* status_str = "Unknown";
    ImVec4      status_col = {1,1,0,1};
    switch (ai.result) {
        case AuthResult::OK:           status_str = "Active";        status_col = {0,1,0,1}; break;
        case AuthResult::EXPIRED:      status_str = "Expired";       status_col = {1,0.5,0,1}; break;
        case AuthResult::BAD_KEY:      status_str = "Invalid Key";   status_col = {1,0,0,1}; break;
        case AuthResult::HWID_MISMATCH:status_str = "HWID Mismatch"; status_col = {1,0,0,1}; break;
        case AuthResult::NET_ERROR:    status_str = "Net Error";     status_col = {0.7,0.7,0,1}; break;
    }
    ImGui::TextColored(status_col, "Status:    %s", status_str);
    ImGui::Separator();
    ImGui::Text("Tecla do menu: %s", key_name(cfg.menu_key).c_str());
    ImGui::TextWrapped("%s", cfg.game_status.c_str());
    ImGui::TextWrapped("Os recursos ligados ao jogo dependem da compatibilidade dos offsets com a versao instalada.");
}

bool Menu::want_capture_keyboard() const { return ImGui::GetIO().WantCaptureKeyboard; }
bool Menu::want_capture_mouse()    const { return ImGui::GetIO().WantCaptureMouse; }
