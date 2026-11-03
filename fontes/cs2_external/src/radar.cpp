#include "radar.hpp"
#include "imgui.h"
#include "json.hpp"
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <fstream>
#include <algorithm>
#include <limits>
using Microsoft::WRL::ComPtr;
using json=nlohmann::json;

int RadarDefinition::floor_at(float z) const {
    for (size_t i=0;i<floors.size();++i) if (z>=floors[i].min_z && z<floors[i].max_z) return static_cast<int>(i);
    for (size_t i=0;i<floors.size();++i) if (floors[i].name=="default") return static_cast<int>(i);
    return floors.empty() ? -1 : 0;
}
bool RadarDefinition::load(const std::filesystem::path& folder,const std::string& map,RadarDefinition& result) {
    try {
        if (map.empty() || map.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos) return false;
        std::ifstream input(folder/(map+".json"));
        if (!input) return false;
        const auto j=json::parse(input);
        RadarDefinition next; next.name=map; next.image=j.at("image").get<std::string>();
        next.pos_x=j.at("pos_x"); next.pos_y=j.at("pos_y"); next.scale=j.at("scale"); next.reference_size=j.value("reference_size",1024.f);
        if (!std::isfinite(next.pos_x) || !std::isfinite(next.pos_y) || !std::isfinite(next.scale) || next.scale<=0 ||
            !std::isfinite(next.reference_size) || next.reference_size<=0) return false;
        auto safe_image=[](const std::string& name) { return !name.empty() && name.find_first_of("/\\:")==std::string::npos && std::filesystem::path(name).extension()==".png"; };
        if (!safe_image(next.image)) return false;
        for (const auto& value:j.at("floors")) {
            RadarFloor floor{value.at("name"),value.at("image"),value.at("min_z"),value.at("max_z")};
            if (!safe_image(floor.image) || !std::isfinite(floor.min_z) || !std::isfinite(floor.max_z) || floor.min_z>=floor.max_z || next.floors.size()>=32) return false;
            next.floors.push_back(std::move(floor));
        }
        if (next.floors.empty()) return false;
        for (const auto& value:j.value("landmarks",json::array())) {
            RadarLandmark marker{value.at("label"),{value.at("x"),value.at("y")}};
            if (marker.label.size()>8 || !std::isfinite(marker.uv.x) || !std::isfinite(marker.uv.y)) return false;
            next.landmarks.push_back(std::move(marker));
        }
        result=std::move(next); return true;
    } catch (...) { return false; }
}

struct RadarRenderer::Impl {
    ComPtr<ID3D11Device> device;
    std::filesystem::path folder="assets/radar";
    RadarDefinition map;
    std::string requested,image;
    ComPtr<ID3D11ShaderResourceView> texture;
    ULONGLONG retry=0;
    bool loaded=false;
    int floor=0;
    bool load_texture(const std::string& name) {
        texture.Reset();
        if (!device) return false;
        const HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        struct Cleanup { HRESULT hr; ~Cleanup(){if(SUCCEEDED(hr)) CoUninitialize();} } cleanup{co};
        ComPtr<IWICImagingFactory> factory; ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICBitmapFrameDecode> frame; ComPtr<IWICFormatConverter> converter;
        const auto path=folder/name;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))) ||
            FAILED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder)) ||
            FAILED(decoder->GetFrame(0,&frame)) || FAILED(factory->CreateFormatConverter(&converter)) ||
            FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))) return false;
        UINT w=0,h=0; converter->GetSize(&w,&h);
        if (!w || !h || w>4096 || h>4096) return false;
        std::vector<BYTE> pixels(static_cast<size_t>(w)*h*4);
        if (FAILED(converter->CopyPixels(nullptr,w*4,static_cast<UINT>(pixels.size()),pixels.data()))) return false;
        D3D11_TEXTURE2D_DESC desc{}; desc.Width=w; desc.Height=h; desc.MipLevels=desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1; desc.Usage=D3D11_USAGE_IMMUTABLE; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        const D3D11_SUBRESOURCE_DATA data{pixels.data(),w*4,0}; ComPtr<ID3D11Texture2D> resource;
        return SUCCEEDED(device->CreateTexture2D(&desc,&data,&resource)) && SUCCEEDED(device->CreateShaderResourceView(resource.Get(),nullptr,&texture));
    }
};
RadarRenderer::RadarRenderer():impl_(std::make_unique<Impl>()){}
RadarRenderer::~RadarRenderer()=default;
void RadarRenderer::init(ID3D11Device* device,const std::filesystem::path& folder) {
    impl_->device=device; impl_->folder=folder; impl_->requested.clear(); impl_->image.clear(); impl_->texture.Reset(); impl_->retry=0; impl_->loaded=false;
}
bool RadarRenderer::prepare(const std::string& name,float altitude) {
    auto& p=*impl_;
    if (name!=p.requested || (!p.loaded && GetTickCount64()>=p.retry)) {
        p.requested=name; p.loaded=RadarDefinition::load(p.folder,name,p.map); p.image.clear(); p.texture.Reset(); p.retry=GetTickCount64()+1000;
    }
    if (!p.loaded) return false;
    p.floor=p.map.floor_at(altitude);
    if (p.floor<0) return false;
    const auto& image=p.map.floors[p.floor].image;
    if (image!=p.image || (!p.texture && GetTickCount64()>=p.retry)) {
        p.image=image; p.load_texture(image); p.retry=GetTickCount64()+1000;
    }
    return p.texture!=nullptr;
}
void RadarRenderer::draw(ImDrawList* dl,const EntityCache& cache,const Config& cfg,int sw,int sh) {
    const auto* local=cache.local(); const PlayerEnt* anchor=local && local->valid() && local->origin.finite() ? local : nullptr;
    if (!anchor && local) for (const auto& p:cache.players()) if (p.pawn==local->obs_target && p.valid() && p.origin.finite()) { anchor=&p; break; }
    const float size=std::clamp(cfg.radar_sz,80.f,(std::max)(80.f,float((std::min)(sw-24,sh-90))));
    const float x=std::clamp(sw*cfg.radar_x/100.f,8.f,(std::max)(8.f,sw-size-8.f));
    const float y=std::clamp(sh*cfg.radar_y/100.f,30.f,(std::max)(30.f,sh-size-52.f));
    dl->AddRectFilled({x-5,y-27},{x+size+5,y+size+42},IM_COL32(12,17,23,240),5.f);
    const bool ready=prepare(cache.map_name(),anchor ? anchor->origin.z : 0);
    auto& p=*impl_;
    const std::string title=cache.map_name().empty() ? "RADAR / aguardando mapa" : cache.map_name()+
        (p.loaded && p.map.floors[p.floor].name!="default" ? " / inferior" : " / principal");
    dl->PushClipRect({x,y-25},{x+size,y+size+40},true);
    dl->AddText(nullptr,13.f,{x,y-22},IM_COL32(235,240,245,255),title.c_str());
    if (!ready) {
        dl->AddText(nullptr,12.f,{x+5,y+12},IM_COL32(235,190,110,255),"Mapa indisponivel");
        dl->PopClipRect();
        return;
    }
    const float zoom=std::clamp(cfg.radar_zoom,1.f,4.f);
    RadarPoint center=anchor ? p.map.world_to_uv(anchor->origin) : RadarPoint{.5f,.5f};
    const float span=1.f/zoom;
    const RadarPoint uv0{std::clamp(center.x-span*.5f,0.f,1.f-span),std::clamp(center.y-span*.5f,0.f,1.f-span)};
    dl->AddImage((ImTextureID)p.texture.Get(),{x,y},{x+size,y+size},{uv0.x,uv0.y},{uv0.x+span,uv0.y+span});
    dl->AddRect({x,y},{x+size,y+size},IM_COL32(90,115,135,255));
    auto screen=[&](RadarPoint uv) { return ImVec2{x+(uv.x-uv0.x)/span*size,y+(uv.y-uv0.y)/span*size}; };
    auto inside=[&](ImVec2 pos) { return pos.x>=x+4 && pos.x<=x+size-4 && pos.y>=y+4 && pos.y<=y+size-4; };
    dl->PushClipRect({x+1,y+1},{x+size-1,y+size-1},true);
    for (const auto& landmark:p.map.landmarks) {
        const auto pos=screen(landmark.uv);
        if (inside(pos)) dl->AddText(nullptr,16.f,{pos.x-4,pos.y-8},IM_COL32(255,222,100,255),landmark.label.c_str());
    }
    int offscreen=0;
    std::vector<ImVec4> occupied;
    auto place_label=[&](const ImVec2 marker,const std::string& text,ImU32 color) {
        const float width=ImGui::CalcTextSize(text.c_str()).x*12.f/ImGui::GetFontSize();
        for (int ring=0;ring<5;++ring) for (int side=0;side<4;++side) {
            const float left=side&1 ? marker.x-width-8-ring*5.f : marker.x+8+ring*5.f;
            const float top=marker.y-7+(side&2 ? 1.f : -1.f)*ring*15.f;
            const ImVec4 box{left,top,left+width,top+13};
            if (box.x<x+3 || box.z>x+size-3 || box.y<y+3 || box.w>y+size-3) continue;
            bool overlaps=false;
            for (const auto& other:occupied) if (box.x<other.z+2 && box.z>other.x-2 && box.y<other.w+2 && box.w>other.y-2) { overlaps=true; break; }
            if (overlaps) continue;
            occupied.push_back(box);
            if (ring>0) dl->AddLine(marker,{std::clamp(marker.x,box.x,box.z),std::clamp(marker.y,box.y,box.w)},color,1.f);
            dl->AddText(nullptr,12.f,{left+1,top+1},IM_COL32(0,0,0,255),text.c_str());
            dl->AddText(nullptr,12.f,{left,top},color,text.c_str()); return true;
        }
        return false;
    };
    for (const auto& player:cache.players()) {
        if (!player.valid() || !player.origin.finite()) continue;
        const auto pos=screen(p.map.world_to_uv(player.origin));
        if (!inside(pos)) { ++offscreen; continue; }
        const bool self=&player==anchor;
        const bool ally=local && player.team==local->team;
        const auto color=self ? IM_COL32(255,255,255,255) : ally ? IM_COL32(70,175,255,255) : IM_COL32(255,0,96,255);
        const float angle=player.eye_angles.y*3.14159265f/180.f;
        const ImVec2 direction{std::cos(angle),-std::sin(angle)};
        dl->AddCircleFilled(pos,6.f,IM_COL32(5,10,15,230));
        if (self) dl->AddTriangleFilled({pos.x+direction.x*8,pos.y+direction.y*8},
            {pos.x-direction.x*5-direction.y*5,pos.y-direction.y*5+direction.x*5},
            {pos.x-direction.x*5+direction.y*5,pos.y-direction.y*5-direction.x*5},color);
        else if (ally) dl->AddCircleFilled(pos,4.f,color);
        else dl->AddRectFilled({pos.x-4,pos.y-4},{pos.x+4,pos.y+4},color,1.f);
        if (!self) dl->AddLine(pos,{pos.x+direction.x*9,pos.y+direction.y*9},color,1.5f);
        std::string label=self ? (player.is_local ? "VOCE" : "OBS") : std::to_string(player.controller_index);
        const int floor=p.map.floor_at(player.origin.z);
        if (floor!=p.floor) label+=(player.origin.z>(anchor ? anchor->origin.z : 0) ? " ^" : " v");
        if (cfg.radar_names && player.name[0] && !self) {
            std::string name=player.name;
            if (name.size()>14) { size_t end=14; while (end && (static_cast<unsigned char>(name[end])&0xC0)==0x80) --end; name.resize(end); name+="..."; }
            label+=' '; label+=name;
        }
        if (!place_label(pos,label,color)) place_label(pos,self ? "VOCE" : std::to_string(player.controller_index),color);
    }
    if (cache.bomb().planted) {
        const auto pos=screen(p.map.world_to_uv(cache.bomb().pos));
        if (inside(pos)) { dl->AddCircle(pos,6,IM_COL32(255,215,70,255),16,2.f); dl->AddText(nullptr,12.f,{pos.x+8,pos.y},IM_COL32(255,215,70,255),"C4"); }
    }
    dl->PopClipRect();
    dl->AddText(nullptr,11.f,{x,y+size+5},IM_COL32(220,230,240,255),size<280 ? "Azul: aliado | Rosa: inimigo" : "VOCE: seta | ALIADO: azul | INIMIGO: rosa");
    const std::string footer=(size<280 ? "Norte acima | Fora: " : "Norte acima | ^ cima / v baixo | Fora: ")+std::to_string(offscreen);
    dl->AddText(nullptr,12.f,{x,y+size+21},IM_COL32(160,180,195,255),footer.c_str());
    dl->PopClipRect();
}
