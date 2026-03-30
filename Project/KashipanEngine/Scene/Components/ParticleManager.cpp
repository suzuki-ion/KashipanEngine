#include "Scene/Components/ParticleManager.h"

#include "Scene/SceneContext.h"
#include "Scene/Components/SceneDefaultVariables.h"
#include "Assets/TextureManager.h"
#include "Utilities/MathUtils/Easings.h"
#include "Utilities/TimeUtils.h"
#include "Utilities/Translation.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

#include <algorithm>
#include <random>
#include <cstdio>

namespace KashipanEngine {

namespace {
Vector3 NormalizeSafe(const Vector3& v) {
    const float len = v.Length();
    if (len <= 0.0001f) return Vector3{ 0.0f, 1.0f, 0.0f };
    return v / len;
}

Vector3 ReadVector3OrDefault(const JSON& json, const Vector3& fallback) {
    if (json.is_array() && json.size() >= 3) {
        return Vector3{
            json[0].get<float>(),
            json[1].get<float>(),
            json[2].get<float>()
        };
    }
    if (json.is_object()) {
        return Vector3{
            json.value("x", fallback.x),
            json.value("y", fallback.y),
            json.value("z", fallback.z)
        };
    }
    return fallback;
}

Vector4 ReadVector4OrDefault(const JSON& json, const Vector4& fallback) {
    if (json.is_array() && json.size() >= 4) {
        return Vector4{
            json[0].get<float>(),
            json[1].get<float>(),
            json[2].get<float>(),
            json[3].get<float>()
        };
    }
    if (json.is_object()) {
        return Vector4{
            json.value("x", fallback.x),
            json.value("y", fallback.y),
            json.value("z", fallback.z),
            json.value("w", fallback.w)
        };
    }
    return fallback;
}

Vector3 RandomVector3(const Vector3& min, const Vector3& max) {
    return Vector3{
        GetRandomFloat(min.x, max.x),
        GetRandomFloat(min.y, max.y),
        GetRandomFloat(min.z, max.z)
    };
}

const char* ToString(ParticleManager::ParticleShape2D shape) {
    switch (shape) {
    case ParticleManager::ParticleShape2D::Ellipse:
        return "Ellipse";
    case ParticleManager::ParticleShape2D::Triangle:
        return "Triangle";
    case ParticleManager::ParticleShape2D::Rect:
        return "Rect";
    case ParticleManager::ParticleShape2D::Sprite:
    default:
        return "Sprite";
    }
}

const char* ToString(ParticleManager::ParticleShape3D shape) {
    switch (shape) {
    case ParticleManager::ParticleShape3D::Box:
        return "Box";
    case ParticleManager::ParticleShape3D::Sphere:
        return "Sphere";
    case ParticleManager::ParticleShape3D::Triangle:
        return "Triangle";
    case ParticleManager::ParticleShape3D::Plane:
    default:
        return "Plane";
    }
}

ParticleManager::ParticleShape2D ParseShape2D(const std::string& value, ParticleManager::ParticleShape2D fallback) {
    if (value == "Ellipse") return ParticleManager::ParticleShape2D::Ellipse;
    if (value == "Triangle") return ParticleManager::ParticleShape2D::Triangle;
    if (value == "Rect") return ParticleManager::ParticleShape2D::Rect;
    if (value == "Sprite") return ParticleManager::ParticleShape2D::Sprite;
    return fallback;
}

ParticleManager::ParticleShape3D ParseShape3D(const std::string& value, ParticleManager::ParticleShape3D fallback) {
    if (value == "Box") return ParticleManager::ParticleShape3D::Box;
    if (value == "Sphere") return ParticleManager::ParticleShape3D::Sphere;
    if (value == "Triangle") return ParticleManager::ParticleShape3D::Triangle;
    if (value == "Plane") return ParticleManager::ParticleShape3D::Plane;
    return fallback;
}
} // namespace

std::uint64_t ParticleManager::MakeRandomBatchKey() {
    static thread_local std::mt19937_64 rng{ std::random_device{}() };
    std::uint64_t v = 0;
    do {
        v = rng();
    } while (v == 0);
    return v;
}

void ParticleManager::Initialize() {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    auto* defaults = ctx->GetComponent<SceneDefaultVariables>();
    if (defaults) {
        if (!screenBuffer2D_) {
            screenBuffer2D_ = defaults->GetScreenBuffer2D();
        }
        if (!screenBuffer3D_) {
            screenBuffer3D_ = defaults->GetScreenBuffer3D();
        }
    }

#if defined(USE_IMGUI)
    std::snprintf(nameBuffer_.data(), nameBuffer_.size(), "%s", newGroupConfig_.name.c_str());
    std::snprintf(pipelineBuffer_.data(), pipelineBuffer_.size(), "%s", newGroupConfig_.pipelineName.c_str());
    std::snprintf(textureBuffer_.data(), textureBuffer_.size(), "%s", newGroupConfig_.textureName.c_str());
#endif

    for (auto& group : groups_) {
        group.emitCenter = group.config.spawnCenter;
        group.isEmitting = group.config.spawnOnStart;
    }
}

void ParticleManager::Finalize() {
    ClearGroups();
}

void ParticleManager::Update() {
    const float dt = std::max(0.0f, GetDeltaTime());
    if (dt <= 0.0f) return;

    for (auto& group : groups_) {
        auto& cfg = group.config;
        if (cfg.lifeTimeSec <= 0.0f) continue;

        if (group.isEmitting && group.spawnedCount < cfg.count) {
            if (cfg.spawnIntervalSec <= 0.0f) {
                while (group.spawnedCount < cfg.count) {
                    CreateParticleInstance(group);
                }
            } else {
                group.spawnTimer += dt;
                while (group.spawnTimer >= cfg.spawnIntervalSec && group.spawnedCount < cfg.count) {
                    group.spawnTimer -= cfg.spawnIntervalSec;
                    CreateParticleInstance(group);
                }
            }
        }

        for (auto& particle : group.particles) {
            if (!particle.active) continue;

            particle.elapsed += dt;

            const Vector3 gravity = cfg.gravity;
            particle.velocity += gravity * dt;
            if (cfg.linearDamping > 0.0f) {
                const float damp = std::max(0.0f, 1.0f - cfg.linearDamping * dt);
                particle.velocity = particle.velocity * damp;
            }

            const float tNorm = (particle.lifeTimeSec > 0.0f) ? std::clamp(particle.elapsed / particle.lifeTimeSec, 0.0f, 1.0f) : 1.0f;
            const float scaleFactor = (tNorm < 0.5f)
                ? EaseOutCubic(cfg.startScale, cfg.peakScale, tNorm * 2.0f)
                : EaseInCubic(cfg.peakScale, cfg.endScale, (tNorm - 0.5f) * 2.0f);
            const Vector4 color = Vector4::Lerp(cfg.startColor, cfg.endColor, tNorm);

            if (particle.kind == ParticleKind::D3D) {
                if (!particle.object3D) continue;
                auto* tr = particle.object3D->GetComponent3D<Transform3D>();
                if (!tr) continue;

                const Vector3 pos = tr->GetTranslate();
                tr->SetTranslate(pos + particle.velocity * (cfg.speed * dt));
                tr->SetScale(cfg.baseScale * scaleFactor);
                tr->SetRotate(tr->GetRotate() + cfg.rotationSpeed * dt);

                if (auto* mat = particle.object3D->GetComponent3D<Material3D>()) {
                    mat->SetColor(color);
                }
            } else {
                if (!particle.object2D) continue;
                auto* tr = particle.object2D->GetComponent2D<Transform2D>();
                if (!tr) continue;

                const Vector3 pos = tr->GetTranslate();
                tr->SetTranslate(pos + particle.velocity * (cfg.speed * dt));
                tr->SetScale(cfg.baseScale * scaleFactor);
                tr->SetRotate(tr->GetRotate() + cfg.rotationSpeed * dt);

                if (auto* mat = particle.object2D->GetComponent2D<Material2D>()) {
                    mat->SetColor(color);
                }
            }

            if (particle.elapsed >= particle.lifeTimeSec) {
                if (cfg.loop) {
                    RespawnParticle(group, particle);
                } else {
                    particle.active = false;
                    HideParticle(particle);
                }
            }
        }
    }
}

ScreenBuffer* ParticleManager::ResolveTargetBuffer(const std::string& target) const {
    if (target == "2D") {
        return screenBuffer2D_ ? screenBuffer2D_ : screenBuffer3D_;
    }
    return screenBuffer3D_ ? screenBuffer3D_ : screenBuffer2D_;
}

Vector3 ParticleManager::MakeSpawnPosition(const ParticleGroup& group) const {
    const auto& cfg = group.config;
    if (cfg.useSpawnBox) {
        const Vector3 min = group.emitCenter + cfg.spawnBox.min;
        const Vector3 max = group.emitCenter + cfg.spawnBox.max;
        return RandomVector3(min, max);
    }

    if (cfg.spawnRadius > 0.0f) {
        Vector3 dir = NormalizeSafe(RandomVector3(Vector3{ -1.0f, -1.0f, -1.0f }, Vector3{ 1.0f, 1.0f, 1.0f }));
        const float r = GetRandomFloat(0.0f, cfg.spawnRadius);
        return group.emitCenter + dir * r;
    }

    return group.emitCenter;
}

void ParticleManager::HideParticle(ParticleInstance& particle) {
    if (particle.kind == ParticleKind::D3D) {
        if (!particle.object3D) return;
        if (auto* tr = particle.object3D->GetComponent3D<Transform3D>()) {
            tr->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
        }
    } else {
        if (!particle.object2D) return;
        if (auto* tr = particle.object2D->GetComponent2D<Transform2D>()) {
            tr->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
        }
    }
}

void ParticleManager::RespawnParticle(ParticleGroup& group, ParticleInstance& particle) {
    const auto& cfg = group.config;
    const Vector3 pos = MakeSpawnPosition(group);
    const Vector3 rotation = RandomVector3(cfg.initialRotationMin, cfg.initialRotationMax);

    if (particle.kind == ParticleKind::D3D) {
        if (!particle.object3D) return;
        auto* tr = particle.object3D->GetComponent3D<Transform3D>();
        if (!tr) return;
        tr->SetTranslate(pos);
        tr->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
        tr->SetRotate(rotation);
    } else {
        if (!particle.object2D) return;
        auto* tr = particle.object2D->GetComponent2D<Transform2D>();
        if (!tr) return;
        tr->SetTranslate(pos);
        tr->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
        tr->SetRotate(rotation);
    }

    particle.velocity = NormalizeSafe(RandomVector3(cfg.initialVelocityMin, cfg.initialVelocityMax));
    particle.elapsed = 0.0f;
    particle.lifeTimeSec = std::max(0.01f, cfg.lifeTimeSec + GetRandomFloat(-cfg.lifeTimeRandomRange, cfg.lifeTimeRandomRange));
    particle.active = true;
}

std::unique_ptr<Object2DBase> ParticleManager::CreateParticleObject2D(const ParticleGroup& group, std::size_t index) const {
    const auto& cfg = group.config;
    const std::string baseName = cfg.name.empty() ? "ParticleGroup" : cfg.name;
    switch (cfg.shape2D) {
    case ParticleShape2D::Ellipse: {
        auto obj = std::make_unique<Ellipse>();
        obj->SetName(baseName + "_" + std::to_string(index));
        return obj;
    }
    case ParticleShape2D::Triangle: {
        auto obj = std::make_unique<Triangle2D>();
        obj->SetName(baseName + "_" + std::to_string(index));
        return obj;
    }
    case ParticleShape2D::Rect: {
        auto obj = std::make_unique<Rect>();
        obj->SetName(baseName + "_" + std::to_string(index));
        return obj;
    }
    case ParticleShape2D::Sprite:
    default: {
        auto obj = std::make_unique<Sprite>();
        obj->SetName(baseName + "_" + std::to_string(index));
        return obj;
    }
    }
}

std::unique_ptr<Object3DBase> ParticleManager::CreateParticleObject3D(const ParticleGroup& group, std::size_t index) const {
    const auto& cfg = group.config;
    const std::string baseName = cfg.name.empty() ? "ParticleGroup" : cfg.name;
    switch (cfg.shape3D) {
    case ParticleShape3D::Box: {
        auto obj = std::make_unique<Box>();
        obj->SetName(baseName + "_" + std::to_string(index));
        return obj;
    }
    case ParticleShape3D::Sphere: {
        auto obj = std::make_unique<Sphere>();
        obj->SetName(baseName + "_" + std::to_string(index));
        return obj;
    }
    case ParticleShape3D::Triangle: {
        auto obj = std::make_unique<Triangle3D>();
        obj->SetName(baseName + "_" + std::to_string(index));
        return obj;
    }
    case ParticleShape3D::Plane:
    default: {
        auto obj = std::make_unique<Plane3D>();
        obj->SetName(baseName + "_" + std::to_string(index));
        return obj;
    }
    }
}

void ParticleManager::CreateParticleInstance(ParticleGroup& group) {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    auto& cfg = group.config;
    const bool is2D = (cfg.target == "2D");
    const auto textureHandle = TextureManager::GetTextureFromFileName(cfg.textureName);
    const auto fallbackTexture = TextureManager::GetTextureFromFileName("white1x1.png");
    const auto texToUse = (textureHandle != TextureManager::kInvalidHandle) ? textureHandle : fallbackTexture;

    const std::size_t idx = group.spawnedCount;

    ParticleInstance instance{};
    if (is2D) {
        auto obj = CreateParticleObject2D(group, idx);
        obj->SetBatchKey(group.batchKey, RenderType::Instancing);

        if (auto* mat = obj->GetComponent2D<Material2D>()) {
            mat->SetTexture(texToUse);
            mat->SetColor(cfg.startColor);
        }

        obj->AttachToRenderer(ResolveTargetBuffer(cfg.target), cfg.pipelineName);

        instance.kind = ParticleKind::D2D;
        instance.object2D = obj.get();
        group.particles.push_back(instance);
        RespawnParticle(group, group.particles.back());

        ctx->AddObject2D(std::move(obj));
    } else {
        auto obj = CreateParticleObject3D(group, idx);
        obj->SetBatchKey(group.batchKey, RenderType::Instancing);

        if (auto* mat = obj->GetComponent3D<Material3D>()) {
            mat->SetEnableLighting(false);
            mat->SetTexture(texToUse);
            mat->SetColor(cfg.startColor);
        }

        obj->AttachToRenderer(ResolveTargetBuffer(cfg.target), cfg.pipelineName);

        instance.kind = ParticleKind::D3D;
        instance.object3D = obj.get();
        group.particles.push_back(instance);
        RespawnParticle(group, group.particles.back());

        ctx->AddObject3D(std::move(obj));
    }

    group.spawnedCount = group.particles.size();
}

bool ParticleManager::AddGroup(const ParticleGroupConfig& config) {
    auto* ctx = GetOwnerContext();
    if (!ctx) return false;

    ScreenBuffer* targetBuffer = ResolveTargetBuffer(config.target);
    if (!targetBuffer) return false;

    ParticleGroup group;
    group.config = config;
    group.batchKey = MakeRandomBatchKey();
    group.emitCenter = config.spawnCenter;
    group.isEmitting = config.spawnOnStart;

    if (group.config.startColor == Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } &&
        group.config.endColor == Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } &&
        group.config.color != Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }) {
        group.config.startColor = group.config.color;
        group.config.endColor = group.config.color;
    }

    if (group.isEmitting && config.spawnIntervalSec <= 0.0f) {
        while (group.spawnedCount < config.count) {
            CreateParticleInstance(group);
        }
    }

    groups_.push_back(std::move(group));
    return true;
}

void ParticleManager::DestroyGroupObjects(ParticleGroup& group) {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    for (auto& particle : group.particles) {
        if (particle.kind == ParticleKind::D2D) {
            if (!particle.object2D) continue;
            ctx->RemoveObject2D(particle.object2D);
            particle.object2D = nullptr;
        } else {
            if (!particle.object3D) continue;
            ctx->RemoveObject3D(particle.object3D);
            particle.object3D = nullptr;
        }
    }
    group.particles.clear();
    group.spawnedCount = 0;
}

bool ParticleManager::RemoveGroup(std::size_t index) {
    if (index >= groups_.size()) return false;
    DestroyGroupObjects(groups_[index]);
    groups_.erase(groups_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

void ParticleManager::ClearGroups() {
    for (auto& group : groups_) {
        DestroyGroupObjects(group);
    }
    groups_.clear();
}

bool ParticleManager::Spawn(const std::string& groupName, std::optional<Vector3> center) {
    for (auto& group : groups_) {
        if (group.config.name != groupName) continue;

        if (center.has_value()) {
            group.emitCenter = center.value();
        } else {
            group.emitCenter = group.config.spawnCenter;
        }

        group.isEmitting = true;
        group.spawnTimer = 0.0f;
        group.spawnedCount = group.particles.size();

        if (group.config.resetOnSpawn) {
            for (auto& particle : group.particles) {
                RespawnParticle(group, particle);
            }
        }

        if (group.config.spawnIntervalSec <= 0.0f) {
            while (group.spawnedCount < group.config.count) {
                CreateParticleInstance(group);
            }
        }

        return true;
    }

    return false;
}

JSON ParticleManager::SerializeGroup(const ParticleGroup& group) {
    JSON j = JSON::object();
    const auto& cfg = group.config;

    j["name"] = cfg.name;
    j["pipeline"] = cfg.pipelineName;
    j["texture"] = cfg.textureName;
    j["target"] = cfg.target;
    j["shape2D"] = ToString(cfg.shape2D);
    j["shape3D"] = ToString(cfg.shape3D);
    j["count"] = cfg.count;
    j["speed"] = cfg.speed;
    j["lifeTimeSec"] = cfg.lifeTimeSec;
    j["lifeTimeRandomRange"] = cfg.lifeTimeRandomRange;
    j["linearDamping"] = cfg.linearDamping;
    j["spawnCenter"] = { cfg.spawnCenter.x, cfg.spawnCenter.y, cfg.spawnCenter.z };
    j["spawnRadius"] = cfg.spawnRadius;
    j["useSpawnBox"] = cfg.useSpawnBox;
    j["spawnIntervalSec"] = cfg.spawnIntervalSec;
    j["spawnOnStart"] = cfg.spawnOnStart;
    j["resetOnSpawn"] = cfg.resetOnSpawn;
    j["loop"] = cfg.loop;
    j["gravity"] = { cfg.gravity.x, cfg.gravity.y, cfg.gravity.z };
    j["initialVelocityMin"] = { cfg.initialVelocityMin.x, cfg.initialVelocityMin.y, cfg.initialVelocityMin.z };
    j["initialVelocityMax"] = { cfg.initialVelocityMax.x, cfg.initialVelocityMax.y, cfg.initialVelocityMax.z };
    j["initialRotationMin"] = { cfg.initialRotationMin.x, cfg.initialRotationMin.y, cfg.initialRotationMin.z };
    j["initialRotationMax"] = { cfg.initialRotationMax.x, cfg.initialRotationMax.y, cfg.initialRotationMax.z };
    j["rotationSpeed"] = { cfg.rotationSpeed.x, cfg.rotationSpeed.y, cfg.rotationSpeed.z };
    j["baseScale"] = { cfg.baseScale.x, cfg.baseScale.y, cfg.baseScale.z };
    j["startScale"] = cfg.startScale;
    j["peakScale"] = cfg.peakScale;
    j["endScale"] = cfg.endScale;
    j["startColor"] = { cfg.startColor.x, cfg.startColor.y, cfg.startColor.z, cfg.startColor.w };
    j["endColor"] = { cfg.endColor.x, cfg.endColor.y, cfg.endColor.z, cfg.endColor.w };
    j["color"] = { cfg.color.x, cfg.color.y, cfg.color.z, cfg.color.w };

    JSON spawn = JSON::object();
    spawn["min"] = { cfg.spawnBox.min.x, cfg.spawnBox.min.y, cfg.spawnBox.min.z };
    spawn["max"] = { cfg.spawnBox.max.x, cfg.spawnBox.max.y, cfg.spawnBox.max.z };
    j["spawnBox"] = std::move(spawn);

    return j;
}

ParticleManager::ParticleGroupConfig ParticleManager::DeserializeGroupConfig(const JSON& json, const ParticleGroupConfig& fallback) {
    ParticleGroupConfig cfg = fallback;
    if (!json.is_object()) return cfg;

    cfg.name = json.value("name", cfg.name);
    cfg.pipelineName = json.value("pipeline", cfg.pipelineName);
    cfg.textureName = json.value("texture", cfg.textureName);
    cfg.target = json.value("target", cfg.target);
    if (json.contains("shape2D")) {
        cfg.shape2D = ParseShape2D(json.value("shape2D", std::string{}), cfg.shape2D);
    }
    if (json.contains("shape3D")) {
        cfg.shape3D = ParseShape3D(json.value("shape3D", std::string{}), cfg.shape3D);
    }
    cfg.count = static_cast<std::size_t>(std::max(0, json.value("count", static_cast<int>(cfg.count))));
    cfg.speed = json.value("speed", cfg.speed);
    cfg.lifeTimeSec = json.value("lifeTimeSec", cfg.lifeTimeSec);
    cfg.lifeTimeRandomRange = json.value("lifeTimeRandomRange", cfg.lifeTimeRandomRange);
    cfg.linearDamping = json.value("linearDamping", cfg.linearDamping);
    cfg.spawnRadius = json.value("spawnRadius", cfg.spawnRadius);
    cfg.useSpawnBox = json.value("useSpawnBox", cfg.useSpawnBox);
    cfg.spawnIntervalSec = json.value("spawnIntervalSec", cfg.spawnIntervalSec);
    cfg.spawnOnStart = json.value("spawnOnStart", cfg.spawnOnStart);
    cfg.resetOnSpawn = json.value("resetOnSpawn", cfg.resetOnSpawn);
    cfg.loop = json.value("loop", cfg.loop);
    cfg.startScale = json.value("startScale", cfg.startScale);
    cfg.peakScale = json.value("peakScale", cfg.peakScale);
    cfg.endScale = json.value("endScale", cfg.endScale);

    if (json.contains("baseScale")) {
        cfg.baseScale = ReadVector3OrDefault(json["baseScale"], cfg.baseScale);
    }
    if (json.contains("color")) {
        cfg.color = ReadVector4OrDefault(json["color"], cfg.color);
    }
    if (json.contains("startColor")) {
        cfg.startColor = ReadVector4OrDefault(json["startColor"], cfg.startColor);
    }
    if (json.contains("endColor")) {
        cfg.endColor = ReadVector4OrDefault(json["endColor"], cfg.endColor);
    }
    if (json.contains("gravity")) {
        cfg.gravity = ReadVector3OrDefault(json["gravity"], cfg.gravity);
    }
    if (json.contains("spawnCenter")) {
        cfg.spawnCenter = ReadVector3OrDefault(json["spawnCenter"], cfg.spawnCenter);
    }
    if (json.contains("initialVelocityMin")) {
        cfg.initialVelocityMin = ReadVector3OrDefault(json["initialVelocityMin"], cfg.initialVelocityMin);
    }
    if (json.contains("initialVelocityMax")) {
        cfg.initialVelocityMax = ReadVector3OrDefault(json["initialVelocityMax"], cfg.initialVelocityMax);
    }
    if (json.contains("initialRotationMin")) {
        cfg.initialRotationMin = ReadVector3OrDefault(json["initialRotationMin"], cfg.initialRotationMin);
    }
    if (json.contains("initialRotationMax")) {
        cfg.initialRotationMax = ReadVector3OrDefault(json["initialRotationMax"], cfg.initialRotationMax);
    }
    if (json.contains("rotationSpeed")) {
        cfg.rotationSpeed = ReadVector3OrDefault(json["rotationSpeed"], cfg.rotationSpeed);
    }
    if (json.contains("spawnBox") && json["spawnBox"].is_object()) {
        const auto& sb = json["spawnBox"];
        if (sb.contains("min")) {
            cfg.spawnBox.min = ReadVector3OrDefault(sb["min"], cfg.spawnBox.min);
        }
        if (sb.contains("max")) {
            cfg.spawnBox.max = ReadVector3OrDefault(sb["max"], cfg.spawnBox.max);
        }
    }

    if (!json.contains("startColor") && !json.contains("endColor") && json.contains("color")) {
        cfg.startColor = cfg.color;
        cfg.endColor = cfg.color;
    }

    return cfg;
}

bool ParticleManager::LoadFromJsonFile(const std::string& filepath) {
    if (filepath.empty()) return false;

    JSON jsonData = LoadJSON(filepath);
    if (jsonData.is_discarded() || !jsonData.is_object()) return false;

    const auto groupsJson = jsonData.value("groups", JSON::array());
    if (!groupsJson.is_array()) return false;

    ClearGroups();

    bool ok = false;
    for (const auto& entry : groupsJson) {
        const auto cfg = DeserializeGroupConfig(entry, ParticleGroupConfig{});
        ok = AddGroup(cfg) || ok;
    }

    return ok;
}

bool ParticleManager::SaveToJsonFile(const std::string& filepath) const {
    if (filepath.empty()) return false;

    JSON jsonData = JSON::object();
    jsonData["version"] = 1;

    JSON groups = JSON::array();
    for (const auto& group : groups_) {
        groups.push_back(SerializeGroup(group));
    }

    jsonData["groups"] = std::move(groups);
    return SaveJSON(jsonData, filepath, 4);
}

#if defined(USE_IMGUI)
void ParticleManager::ShowImGui() {
    if (!ImGui::Begin(Translation("engine.imgui.particle_manager.window").c_str())) {
        ImGui::End();
        return;
    }

    ImGui::SeparatorText(Translation("engine.imgui.particle_manager.section.create").c_str());
    ImGui::InputText(Translation("engine.imgui.particle_manager.name").c_str(), nameBuffer_.data(), nameBuffer_.size());

    const char* pipelines[] = {
        "Object3D.Solid.BlendNormal",
        "Object2D.DoubleSidedCulling.BlendNormal"
    };
    int pipelineIndex = newGroupConfig_.pipelineName == "Object2D.DoubleSidedCulling.BlendNormal" ? 1 : 0;
    if (ImGui::Combo(Translation("engine.imgui.particle_manager.pipeline").c_str(), &pipelineIndex, pipelines, 2)) {
        if (pipelineIndex == 0) {
            newGroupConfig_.pipelineName = "Object3D.Solid.BlendNormal";
        } else if (pipelineIndex == 1) {
            newGroupConfig_.pipelineName = "Object2D.DoubleSidedCulling.BlendNormal";
        }
    }
    ImGui::InputText(Translation("engine.imgui.particle_manager.texture").c_str(), textureBuffer_.data(), textureBuffer_.size());

    const char* targets[] = {
        Translation("engine.imgui.particle_manager.target.3d").c_str(),
        Translation("engine.imgui.particle_manager.target.2d").c_str()
    };
    int targetIndex = (newGroupConfig_.target == "2D") ? 1 : 0;
    if (ImGui::Combo(Translation("engine.imgui.particle_manager.target").c_str(), &targetIndex, targets, 2)) {
        newGroupConfig_.target = (targetIndex == 1) ? "2D" : "3D";
    }

    const char* shapes2D[] = {
        Translation("engine.imgui.particle_manager.shape2d.sprite").c_str(),
        Translation("engine.imgui.particle_manager.shape2d.ellipse").c_str(),
        Translation("engine.imgui.particle_manager.shape2d.triangle").c_str(),
        Translation("engine.imgui.particle_manager.shape2d.rect").c_str()
    };
    int shape2DIndex = static_cast<int>(newGroupConfig_.shape2D);
    if (ImGui::Combo(Translation("engine.imgui.particle_manager.shape2d").c_str(), &shape2DIndex, shapes2D, 4)) {
        newGroupConfig_.shape2D = static_cast<ParticleShape2D>(shape2DIndex);
    }

    const char* shapes3D[] = {
        Translation("engine.imgui.particle_manager.shape3d.plane").c_str(),
        Translation("engine.imgui.particle_manager.shape3d.box").c_str(),
        Translation("engine.imgui.particle_manager.shape3d.sphere").c_str(),
        Translation("engine.imgui.particle_manager.shape3d.triangle").c_str()
    };
    int shape3DIndex = static_cast<int>(newGroupConfig_.shape3D);
    if (ImGui::Combo(Translation("engine.imgui.particle_manager.shape3d").c_str(), &shape3DIndex, shapes3D, 4)) {
        newGroupConfig_.shape3D = static_cast<ParticleShape3D>(shape3DIndex);
    }

    int count = static_cast<int>(newGroupConfig_.count);
    if (ImGui::DragInt(Translation("engine.imgui.particle_manager.count").c_str(), &count, 1.0f, 1, 10000)) {
        newGroupConfig_.count = static_cast<std::size_t>(std::max(1, count));
    }

    ImGui::DragFloat(Translation("engine.imgui.particle_manager.speed").c_str(), &newGroupConfig_.speed, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat(Translation("engine.imgui.particle_manager.lifetime").c_str(), &newGroupConfig_.lifeTimeSec, 0.1f, 0.1f, 60.0f);
    ImGui::DragFloat(Translation("engine.imgui.particle_manager.lifetime_rand").c_str(), &newGroupConfig_.lifeTimeRandomRange, 0.1f, 0.0f, 60.0f);
    ImGui::DragFloat(Translation("engine.imgui.particle_manager.linear_damping").c_str(), &newGroupConfig_.linearDamping, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat3(Translation("engine.imgui.particle_manager.base_scale").c_str(), &newGroupConfig_.baseScale.x, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat(Translation("engine.imgui.particle_manager.start_scale").c_str(), &newGroupConfig_.startScale, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat(Translation("engine.imgui.particle_manager.peak_scale").c_str(), &newGroupConfig_.peakScale, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat(Translation("engine.imgui.particle_manager.end_scale").c_str(), &newGroupConfig_.endScale, 0.01f, 0.0f, 10.0f);
    ImGui::ColorEdit4(Translation("engine.imgui.particle_manager.start_color").c_str(), &newGroupConfig_.startColor.x);
    ImGui::ColorEdit4(Translation("engine.imgui.particle_manager.end_color").c_str(), &newGroupConfig_.endColor.x);

    ImGui::SeparatorText(Translation("engine.imgui.particle_manager.section.spawn").c_str());
    ImGui::Checkbox(Translation("engine.imgui.particle_manager.use_spawn_box").c_str(), &newGroupConfig_.useSpawnBox);
    ImGui::DragFloat3(Translation("engine.imgui.particle_manager.spawn_center").c_str(), &newGroupConfig_.spawnCenter.x, 0.1f, -10000.0f, 10000.0f);
    ImGui::DragFloat(Translation("engine.imgui.particle_manager.spawn_radius").c_str(), &newGroupConfig_.spawnRadius, 0.1f, 0.0f, 10000.0f);
    ImGui::DragFloat3(Translation("engine.imgui.particle_manager.spawn_box_min").c_str(), &newGroupConfig_.spawnBox.min.x, 0.1f, -10000.0f, 10000.0f);
    ImGui::DragFloat3(Translation("engine.imgui.particle_manager.spawn_box_max").c_str(), &newGroupConfig_.spawnBox.max.x, 0.1f, -10000.0f, 10000.0f);
    ImGui::DragFloat(Translation("engine.imgui.particle_manager.spawn_interval").c_str(), &newGroupConfig_.spawnIntervalSec, 0.1f, 0.0f, 60.0f);
    ImGui::Checkbox(Translation("engine.imgui.particle_manager.spawn_on_start").c_str(), &newGroupConfig_.spawnOnStart);
    ImGui::Checkbox(Translation("engine.imgui.particle_manager.reset_on_spawn").c_str(), &newGroupConfig_.resetOnSpawn);
    ImGui::Checkbox(Translation("engine.imgui.particle_manager.loop").c_str(), &newGroupConfig_.loop);

    ImGui::SeparatorText(Translation("engine.imgui.particle_manager.section.motion").c_str());
    ImGui::DragFloat3(Translation("engine.imgui.particle_manager.gravity").c_str(), &newGroupConfig_.gravity.x, 0.1f, -1000.0f, 1000.0f);
    ImGui::DragFloat3(Translation("engine.imgui.particle_manager.velocity_min").c_str(), &newGroupConfig_.initialVelocityMin.x, 0.1f, -1000.0f, 1000.0f);
    ImGui::DragFloat3(Translation("engine.imgui.particle_manager.velocity_max").c_str(), &newGroupConfig_.initialVelocityMax.x, 0.1f, -1000.0f, 1000.0f);
    ImGui::DragFloat3(Translation("engine.imgui.particle_manager.rotation_min").c_str(), &newGroupConfig_.initialRotationMin.x, 0.1f, -1000.0f, 1000.0f);
    ImGui::DragFloat3(Translation("engine.imgui.particle_manager.rotation_max").c_str(), &newGroupConfig_.initialRotationMax.x, 0.1f, -1000.0f, 1000.0f);
    ImGui::DragFloat3(Translation("engine.imgui.particle_manager.rotation_speed").c_str(), &newGroupConfig_.rotationSpeed.x, 0.1f, -1000.0f, 1000.0f);

    if (ImGui::Button(Translation("engine.imgui.particle_manager.add_group").c_str())) {
        newGroupConfig_.name = nameBuffer_.data();
        newGroupConfig_.textureName = textureBuffer_.data();
        AddGroup(newGroupConfig_);
    }

    ImGui::SeparatorText(Translation("engine.imgui.particle_manager.section.json").c_str());
    ImGui::InputText(Translation("engine.imgui.particle_manager.json_path").c_str(), jsonPathBuffer_.data(), jsonPathBuffer_.size());
    if (ImGui::Button(Translation("engine.imgui.particle_manager.load").c_str())) {
        LoadFromJsonFile(std::string(jsonPathBuffer_.data()));
    }
    ImGui::SameLine();
    if (ImGui::Button(Translation("engine.imgui.particle_manager.save").c_str())) {
        SaveToJsonFile(std::string(jsonPathBuffer_.data()));
    }

    ImGui::SeparatorText(Translation("engine.imgui.particle_manager.section.groups").c_str());
    if (groups_.empty()) {
        ImGui::TextUnformatted(Translation("engine.imgui.particle_manager.none").c_str());
    } else {
        std::size_t removeIndex = static_cast<std::size_t>(-1);
        for (std::size_t i = 0; i < groups_.size(); ++i) {
            const auto& group = groups_[i];
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::TreeNode(group.config.name.c_str())) {
                ImGui::Text("%s: %zu", Translation("engine.imgui.particle_manager.count").c_str(), group.config.count);
                ImGui::Text("%s: %s", Translation("engine.imgui.particle_manager.target").c_str(), group.config.target.c_str());
                ImGui::Text("%s: %s", Translation("engine.imgui.particle_manager.pipeline").c_str(), group.config.pipelineName.c_str());
                ImGui::Text("%s: %s", Translation("engine.imgui.particle_manager.texture").c_str(), group.config.textureName.c_str());
                ImGui::Text("%s: 0x%llX", Translation("engine.imgui.particle_manager.batch_key").c_str(), static_cast<unsigned long long>(group.batchKey));
                ImGui::Text("%s: %zu", Translation("engine.imgui.particle_manager.spawned").c_str(), group.spawnedCount);
                if (ImGui::SmallButton(Translation("engine.imgui.particle_manager.spawn").c_str())) {
                    Spawn(group.config.name, std::nullopt);
                }
                ImGui::TreePop();
            }
            if (ImGui::SmallButton(Translation("engine.imgui.particle_manager.remove").c_str())) {
                removeIndex = i;
            }
            ImGui::PopID();
        }
        if (removeIndex != static_cast<std::size_t>(-1)) {
            RemoveGroup(removeIndex);
        }
    }

    ImGui::End();
}
#endif

} // namespace KashipanEngine
