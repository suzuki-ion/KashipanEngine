#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "Scene/Components/ISceneComponent.h"
#include "Graphics/ScreenBuffer.h"
#include "Objects/Object3DBase.h"
#include "Objects/Object2DBase.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Objects/Components/3D/Material3D.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/2D/Material2D.h"
#include "Objects/GameObjects/3D/Plane3D.h"
#include "Objects/GameObjects/2D/Sprite.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/RandomValue.h"

namespace KashipanEngine {

class ParticleManager final : public ISceneComponent {
public:
    struct SpawnBox {
        Vector3 min{ -5.0f, 0.0f, -5.0f };
        Vector3 max{ 5.0f, 5.0f, 5.0f };
    };

    struct ParticleGroupConfig {
        std::string name = "ParticleGroup";
        std::string pipelineName = "Object3D.Solid.BlendNormal";
        std::string textureName = "white1x1.png";
        std::string target = "3D";
        std::size_t count = 16;
        SpawnBox spawnBox{};
        float speed = 2.0f;
        float lifeTimeSec = 2.0f;
        Vector3 baseScale{ 0.5f, 0.5f, 0.5f };
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    ParticleManager()
        : ISceneComponent("ParticleManager", 1) {}

    ParticleManager(ScreenBuffer* screenBuffer2D, ScreenBuffer* screenBuffer3D = nullptr)
        : ISceneComponent("ParticleManager", 1)
        , screenBuffer2D_(screenBuffer2D)
        , screenBuffer3D_(screenBuffer3D) {}

    ~ParticleManager() override = default;

    void Initialize() override;
    void Finalize() override;
    void Update() override;

    void SetScreenBuffer2D(ScreenBuffer* buffer) { screenBuffer2D_ = buffer; }
    void SetScreenBuffer3D(ScreenBuffer* buffer) { screenBuffer3D_ = buffer; }

    ScreenBuffer* GetScreenBuffer2D() const { return screenBuffer2D_; }
    ScreenBuffer* GetScreenBuffer3D() const { return screenBuffer3D_; }

    bool AddGroup(const ParticleGroupConfig& config);
    bool RemoveGroup(std::size_t index);
    void ClearGroups();

    bool LoadFromJsonFile(const std::string& filepath);
    bool SaveToJsonFile(const std::string& filepath) const;

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    enum class ParticleKind {
        D3D,
        D2D
    };

    struct ParticleInstance {
        ParticleKind kind = ParticleKind::D3D;
        Object3DBase* object3D = nullptr;
        Object2DBase* object2D = nullptr;
        Vector3 velocity{ 0.0f, 1.0f, 0.0f };
        float elapsed = 0.0f;
    };

    struct ParticleGroup {
        ParticleGroupConfig config;
        std::uint64_t batchKey = 0;
        std::vector<ParticleInstance> particles;
    };

    static std::uint64_t MakeRandomBatchKey();
    ScreenBuffer* ResolveTargetBuffer(const std::string& target) const;

    static JSON SerializeGroup(const ParticleGroup& group);
    static ParticleGroupConfig DeserializeGroupConfig(const JSON& json, const ParticleGroupConfig& fallback);

    void DestroyGroupObjects(ParticleGroup& group);
    void RespawnParticle(ParticleGroup& group, ParticleInstance& particle);

    ScreenBuffer* screenBuffer2D_ = nullptr;
    ScreenBuffer* screenBuffer3D_ = nullptr;

    std::vector<ParticleGroup> groups_;

#if defined(USE_IMGUI)
    ParticleGroupConfig newGroupConfig_{};
    std::array<char, 128> nameBuffer_{};
    std::array<char, 128> pipelineBuffer_{};
    std::array<char, 128> textureBuffer_{};
    std::array<char, 260> jsonPathBuffer_{};
#endif
};

} // namespace KashipanEngine
