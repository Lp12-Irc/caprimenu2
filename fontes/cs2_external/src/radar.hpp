#pragma once
#include "entities.hpp"
#include "config.hpp"
#include <filesystem>
#include <memory>
struct ID3D11Device;
struct ImDrawList;
struct RadarPoint { float x=0,y=0; };
struct RadarFloor { std::string name,image; float min_z=0,max_z=0; };
struct RadarLandmark { std::string label; RadarPoint uv; };
struct RadarDefinition {
    std::string name,image;
    float pos_x=0,pos_y=0,scale=1,reference_size=1024;
    std::vector<RadarFloor> floors;
    std::vector<RadarLandmark> landmarks;
    RadarPoint world_to_uv(const Vec3& point) const { return {(point.x-pos_x)/(scale*reference_size),(pos_y-point.y)/(scale*reference_size)}; }
    int floor_at(float z) const;
    static bool load(const std::filesystem::path& folder,const std::string& name,RadarDefinition& result);
};
class RadarRenderer {
public:
    RadarRenderer();
    ~RadarRenderer();
    void init(ID3D11Device* device,const std::filesystem::path& folder="assets/radar");
    bool prepare(const std::string& map,float altitude);
    void draw(ImDrawList*,const EntityCache&,const Config&,int sw,int sh);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
