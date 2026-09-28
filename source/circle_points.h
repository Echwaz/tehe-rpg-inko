#pragma once
#include <cmath>

struct CirclePoint { float x, y; };

// Titik lingkaran satuan, dihitung sekali saja.
inline const CirclePoint* circlePoints(int segments) {
    struct Tables {
        CirclePoint points[29][29] = {};
        Tables() {
            for (int n = 10; n <= 28; ++n)
                for (int i = 0; i <= n; ++i) {
                    const float angle = 6.2831853f * i / n;
                    points[n][i] = {std::cos(angle), std::sin(angle)};
                }
        }
    };
    static const Tables tables;
    return tables.points[segments];
}
