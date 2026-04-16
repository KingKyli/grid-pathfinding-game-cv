#pragma once
#include <vector>

namespace grid {

struct Point {
    int x{}, y{};
    bool operator==(const Point& o) const { return x==o.x && y==o.y; }
};

using Path = std::vector<Point>;

}
