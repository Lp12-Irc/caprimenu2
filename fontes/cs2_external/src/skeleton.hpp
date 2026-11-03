#pragma once
#include <array>
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace bones {
// Canonical fallback verified in both live SAS and Phoenix assets, build 14194.
inline constexpr int HEAD=7, NECK=6, CHEST_UP=4, CHEST_LO=2;
inline constexpr int LSHOULDER=9, LELBOW=10, LWRIST=11, RSHOULDER=13, RELBOW=14, RWRIST=15;
inline constexpr int LHIP=17, LKNEE=18, LANKLE=19, RHIP=20, RKNEE=21, RANKLE=22, ROOT=1;
enum Joint { Head, Neck, SpineTop, Chest, SpineLow, Pelvis, LClavicle, LShoulder, LElbow, LWrist,
    RClavicle, RShoulder, RElbow, RWrist, LHip, LKnee, LAnkle, RHip, RKnee, RAnkle, JointCount };
inline constexpr int CONNECTIONS[][2] = {
    {Head,Neck},{Neck,SpineTop},{SpineTop,Chest},{Chest,SpineLow},{SpineLow,Pelvis},
    {SpineTop,LClavicle},{LClavicle,LShoulder},{LShoulder,LElbow},{LElbow,LWrist},
    {SpineTop,RClavicle},{RClavicle,RShoulder},{RShoulder,RElbow},{RElbow,RWrist},
    {Pelvis,LHip},{LHip,LKnee},{LKnee,LAnkle},{Pelvis,RHip},{RHip,RKnee},{RKnee,RAnkle}
};
struct Layout {
    std::array<int,JointCount> joint{7,6,5,4,2,1,8,9,10,11,12,13,14,15,17,18,19,20,21,22};
    bool named = false;
    int model_count = 0;
    int target(int choice) const { return joint[choice==1 ? Neck : choice==2 ? Chest : Head]; }
    int read_count() const { return std::max(model_count,*std::max_element(joint.begin(),joint.end())+1); }
};
inline Layout from_names(const std::vector<std::string>& names) {
    constexpr std::array<std::array<std::string_view,3>,JointCount> aliases{{
        {"head_0","head",""},{"neck_0","neck",""},{"spine_3","spine3",""},
        {"spine_2","spine2",""},{"spine_0","spine0",""},{"pelvis","hips",""},
        {"clavicle_l","clavicle_left",""},{"arm_upper_l","upperarm_l",""},
        {"arm_lower_l","forearm_l",""},{"hand_l","wrist_l",""},
        {"clavicle_r","clavicle_right",""},{"arm_upper_r","upperarm_r",""},
        {"arm_lower_r","forearm_r",""},{"hand_r","wrist_r",""},
        {"leg_upper_l","thigh_l",""},{"leg_lower_l","calf_l",""},{"ankle_l","foot_l",""},
        {"leg_upper_r","thigh_r",""},{"leg_lower_r","calf_r",""},{"ankle_r","foot_r",""}
    }};
    Layout layout; layout.joint.fill(-1); layout.named=true; layout.model_count=std::min(128,static_cast<int>(names.size()));
    for (size_t i=0;i<names.size();++i) {
        auto name=names[i];
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        for (int j=0;j<JointCount;++j) for (const auto alias:aliases[j])
            if (!alias.empty() && name==alias) layout.joint[j]=static_cast<int>(i);
    }
    return layout;
}
}
