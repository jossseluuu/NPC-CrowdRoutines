#pragma once

#include "core/types.hpp"
#include <memory>
#include <vector>

namespace npc_crowd::storage {

// Reference implementation: one separately allocated object per citizen.
struct OopAgentStore {
    void reserve(std::size_t count) { agents.reserve(count); }
    void append(const Agent& agent) { agents.push_back(std::make_unique<Agent>(agent)); }
    std::size_t size() const { return agents.size(); }

    std::vector<std::unique_ptr<Agent>> agents;
};

} // namespace npc_crowd::storage
