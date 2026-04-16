#include "AStar.h"
#include "../core/Map.h"
#include <algorithm>
#include <cstdlib>
#include <queue>
#include <unordered_map>
#include <functional>

namespace grid {

static int manhattan(const Point& a, const Point& b) {
    return abs(a.x-b.x) + abs(a.y-b.y);
}

std::optional<Path> findPath(const Map& map, const Point& start, const Point& goal, VisitCallback cb) {
    if(!map.isFree(start) || !map.isFree(goal)) return std::nullopt;

    struct Node {
        Point p;
        int f,g;
    };

    auto cmp = [](const Node& a, const Node& b){ return a.f > b.f; };
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> open(cmp);

    std::unordered_map<int, int> gscore; // key = y*W + x, value = g
    std::unordered_map<int, Point> cameFrom;

    auto key = [&](const Point& p)->int{ return p.y * map.width() + p.x; };

    open.push({start, manhattan(start, goal), 0});
    if (cb) cb(start, "open");
    gscore[key(start)] = 0;

    while(!open.empty()) {
        Node cur = open.top(); open.pop();
        if (cb) cb(cur.p, "closed");
        if(cur.p == goal) {
            // Ανακατασκευή μονοπατιού.
            Path path;
            Point p = cur.p;
            while(!(p == start)) {
                path.push_back(p);
                p = cameFrom[key(p)];
            }
            path.push_back(start);
            std::reverse(path.begin(), path.end());
            if (cb) {
                for (const auto& step : path) {
                    cb(step, "path");
                }
            }
            return path;
        }
        for(const Point& n : map.neighbors(cur.p)) {
            int tentative_g = cur.g + 1;
            int nk = key(n);
            if(!gscore.count(nk) || tentative_g < gscore[nk]) {
                gscore[nk] = tentative_g;
                int f = tentative_g + manhattan(n, goal);
                cameFrom[nk] = cur.p;
                open.push({n, f, tentative_g});
                if (cb) cb(n, "open");
            }
        }
    }

    return std::nullopt;
}

}
