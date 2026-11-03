#pragma once
#include "config.hpp"
#include <memory>
#include <atomic>

struct InputFrame {Config cfg;int width=0,height=0;bool enabled=false,blocked=false;};
class InputFrames {
    std::atomic<std::shared_ptr<const InputFrame>> current_;
public:
    auto load() const {return current_.load();}
    bool publish(const Config& cfg,int width,int height,bool enabled,bool blocked) {
        const auto previous=load();
        if(previous && previous->width==width && previous->height==height && previous->enabled==enabled && previous->blocked==blocked && previous->cfg==cfg)return false;
        current_.store(std::make_shared<const InputFrame>(InputFrame{cfg,width,height,enabled,blocked}));
        return true;
    }
};
