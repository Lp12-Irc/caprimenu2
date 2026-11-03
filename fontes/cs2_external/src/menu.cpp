// language: C++20, file: menu.cpp, target: Windows 11 x64, ImGui
#include "menu.hpp"
#include "profile_store.hpp"
#include "version.hpp"
#include "game_viewport.hpp"
#include "brand.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <array>
namespace fs = std::filesystem;

static const char* BONE_NAMES[] = {"Cabeca","Pescoco","Peito"};
static const char* FILTER_NAMES[] = {"Inimigos","Aliados","Todos"};
static const char* BOX_STYLES[]   = {"Contorno","Cantos","Preenchida"};
static const char* TARGET_MODES[] = {"Menor angulo","Menor distancia"};

static std::string read_key_name(int key) {
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
struct KeyCatalog {HKL layout=nullptr;std::array<std::string,255> names;std::vector<int> keys;};
static KeyCatalog& key_catalog() {
    static KeyCatalog catalog;
    const auto layout=GetKeyboardLayout(0);
    if(catalog.layout!=layout || catalog.keys.empty()) {
        catalog.layout=layout;catalog.keys.clear();
        for(int i=0;i<255;++i){catalog.names[i]=read_key_name(i);if(i>0 && (i<=VK_XBUTTON2 || MapVirtualKeyW(i,MAPVK_VK_TO_VSC)))catalog.keys.push_back(i);}
    }
    return catalog;
}
static const std::string& key_name(int key) {
    if(key>=0 && key<255)return key_catalog().names[key];
    static const std::string invalid="Tecla invalida";return invalid;
}
static void key_control(const char* label, int& key) {
    if (!ImGui::BeginCombo(label, key_name(key).c_str())) return;
    const auto& catalog=key_catalog();
    ImGuiListClipper clipper;clipper.Begin(static_cast<int>(catalog.keys.size()));
    const auto selected=std::find(catalog.keys.begin(),catalog.keys.end(),key);
    if(selected!=catalog.keys.end())clipper.IncludeItemByIndex(static_cast<int>(selected-catalog.keys.begin()));
    while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
        const int candidate=catalog.keys[i];
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
    st.WindowPadding = {16, 14}; st.FramePadding = {10, 7};
    st.ItemSpacing = {12, 9}; st.ItemInnerSpacing = {6, 4};
    st.WindowRounding = 6; st.ChildRounding = 8; st.FrameRounding = 5;
    st.PopupRounding = 8; st.ScrollbarRounding = 8; st.GrabRounding = 5;
    st.WindowBorderSize = 1; st.ChildBorderSize = 0; st.FrameBorderSize = 0;
    auto* c = st.Colors;
    c[ImGuiCol_WindowBg] = {0.035f, 0.043f, 0.055f, 1};
    c[ImGuiCol_ChildBg] = {0.048f, 0.055f, 0.068f, 1};
    c[ImGuiCol_PopupBg] = {0.075f, 0.09f, 0.12f, 1};
    c[ImGuiCol_Border] = {0.19f, 0.21f, 0.26f, 1};
    c[ImGuiCol_Text] = {0.91f, 0.94f, 0.97f, 1};
    c[ImGuiCol_TextDisabled] = {0.47f, 0.54f, 0.63f, 1};
    c[ImGuiCol_FrameBg] = {0.085f, 0.085f, 0.105f, 1};
    c[ImGuiCol_FrameBgHovered] = {0.16f, 0.21f, 0.27f, 1};
    c[ImGuiCol_FrameBgActive] = {0.24f, 0.08f, 0.15f, 1};
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
ImU32 pink = IM_COL32(255,0,96,255);
ImU32 panel_surface=IM_COL32(16,20,26,255);
ImU32 accent_alpha(int alpha){return (pink&0x00ffffff)|IM_COL32(0,0,0,alpha);}
bool ui_animated=false;
float widget_phase(ImGuiID id,float target) {
    auto* storage=ImGui::GetStateStorage();
    const float next=ui_animated?ui_motion::approach(storage->GetFloat(id,target),target,ImGui::GetIO().DeltaTime):target;
    storage->SetFloat(id,next);return next;
}
ImU32 ui_color(ImU32 color) {auto c=ImGui::ColorConvertU32ToFloat4(color);c.w*=ImGui::GetStyle().Alpha;return ImGui::ColorConvertFloat4ToU32(c);}
bool animated_toggle(const char* label,bool* value) {
    ImGui::PushID(label);
    const auto p=ImGui::GetCursorScreenPos();const float w=ImGui::GetContentRegionAvail().x;
    const bool clicked=ImGui::InvisibleButton("toggle",{w,34});
    if(clicked)*value=!*value;
    const float t=widget_phase(ImGui::GetID("phase"),*value?1.f:0.f);
    auto* d=ImGui::GetWindowDrawList();
    const char* end=ImGui::FindRenderedTextEnd(label);
    d->AddText({p.x,p.y+7},ImGui::GetColorU32(ImGuiCol_Text),label,end);
    const auto col=ImLerp(ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered),ImGui::ColorConvertU32ToFloat4(pink),t);
    d->AddRectFilled({p.x+w-44,p.y+5},{p.x+w,p.y+29},ImGui::GetColorU32(col),12);
    d->AddCircleFilled({p.x+w-32+20*t,p.y+17},8.5f,ImGui::GetColorU32(ImVec4(.96f,.96f,.99f,1)));
    if(ImGui::IsItemFocused())d->AddRect(p,{p.x+w,p.y+34},ui_color(pink),4);
    ImGui::PopID();return clicked;
}
void toggle(const char* label,bool& value) {animated_toggle(label,&value);ImGui::Separator();}
bool animated_slider(const char* label,float* value,float low,float high,const char* format="%.3f",ImGuiSliderFlags flags=0) {
    // Keep ImGui's keyboard, navigation, temporary numeric entry and clamping.
    const float frame_width=ImGui::CalcItemWidth();
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(0,0,0,0));
    const bool changed=ImGui::SliderFloat(label,value,low,high,format,flags|ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopStyleColor(2);
    const auto* window=ImGui::GetCurrentWindow();
    const auto& item=GImGui->LastItemData;
    if(!window->SkipItems && high>low && !(GImGui->TempInputId==item.ID)) {
        const auto p=item.Rect.Min;const float width=frame_width;
        // Label is outside the frame. The current slider frame width is CalcItemWidth.
        const float h=ImGui::GetFrameHeight(),t=std::clamp((*value-low)/(high-low),0.f,1.f);
        const float inset=9.f,x=p.x+inset+(width-2*inset)*t,y=p.y+h-4;
        auto* d=ImGui::GetWindowDrawList();
        d->AddLine({p.x+inset,y},{p.x+width-inset,y},ui_color(IM_COL32(64,65,79,255)),2);
        d->AddLine({p.x+inset,y},{x,y},ui_color(pink),2);
        if(ImGui::IsItemHovered() || ImGui::IsItemActive())d->AddCircleFilled({x,y},7,ui_color(accent_alpha(28)));
        d->AddCircleFilled({x,y},3,ui_color(IM_COL32(250,225,239,255)));
        if(ImGui::IsItemActive() && !ImGui::IsItemActivated()) {ImGui::BeginTooltip();ImGui::Text(format,*value);ImGui::EndTooltip();}
    }
    return changed;
}
bool animated_button(const char* label,const ImVec2& size=ImVec2(0,0)) {
    const bool clicked=ImGui::Button(label,size);
    const float t=widget_phase(ImGui::GetItemID()^0x64a2,ImGui::IsItemHovered()?1.f:0.f);
    if(t>.01f){const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();ImGui::GetWindowDrawList()->AddRect({a.x-1-t,a.y-1-t},{b.x+1+t,b.y+1+t},ui_color(accent_alpha(int(100*t))),4,0,1);}
    return clicked;
}
void slider_row(const char* label,float* value,float low,float high,const char* format) {
    ImGui::PushID(label);float w=ImGui::GetContentRegionAvail().x;
    ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(label);
    ImGui::SameLine(w*.46f);ImGui::SetNextItemWidth(w*.53f);
    animated_slider("##value",value,low,high,format);ImGui::PopID();
}
void card(const char* id,const char* title,float height=0) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{16,12});ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,8);ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize,1);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImGui::ColorConvertU32ToFloat4(panel_surface));
    ImGui::BeginChild(id,{0,0},ImGuiChildFlags_Borders|ImGuiChildFlags_AlwaysUseWindowPadding|(height>0?ImGuiChildFlags_AutoResizeY:ImGuiChildFlags_None));ImGui::PopStyleColor();ImGui::PopStyleVar(3);
    ImGui::SetWindowFontScale(1.08f);ImGui::TextUnformatted(title);ImGui::SetWindowFontScale(1);ImGui::Separator(); ImGui::Spacing();
    ImGui::PushItemWidth(-150);
}
void endcard(){ImGui::PopItemWidth();ImGui::EndChild();}
}
void Menu::apply_theme(const Config& cfg) {
    auto opaque=[](ImU32 color){auto result=ImGui::ColorConvertU32ToFloat4(color);result.w=1;return result;};
    const auto accent=opaque(cfg.menu_accent),background=opaque(cfg.menu_background),surface=opaque(cfg.menu_surface),text=opaque(cfg.menu_text),border=opaque(cfg.menu_border);
    pink=cfg.menu_accent|IM_COL32(0,0,0,255);panel_surface=cfg.menu_surface|IM_COL32(0,0,0,255);auto* c=ImGui::GetStyle().Colors;
    c[ImGuiCol_WindowBg]=background;c[ImGuiCol_ChildBg]=surface;c[ImGuiCol_PopupBg]=surface;
    c[ImGuiCol_Text]=text;c[ImGuiCol_TextDisabled]=ImLerp(background,text,.6f);
    c[ImGuiCol_Border]=c[ImGuiCol_Separator]=border;
    c[ImGuiCol_FrameBg]=ImLerp(background,surface,.5f);
    c[ImGuiCol_FrameBgHovered]=ImLerp(surface,text,.12f);c[ImGuiCol_FrameBgActive]=ImLerp(surface,accent,.2f);
    c[ImGuiCol_Button]=ImLerp(surface,accent,.16f);c[ImGuiCol_ButtonHovered]=ImLerp(surface,accent,.4f);c[ImGuiCol_ButtonActive]=ImLerp(surface,accent,.65f);
    c[ImGuiCol_CheckMark]=c[ImGuiCol_SliderGrab]=accent;c[ImGuiCol_SliderGrabActive]=ImLerp(accent,text,.3f);
    c[ImGuiCol_Header]=ImLerp(surface,accent,.2f);c[ImGuiCol_HeaderHovered]=ImLerp(surface,accent,.3f);c[ImGuiCol_HeaderActive]=ImLerp(surface,accent,.4f);
}
void Menu::motion_tick(const Config& cfg,bool focused) {
    static BOOL allowed=TRUE;static ULONGLONG checked=0;const auto now=GetTickCount64();
    if(!checked || now-checked>=1000){SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&allowed,0);checked=now;}
    motion_enabled_=cfg.menu_open && focused && cfg.menu_motion && allowed;
    particles_.advance(GetTickCount64(),motion_enabled_);
}
void Menu::render(Config& cfg, const AuthInfo& auth_info, bool& running) {
    apply_theme(cfg);
    auto& io=ImGui::GetIO();ui_animated=motion_enabled_;
    const ImVec2 presets[]={{980,640},{1120,700},{860,580}};
    const auto requested=presets[std::clamp(cfg.menu_size,0,2)];
    const ImVec2 size{(std::min)(requested.x,io.DisplaySize.x-24),(std::min)(requested.y,io.DisplaySize.y-24)};
    const bool compact=size.x<850;
    ImGui::SetNextWindowSize(size,ImGuiCond_Always);
    ImGui::SetNextWindowPos({io.DisplaySize.x*.5f,io.DisplaySize.y*.5f},ImGuiCond_FirstUseEver,{.5f,.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    const bool visible=ImGui::Begin("Capri",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();if(!visible){ImGui::End();return;}
    const auto origin=ImGui::GetWindowPos();auto* draw=ImGui::GetWindowDrawList();
    const float sidebar=compact?154.f:184.f,footer=88.f;
    draw->AddRectFilled({origin.x,origin.y},{origin.x+sidebar,origin.y+size.y-footer},panel_surface,8);
    draw->AddLine({origin.x+sidebar,origin.y},{origin.x+sidebar,origin.y+size.y-footer},ImGui::GetColorU32(ImGuiCol_Border));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{16,20});
    ImGui::BeginChild("sidebar",{sidebar,size.y-footer},false,ImGuiWindowFlags_AlwaysUseWindowPadding|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    Brand::avatar(36);ImGui::SameLine();ImGui::BeginGroup();
    ImGui::TextUnformatted("CAPRI");ImGui::TextDisabled("%s",compact?"CS2":"Counter-Strike 2");ImGui::EndGroup();
    ImGui::Dummy({0,12});ImGui::TextDisabled("AJUSTES DA PARTIDA");ImGui::Dummy({0,4});
    const char* labels[]={"Aimbot","Triggerbot","Visuais","Radar","Partida","Configuracoes","Conta"};
    const char* hints[]={"Mira e ativacao","Precisao e tempo","Corpo, cores e tela","Mapa e posicao","Utilitarios","Temas e perfis","Licenca e versao"};
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{8,4});
    for(int i=0;i<7;++i){
        ImGui::PushID(i);const auto at=ImGui::GetCursorScreenPos();
        if(ImGui::Selectable(labels[i],active_tab_==i,0,{0,34}) && active_tab_!=i){active_tab_=i;tab_alpha_=motion_enabled_?0.f:1.f;}
        if(active_tab_==i){auto* list=ImGui::GetWindowDrawList();list->AddRectFilled({at.x-8,at.y+7},{at.x-5,at.y+29},pink,2);}
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",hints[i]);ImGui::PopID();
    }
    ImGui::PopStyleVar();
    if(!compact){ImGui::Dummy({0,8});ImGui::Separator();ImGui::TextDisabled("PERFIL EM USO");ImGui::TextWrapped("%s",profile_name_);}
    ImGui::EndChild();ImGui::PopStyleVar();ImGui::SameLine(0,0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{20,18});
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(0,0,0,0));
    ImGui::BeginChild("workspace",{0,size.y-footer},false,ImGuiWindowFlags_AlwaysUseWindowPadding);
    ImGui::TextDisabled("CAPRI  /  %s",hints[active_tab_]);
    ImGui::SetWindowFontScale(1.55f);ImGui::TextUnformatted(labels[active_tab_]);ImGui::SetWindowFontScale(1);
    ImGui::SameLine(ImGui::GetWindowWidth()-130);
    if(ImGui::SmallButton("Ocultar"))cfg.menu_open=false;
    ImGui::SameLine();if(ImGui::SmallButton("X"))running=false;
    ImGui::TextDisabled("Ajuste os controles e salve no seu perfil.");ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
    tab_alpha_=motion_enabled_?ui_motion::approach(tab_alpha_,1.f,io.DeltaTime):1.f;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*tab_alpha_);
    switch(active_tab_){
    case 0:tab_aimbot(cfg);break;
    case 1:tab_trigger(cfg);break;
    case 2:if(ImGui::CollapsingHeader("Tela e proporcao")){card("display_settings","Exibicao",134.f);tab_display(cfg);endcard();}ImGui::Spacing();card("visuals","Jogadores e estilo");tab_esp(cfg);endcard();break;
    case 3:tab_radar(cfg);break;
    case 4:card("utilities","Informacoes da partida");tab_misc(cfg);endcard();break;
    case 5:tab_config(cfg);break;
    case 6:tab_info(cfg,auth_info);break;
    }
    ImGui::PopStyleVar();ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopStyleVar();
    ImGui::SetCursorPos({0,size.y-footer});ImGui::Separator();
    ImGui::SetCursorPos({18,size.y-footer+12});ImGui::Text("Perfil: %s",profile_name_);
    ImGui::SetCursorPos({18,size.y-footer+34});ImGui::TextDisabled("v%s  /  %s abre o painel",CAPRI_CLIENT_VERSION,key_name(cfg.menu_key).c_str());
    ImGui::SetCursorPos({size.x-(compact?252.f:304.f),size.y-footer+12});
    if(animated_button("Restaurar",{compact?94.f:112.f,34}))ImGui::OpenPopup("Restaurar ajustes?");
    ImGui::SameLine();ImGui::PushStyleColor(ImGuiCol_Button,ImLerp(ImGui::GetStyleColorVec4(ImGuiCol_Button),ImGui::ColorConvertU32ToFloat4(pink),.5f));
    if(animated_button("Salvar perfil",{compact?124.f:154.f,34}))save_profile(cfg,profile_name_);
    ImGui::PopStyleColor();
    if(ImGui::BeginPopupModal("Restaurar ajustes?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted("Restaurar os ajustes do painel para o padrao?");
        if(animated_button("Restaurar")){Config defaults;auto key=std::string(cfg.saved_key);defaults.menu_key=cfg.menu_key;defaults.menu_open=true;defaults.game_status=cfg.game_status;defaults.game_ready=cfg.game_ready;defaults.write_available=cfg.write_available;strcpy_s(defaults.saved_key,key.c_str());cfg=defaults;ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(animated_button("Cancelar"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    ImGui::SetCursorPos({18,size.y-25});
    ImGui::PushTextWrapPos(size.x-18);ImGui::TextDisabled("%s",status_.empty()?"Alteracoes so ficam gravadas depois de Salvar perfil.":status_.c_str());ImGui::PopTextWrapPos();
    ImGui::End();
}

void Menu::tab_display(Config& cfg) {
    static const char* aspects[]={"Automatico (janela)","16:9","16:10","4:3"};
    static const char* formats[]={"Esticado / tela inteira","Barras pretas"};
    const bool wide=ImGui::GetContentRegionAvail().x>620;
    if(wide)ImGui::Columns(2,"display_columns",false);
    ImGui::SetNextItemWidth(-100);ImGui::Combo("Proporcao",&cfg.display_aspect,aspects,4);
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Escolha a proporcao do CS2. Automatico usa a janela detectada.");
    if(wide)ImGui::NextColumn();
    ImGui::SetNextItemWidth(-100);ImGui::Combo("Exibicao",&cfg.display_fit,formats,2);
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Escolha igual ao jogo: esticado ocupa a janela inteira; barras pretas centralizam a imagem.");
    if(wide)ImGui::Columns(1);
    const auto size=ImGui::GetIO().DisplaySize;
    const auto area=game_viewport(cfg,int(size.x),int(size.y));
    ImGui::TextDisabled("Janela: %.0f x %.0f | Area configurada: %.0f x %.0f",size.x,size.y,area.width,area.height);

}

void Menu::tab_esp(Config& cfg) {
    animated_toggle("Ativar ESP", &cfg.esp);
    ImGui::Separator();
    if (!cfg.esp) ImGui::TextDisabled("Desativado. Voce pode preparar o estilo antes de ligar.");

    const bool wide=ImGui::GetContentRegionAvail().x>600;
    if(wide)ImGui::Columns(2,"esp_settings",false);
    ImGui::TextDisabled("ELEMENTOS");
    animated_toggle("Caixa do jogador", &cfg.esp_box);
    if (cfg.esp_box && ImGui::TreeNode("Detalhes da caixa")) {
        ImGui::Combo("Estilo##box", &cfg.esp_box_style, BOX_STYLES, 3);
        animated_slider("Espessura##box", &cfg.esp_box_thick, 0.5f, 4.f);
        ImGui::TreePop();
    }
    animated_toggle("Corpo / esqueleto", &cfg.esp_body);
    static const char* wall_styles[]{"Corpo neon", "Esqueleto"};
    ImGui::Combo("Estilo do wall", &cfg.esp_player_style, wall_styles, 2);
    if(cfg.esp_player_style==1)
        animated_slider("Espessura##sk", &cfg.esp_skel_thick, 0.5f, 4.f);
    animated_toggle("Linhas de referencia",   &cfg.esp_snaplines);
    animated_toggle("Distancia",     &cfg.esp_distance);
    animated_toggle("Barra de vida",   &cfg.esp_healthbar);
    if (cfg.esp_healthbar)
        animated_toggle("Vida numerica", &cfg.esp_health_num);
    animated_toggle("Arma equipada",  &cfg.esp_weapon);
    animated_toggle("Cor de jogador avistado", &cfg.esp_vischeck);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Usa o estado de avistamento do jogo; nao representa um teste de paredes.");

    if(wide)ImGui::NextColumn();else ImGui::Spacing();
    ImGui::TextDisabled("FILTROS E CORES");
    ImGui::Combo("Jogadores##esp", &cfg.esp_filter, FILTER_NAMES, 3);
    animated_slider("Distancia maxima (m)", &cfg.esp_max_dist, 10.f, 1000.f);
    ImGui::Separator();
    color_edit("Cor dos CTs",      cfg.col_ct);
    color_edit("Cor dos TRs",      cfg.col_tr);
    color_edit("Jogador avistado", cfg.col_visible);
    if(cfg.esp_player_style==1)color_edit("Cor do esqueleto",cfg.col_skeleton);
    else {
        float body_rgb[3]{float(cfg.col_body&255)/255.f,float((cfg.col_body>>8)&255)/255.f,float((cfg.col_body>>16)&255)/255.f};
        if(ImGui::ColorEdit3("Cor neon do corpo",body_rgb,ImGuiColorEditFlags_NoInputs))
            cfg.col_body=IM_COL32(uint8_t(body_rgb[0]*255),uint8_t(body_rgb[1]*255),uint8_t(body_rgb[2]*255),255);
    }
    color_edit("Cor das linhas",cfg.col_snapline);

    if(wide)ImGui::Columns(1);
}

void Menu::tab_aimbot(Config& cfg) {
    bool wide=ImGui::GetContentRegionAvail().x>650;
    if(wide) ImGui::Columns(2,"aim-layout",false);
    card("general","Configuracao geral",316);
    toggle("Ativar aimbot",cfg.aim);
    {const float w=ImGui::GetContentRegionAvail().x;ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("Tecla de ativacao");ImGui::SameLine(w*.46f);ImGui::SetNextItemWidth(-1);key_control("##aim_activation",cfg.aim_key);}
    ImGui::TextDisabled("Modo de ativacao: segurar");
    ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Campo de visao");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x*.46f);
    ImGui::SetNextItemWidth(-1);
    ImGui::Text("%.1f graus",cfg.aim_fov);
    ImGui::SameLine();
    static char fov_draft[32]{};
    static bool fov_error=false;
    if(animated_button("Editar##fov")) {
        sprintf_s(fov_draft,"%.1f",cfg.aim_fov);fov_error=false;
        ImGui::OpenPopup("Editar campo de visao");
    }
    if(ImGui::BeginPopupModal("Editar campo de visao",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Valor atual: %.1f graus",cfg.aim_fov);
        ImGui::TextUnformatted("Novo valor de 1 a 45 graus (use ponto decimal):");
        ImGui::SetNextItemWidth(240);
        ImGui::InputText("##fov_draft",fov_draft,sizeof(fov_draft),ImGuiInputTextFlags_CharsDecimal);
        if(fov_error) ImGui::TextUnformatted("Digite um numero valido entre 1 e 45.");
        if(animated_button("Aplicar",{110,32})) {
            char* end=nullptr;float value=std::strtof(fov_draft,&end);
            if(end!=fov_draft && *end=='\0' && std::isfinite(value) && value>=1 && value<=45) {
                cfg.aim_fov=std::round(value*10.f)/10.f;
                ImGui::CloseCurrentPopup();
            } else fov_error=true;
        }
        ImGui::SameLine();if(animated_button("Cancelar",{110,32})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    slider_row("Suavizacao",&cfg.aim_smooth,1,15,"%.1f");
    auto combo_row=[](const char* label,int& value,const char* const* names,int count){ImGui::PushID(label);const float w=ImGui::GetContentRegionAvail().x;ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(label);ImGui::SameLine(w*.46f);ImGui::SetNextItemWidth(-1);ImGui::Combo("##value",&value,names,count);ImGui::PopID();};
    combo_row("Area da mira",cfg.aim_bone,BONE_NAMES,3);
    combo_row("Prioridade",cfg.aim_target,TARGET_MODES,2);
    endcard();
    card("preview","Previa da mira",142);
    auto p=ImGui::GetCursorScreenPos();auto area=ImGui::GetContentRegionAvail();area.y=92.f;
    ImVec2 center{p.x+area.x*.28f,p.y+area.y*.5f};
    auto* d=ImGui::GetWindowDrawList();float radius=(std::min)(area.y*.42f,25+cfg.aim_fov*1.5f);
    d->AddCircle(center,radius,pink,96,1.5f);
    d->AddLine({center.x-12,center.y},{center.x+12,center.y},IM_COL32_WHITE,2);
    d->AddLine({center.x,center.y-12},{center.x,center.y+12},IM_COL32_WHITE,2);
    d->AddLine({p.x+area.x*.54f,p.y},{p.x+area.x*.54f,p.y+area.y},ImGui::GetColorU32(ImGuiCol_Border));
    char summary[96]{};sprintf_s(summary,"FOV  %.1f graus\nSuavizacao  %.1f\n%s",cfg.aim_fov,cfg.aim_smooth,BONE_NAMES[std::clamp(cfg.aim_bone,0,2)]);
    d->AddText({p.x+area.x*.59f,p.y+4},ImGui::GetColorU32(ImGuiCol_Text),summary);
    ImGui::Dummy(area);endcard();
    if(wide)ImGui::NextColumn();
    card("activation","Filtros e ativacao",cfg.aim_auto_sens?330.f:364.f);
    toggle("Verificar obstaculos do mapa",cfg.aim_visibility);
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Geometria estatica. Sem varacao automatica; portas, objetos moveis e fumaca nao sao simulados.");
    toggle("Exigir mouse direito",cfg.aim_ads_only);
    toggle("Exibir circulo de FOV",cfg.aim_fov_circle);
    slider_row("Atraso (ms)",&cfg.aim_delay_ms,0,200,"%.0f");
    toggle("Usar sensibilidade do CS2",cfg.aim_auto_sens);
    if(!cfg.aim_auto_sens)slider_row("Calibracao",&cfg.aim_sens,.1f,5.f,"%.2f");
    slider_row("Distancia (m)",&cfg.aim_max_dist,5,500,"%.0f");
    endcard();
    card("rcs","Controle de recuo",142);
    toggle("Ativar RCS",cfg.rcs);
    slider_row("Compensacao",&cfg.rcs_intensity,.1f,2.f,"%.2f");endcard();
    if(wide)ImGui::Columns(1);
}

void Menu::tab_misc(Config& cfg) {
    ImGui::Columns(2, nullptr, false);

    animated_toggle("No Flash", &cfg.no_flash);
    if (cfg.no_flash) {
        ImGui::TextWrapped("%s", cfg.write_available ? "Escrita conectada." : "Escrita indisponivel; confira a permissao do processo.");
        animated_slider("Max Alpha", &cfg.no_flash_max, 0.f, 50.f);
    }
    ImGui::Separator();
    animated_toggle("Custom Crosshair", &cfg.crosshair);
    if (cfg.crosshair) {
        animated_slider("Size##ch",  &cfg.ch_size,  1.f, 20.f);
        animated_slider("Gap##ch",   &cfg.ch_gap,   0.f, 10.f);
        animated_slider("Thick##ch", &cfg.ch_thick, 0.5f, 4.f);
        animated_toggle("Dot", &cfg.ch_dot);
        color_edit("CH Color", cfg.ch_col);
    }

    ImGui::NextColumn();

    animated_toggle("Espectadores", &cfg.spectator_list);
    animated_toggle("Tempo da bomba", &cfg.bomb_timer);
    animated_toggle("Item ESP", &cfg.item_esp);
    ImGui::Columns(1);
}

void Menu::save_profile(Config& cfg,const std::string& name) {
    std::error_code ec;fs::create_directories("configs",ec);
    if(ec){status_="Falha ao criar a pasta de perfis.";return;}
    std::string error;const bool saved=profile_store::save(cfg,name,&error);
    status_=saved?"Perfil salvo. Sera restaurado ao abrir o Capri.":"Falha ao salvar perfil: "+error;
    if(saved)profiles_.invalidate();
}
void Menu::tab_config(Config& cfg) {
    card("appearance","Aparencia do painel",270);
    static const char* sizes[]={"Compacto (980 x 640)","Padrao (1120 x 700)","Pequeno (860 x 580)"};
    ImGui::SetNextItemWidth(230);ImGui::Combo("Tamanho",&cfg.menu_size,sizes,3);
    auto color=[&](const char* label,ImU32& value){auto c=ImGui::ColorConvertU32ToFloat4(value);float rgb[]={c.x,c.y,c.z};
        if(ImGui::ColorEdit3(label,rgb,ImGuiColorEditFlags_NoInputs))value=ImGui::ColorConvertFloat4ToU32({rgb[0],rgb[1],rgb[2],1});};
    ImGui::Columns(2,"theme_colors",false);
    color("Destaque",cfg.menu_accent);color("Fundo",cfg.menu_background);color("Cartoes",cfg.menu_surface);
    ImGui::NextColumn();color("Texto",cfg.menu_text);color("Bordas",cfg.menu_border);
    ImGui::Columns(1);
    if(animated_button("Restaurar tema rosa")){Config defaults;cfg.menu_accent=defaults.menu_accent;cfg.menu_background=defaults.menu_background;cfg.menu_surface=defaults.menu_surface;cfg.menu_text=defaults.menu_text;cfg.menu_border=defaults.menu_border;}
    ImGui::TextWrapped("As cores mudam na hora. Use Salvar perfil para manter seu tema.");endcard();
    key_control("Abrir / ocultar menu", cfg.menu_key);
    ImGui::TextWrapped("Salve seus ajustes e alterne entre perfis. A sessao de acesso atual e preservada.");
    ImGui::Spacing();
    ImGui::InputText("Nome do perfil", profile_name_, sizeof(profile_name_));
    std::string name = profile_name_;
    const bool valid=profile_store::valid_name(name);
    ImGui::BeginDisabled(!valid);
    auto save = [&] {save_profile(cfg,name);};
    if (animated_button("Salvar perfil", {145, 36}))save();
    ImGui::SameLine();
    if (animated_button("Carregar", {120, 36})) {
        std::string error;
        const bool loaded=profile_store::load(cfg,name,&error);
        status_=loaded?"Perfil carregado. Sera restaurado ao abrir o Capri.":error;
    }
    ImGui::EndDisabled();
    if (!valid) ImGui::TextWrapped("Use letras, numeros, espacos, traco ou sublinhado.");
    if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::TextDisabled("PERFIS DISPONIVEIS");
    ImGui::SameLine();if(ImGui::SmallButton("Atualizar lista"))profiles_.invalidate();
    profiles_.refresh(GetTickCount64());
    if(!profiles_.error().empty())ImGui::TextDisabled("%s",profiles_.error().c_str());
    else if(profiles_.names().empty())ImGui::TextDisabled("Nenhum perfil disponivel.");
    for(const auto& stem:profiles_.names())if(ImGui::Selectable(stem.c_str(),stem==name,0,{0,32})) {
        std::string error;
        if(profile_store::load(cfg,stem,&error)){strncpy_s(profile_name_,stem.c_str(),_TRUNCATE);status_="Perfil carregado. Sera restaurado ao abrir o Capri.";}
        else status_=error;
    }
}

void Menu::tab_info(Config& cfg, const AuthInfo& ai) {
    ImGui::Text("Versao: %s / build %d",CAPRI_CLIENT_VERSION,CAPRI_CLIENT_BUILD);
    ImGui::Text("Validade: %s", ai.expires_at.empty() ? "Nao informada" : ai.expires_at=="never" ? "Sem vencimento" : ai.expires_at.c_str());

    const char* status_str = "Nao verificado";
    ImVec4      status_col = {1,1,0,1};
    switch (ai.result) {
        case AuthResult::OK:           status_str = "Ativo";        status_col = {.4f,1,.6f,1}; break;
        case AuthResult::EXPIRED:      status_str = "Expirado";       status_col = {1,.5f,0,1}; break;
        case AuthResult::BAD_KEY:      status_str = "Chave invalida";   status_col = {1,0,0,1}; break;
        case AuthResult::HWID_MISMATCH:status_str = "Vinculo incompativel"; status_col = {1,0,0,1}; break;
        case AuthResult::NET_ERROR:    status_str = "Sem conexao com o servidor";     status_col = {.7f,.7f,0,1}; break;
        case AuthResult::UPDATE_REQUIRED:status_str = "Atualizacao obrigatoria";status_col={1,.6f,.2f,1};break;
    }
    ImGui::TextColored(status_col, "Status:    %s", status_str);
    ImGui::Separator();
    toggle("Animacoes do painel",cfg.menu_motion);
    ImGui::TextDisabled("Desligue para manter o menu em modo economico.");
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

void Menu::tab_radar(Config& cfg) {
    card("radar_settings","Radar - mover e redimensionar");
    toggle("Exibir radar",cfg.radar);
    ImGui::TextWrapped("Ajuste pelos controles abaixo. As mudancas aparecem imediatamente. Salve o perfil para manter a posicao e o tamanho.");
    ImGui::Spacing();
    ImGui::TextUnformatted("Tamanho do radar");ImGui::SetNextItemWidth(-1);
    animated_slider("##radar_size",&cfg.radar_sz,80.f,600.f,"%.0f px",ImGuiSliderFlags_AlwaysClamp);
    if(animated_button("Diminuir",{130,34}))cfg.radar_sz=(std::max)(80.f,cfg.radar_sz-20.f);
    ImGui::SameLine();if(animated_button("Aumentar",{130,34}))cfg.radar_sz=(std::min)(600.f,cfg.radar_sz+20.f);
    ImGui::SameLine();if(animated_button("Pequeno (120 px)",{170,34}))cfg.radar_sz=120.f;
    if(animated_button("Sobre o minimapa",{180,32})) {cfg.radar_x=1.f;cfg.radar_y=6.f;cfg.radar_sz=160.f;cfg.radar_names=false;}
    ImGui::TextDisabled("Ajuste X e Y conforme a escala do HUD do CS2.");
    ImGui::Spacing();ImGui::TextUnformatted("Posicao horizontal (esquerda / direita)");ImGui::SetNextItemWidth(-1);
    animated_slider("##radar_x",&cfg.radar_x,0.f,100.f,"%.1f %%",ImGuiSliderFlags_AlwaysClamp);
    ImGui::TextUnformatted("Posicao vertical (cima / baixo)");ImGui::SetNextItemWidth(-1);
    animated_slider("##radar_y",&cfg.radar_y,0.f,100.f,"%.1f %%",ImGuiSliderFlags_AlwaysClamp);
    if(animated_button("Esquerda",{105,32}))cfg.radar_x=(std::max)(0.f,cfg.radar_x-2.f);
    ImGui::SameLine();if(animated_button("Direita",{105,32}))cfg.radar_x=(std::min)(100.f,cfg.radar_x+2.f);
    ImGui::SameLine();if(animated_button("Cima",{105,32}))cfg.radar_y=(std::max)(0.f,cfg.radar_y-2.f);
    ImGui::SameLine();if(animated_button("Baixo",{105,32}))cfg.radar_y=(std::min)(100.f,cfg.radar_y+2.f);
    ImGui::Spacing();ImGui::Separator();
    ImGui::TextUnformatted("Zoom do mapa");ImGui::SetNextItemWidth(-1);
    animated_slider("##radar_zoom",&cfg.radar_zoom,1.f,4.f,"%.1fx",ImGuiSliderFlags_AlwaysClamp);
    animated_toggle("Mostrar nomes no radar",&cfg.radar_names);
    if(!cfg.radar) ImGui::TextDisabled("Radar oculto: ative Exibir radar para visualizar os ajustes.");
    endcard();
}

void Menu::render_access_block(const AuthInfo& access,int sw,int sh,bool& running,std::string* requested_key) {
    const bool update=access.result==AuthResult::UPDATE_REQUIRED;
    ImGui::SetNextWindowPos({sw*.5f,sh*.5f},ImGuiCond_Always,{.5f,.5f});
    ImGui::SetNextWindowSize({(std::min)(560.f,sw-24.f),(std::min)(460.f,sh-24.f)},ImGuiCond_Always);
    ImGui::Begin("Capri / Acesso bloqueado",nullptr,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoTitleBar);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(pink),"%s",update?"ATUALIZACAO OBRIGATORIA":"ACESSO INTERROMPIDO");
    ImGui::Separator();ImGui::Text("Instalado: %s (build %d)",CAPRI_CLIENT_VERSION,CAPRI_CLIENT_BUILD);
    if(update)ImGui::Text("Disponivel: %s (build %d)",access.version.c_str(),access.latest_build);
    ImGui::TextWrapped("%s",access.message.c_str());
    if(update){ImGui::BeginChild("release_notes",{0,105},true);ImGui::TextWrapped("%s",access.release_notes.c_str());ImGui::EndChild();}
    else {
        ImGui::Spacing();ImGui::TextUnformatted("Chave de acesso");ImGui::SetNextItemWidth(-1);
        const bool submit=ImGui::InputText("##new_access_key",access_key_,sizeof(access_key_),ImGuiInputTextFlags_Password|ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::BeginDisabled(!access_key_[0] || !requested_key);
        if((animated_button("Validar chave",{180,34}) || submit) && requested_key && access_key_[0])*requested_key=access_key_;
        ImGui::EndDisabled();ImGui::Spacing();
    }
    if(update && !access.download_url.empty() && animated_button(access.package_sha256.empty()?"Baixar pacote":"Atualizar agora",{200,36})){
        if(access.package_sha256.empty())open_release_url(access.download_url);
        else if(launch_client_update(access))running=false;
        else ImGui::OpenPopup("Atualizador indisponivel");
    }
    if(ImGui::BeginPopupModal("Atualizador indisponivel",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted("Reinstale o pacote completo do Capri para recuperar o atualizador.");
        if(animated_button("Fechar"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    ImGui::TextWrapped("%s",update?"Clique em Atualizar agora. O Capri aplica as modificacoes e reabre, mantendo sua chave e suas configuracoes.":"Confira sua conexao e o servico de chaves. Os recursos ficam bloqueados enquanto o acesso estiver indisponivel.");
    ImGui::TextWrapped("Verificacao automatica a cada 2 segundos. Ao reativar a chave no ADM, o acesso volta mantendo suas configuracoes.");
    if(animated_button("Encerrar",{120,30}))running=false;
    ImGui::End();
}

void Menu::tab_trigger(Config& cfg) {
    const bool columns=ImGui::GetContentRegionAvail().x>=650.f &&
        ImGui::BeginTable("trigger_columns",2,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_NoSavedSettings);
    if(columns){ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);}
    card("trigger_activation","Ativacao",1);
    toggle("Ativar triggerbot",cfg.triggerbot);
    ImGui::TextWrapped("Segure a tecla com o menu fechado. O trigger apenas dispara; voce posiciona a mira.");
    ImGui::BeginDisabled(!cfg.triggerbot);
    key_control("Tecla de ativacao",cfg.tb_key);
    if(cfg.tb_key==cfg.menu_key)ImGui::TextWrapped("Escolha uma tecla diferente da usada para abrir o painel.");
    ImGui::EndDisabled();endcard();ImGui::Spacing();

    card("trigger_region","Regiao de disparo",1);
    ImGui::BeginDisabled(!cfg.triggerbot);
    toggle("Somente cabeca",cfg.tb_head_only);
    ImGui::BeginDisabled(!cfg.tb_head_only);
    float margin=cfg.tb_head_margin*100.f;
    if(animated_slider("Margem na cabeca",&margin,30,100,"%.0f%%"))cfg.tb_head_margin=margin/100.f;
    ImGui::EndDisabled();
    ImGui::TextWrapped(cfg.tb_head_only ? "Margem menor exige mirar mais perto do centro da cabeca. Sem dados atualizados, o disparo fica bloqueado." : "Qualquer regiao do inimigo pode ativar o disparo, inclusive o corpo.");
    ImGui::EndDisabled();endcard();ImGui::Spacing();

    if(columns)ImGui::TableSetColumnIndex(1);
    card("trigger_timing","Tempo e repeticao",1);
    ImGui::BeginDisabled(!cfg.triggerbot);
    slider_row("Atraso para disparar",&cfg.tb_delay_ms,0,300,"%.0f ms");
    slider_row("Duracao do clique",&cfg.tb_release_ms,10,100,"%.0f ms");
    slider_row("Intervalo entre cliques",&cfg.tb_interval_ms,0,500,"%.0f ms");
    ImGui::TextWrapped("0 ms remove a espera adicional. A cadencia da arma continua valendo.");
    if(animated_button("Resposta imediata")) {
        cfg.tb_delay_ms=0;cfg.tb_interval_ms=0;cfg.tb_release_ms=16;
    }
    ImGui::EndDisabled();endcard();ImGui::Spacing();

    card("trigger_status","Estado atual",1);
    ImGui::TextWrapped("%s",cfg.trigger_status.c_str());
    if(animated_button("Restaurar ajustes do trigger")) {
        const Config defaults;
        cfg.tb_key=defaults.tb_key;cfg.tb_head_only=defaults.tb_head_only;cfg.tb_head_margin=defaults.tb_head_margin;
        cfg.tb_delay_ms=defaults.tb_delay_ms;cfg.tb_release_ms=defaults.tb_release_ms;cfg.tb_interval_ms=defaults.tb_interval_ms;
    }
    endcard();
    if(columns)ImGui::EndTable();
}
