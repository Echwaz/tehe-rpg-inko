#include "../source/page_navigation.h"
#include <cassert>
#include <cstdio>
#include <limits>

int main() {
    using page_navigation::move;
    for (std::size_t total = 0; total < 20; ++total) {
        std::size_t top = 0;
        const std::size_t last = total > 3 ? total - 3 : 0;
        for (int i = 0; i < 100; ++i) top = move(top, total, 3, 1);
        assert(top == last);
        for (int i = 0; i < 100; ++i) top = move(top, total, 3, -1);
        assert(top == 0);
    }
    assert(move(std::numeric_limits<std::size_t>::max(), 5, 3, 1) == 2);
    assert(move(4, 2, 3, 0) == 0);
    assert(move(0, 3, 1, -1) == 0);
    assert(move(0, 3, 1, 1) == 1);
    assert(move(2, 3, 1, 1) == 2);
    std::puts("page navigation OK");
}
