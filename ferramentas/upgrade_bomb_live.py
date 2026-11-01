from pathlib import Path
root=Path(__file__).parent.parent/'fontes/cs2_external/src'
p=root/'entities.cpp'; s=p.read_text(encoding='utf-8'); start=s.index('void EntityCache::update_bomb()')
s=s[:start]+r'''bool EntityCache::current_bomb_state(BombState& result) const {
    result={};
    if (!mem_ || !c4_list || !globals_addr) return false;
    uintptr_t c4=0;
    if (!mem_->read_into(c4_list,c4)) return false;
    if (!c4) return true;
    constexpr size_t begin=offsets::m_bBombTicking;
    constexpr size_t end=(std::max)({size_t(offsets::m_bBombDefused+1),size_t(offsets::m_flDefuseCountDown+4),
        size_t(offsets::m_flDefuseLength+4),size_t(offsets::m_flTimerLength+4),size_t(offsets::m_nBombSite+4)});
    std::array<uint8_t,end-begin> block{};
    if (!mem_->read_buffer(c4+begin,block.data(),block.size())) return false;
    const auto ticking=field<uint8_t>(block,offsets::m_bBombTicking-begin);
    const auto defused=field<uint8_t>(block,offsets::m_bBombDefused-begin);
    const auto defusing=field<uint8_t>(block,offsets::m_bBeingDefused-begin);
    if (ticking>1 || defused>1 || defusing>1) return false;
    BombState next; next.entity=c4; next.ticking=ticking==1; next.defused=defused==1;
    next.blow_time=field<float>(block,offsets::m_flC4Blow-begin);
    next.timer_length=field<float>(block,offsets::m_flTimerLength-begin);
    next.defuse_end=field<float>(block,offsets::m_flDefuseCountDown-begin);
    next.defuse_length=field<float>(block,offsets::m_flDefuseLength-begin);
    next.site=field<int>(block,offsets::m_nBombSite-begin);
    const auto globals=mem_->read<uintptr_t>(globals_addr);
    if (!globals || !mem_->read_into(globals+offsets::GLOBAL_CURTIME,next.server_time) || !std::isfinite(next.server_time)) return false;
    if (mem_->read<uintptr_t>(c4_list)!=c4) return false;
    const float remaining=next.blow_time-next.server_time;
    next.clock_valid=next.server_time>=0 && std::isfinite(remaining) && std::isfinite(next.timer_length) &&
        next.timer_length>0 && next.timer_length<=3600 && remaining>=-2 && remaining<=next.timer_length+1;
    next.remaining=next.clock_valid ? (std::max)(0.f,remaining) : 0;
    next.planted=next.clock_valid && next.ticking && !next.defused && remaining>0;
    next.defusing=next.planted && defusing==1;
    const float defuse_remaining=next.defuse_end-next.server_time;
    next.defuse_valid=next.defusing && std::isfinite(next.defuse_end) && std::isfinite(next.defuse_length) &&
        next.defuse_length>0 && next.defuse_length<=60 && defuse_remaining>=0 && defuse_remaining<=next.defuse_length+.25f;
    if (next.defuse_valid) next.defuse_remaining=defuse_remaining;
    const auto scene=mem_->read<uintptr_t>(c4+offsets::m_pGameSceneNode);
    if (scene) next.pos=mem_->read<Vec3>(scene+offsets::m_vecAbsOrigin);
    result=next; return true;
}
void EntityCache::update_bomb() { current_bomb_state(bomb_); }
'''
p.write_text(s,encoding='utf-8')
p=root/'esp.cpp'; s=p.read_text(encoding='utf-8'); start=s.index('void Esp::draw_bomb_timer(');end=s.index('// ─── Spectator List',start)
s=s[:start]+r'''void Esp::draw_bomb_timer(ImDrawList* dl,const BombState& bomb,const Config&,int sw,int sh) {
    if (!bomb.entity || !bomb.clock_valid || (!bomb.ticking && !bomb.defused)) return;
    const bool exploded=!bomb.defused && !bomb.planted && bomb.server_time>=bomb.blow_time;
    if (!bomb.planted && !bomb.defused && !exploded) return;
    const float width=320.f,x=sw*.5f-width*.5f,y=sh*.1f;
    dl->AddRectFilled({x,y},{x+width,y+112},IM_COL32(10,16,23,240),6.f);
    const char* site=bomb.site==0 ? "A" : bomb.site==1 ? "B" : "?";
    char title[32]; sprintf_s(title,"BOMBA %s",site);
    dl->AddText(nullptr,13.f,{x+12,y+9},IM_COL32(190,205,220,255),title);
    dl->AddText(nullptr,13.f,{x+174,y+9},IM_COL32(190,205,220,255),"DESARME");
    if (bomb.defused || exploded) {
        dl->AddText(nullptr,24.f,{x+12,y+34},bomb.defused ? IM_COL32(80,235,155,255) : IM_COL32(255,90,80,255),
            bomb.defused ? "BOMBA DESARMADA" : "BOMBA EXPLODIU");
        return;
    }
    const ImU32 bomb_color=bomb.remaining<=5 ? IM_COL32(255,85,75,255) : IM_COL32(255,212,95,255);
    const bool in_time=bomb.defuse_valid && bomb.defuse_end<bomb.blow_time;
    const ImU32 defuse_color=in_time ? IM_COL32(80,235,155,255) : IM_COL32(255,110,90,255);
    char text[32]; sprintf_s(text,"%.2f s",bomb.remaining);
    dl->AddText(nullptr,25.f,{x+12,y+29},bomb_color,text);
    if (bomb.defuse_valid) sprintf_s(text,"%.2f s",bomb.defuse_remaining);
    else strcpy_s(text,bomb.defusing ? "..." : "--");
    dl->AddText(nullptr,25.f,{x+174,y+29},bomb.defuse_valid ? defuse_color : IM_COL32(145,160,175,255),text);
    const float bomb_ratio=std::clamp(bomb.remaining/bomb.timer_length,0.f,1.f);
    const float defuse_ratio=bomb.defuse_valid ? std::clamp(1.f-bomb.defuse_remaining/bomb.defuse_length,0.f,1.f) : 0;
    dl->AddRectFilled({x+12,y+66},{x+146,y+72},IM_COL32(40,50,60,255),2.f);
    dl->AddRectFilled({x+12,y+66},{x+12+134*bomb_ratio,y+72},bomb_color,2.f);
    dl->AddRectFilled({x+174,y+66},{x+308,y+72},IM_COL32(40,50,60,255),2.f);
    dl->AddRectFilled({x+174,y+66},{x+174+134*defuse_ratio,y+72},defuse_color,2.f);
    const char* status=!bomb.defusing ? "SEM DESARME ATIVO" : !bomb.defuse_valid ? "ATUALIZANDO DESARME" :
        in_time ? "DESARME TERMINA ANTES DA EXPLOSAO" : "DESARME NAO TERMINA A TEMPO";
    dl->AddText(nullptr,12.f,{x+12,y+88},bomb.defuse_valid ? defuse_color : IM_COL32(165,180,195,255),status);
}

''' +s[end:]
s=s.replace('    if (cfg.bomb_timer) draw_bomb_timer(dl, cache.bomb(), cfg, sw, sh);','    if (cfg.bomb_timer) { BombState bomb; if (cache.current_bomb_state(bomb)) draw_bomb_timer(dl,bomb,cfg,sw,sh); }')
p.write_text(s,encoding='utf-8')
