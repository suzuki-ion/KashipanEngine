#pragma once
#include <cstddef>

namespace KashipanEngine {

struct Entity {
    size_t id;
    uint32_t generation;
    bool operator==(const Entity &other) const {
        return id == other.id && generation == other.generation;
    }
    bool operator!=(const Entity &other) const {
        return id != other.id || generation != other.generation;
    }
};

} // namespace KashipanEngine