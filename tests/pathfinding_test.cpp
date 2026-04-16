#include <cassert>
#ifdef USE_CATCH2
#include <catch2/catch_test_macros.hpp>
#else
#include <cassert>
#include <iostream>
// Minimal fallback shims so the file compiles without Catch2.
#define TEST_CASE(name, tags) static void _test_##__LINE__(); \
  struct _reg_##__LINE__ { _reg_##__LINE__() { _test_##__LINE__(); } } _inst_##__LINE__; \
  static void _test_##__LINE__()
#define REQUIRE(expr) assert(expr)
#define REQUIRE_FALSE(expr) assert(!(expr))
#define CHECK(expr) assert(expr)
int main() { std::cout << "pathfinding_test passed\n"; }
#endif

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "../grid/core/Map.h"
#include "../grid/pathfinding/AStar.h"

// ─── helpers ────────────────────────────────────────────────────────────────
namespace {

grid::Map mapFromJson(const std::string& json) {
  const auto file = std::filesystem::temp_directory_path() / "gw_test.json";
  { std::ofstream f(file, std::ios::trunc); f << json; }
  grid::Map m;
  m.loadFromFile(file.string());
  return m;
}

bool adjacent4(const grid::Point& a, const grid::Point& b) {
  return (std::abs(a.x - b.x) + std::abs(a.y - b.y)) == 1;
}

} // namespace

// ─── test cases ─────────────────────────────────────────────────────────────

TEST_CASE("A* finds path in open 5x5 grid", "[astar]") {
  auto map = mapFromJson(R"({"width":5,"height":5,"grid":[".....",".....",".....",".....","....."]})" );
  auto result = grid::findPath(map, {0,0}, {4,4});
  REQUIRE(result.has_value());
  REQUIRE(result->front() == grid::Point{0,0});
  REQUIRE(result->back()  == grid::Point{4,4});
  for (size_t i = 1; i < result->size(); ++i) {
    REQUIRE(adjacent4((*result)[i-1], (*result)[i]));
    REQUIRE(map.isFree((*result)[i]));
  }
}

TEST_CASE("A* returns single-node path when start == goal", "[astar]") {
  auto map = mapFromJson(R"({"width":3,"height":3,"grid":["...","...","..."]})" );
  auto result = grid::findPath(map, {1,1}, {1,1});
  REQUIRE(result.has_value());
  REQUIRE(result->size() == 1);
  REQUIRE(result->front() == grid::Point{1,1});
}

TEST_CASE("A* path avoids walls", "[astar]") {
  // Row 1 has a horizontal wall: .###.
  auto map = mapFromJson(R"({"width":5,"height":5,"grid":[".....",".###.",".....",".....","....."]})" );
  auto result = grid::findPath(map, {0,0}, {4,4});
  REQUIRE(result.has_value());
  for (const auto& p : *result) {
    REQUIRE(map.isFree(p));
  }
}

TEST_CASE("A* returns nullopt when start is blocked", "[astar]") {
  auto map = mapFromJson(R"({"width":3,"height":3,"grid":["#..","...","..."]})" );
  REQUIRE_FALSE(grid::findPath(map, {0,0}, {2,2}).has_value());
}

TEST_CASE("A* returns nullopt when goal is blocked", "[astar]") {
  auto map = mapFromJson(R"({"width":3,"height":3,"grid":["...","...","..#"]})" );
  REQUIRE_FALSE(grid::findPath(map, {0,0}, {2,2}).has_value());
}

TEST_CASE("A* returns nullopt when target is fully enclosed", "[astar]") {
  // Goal at (2,2) is surrounded by walls on all sides.
  auto map = mapFromJson(R"({"width":5,"height":5,"grid":[".....",".###.",".#.#.",".###.","....."]})" );
  REQUIRE_FALSE(grid::findPath(map, {0,0}, {2,2}).has_value());
}

TEST_CASE("A* visit callback counts closed nodes", "[astar][callback]") {
  auto map = mapFromJson(R"({"width":5,"height":5,"grid":[".....",".....",".....",".....","....."]})" );
  int closed = 0;
  grid::findPath(map, {0,0}, {4,4}, [&](const grid::Point&, const std::string& phase) {
    if (phase == "closed") ++closed;
  });
  REQUIRE(closed > 0);
}

TEST_CASE("Map::setCell toggles walkability", "[map][editor]") {
  auto map = mapFromJson(R"({"width":3,"height":3,"grid":["...","...","..."]})" );
  const grid::Point p{1,1};
  REQUIRE(map.isFree(p));
  map.setCell(p, 1);   // place wall
  REQUIRE_FALSE(map.isFree(p));
  map.setCell(p, 0);   // erase wall
  REQUIRE(map.isFree(p));
}

TEST_CASE("Map::setCell rejects out-of-bounds coords", "[map][editor]") {
  auto map = mapFromJson(R"({"width":3,"height":3,"grid":["...","...","..."]})" );
  REQUIRE_FALSE(map.setCell({-1, 0}, 1));
  REQUIRE_FALSE(map.setCell({3,  0}, 1));
  REQUIRE_FALSE(map.setCell({0, -1}, 1));
  REQUIRE_FALSE(map.setCell({0,  3}, 1));
}
