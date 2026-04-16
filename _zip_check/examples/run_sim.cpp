#include <iostream>
#include "../grid/sim/Simulation.h"

int main(int argc, char** argv) {
    std::string mapPath = "maps/example.json";
    std::string cfgPath = "configs/example_agents.txt"; // απλό demo format
    if(argc >= 2) mapPath = argv[1];
    if(argc >= 3) cfgPath = argv[2];

    grid::Simulation sim;
    if(!sim.loadMap(mapPath)) { std::cerr << "Failed to load map: " << mapPath << "\n"; return 1; }
    if(!sim.loadAgentsConfig(cfgPath)) { std::cerr << "Failed to load agents config: " << cfgPath << "\n"; return 1; }
    sim.runAscii(200, 150);
    return 0;
}
