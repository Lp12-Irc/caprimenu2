#include "neon_model.hpp"
#include "render_capture.hpp"
#include "json.hpp"
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include <chrono>

static void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static void quaternion(const float* m,float* q) {
    const float trace=m[0]+m[5]+m[10];
    if(trace>0) {
        const float s=std::sqrt(trace+1.f)*2;
        q[3]=s*.25f;q[0]=(m[9]-m[6])/s;q[1]=(m[2]-m[8])/s;q[2]=(m[4]-m[1])/s;
    } else {
        const int i=m[0]>m[5]?(m[0]>m[10]?0:2):(m[5]>m[10]?1:2),j=(i+1)%3,k=(i+2)%3;
        const float s=std::sqrt(1.f+m[i*4+i]-m[j*4+j]-m[k*4+k])*2;
        q[i]=s*.25f;q[3]=(m[k*4+j]-m[j*4+k])/s;
        q[j]=(m[j*4+i]+m[i*4+j])/s;q[k]=(m[k*4+i]+m[i*4+k])/s;
    }
}
static PlayerEnt pose(const std::filesystem::path& path,const std::string& name) {
    std::ifstream file(path,std::ios::binary);char magic[4];uint32_t joints,vertices,indices;
    file.read(magic,4);file.read(reinterpret_cast<char*>(&joints),4);file.read(reinterpret_cast<char*>(&vertices),4);file.read(reinterpret_cast<char*>(&indices),4);
    check(joints<=128,"asset joint limit");
    PlayerEnt p;p.bones_valid=true;p.collision_valid=true;p.collision_max={32,32,90};p.model_binding=1;
    auto info=std::make_shared<ModelInfo>();info->name=name;
    for(uint32_t i=0;i<joints;++i) {
        char joint[64];float inverse[12],rest[12];file.read(joint,64);file.read(reinterpret_cast<char*>(inverse),48);file.read(reinterpret_cast<char*>(rest),48);
        info->bones.emplace_back(joint);p.bone[i].position={rest[3],rest[7],rest[11]};p.bone[i].scale=1;quaternion(rest,p.bone[i].rotation);
    }
    check(bool(file),"asset rest pose read");p.model_info=info;p.rig=bones::from_names(info->bones);return p;
}
int main(int argc,char** argv) {
    try {
        check(argc>=2,"model asset directory required");const std::filesystem::path assets=argv[1];
        std::ifstream input(assets/"manifest.json");nlohmann::json manifest;input>>manifest;
        ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={960,720};io.DeltaTime=1.f/60;
        io.BackendFlags|=ImGuiBackendFlags_RendererHasVtxOffset;io.IniFilename=nullptr;
        unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        RenderCapture capture;check(capture.init(960,720),"WARP capture device");
        NeonModels renderer;renderer.init(assets);
        Matrix4x4 view{};view.m[0][1]=1.f/52;view.m[1][2]=1.f/55;view.m[1][3]=-.65f;view.m[3][3]=1;
        int count=0;double elapsed=0;
        for(const auto& [name,record]:manifest.at("models").items()) {
            auto p=pose(assets/record.at("file").get<std::string>(),name);
            capture.begin();ImGui::NewFrame();auto* dl=ImGui::GetBackgroundDrawList();
            const auto begin=std::chrono::steady_clock::now();
            check(renderer.draw(dl,p,view,960,720,IM_COL32(0,235,255,255)),"real model produced no geometry");
            elapsed+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
            check(dl->VtxBuffer.Size>1000,"model is not a full mesh");
            float min_y=720,max_y=0,min_x=960,max_x=0;
            for(const auto& v:dl->VtxBuffer) {
                check(std::isfinite(v.pos.x)&&std::isfinite(v.pos.y),"nonfinite projected vertex");
                check(v.col==IM_COL32(0,235,255,255),"custom neon color/opacity mismatch");
                min_y=(std::min)(min_y,v.pos.y);max_y=(std::max)(max_y,v.pos.y);
                min_x=(std::min)(min_x,v.pos.x);max_x=(std::max)(max_x,v.pos.x);
            }
            check(max_y-min_y>350 && max_y-min_y<650,"rest pose body height");
            check(max_x-min_x>100 && max_x-min_x<850,"rest pose body width");
            ImGui::Render();
            if(argc>=3 && name=="agents/models/ctm_sas/ctm_sas.vmdl")check(capture.save(argv[2]),"model preview PNG");
            // Independent rotation, translation and color must still yield a finite mesh.
            for(auto& b:p.bone) {
                const auto position=b.position;b.position={-position.y+8,position.x+4,position.z};
                const auto x=b.rotation[0],y=b.rotation[1],z=b.rotation[2],w=b.rotation[3];
                constexpr float s=.70710678118f;b.rotation[0]=s*(x-y);b.rotation[1]=s*(x+y);b.rotation[2]=s*(w+z);b.rotation[3]=s*(w-z);
            }
            p.origin={8,4,0};capture.begin();ImGui::NewFrame();dl=ImGui::GetBackgroundDrawList();
            check(renderer.draw(dl,p,view,960,720,IM_COL32(255,0,200,30)),"rotated model missing");
            for(const auto& v:dl->VtxBuffer)check(v.col==IM_COL32(255,0,200,255),"solid magenta custom color");
            ImGui::Render();
            if(name=="agents/models/ctm_sas/ctm_sas.vmdl") {
                capture.begin();ImGui::NewFrame();dl=ImGui::GetBackgroundDrawList();
                const auto hot=std::chrono::steady_clock::now();
                for(int player=0;player<10;++player)check(renderer.draw(dl,p,view,960,720,IM_COL32(0,235,255,255)),"ten player batch");
                const auto milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-hot).count();
                std::printf("Ten cached SAS bodies: %.2f ms CPU, %d vertices\n",milliseconds,dl->VtxBuffer.Size);
                ImGui::Render();
            }
            auto missing=std::make_shared<ModelInfo>(*p.model_info);missing->name="missing.vmdl";p.model_info=missing;
            capture.begin();ImGui::NewFrame();dl=ImGui::GetBackgroundDrawList();
            check(!renderer.draw(dl,p,view,960,720,0)&&dl->VtxBuffer.empty(),"unknown model must not invent geometry");
            ImGui::Render();++count;
        }
        capture.shutdown();ImGui::DestroyContext();
        std::printf("PASS: %d real agents, rest/rotated poses, custom colors; first-draw mean %.2f ms (includes asset load)\n",count,elapsed/count);
        return 0;
    } catch(const std::exception& error){std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
