#pragma once
#include <optional>
#include "../core/types.h"

namespace grid {

class Map;

std::optional<Path> findPath(const Map& map, const Point& start, const Point& goal);

} 
 