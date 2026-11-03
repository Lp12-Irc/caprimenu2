#pragma once
#include "entities.hpp"
#include <limits>

namespace fov_geometry {
constexpr float radians=.017453292519943295f;
inline float cone_angle(const Vec3& angular_error) {
    if(!angular_error.finite() || std::fabs(angular_error.x)>=90 || std::fabs(angular_error.y)>=90)
        return (std::numeric_limits<float>::infinity)();
    return std::atan(std::hypot(std::tan(angular_error.x*radians),std::tan(angular_error.y*radians)))/radians;
}
inline bool focal(const Matrix4x4& camera,float& x,float& y) {
    auto length=[](const float row[4]){return std::hypot(row[0],row[1],row[2]);};
    const float forward=length(camera.m[3]);
    if(!std::isfinite(forward) || forward<.001f)return false;
    x=length(camera.m[0])/forward;y=length(camera.m[1])/forward;
    return std::isfinite(x) && std::isfinite(y) && x>.001f && y>.001f;
}
inline bool radii(const Matrix4x4& camera,float degrees,int width,int height,float& x,float& y) {
    float fx=0,fy=0;
    if(width<1 || height<1 || !std::isfinite(degrees) || degrees<=0 || degrees>=90 || !focal(camera,fx,fy))return false;
    const float tangent=std::tan(degrees*radians);
    x=width*.5f*fx*tangent;y=height*.5f*fy*tangent;
    return std::isfinite(x) && std::isfinite(y) && x>0 && y>0;
}
}
