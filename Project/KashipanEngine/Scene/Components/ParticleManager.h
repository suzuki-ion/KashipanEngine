#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

#include "Scene/Components/ISceneComponent.h"
#include "Graphics/ScreenBuffer.h"
#include "Objects/Object3DBase.h"
#include "Objects/Object2DBase.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Objects/Components/3D/Material3D.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/2D/Material2D.h"
#include "Objects/GameObjects/3D/Plane3D.h"
#include "Objects/GameObjects/3D/Box.h"
#include "Objects/GameObjects/3D/Sphere.h"
#include "Objects/GameObjects/3D/Triangle3D.h"
#include "Objects/GameObjects/2D/Sprite.h"
#include "Objects/GameObjects/2D/Ellipse.h"
#include "Objects/GameObjects/2D/Triangle2D.h"
#include "Objects/GameObjects/2D/Rect.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/RandomValue.h"

namespace KashipanEngine {

class ParticleManager final : public ISceneComponent {
public:
    enum class ParticleShape2D {
        Sprite,
        Ellipse,
        Triangle,
        Rect
    };

    enum class ParticleShape3D {
        Plane,
        Box,
        Sphere,
        Triangle
    };

    struct SpawnBox {
        Vector3 min{ -5.0f, 0.0f, -5.0f };
        Vector3 max{ 5.0f, 5.0f, 5.0f };
    };

    struct ParticleGroupConfig {
        std::string name = "ParticleGroup";
        std::string pipelineName = "Object3D.Solid.BlendNormal";
        std::string textureName = "white1x1.png";
        std::string target = "3D";
        ParticleShape2D shape2D = ParticleShape2D::Sprite;
        ParticleShape3D shape3D = ParticleShape3D::Plane;
        std::size_t count = 16;
        SpawnBox spawnBox{};
        Vector3 spawnCenter{ 0.0f, 0.0f, 0.0f };
        float spawnRadius = 0.0f;
        bool useSpawnBox = true;
        float spawnIntervalSec = 0.0f;
        bool spawnOnStart = true;
        bool resetOnSpawn = true;
        bool loop = true;
        Vector3 gravity{ 0.0f, -9.8f, 0.0f };
        Vector3 initialVelocityMin{ -1.0f, -1.0f, -1.0f };
        Vector3 initialVelocityMax{ 1.0f, 1.0f, 1.0f };
        Vector3 initialRotationMin{ 0.0f, 0.0f, 0.0f };
        Vector3 initialRotationMax{ 0.0f, 0.0f, 0.0f };
        Vector3 rotationSpeed{ 0.0f, 0.0f, 0.0f };
        float speed = 2.0f;
        float lifeTimeSec = 2.0f;
        float lifeTimeRandomRange = 0.0f;
        float linearDamping = 0.0f;
        Vector3 baseScale{ 0.5f, 0.5f, 0.5f };
        float startScale = 0.0f;
        float peakScale = 1.0f;
        float endScale = 0.0f;
        Vector4 startColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        Vector4 endColor{ 1.0f, 1.0f, 1.0f, 1.0f };
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

    bool Spawn(const std::string& groupName, std::optional<Vector3> center = std::nullopt);
    bool SetEmitCenter(const std::string& groupName, const Vector3& center, bool respawnExisting = false);
    bool SetEmitting(const std::string& groupName, bool isEmitting);
    bool SetParentTransform(const std::string& groupName, Transform3D* parentTransform);

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
        float lifeTimeSec = 0.0f;
        bool active = true;
    };

    struct ParticleGroup {
        ParticleGroupConfig config;
        std::uint64_t batchKey = 0;
        std::vector<ParticleInstance> particles;
        std::size_t spawnedCount = 0;
        float spawnTimer = 0.0f;
        bool isEmitting = false;
        Vector3 emitCenter{ 0.0f, 0.0f, 0.0f };
        Transform3D* parentTransform = nullptr;
    };

    static std::uint64_t MakeRandomBatchKey();
    ScreenBuffer* ResolveTargetBuffer(const std::string& target) const;

    static JSON SerializeGroup(const ParticleGroup& group);
    static ParticleGroupConfig DeserializeGroupConfig(const JSON& json, const ParticleGroupConfig& fallback);

    void DestroyGroupObjects(ParticleGroup& group);
    void RespawnParticle(ParticleGroup& group, ParticleInstance& particle);
    void HideParticle(ParticleInstance& particle);
    void CreateParticleInstance(ParticleGroup& group);
    Vector3 MakeSpawnPosition(const ParticleGroup& group) const;
    std::unique_ptr<Object2DBase> CreateParticleObject2D(const ParticleGroup& group, std::size_t index) const;
    std::unique_ptr<Object3DBase> CreateParticleObject3D(const ParticleGroup& group, std::size_t index) const;

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
