#pragma once
#include <algorithm>
#include <cstddef>

namespace page_navigation {
inline std::size_t move(std::size_t top, std::size_t total, std::size_t visible, int direction) {
    const std::size_t last = total > visible ? total - visible : 0;
    top = std::min(top, last);
    if (direction < 0 && top > 0) --top;
    if (direction > 0 && top < last) ++top;
    return top;
}
}
