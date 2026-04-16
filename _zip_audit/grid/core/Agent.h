#pragma once
#include "types.h"
#include <cstddef>
#include <optional>

namespace grid {

enum class AgentState {
    Idle,
    Moving,
    ReachedGoal,
    NoPath
};

class Agent {
public:
    Agent(int id, const Point& start, const Point& goal);
    int id() const { return id_; }
    const Point& position() const { return pos; }
    const Point& goalPoint() const { return goal; }
    void setGoalPoint(const Point& g);
    bool atGoal() const;
    void setPath(const Path& p);
    void clearPath();
    void setNoPath();
    void step(); // advance one tick 

    AgentState state() const { return state_; }
    bool goalDirty() const { return goal_dirty_; }
    bool consumeGoalDirty();

    const Path& currentPath() const { return path; }
    size_t currentPathIndex() const { return pathIndex; }

private:
    int id_;
    Point pos;
    Point goal;
    Path path;
    size_t pathIndex{0};
    AgentState state_{AgentState::Idle};
    bool goal_dirty_{true};
};

} 
