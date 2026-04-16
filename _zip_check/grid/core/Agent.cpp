#include "Agent.h"

namespace grid {

Agent::Agent(int id, const Point& start, const Point& goal)
: id_(id), pos(start), goal(goal) {
    goal_dirty_ = true;
    state_ = (pos == goal) ? AgentState::ReachedGoal : AgentState::Moving;
}

void Agent::setGoalPoint(const Point& g) {
    if (g == goal) return;
    goal = g;
    goal_dirty_ = true;
    clearPath();
    state_ = (pos == goal) ? AgentState::ReachedGoal : AgentState::Moving;
}

bool Agent::atGoal() const {
    return pos == goal;
}

bool Agent::consumeGoalDirty() {
    const bool was = goal_dirty_;
    goal_dirty_ = false;
    return was;
}

void Agent::clearPath() {
    path.clear();
    pathIndex = 0;
}

void Agent::setNoPath() {
    clearPath();
    goal_dirty_ = false;
    state_ = AgentState::NoPath;
}

void Agent::setPath(const Path& p) {
    path = p;
    pathIndex = 0;
    goal_dirty_ = false;

    if (path.empty()) {
        state_ = atGoal() ? AgentState::ReachedGoal : AgentState::Idle;
        return;
    }

    state_ = AgentState::Moving;
    // Το μονοπάτι του A* περιλαμβάνει και τη θέση εκκίνησης ως πρώτο node.
    // Αν δίνουμε νέο path σε κάθε tick, ξεκινώντας από index 0 θα ξανα-εφαρμόζαμε
    // η τρέχουσα θέση και ο agent θα έμοιαζε κολλημενος
    if (!path.empty() && path.front() == pos && path.size() >= 2) {
        pathIndex = 1;
    }
}

void Agent::step() {
    if (atGoal()) {
        state_ = AgentState::ReachedGoal;
        clearPath();
        return;
    }

    if (path.empty() || pathIndex >= path.size()) {
        if (state_ != AgentState::NoPath) {
            state_ = AgentState::Idle;
        }
        return;
    }
    // Κινήσου ένα βήμα πάνω στο path.
    pos = path[pathIndex];
    ++pathIndex;

    if (atGoal()) {
        state_ = AgentState::ReachedGoal;
        clearPath();
    } else {
        state_ = AgentState::Moving;
    }
}

} 
