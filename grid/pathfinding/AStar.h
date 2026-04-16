#pragma once
#include <functional>
#include <optional>
#include <string>
#include "../core/types.h"

namespace grid {

class Map;

using VisitCallback = std::function<void(const Point&, const std::string&)>;

std::optional<Path> findPath(
	const Map& map,
	const Point& start,
	const Point& goal,
	VisitCallback cb = nullptr
);

} 
 