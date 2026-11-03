#include "neon_model.hpp"
#include "json.hpp"
#include <fstream>
#include <cstring>

void NeonModels::init(const std::filesystem::path& folder) {
    folder_=folder;files_.clear();meshes_.clear();bindings_.clear();
    try {
        std::ifstream file(folder/"manifest.json");nlohmann::json manifest;file>>manifest;
        if(manifest.value("build",0)!=offsets::BUILD_NUMBER)return;
        for(const auto& [name,value]:manifest.at("models").items()) {
            const auto filename=value.at("file").get<std::string>();
            if(std::filesystem::path(filename).filename()==filename && filename.ends_with(".cnm"))files_[name]=filename;
        }
    } catch(...) {files_.clear();}
}

NeonModels::Mesh* NeonModels::load(const std::string& name) {
    auto existing=meshes_.find(name);if(existing!=meshes_.end())return existing->second.vertices.empty()?nullptr:&existing->second;
    const auto entry=files_.find(name);if(entry==files_.end())return nullptr;
    // Remember failures so a missing asset never causes repeated disk reads per frame.
    auto& mesh=meshes_[name];
    std::ifstream file(folder_/entry->second,std::ios::binary|std::ios::ate);if(!file)return nullptr;
    const auto length=file.tellg();file.seekg(0);
    char magic[4]{};uint32_t joints=0,vertices=0,indices=0;
    file.read(magic,4);file.read(reinterpret_cast<char*>(&joints),4);file.read(reinterpret_cast<char*>(&vertices),4);file.read(reinterpret_cast<char*>(&indices),4);
    if(!file || std::memcmp(magic,"CNM3",4) || joints<1 || joints>128 || vertices<3 || vertices>65535 || indices<3 || indices>1200000 || indices%3)return nullptr;
    static_assert(sizeof(Vertex)==14 && sizeof(Joint)==160);
    const uint64_t expected=16+uint64_t(joints)*sizeof(Joint)+uint64_t(vertices)*sizeof(Vertex)+uint64_t(indices)*2;
    if(length!=static_cast<std::streamoff>(expected))return nullptr;
    Mesh candidate;candidate.joints.resize(joints);candidate.vertices.resize(vertices);candidate.indices.resize(indices);
    file.read(reinterpret_cast<char*>(candidate.joints.data()),joints*sizeof(Joint));
    file.read(reinterpret_cast<char*>(candidate.vertices.data()),vertices*sizeof(Vertex));
    file.read(reinterpret_cast<char*>(candidate.indices.data()),indices*2);if(!file)return nullptr;
    for(const auto& j:candidate.joints) {
        if(!std::memchr(j.name,0,sizeof(j.name)))return nullptr;
        for(float f:j.inverse)if(!std::isfinite(f))return nullptr;
    }
    for(const auto& v:candidate.vertices) {
        float sum=0;
        for(int k=0;k<4;++k){if(v.joint[k]>=joints)return nullptr;sum+=v.weight[k];}
        if(sum!=255.f)return nullptr;
    }
    for(auto i:candidate.indices)if(i>=vertices)return nullptr;
    mesh=std::move(candidate);return &mesh;
}

bool NeonModels::draw(ImDrawList* dl,const PlayerEnt& p,const Matrix4x4& view,int sw,int sh,ImU32 color) {
    if(!p.bones_valid || !p.model_info || sw<=0 || sh<=0)return false;
    auto* mesh=load(p.model_info->name);if(!mesh)return false;
    // Match by name, never assume that different agents share their bone indices.
    if(bindings_.size()>256)bindings_.clear();
    auto& binding=bindings_[p.model_binding];
    if(binding.identity!=p.model_info) {
        binding.identity=p.model_info;binding.remap.assign(mesh->joints.size(),-1);
        for(size_t i=0;i<mesh->joints.size();++i) {
            const auto& names=p.model_info->bones;
            const auto it=std::find(names.begin(),names.end(),mesh->joints[i].name);
            if(it!=names.end() && it-names.begin()<static_cast<ptrdiff_t>(p.bone.size()))binding.remap[i]=static_cast<int>(it-names.begin());
        }
    }
    std::array<std::array<float,12>,128> transforms{};
    std::array<bool,128> valid{};
    for(size_t i=0;i<mesh->joints.size();++i) {
        const int index=binding.remap[i];Vec3 position{};
        if(index<0 || !p.bone_point(index,position))continue;
        const auto& bone=p.bone[index];const float* q=bone.rotation;
        const float norm=q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3];
        if(!std::isfinite(norm) || norm<.8f || norm>1.2f)continue;
        const float s=2.f/norm,x=q[0],y=q[1],z=q[2],w=q[3];
        const float r[9]{1-s*(y*y+z*z),s*(x*y-z*w),s*(x*z+y*w),
            s*(x*y+z*w),1-s*(x*x+z*z),s*(y*z-x*w),
            s*(x*z-y*w),s*(y*z+x*w),1-s*(x*x+y*y)};
        const auto* inverse=mesh->joints[i].inverse;auto& t=transforms[i];
        for(int row=0;row<3;++row)for(int col=0;col<4;++col) {
            t[row*4+col]=0;
            for(int k=0;k<3;++k)t[row*4+col]+=bone.scale*r[row*3+k]*inverse[k*4+col];
        }
        t[3]+=position.x;t[7]+=position.y;t[11]+=position.z;valid[i]=true;
    }
    projected_.resize(mesh->vertices.size());
    for(size_t i=0;i<mesh->vertices.size();++i) {
        auto& screen=projected_[i];screen.valid=false;
        const auto& v=mesh->vertices[i];Vec3 world{};bool complete=true;
        for(int k=0;k<4;++k) {
            if(v.weight[k]==0)continue;
            if(!valid[v.joint[k]]){complete=false;break;}
            const auto& t=transforms[v.joint[k]];const Vec3 a{v.position[0]/8192.f,v.position[1]/8192.f,v.position[2]/8192.f};
            world.x+=(v.weight[k]/255.f)*(t[0]*a.x+t[1]*a.y+t[2]*a.z+t[3]);
            world.y+=(v.weight[k]/255.f)*(t[4]*a.x+t[5]*a.y+t[6]*a.z+t[7]);
            world.z+=(v.weight[k]/255.f)*(t[8]*a.x+t[9]*a.y+t[10]*a.z+t[11]);
        }
        if(!complete || !world.finite() || (world-p.origin).len()>160.f)continue;
        const float clip=view.m[3][0]*world.x+view.m[3][1]*world.y+view.m[3][2]*world.z+view.m[3][3];
        if(!std::isfinite(clip) || clip<.01f)continue;
        const float px=(view.m[0][0]*world.x+view.m[0][1]*world.y+view.m[0][2]*world.z+view.m[0][3])/clip;
        const float py=(view.m[1][0]*world.x+view.m[1][1]*world.y+view.m[1][2]*world.z+view.m[1][3])/clip;
        if(!std::isfinite(px) || !std::isfinite(py) || std::fabs(px)>100 || std::fabs(py)>100)continue;
        screen.point={sw*.5f*(1+px),sh*.5f*(1-py)};screen.valid=true;
    }
    // Opaque color prevents seams and overlapping limbs from changing brightness.
    color|=IM_COL32_A_MASK;
    const auto uv=ImGui::GetFontTexUvWhitePixel();
    visible_indices_.clear();
    const ImVec4 clip=dl->_CmdHeader.ClipRect;
    for(size_t i=0;i<mesh->indices.size();i+=3) {
        const auto ia=mesh->indices[i],ib=mesh->indices[i+1],ic=mesh->indices[i+2];
        const auto& a=projected_[ia];const auto& b=projected_[ib];const auto& c=projected_[ic];
        if(!a.valid || !b.valid || !c.valid)continue;
        if((std::max)({a.point.x,b.point.x,c.point.x})<clip.x || (std::min)({a.point.x,b.point.x,c.point.x})>clip.z ||
           (std::max)({a.point.y,b.point.y,c.point.y})<clip.y || (std::min)({a.point.y,b.point.y,c.point.y})>clip.w)continue;
        visible_indices_.push_back(ia);visible_indices_.push_back(ib);visible_indices_.push_back(ic);
    }
    if(visible_indices_.empty())return false;
    // The DX11 backend supports VtxOffset. Keep shared mesh vertices indexed;
    // do not duplicate every triangle's three vertices on every frame.
    dl->PrimReserve(static_cast<int>(visible_indices_.size()),static_cast<int>(projected_.size()));
    const auto base=dl->_VtxCurrentIdx;
    for(const auto index:visible_indices_)dl->PrimWriteIdx(static_cast<ImDrawIdx>(base+index));
    for(const auto& point:projected_)dl->PrimWriteVtx(point.valid?point.point:ImVec2{},uv,color);
    return true;
}
