#pragma once
#include <algorithm>
#include <cmath>

namespace aim_math {
// Relative input has integral counts. Never round past the current angular error;
// discard fractional credit when the target changes direction or enters deadzone.
inline long bounded_count(float request,float remaining,float& fraction) {
    if(!std::isfinite(request) || !std::isfinite(remaining) || std::fabs(remaining)<1.f || request*remaining<0) {fraction=0;return 0;}
    if(fraction*remaining<0)fraction=0;
    const float total=std::clamp(request+fraction,-std::fabs(remaining),std::fabs(remaining));
    const long count=static_cast<long>(std::clamp(total,-32767.f,32767.f));
    fraction=total-float(count);return count;
}
}
