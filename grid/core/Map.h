#pragma once
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "types.h"

namespace grid {

class Map {
public:
    Map() = default;
    bool loadFromFile(const std::string& path); // Φορτώνει χάρτη από JSON.
    bool isFree(const Point& p) const;
    std::vector<Point> neighbors(const Point& p) const;
    int width() const { return w; }
    int height() const { return h; }

private:
    int w{0}, h{0};
    // 0 = ελεύθερο, 1 = εμπόδιο
    std::vector<int> cells;
    inline bool inBounds(const Point& p) const { return p.x>=0 && p.x<w && p.y>=0 && p.y<h; }
    inline int index(const Point& p) const { return p.y * w + p.x; }
};

} 
