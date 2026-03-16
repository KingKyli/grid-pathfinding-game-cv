#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "../grid/core/Map.h"
#include "../grid/pathfinding/AStar.h"

namespace {

std::filesystem::path writeTempMapJson(const std::string& json) {
    const auto dir = std::filesystem::temp_directory_path();
    const auto file = dir / "gridworld_test_map.json";

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << json;
    out.close();

    return file;
}

void assertAdjacent4(const grid::Point& a, const grid::Point& b) {
    const int dx = std::abs(a.x - b.x);
    const int dy = std::abs(a.y - b.y);
    assert((dx + dy) == 1);
}

} // namespace

int main() {
    using grid::Point;

  // 1) Βασικό path σε άδειο grid.
    {
        const std::string json = R"JSON({
  "width": 5,
  "height": 5,
  "grid": [
    ".....",
    ".....",
    ".....",
    ".....",
    "....."
  ]
})JSON";

        grid::Map map;
        const auto pathFile = writeTempMapJson(json);
        assert(map.loadFromFile(pathFile.string()));

        const Point start{0, 0};
        const Point goal{4, 4};
        const auto pathOpt = grid::findPath(map, start, goal);
        assert(pathOpt.has_value());

        const auto& path = *pathOpt;
        assert(!path.empty());
        assert(path.front() == start);
        assert(path.back() == goal);

        for (size_t i = 0; i < path.size(); ++i) {
            assert(map.isFree(path[i]));
            if (i > 0) assertAdjacent4(path[i - 1], path[i]);
        }
    }

    // 2) start == goal επιστρέφει path με 1 node.
    {
        const std::string json = R"JSON({
  "width": 3,
  "height": 3,
  "grid": [
    "...",
    "...",
    "..."
  ]
})JSON";

        grid::Map map;
        const auto pathFile = writeTempMapJson(json);
        assert(map.loadFromFile(pathFile.string()));

        const Point start{1, 1};
        const Point goal{1, 1};
        const auto pathOpt = grid::findPath(map, start, goal);
        assert(pathOpt.has_value());
        assert(pathOpt->size() == 1);
        assert(pathOpt->front() == start);
    }

    // 3) Εμπόδια: το path δεν πρέπει να πατάει πάνω σε '#'.
    {
        const std::string json = R"JSON({
  "width": 5,
  "height": 5,
  "grid": [
    ".....",
    ".###.",
    ".....",
    ".....",
    "....."
  ]
})JSON";

        grid::Map map;
        const auto pathFile = writeTempMapJson(json);
        assert(map.loadFromFile(pathFile.string()));

        const Point start{0, 0};
        const Point goal{4, 4};
        const auto pathOpt = grid::findPath(map, start, goal);
        assert(pathOpt.has_value());

        for (const auto& p : *pathOpt) {
            assert(map.isFree(p));
        }
    }

    // 4) Αν start ή goal είναι blocked => δεν υπάρχει path.
    {
        const std::string json = R"JSON({
  "width": 3,
  "height": 3,
  "grid": [
    "#..",
    "...",
    "..#"
  ]
})JSON";

        grid::Map map;
        const auto pathFile = writeTempMapJson(json);
        assert(map.loadFromFile(pathFile.string()));

        assert(!grid::findPath(map, Point{0, 0}, Point{1, 1}).has_value());
        assert(!grid::findPath(map, Point{1, 1}, Point{2, 2}).has_value());
    }

    std::cout << "pathfinding_test passed\n";
    return 0;
}
