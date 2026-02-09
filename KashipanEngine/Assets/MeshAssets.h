#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Math/Vector3.h"

namespace KashipanEngine {

class GameEngine;

class MeshAssets {
public:
    struct MeshHandle {
        std::size_t id = 0;
        bool IsValid() const { return id != 0; }
        explicit operator bool() const { return IsValid(); }
        bool operator==(const MeshHandle &o) const { return id == o.id; }
        bool operator!=(const MeshHandle &o) const { return id != o.id; }
    };

    MeshAssets(Passkey<GameEngine>) {}
    ~MeshAssets() = default;

    MeshHandle AddMesh(const std::vector<Vector3> &vertices, const std::vector<std::uint32_t> &indices) {
        meshes_.push_back({ vertices, indices });
        return MeshHandle{ meshes_.size() };
    }

    bool IsValid(MeshHandle handle) const {
        return handle.id > 0 && handle.id <= meshes_.size();
    }

private:
    struct MeshData {
        std::vector<Vector3> vertices;
        std::vector<std::uint32_t> indices;
    };

    const MeshData *GetMeshData(MeshHandle handle) const {
        if (!IsValid(handle)) return nullptr;
        return &meshes_[handle.id - 1];
    }

    std::vector<MeshData> meshes_;

    friend class Renderer;
};

using MeshHandle = MeshAssets::MeshHandle;

} // namespace KashipanEngine
