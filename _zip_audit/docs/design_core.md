# GridWorld — Core Design

Part A (Core & Pathfinding)

- `Map`:
  - Loads `maps/*.json` with `width`, `height`, and `grid` array (strings of '.' and '#').
  - API: `isFree(Point)`, `neighbors(Point)`, `width()`, `height()`.

- `AStar`:
  - `std::optional<Path> findPath(const Map&, Point start, Point goal)`.
  - Uses Manhattan heuristic, 4-way movement.

- Types: `Point {int x,y}`, `Path = vector<Point>`.

Unit tests (έλεγχοι):
- Pathfinding tests for simple scenarios.

Σημειώσεις:
- Για το demo υπάρχει ένα πολύ απλό ("χειροποίητο") parsing για `maps/*.json`. Αν χρειαστεί να επεκταθεί, αξίζει να μπει κανονική JSON βιβλιοθήκη.
