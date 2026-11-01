// language: C++20, file: menu.cpp, target: Windows 11 x64, ImGui
#include "menu.hpp"
#include "brand.hpp"
#include "imgui.h"
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdlib>
namespace fs = std::filesystem;

static const char* BONE_NAMES[] = {"Cabeca","Pescoco","Peito"};
static const char* FILTER_NAMES[] = {"Inimigos","Aliados","Todos"};
static const char* BOX_STYLES[]   = {"Contorno","Cantos","Preenchida"};
static const char* TARGET_MODES[] = {"Menor angulo","Menor distancia"};

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
    st.WindowRounding = 9; st.ChildRounding = 7; st.FrameRounding = 6;
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


namespace {
constexpr ImU32 pink = IM_COL32(255,0,96,255);
void toggle(const char* label, bool& value) {
    ImGui::PushID(label);
    auto p=ImGui::GetCursorScreenPos(); float w=ImGui::GetContentRegionAvail().x;
    bool clicked=ImGui::InvisibleButton("toggle",{w,36});
    if(clicked) value=!value;
    auto* d=ImGui::GetWindowDrawList();
    d->AddText({p.x,p.y+8},ImGui::GetColorU32(ImGuiCol_Text),label);
    d->AddRectFilled({p.x+w-48,p.y+5},{p.x+w,p.y+29},value?pink:IM_COL32(48,50,62,255),12);
    d->AddCircleFilled({p.x+w-(value?12.f:36.f),p.y+17},9,IM_COL32(237,237,245,255));
    if(ImGui::IsItemFocused()) d->AddRect(p,{p.x+w,p.y+36},pink,4);
    ImGui::Separator(); ImGui::PopID();
}
void slider_row(const char* label,float* value,float low,float high,const char* format) {
    ImGui::PushID(label); float w=ImGui::GetContentRegionAvail().x;
    ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(label);
    ImGui::SameLine(w*.46f);ImGui::SetNextItemWidth(w*.53f);
    ImGui::SliderFloat("##value",value,low,high,format);ImGui::PopID();
}
void card(const char* id,const char* title,float height=0) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{16,12}); ImGui::BeginChild(id,{0,height},true,ImGuiWindowFlags_AlwaysUseWindowPadding); ImGui::PopStyleVar();
    ImGui::TextUnformatted(title); ImGui::Separator(); ImGui::Spacing();
    ImGui::PushItemWidth(-150);
}
void endcard(){ImGui::PopItemWidth();ImGui::EndChild();}
}
void Menu::render(Config& cfg, const AuthInfo& auth_info, bool& running) {
    auto& io=ImGui::GetIO();
    ImVec2 size{(std::min)(1320.f,io.DisplaySize.x-24),(std::min)(760.f,io.DisplaySize.y-24)};
    ImGui::SetNextWindowSize(size,ImGuiCond_Always);
    ImGui::SetNextWindowPos({io.DisplaySize.x*.5f,io.DisplaySize.y*.5f},ImGuiCond_FirstUseEver,{.5f,.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::PushStyleColor(ImGuiCol_Border,{1,0,.36f,1});
    bool visible=ImGui::Begin("Capri",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor(); ImGui::PopStyleVar();
    if(!visible){ImGui::End();return;}
    const float sidebar=size.x>1000?250.f:194.f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{20,22});
    ImGui::BeginChild("sidebar",{sidebar,-66},true);
    Brand::avatar(62); ImGui::SameLine();
    ImGui::BeginGroup(); ImGui::TextUnformatted("Capri Cheat"); ImGui::TextDisabled("CS2"); ImGui::EndGroup();
    ImGui::Dummy({0,22});
    const char* labels[]={"Aimbot","Triggerbot","Visuais","Recuo","Utilitarios","Configuracoes","Conta"};
    for(int i=0;i<7;i++){
        ImGui::PushID(i);
        auto p=ImGui::GetCursorScreenPos();
        if(ImGui::Selectable(labels[i],active_tab_==i,0,{0,46})) active_tab_=i;
        if(active_tab_==i) ImGui::GetWindowDrawList()->AddRectFilled({p.x-18,p.y},{p.x-14,p.y+46},pink);
        ImGui::PopID();
    }
    ImGui::EndChild(); ImGui::SameLine(0,0);
    ImGui::BeginChild("workspace",{0,-66},false,ImGuiWindowFlags_AlwaysUseWindowPadding);
    ImGui::SetWindowFontScale(1.7f); ImGui::TextUnformatted(labels[active_tab_]); ImGui::SetWindowFontScale(1);
    ImGui::SameLine(ImGui::GetWindowWidth()-160);
    if(ImGui::SmallButton("Ocultar")) cfg.menu_open=false;
    ImGui::SameLine(); if(ImGui::SmallButton("X")) running=false;
    const char* subtitles[]={"Configuracoes de assistencia de mira","Configuracoes de disparo","Exibicao dos jogadores e cores","Controle de recuo","Radar e informacoes da partida","Gerencie seus ajustes e perfis","Informacoes da sessao"};
    ImGui::TextDisabled("%s",subtitles[active_tab_]); ImGui::Dummy({0,8});
    switch(active_tab_){
    case 0: tab_aimbot(cfg);break;
    case 1: card("trigger","Geral"); toggle("Ativar triggerbot",cfg.triggerbot); key_control("Tecla de ativacao",cfg.tb_key); ImGui::SliderFloat("Atraso (ms)",&cfg.tb_delay_ms,0,300); ImGui::SliderFloat("Soltar (ms)",&cfg.tb_release_ms,10,100); ImGui::TextWrapped("%s",cfg.trigger_status.c_str()); endcard();break;
    case 2: card("visuals","Jogadores");tab_esp(cfg);endcard();break;
    case 3: card("recoil","Controle de recuo");toggle("Ativar RCS",cfg.rcs);ImGui::SliderFloat("Intensidade",&cfg.rcs_intensity,.1f,2.f);endcard();break;
    case 4: card("utilities","Partida");tab_misc(cfg);endcard();break;
    case 5: tab_config(cfg);break;
    case 6: tab_info(cfg,auth_info);break;
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::Separator(); ImGui::SetCursorPos({22,size.y-49});
    ImGui::Text("Perfil: %s",profile_name_); ImGui::SameLine(); ImGui::TextDisabled(" | %s",key_name(cfg.menu_key).c_str());
    ImGui::SameLine((std::max)(300.f,size.x-340));
    if(ImGui::Button("Restaurar",{130,36})) ImGui::OpenPopup("Restaurar ajustes?");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,{1,0,.36f,1}); bool save_profile=ImGui::Button("Salvar perfil",{160,36}); ImGui::PopStyleColor(); if(save_profile) {
        std::string name=profile_name_;
        if(name.empty() || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ")!=std::string::npos || name.back()==' ') status_="Nome de perfil invalido.";
        else { std::error_code ec;fs::create_directories("configs",ec);std::string error;status_=!ec&&cfg.save("configs/"+name+".hbc",&error)?"Perfil salvo.":"Falha ao salvar perfil: "+error; }
    }
    if(ImGui::BeginPopupModal("Restaurar ajustes?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted("Restaurar os ajustes do painel para o padrao?");
        if(ImGui::Button("Restaurar")){Config defaults;auto key=std::string(cfg.saved_key);defaults.menu_key=cfg.menu_key;defaults.menu_open=true;defaults.game_status=cfg.game_status;defaults.game_ready=cfg.game_ready;defaults.write_available=cfg.write_available;strcpy_s(defaults.saved_key,key.c_str());cfg=defaults;ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(ImGui::Button("Cancelar"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    if(!status_.empty()){ImGui::SetCursorPos({22,size.y-22});ImGui::TextDisabled("%s",status_.c_str());}
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
    bool wide=ImGui::GetContentRegionAvail().x>670;
    if(wide) ImGui::Columns(2,"aim-layout",false);
    card("general","Geral",350);
    toggle("Ativar aimbot",cfg.aim);
    key_control("Tecla de ativacao",cfg.aim_key);
    ImGui::TextDisabled("Modo de ativacao: segurar");
    ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Campo de visao");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x*.46f);
    ImGui::SetNextItemWidth(-1);
    ImGui::Text("%.1f graus",cfg.aim_fov);
    ImGui::SameLine();
    static char fov_draft[32]{};
    static bool fov_error=false;
    if(ImGui::Button("Editar##fov")) {
        sprintf_s(fov_draft,"%.1f",cfg.aim_fov);fov_error=false;
        ImGui::OpenPopup("Editar campo de visao");
    }
    if(ImGui::BeginPopupModal("Editar campo de visao",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Valor atual: %.1f graus",cfg.aim_fov);
        ImGui::TextUnformatted("Novo valor de 1 a 45 graus (use ponto decimal):");
        ImGui::SetNextItemWidth(240);
        ImGui::InputText("##fov_draft",fov_draft,sizeof(fov_draft),ImGuiInputTextFlags_CharsDecimal);
        if(fov_error) ImGui::TextUnformatted("Digite um numero valido entre 1 e 45.");
        if(ImGui::Button("Aplicar",{110,32})) {
            char* end=nullptr;float value=std::strtof(fov_draft,&end);
            if(end!=fov_draft && *end=='\0' && std::isfinite(value) && value>=1 && value<=45) {
                cfg.aim_fov=std::round(value*10.f)/10.f;
                ImGui::CloseCurrentPopup();
            } else fov_error=true;
        }
        ImGui::SameLine();if(ImGui::Button("Cancelar",{110,32})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    slider_row("Suavizacao",&cfg.aim_smooth,1,15,"%.1f");
    ImGui::Combo("Area da mira",&cfg.aim_bone,BONE_NAMES,3);
    ImGui::Combo("Prioridade",&cfg.aim_target,TARGET_MODES,2);
    endcard();
    card("preview","Previa do campo de visao",155);
    auto p=ImGui::GetCursorScreenPos();auto area=ImGui::GetContentRegionAvail();
    ImVec2 center{p.x+area.x*.5f,p.y+area.y*.5f};
    auto* d=ImGui::GetWindowDrawList();float radius=(std::min)(area.y*.42f,25+cfg.aim_fov*1.5f);
    d->AddCircle(center,radius,pink,96,1.5f);
    d->AddLine({center.x-12,center.y},{center.x+12,center.y},IM_COL32_WHITE,2);
    d->AddLine({center.x,center.y-12},{center.x,center.y+12},IM_COL32_WHITE,2);
    ImGui::Dummy(area);endcard();
    if(wide)ImGui::NextColumn();
    card("activation","Ativacao",250);
    toggle("Exigir mouse direito",cfg.aim_ads_only);
    toggle("Exibir circulo de FOV",cfg.aim_fov_circle);
    slider_row("Atraso (ms)",&cfg.aim_delay_ms,0,200,"%.0f");
    slider_row("Sensibilidade",&cfg.aim_sens,.1f,5.f,"%.2f");
    endcard();
    card("rcs","Controle de recuo",155);
    toggle("Ativar RCS",cfg.rcs);
    slider_row("Compensacao",&cfg.rcs_intensity,.1f,2.f,"%.2f");endcard();
    card("compatibility","Disponibilidade",100);
    ImGui::TextWrapped("%s",cfg.game_status.c_str());
    endcard();
    if(wide)ImGui::Columns(1);
}

void Menu::tab_misc(Config& cfg) {
    ImGui::Columns(2, nullptr, false);

    ImGui::Checkbox("Radar", &cfg.radar);
    if (cfg.radar) {
        ImGui::SliderFloat("Pos X%%", &cfg.radar_x, 0.f, 90.f);
        ImGui::SliderFloat("Pos Y%%", &cfg.radar_y, 0.f, 90.f);
        ImGui::SliderFloat("Tamanho do mapa", &cfg.radar_sz, 80.f, 600.f,"%.0f px",ImGuiSliderFlags_AlwaysClamp);
        ImGui::TextWrapped("Com o menu aberto, arraste o titulo do radar para mover. Arraste o canto inferior direito para redimensionar. Salve o perfil para manter os ajustes.");
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

void Menu::radar_controls(Config& cfg,int sw,int sh) {
    if(!cfg.menu_open || !cfg.radar || sw<=0 || sh<=0) return;
    float size=std::clamp(cfg.radar_sz,80.f,(std::max)(80.f,float((std::min)(sw-24,sh-90))));
    float x=std::clamp(sw*cfg.radar_x/100.f,8.f,(std::max)(8.f,sw-size-8.f));
    float y=std::clamp(sh*cfg.radar_y/100.f,30.f,(std::max)(30.f,sh-size-52.f));
    ImGui::SetNextWindowPos({x-5,y-27},ImGuiCond_Always);
    ImGui::SetNextWindowSize({size+10,27},ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    constexpr auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoFocusOnAppearing;
    ImGui::Begin("##radar_move",nullptr,flags);
    ImGui::InvisibleButton("Arrastar radar",{size+10,27});
    if(ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        auto delta=ImGui::GetIO().MouseDelta;
        x=std::clamp(x+delta.x,8.f,(std::max)(8.f,sw-size-8.f));
        y=std::clamp(y+delta.y,30.f,(std::max)(30.f,sh-size-52.f));
        cfg.radar_x=x*100.f/sw; cfg.radar_y=y*100.f/sh;
    }
    if(ImGui::IsItemHovered()) {ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);ImGui::SetTooltip("Arraste para mover o radar");}
    ImGui::End();
    ImGui::SetNextWindowPos({x+size-18,y+size-18},ImGuiCond_Always);
    ImGui::SetNextWindowSize({23,23},ImGuiCond_Always);
    ImGui::Begin("##radar_resize",nullptr,flags);
    ImGui::InvisibleButton("Redimensionar radar",{23,23});
    if(ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        const auto delta=ImGui::GetIO().MouseDelta;
        const float change=std::fabs(delta.x)>std::fabs(delta.y)?delta.x:delta.y;
        const float max_size=(std::max)(80.f,(std::min)({600.f,sw-x-8.f,sh-y-52.f}));
        cfg.radar_sz=std::clamp(size+change,80.f,max_size);
    }
    auto p=ImGui::GetWindowPos();
    ImGui::GetWindowDrawList()->AddLine({p.x+6,p.y+18},{p.x+18,p.y+6},pink,2);
    if(ImGui::IsItemHovered()) {ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);ImGui::SetTooltip("Arraste para mudar o tamanho");}
    ImGui::End();ImGui::PopStyleVar();
}
