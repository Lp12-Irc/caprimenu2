#pragma once
#include "entities.hpp"
#include "imgui.h"
#include <filesystem>
#include <unordered_map>

// A real skinned mesh, rendered in the existing external overlay.
class NeonModels {
public:
    void init(const std::filesystem::path& folder);
    bool draw(ImDrawList*,const PlayerEnt&,const Matrix4x4&,int,int,ImU32);
private:
    struct Joint { char name[64]; float inverse[12],rest[12]; };
    struct Vertex { int16_t position[3]; uint8_t joint[4]; uint8_t weight[4]; };
    struct Mesh { std::vector<Joint> joints; std::vector<Vertex> vertices; std::vector<uint16_t> indices; };
    struct Binding { std::shared_ptr<const ModelInfo> identity; std::vector<int> remap; };
    std::filesystem::path folder_;
    std::unordered_map<std::string,std::string> files_;
    std::unordered_map<std::string,Mesh> meshes_;
    std::unordered_map<uintptr_t,Binding> bindings_;
    Mesh* load(const std::string&);
    struct Projection { ImVec2 point; bool valid=false; };
    std::vector<Projection> projected_;
    std::vector<uint16_t> visible_indices_;
};
