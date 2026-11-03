#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

// Fixed storage: no allocation, neighbour search or integration during painting.
namespace ui_motion {
struct Point {float x,y;};
struct Link {unsigned a,b;};
class Plexus {
    float phase_=0;
    std::uint64_t tick_=0;
    bool active_=false;
public:
    static constexpr unsigned count=28;
    std::array<Point,count> points{};
    std::array<Link,count*2> links{};
    Plexus() {update_points();for(unsigned i=0;i<count;++i){links[i*2]={i,(i+1)%count};links[i*2+1]={i,(i+7)%count};}}
    void advance(std::uint64_t now,bool active) {
        if(active && active_ && now>=tick_) {phase_+=float((std::min)(now-tick_,std::uint64_t(100)))*.001f;update_points();}
        tick_=now;active_=active;
    }
    float phase() const {return phase_;}
    float pulse() const {return .5f+.5f*std::sin(phase_*1.5f);}
private:
    void update_points() {
        for(unsigned i=0;i<count;++i){
            const float x=float((i*37+11)%101)/100.f,y=float((i*61+19)%103)/102.f;
            points[i]={.05f+.9f*x+.025f*std::sin(phase_*.21f+i),.06f+.88f*y+.035f*std::cos(phase_*.18f+i*1.7f)};
        }
    }
};
inline float approach(float value,float target,float dt) {
    const float next=value+(target-value)*std::clamp(dt*12.f,0.f,1.f);
    return std::fabs(next-target)<.005f?target:next;
}
}
