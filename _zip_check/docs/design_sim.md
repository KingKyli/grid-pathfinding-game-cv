# GridWorld — Simulation & UI Design

Part B (Agents, Simulation & UI)

- `Agent`:
  - Holds id, current position, goal, current path.
  - `step()` moves one cell along path per tick.

- `Simulation`:
  - Loads map and agents config (`configs/*.txt` απλό demo format: `id sx sy gx gy`).
  - Main tick loop: request path (A*), step agents, apply simple local avoidance, render, log metrics.

- `AsciiRenderer`:
  - Draws the grid to stdout with '.' free, '#' obstacle, 'A' agent.

Metrics to log:
- average path length, collisions count, time to goal per agent.

Συμφωνία/"contract" σύνδεσης:
- `findPath` function in `grid/pathfinding/AStar.h` must be available to Part B.
- `Map` API as specified in `design_core.md`.
