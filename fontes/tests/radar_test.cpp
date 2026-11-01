#include "radar.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <cmath>
#include <fstream>
#include <cstdio>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        const std::filesystem::path folder=RADAR_ASSET_DIR;
        RadarDefinition map;
        check(RadarDefinition::load(folder,"de_dust2",map),"load installed Dust2 overview");
        auto point=map.world_to_uv({-2476,3239,0});
        check(std::fabs(point.x)<.0001f && std::fabs(point.y)<.0001f,"world origin maps to texture origin");
        point=map.world_to_uv({-2476+4.4f*1024,3239-4.4f*1024,0});
        check(std::fabs(point.x-1)<.0001f && std::fabs(point.y-1)<.0001f,"overview scale maps full world span");
        check(RadarDefinition::load(folder,"de_nuke",map),"load Nuke");
        check(map.floors[map.floor_at(-494)].name=="default" && map.floors[map.floor_at(-496)].name=="lower","choose correct altitude floor");
        check(map.floors[map.floor_at(-495)].name=="default","floor boundary is unambiguous");
        check(!RadarDefinition::load(folder,"../default",map),"map path traversal rejected");
        ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context; D3D_FEATURE_LEVEL level;
        check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"offscreen DX11 device");
        RadarRenderer renderer; renderer.init(device.Get(),folder);
        int count=0;
        for (const auto& entry:std::filesystem::directory_iterator(folder)) if (entry.path().extension()==".json") {
            RadarDefinition asset;
            check(RadarDefinition::load(folder,entry.path().stem().string(),asset),"parse each installed overview");
            for (const auto& floor:asset.floors) {
                const float z=floor.min_z+(floor.max_z-floor.min_z)*.5f;
                check(renderer.prepare(asset.name,z),"decode map floor PNG into a real DX11 texture");
            }
            ++count;
        }
        check(count>=20,"map set is present");
        check(!renderer.prepare("map_that_does_not_exist",0),"missing map is reported, not represented by unrelated dots");
        std::printf("PASS: %d map definitions, floor selection, world/texture calibration, PNG decoding and DX11 uploads.\n",count);
    } catch (const std::exception& error) { std::fprintf(stderr,"FAIL: %s\n",error.what()); return 1; }
}
