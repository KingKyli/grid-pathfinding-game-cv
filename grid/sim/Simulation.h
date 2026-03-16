#pragma once
#include "../core/Map.h"
#include "../core/Agent.h"
#include <vector>
#include <string>

namespace grid {

class Simulation {
public:
    Simulation() = default;
    bool loadMap(const std::string& mapPath);
    bool loadAgentsConfig(const std::string& cfgPath);
    void runAscii(int ticks = 200, int delayMs = 100);

private:
    Map map;
    std::vector<Agent> agents;
};

}
