#pragma once
#include <cstdint>

class MenuFrameGate {
    bool painted_=false;
    std::uint64_t last_time_=0,last_revision_=0;
public:
    bool due(std::uint64_t now,std::uint64_t revision,bool live_content,bool interacting,bool changed=false,bool animated=false) const {
        return !painted_ || live_content || interacting || changed || revision!=last_revision_ || now-last_time_>=(animated?40:250);
    }
    void commit(std::uint64_t now,std::uint64_t revision) {painted_=true;last_time_=now;last_revision_=revision;}
    void reset(){painted_=false;}
    std::uint64_t wait_ms(std::uint64_t now,bool animated=false) const {const std::uint64_t period=animated?40:250;return !painted_ || now-last_time_>=period?0:period-(now-last_time_);}
};
