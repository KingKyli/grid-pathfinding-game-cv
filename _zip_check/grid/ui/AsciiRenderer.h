#pragma once
#include "../core/Map.h"
#include "../core/Agent.h"
#include <vector>

namespace grid {

class AsciiRenderer {
public:
    static void draw(const Map& map, const std::vector<Agent>& agents);
};

} 