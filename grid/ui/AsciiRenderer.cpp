#include "AsciiRenderer.h"
#include <iostream>

namespace grid {

void AsciiRenderer::draw(const Map& map, const std::vector<Agent>& agents) {
    int w = map.width();
    int h = map.height();
    for(int y=0;y<h;++y) {
        for(int x=0;x<w;++x) {
            Point p{x,y};
            bool drawn=false;
            for(const auto &a : agents) {
                if(a.position() == p) { std::cout << 'A'; drawn=true; break; }
            }
            if(!drawn) std::cout << (map.isFree(p) ? '.' : '#');
        }
        std::cout << '\n';
    }
}

} 
