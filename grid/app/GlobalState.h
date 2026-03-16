#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include "../core/Map.h"
#include "Entity.h"

namespace grid {

struct GlobalState {
    Map map;

    // ticks προσομοίωσης
    int tick_delay_ms = 150;
    float accumulator_ms = 0.0f;

    // Βασικά χειριστήρια
    bool paused = false;
    bool step_once = false;

    // Επιλογή από UI
    int selected_agent_id = -1;

    // Προαιρετικό autopilot: αν είναι id κάποιου agent, αυτός διαλέγει targets μόνος του.
    int autopilot_agent_id = -1;

    // Στόχοι / score (απλά counters)
    int targets_total = 0;
    int targets_collected = 0;

    // Χρόνος match
    int match_duration_sec = 120;
    float match_time_left_ms = 120000.0f;
    bool match_started = false;
    bool match_over = false;

    // Τοπικός έλεγχος 2 παικτών (keyboard). Γεμίζει στην αρχή από τους πρώτους 2 agents
    int player1_agent_id = -1; // WASD
    int player2_agent_id = -1; // Arrow keys

    // Προαιρετικός CPU-controlled agent (autopilot), συνήθως ο 3ος agent.
    int cpu_agent_id = -1;

    // Δυσκολία CPU (επηρεάζει μόνο τη συμπεριφορά του cpu_agent_id)
    // 0 = Easy, 1 = Normal, 2 = Hard
    int cpu_difficulty = 1;

    // Πολυμορφικά entities (dynamic memory μέσω new/make_unique)
    std::vector<std::unique_ptr<Entity>> entities;

   
    // Τα κρατάμε απλά: απλώς καλούν/προωθούν την ενέργεια στα entities.
    void init() {
        accumulator_ms = 0.0f;
    }

    // Κάνει update σε όλα τα live entities και πετάει όσα "λήγουν"
    void update(float dt_ms) {
        for (auto it = entities.begin(); it != entities.end();) {
            if (!(*it)->update(*this, dt_ms)) {
                it = entities.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Κάνει draw σε όλα τα entities
    void draw() const {
        for (const auto& e : entities) {
            e->draw(*this);
        }
    }
};

} 
