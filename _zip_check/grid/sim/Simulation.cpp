#include "Simulation.h"
#include "../pathfinding/AStar.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <fstream>

namespace grid {

bool Simulation::loadMap(const std::string& mapPath) {
    return map.loadFromFile(mapPath);
}

bool Simulation::loadAgentsConfig(const std::string& cfgPath) {
    std::ifstream in(cfgPath);
    if(!in) return false;
    // Απλός/πρόχειρος parser: περιμένει γραμμές `id x y gx gy`.
    int id, sx, sy, gx, gy;
    agents.clear();
    while(in >> id >> sx >> sy >> gx >> gy) {
        agents.emplace_back(id, Point{sx, sy}, Point{gx, gy});
    }
    return true;
}

void Simulation::runAscii(int ticks, int delayMs) {
    for(int t=0;t<ticks;++t) {
        // Ζήτα νέο path αν χρειάζεται.
        for(auto &a : agents) {
            if(a.atGoal()) continue;
            const auto& cur = a.currentPath();
            const bool no_remaining_path = cur.empty() || a.currentPathIndex() >= cur.size();
            if (a.consumeGoalDirty() || no_remaining_path) {
                auto p = findPath(map, a.position(), a.goalPoint());
                if (p) {
                    a.setPath(*p);
                } else {
                    a.setNoPath();
                }
            }
        }
        // Κάνε ένα βήμα τους agents.
        for(auto &a : agents) {
            a.step();
        }
        // Απόδοση  στο ASCII.
        int w = map.width();
        int h = map.height();
        for(int y=0;y<h;++y) {
            for(int x=0;x<w;++x) {
                Point p{x,y};
                bool drawn=false;
                for(const auto &a : agents) {
                    if(a.position() == p) { std::cout << 'A'; drawn=true; break; }
                }
                if(!drawn) {
                    // Πρόχειρος έλεγχος κελιού μέσω isFree.
                    std::cout << (map.isFree(p) ? '.' : '#');
                }
            }
            std::cout << '\n';
        }
        std::cout << "--- tick " << t << " ---\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
}

} 
