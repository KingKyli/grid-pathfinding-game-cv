#pragma once

namespace grid {

struct GlobalState;

class Entity {
public:
    virtual ~Entity() = default;

    // Επιστρέφει false όταν το entity πρέπει να καταστραφεί 
    virtual bool update(GlobalState& state, float dt_ms) = 0;
    virtual void draw(const GlobalState& state) const = 0;
};

} 
