#pragma once
#include "config.hpp"
#include "entities.hpp"

struct GameViewport {
    float x=0,y=0,width=0,height=0;
    float right() const {return x+width;}
    float bottom() const {return y+height;}
};

inline GameViewport game_viewport(const Config& cfg,int width,int height) {
    if(width<1 || height<1)return {};
    GameViewport result{0,0,float(width),float(height)};
    // Stretched output already fills the window. The game's view matrix contains
    // its source aspect: applying another aspect multiplier would distort it twice.
    if(cfg.display_fit!=1 || cfg.display_aspect==0)return result;
    float aspect=0;
    switch(cfg.display_aspect) {
        case 1:aspect=16.f/9.f;break;
        case 2:aspect=16.f/10.f;break;
        case 3:aspect=4.f/3.f;break;
        default:return result;
    }
    if(result.width/result.height>aspect) {
        result.width=result.height*aspect;result.x=(width-result.width)*.5f;
    } else {
        result.height=result.width/aspect;result.y=(height-result.height)*.5f;
    }
    return result;
}

inline Matrix4x4 viewport_projection(const Matrix4x4& camera,const GameViewport& area,int width,int height) {
    if(width<1 || height<1)return camera;
    Matrix4x4 projected=camera;
    const float sx=area.width/width,sy=area.height/height;
    // All supported viewports are centered. Scaling the clip-plane rows maps the
    // source NDC into the image rectangle while preserving world-space aiming.
    for(int column=0;column<4;++column) {
        projected.m[0][column]*=sx;projected.m[1][column]*=sy;
    }
    return projected;
}
