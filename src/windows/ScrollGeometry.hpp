#pragma once
#include <algorithm>
#include <cstdint>

// Win32 scroll ranges have an inclusive maximum. Use wide intermediates for
// large lists and clamp again when pages/ranges shrink during a drag.
namespace ScrollGeometry {
inline int Maximum(int low, int high, unsigned page) {
    return int((std::max)(int64_t(low), int64_t(high) - (page ? int64_t(page)-1 : 0)));
}
inline int Clamp(int value, int low, int high, unsigned page) {
    return (std::max)(low, (std::min)(Maximum(low, high, page), value));
}
struct Thumb {
    int start, length, travel;
    int low, high;
    int Position(int pixel, int grab) const {
        if (travel<=0 || high<=low) return low;
        int offset=(std::max)(0, (std::min)(travel, pixel-grab));
        return int(int64_t(low) + (int64_t(offset)*(int64_t(high)-low)+travel/2)/travel);
    }
};
inline Thumb Calculate(int extent, int low, int high, unsigned page, int position, int minimum) {
    Thumb result = {0, 0, 0, low, Maximum(low, high, page)};
    int64_t range = int64_t(high)-low+1;
    if (extent<=0 || range<=0 || result.high<=low) return result;
    result.length=(std::min)(extent, (std::max)(minimum, int(int64_t(extent)*page/range)));
    result.travel=extent-result.length;
    result.start=int(int64_t(Clamp(position,low,high,page)-int64_t(low))*result.travel/(int64_t(result.high)-low));
    return result;
}
}
