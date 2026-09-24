#ifndef CTP2_DISPLAY_SCALING_H
#define CTP2_DISPLAY_SCALING_H

#include <algorithm>
#include <cstdint>

// The legacy layouts require at least 800 x 600 logical pixels. Scale in
// 25-percent steps and cap uniformly, so small displays never crop the UI.
struct DisplayScaling
{
    int width;
    int height;
    int percent;
};

inline DisplayScaling CalculateDisplayScaling(int width, int height, int percent)
{
    const int maximum = static_cast<int>(std::min<std::int64_t>(300,
        std::min(std::int64_t(width) * 100 / 800,
                 std::int64_t(height) * 100 / 600)));
    percent = std::max(100, std::min(percent, maximum) / 25 * 25);
    return {static_cast<int>(std::int64_t(width) * 100 / percent),
            static_cast<int>(std::int64_t(height) * 100 / percent), percent};
}

#endif
