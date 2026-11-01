from pathlib import Path
p=Path('fontes/cs2_external/src/menu.cpp'); s=p.read_text(encoding='utf-8')
a=s.index('void Menu::render('); b=s.index('void Menu::tab_esp',a)
s=s[:a]+r'''
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
void card(const char* id,const char* title,float height=0) {
    ImGui::BeginChild(id,{0,height},true);
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
    ImGui::BeginChild("workspace",{0,-66},false);
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
    if(ImGui::Button("Salvar perfil",{160,36})) {
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

''' +s[b:]
a=s.index('void Menu::tab_aimbot'); b=s.index('void Menu::tab_misc',a)
s=s[:a]+r'''void Menu::tab_aimbot(Config& cfg) {
    bool wide=ImGui::GetContentRegionAvail().x>670;
    if(wide) ImGui::Columns(2,"aim-layout",false);
    card("general","Geral",365);
    toggle("Ativar aimbot",cfg.aim);
    key_control("Tecla de ativacao",cfg.aim_key);
    ImGui::TextDisabled("Modo de ativacao: segurar");
    ImGui::SliderFloat("Campo de visao",&cfg.aim_fov,1,45,"%.1f graus");
    ImGui::SliderFloat("Suavizacao",&cfg.aim_smooth,1,15,"%.1f");
    ImGui::Combo("Area da mira",&cfg.aim_bone,BONE_NAMES,3);
    ImGui::Combo("Prioridade",&cfg.aim_target,TARGET_MODES,2);
    endcard();
    card("preview","Previa do campo de visao",180);
    auto p=ImGui::GetCursorScreenPos();auto area=ImGui::GetContentRegionAvail();
    ImVec2 center{p.x+area.x*.5f,p.y+area.y*.5f};
    auto* d=ImGui::GetWindowDrawList();float radius=(std::min)(area.y*.42f,25+cfg.aim_fov*1.5f);
    d->AddCircle(center,radius,pink,96,1.5f);
    d->AddLine({center.x-12,center.y},{center.x+12,center.y},IM_COL32_WHITE,2);
    d->AddLine({center.x,center.y-12},{center.x,center.y+12},IM_COL32_WHITE,2);
    ImGui::Dummy(area);endcard();
    if(wide)ImGui::NextColumn();
    card("activation","Ativacao",220);
    toggle("Exigir mouse direito",cfg.aim_ads_only);
    toggle("Exibir circulo de FOV",cfg.aim_fov_circle);
    ImGui::SliderFloat("Atraso (ms)",&cfg.aim_delay_ms,0,200,"%.0f");
    ImGui::SliderFloat("Sensibilidade",&cfg.aim_sens,.1f,5.f,"%.2f");
    endcard();
    card("rcs","Controle de recuo",155);
    toggle("Ativar RCS",cfg.rcs);
    ImGui::SliderFloat("Compensacao",&cfg.rcs_intensity,.1f,2.f,"%.2f");endcard();
    card("compatibility","Disponibilidade",150);
    ImGui::TextWrapped("%s",cfg.game_status.c_str());
    ImGui::Spacing();ImGui::TextDisabled("Silent Aim indisponivel nesta versao.");endcard();
    if(wide)ImGui::Columns(1);
}

''' + s[b:]
s=s.replace('static const char* TARGET_MODES[] = {"Min FOV","Min Distance"};','static const char* TARGET_MODES[] = {"Menor angulo","Menor distancia"};')
s=s.replace('st.ChildRounding = 10','st.ChildRounding = 7').replace('st.WindowRounding = 14','st.WindowRounding = 9')
p.write_text(s,encoding='utf-8')
p=Path('fontes/cs2_external/src/menu.hpp');s=p.read_text();s=s.replace('int active_tab_ = 0','int active_tab_ = 0');p.write_text(s)
