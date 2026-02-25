#include "Scene/Components/ParticleManager.h"

#include "Scene/SceneContext.h"
#include "Scene/Components/SceneDefaultVariables.h"
#include "Assets/TextureManager.h"
#include "Utilities/MathUtils/Easings.h"
#include "Utilities/TimeUtils.h"

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
}

void ParticleManager::Finalize() {
    ClearGroups();
}

void ParticleManager::Update() {
    const float dt = std::max(0.0f, GetDeltaTime());
    if (dt <= 0.0f) return;

    for (auto& group : groups_) {
        const auto& cfg = group.config;
        if (cfg.lifeTimeSec <= 0.0f) continue;

        for (auto& particle : group.particles) {
            if (particle.kind == ParticleKind::D3D) {
                if (!particle.object3D) continue;
                auto* tr = particle.object3D->GetComponent3D<Transform3D>();
                if (!tr) continue;

                particle.elapsed += dt;

                const Vector3 pos = tr->GetTranslate();
                tr->SetTranslate(pos + particle.velocity * (cfg.speed * dt));

                const float half = cfg.lifeTimeSec * 0.5f;
                float scaleFactor = 0.0f;
                if (particle.elapsed < half) {
                    const float t = (half > 0.0f) ? std::clamp(particle.elapsed / half, 0.0f, 1.0f) : 1.0f;
                    scaleFactor = EaseOutCubic(0.0f, 1.0f, t);
                } else {
                    const float t = (half > 0.0f) ? std::clamp((particle.elapsed - half) / half, 0.0f, 1.0f) : 1.0f;
                    scaleFactor = EaseInCubic(1.0f, 0.0f, t);
                }
                tr->SetScale(cfg.baseScale * scaleFactor);

                if (particle.elapsed >= cfg.lifeTimeSec) {
                    RespawnParticle(group, particle);
                }
            } else {
                if (!particle.object2D) continue;
                auto* tr = particle.object2D->GetComponent2D<Transform2D>();
                if (!tr) continue;

                particle.elapsed += dt;

                const Vector3 pos = tr->GetTranslate();
                tr->SetTranslate(pos + particle.velocity * (cfg.speed * dt));

                const float half = cfg.lifeTimeSec * 0.5f;
                float scaleFactor = 0.0f;
                if (particle.elapsed < half) {
                    const float t = (half > 0.0f) ? std::clamp(particle.elapsed / half, 0.0f, 1.0f) : 1.0f;
                    scaleFactor = EaseOutCubic(0.0f, 1.0f, t);
                } else {
                    const float t = (half > 0.0f) ? std::clamp((particle.elapsed - half) / half, 0.0f, 1.0f) : 1.0f;
                    scaleFactor = EaseInCubic(1.0f, 0.0f, t);
                }
                tr->SetScale(cfg.baseScale * scaleFactor);

                if (particle.elapsed >= cfg.lifeTimeSec) {
                    RespawnParticle(group, particle);
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

void ParticleManager::RespawnParticle(ParticleGroup& group, ParticleInstance& particle) {
    const auto& box = group.config.spawnBox;
    const Vector3 pos{
        GetRandomFloat(box.min.x, box.max.x),
        GetRandomFloat(box.min.y, box.max.y),
        GetRandomFloat(box.min.z, box.max.z)
    };

    if (particle.kind == ParticleKind::D3D) {
        if (!particle.object3D) return;
        auto* tr = particle.object3D->GetComponent3D<Transform3D>();
        if (!tr) return;
        tr->SetTranslate(pos);
        tr->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
    } else {
        if (!particle.object2D) return;
        auto* tr = particle.object2D->GetComponent2D<Transform2D>();
        if (!tr) return;
        tr->SetTranslate(pos);
        tr->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
    }

    Vector3 dir{
        GetRandomFloat(-1.0f, 1.0f),
        GetRandomFloat(-1.0f, 1.0f),
        GetRandomFloat(-1.0f, 1.0f)
    };
    particle.velocity = NormalizeSafe(dir);
    particle.elapsed = 0.0f;
}

bool ParticleManager::AddGroup(const ParticleGroupConfig& config) {
    auto* ctx = GetOwnerContext();
    if (!ctx) return false;

    ScreenBuffer* targetBuffer = ResolveTargetBuffer(config.target);
    if (!targetBuffer) return false;

    ParticleGroup group;
    group.config = config;
    group.batchKey = MakeRandomBatchKey();

    const auto textureHandle = TextureManager::GetTextureFromFileName(config.textureName);
    const auto fallbackTexture = TextureManager::GetTextureFromFileName("white1x1.png");
    const auto texToUse = (textureHandle != TextureManager::kInvalidHandle) ? textureHandle : fallbackTexture;

    const std::string baseName = config.name.empty() ? "ParticleGroup" : config.name;
    const bool is2D = (config.target == "2D");

    for (std::size_t i = 0; i < config.count; ++i) {
        ParticleInstance instance{};
        if (is2D) {
            auto obj = std::make_unique<Sprite>();
            obj->SetName(baseName + "_" + std::to_string(i));
            obj->SetBatchKey(group.batchKey, RenderType::Instancing);

            if (auto* mat = obj->GetComponent2D<Material2D>()) {
                mat->SetTexture(texToUse);
                mat->SetColor(config.color);
            }

            obj->AttachToRenderer(targetBuffer, config.pipelineName);

            instance.kind = ParticleKind::D2D;
            instance.object2D = obj.get();
            group.particles.push_back(instance);
            RespawnParticle(group, group.particles.back());

            ctx->AddObject2D(std::move(obj));
        } else {
            auto obj = std::make_unique<Plane3D>();
            obj->SetName(baseName + "_" + std::to_string(i));
            obj->SetBatchKey(group.batchKey, RenderType::Instancing);

            if (auto* mat = obj->GetComponent3D<Material3D>()) {
                mat->SetEnableLighting(false);
                mat->SetTexture(texToUse);
                mat->SetColor(config.color);
            }

            obj->AttachToRenderer(targetBuffer, config.pipelineName);

            instance.kind = ParticleKind::D3D;
            instance.object3D = obj.get();
            group.particles.push_back(instance);
            RespawnParticle(group, group.particles.back());

            ctx->AddObject3D(std::move(obj));
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

JSON ParticleManager::SerializeGroup(const ParticleGroup& group) {
    JSON j = JSON::object();
    const auto& cfg = group.config;

    j["name"] = cfg.name;
    j["pipeline"] = cfg.pipelineName;
    j["texture"] = cfg.textureName;
    j["target"] = cfg.target;
    j["count"] = cfg.count;
    j["speed"] = cfg.speed;
    j["lifeTimeSec"] = cfg.lifeTimeSec;

    j["baseScale"] = { cfg.baseScale.x, cfg.baseScale.y, cfg.baseScale.z };
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
    cfg.count = static_cast<std::size_t>(std::max(0, json.value("count", static_cast<int>(cfg.count))));
    cfg.speed = json.value("speed", cfg.speed);
    cfg.lifeTimeSec = json.value("lifeTimeSec", cfg.lifeTimeSec);

    if (json.contains("baseScale")) {
        cfg.baseScale = ReadVector3OrDefault(json["baseScale"], cfg.baseScale);
    }
    if (json.contains("color")) {
        cfg.color = ReadVector4OrDefault(json["color"], cfg.color);
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
    if (!ImGui::Begin("ParticleManager")) {
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Create Group");
    ImGui::InputText("Name", nameBuffer_.data(), nameBuffer_.size());
    ImGui::InputText("Pipeline", pipelineBuffer_.data(), pipelineBuffer_.size());
    ImGui::InputText("Texture", textureBuffer_.data(), textureBuffer_.size());

    const char* targets[] = { "3D", "2D" };
    int targetIndex = (newGroupConfig_.target == "2D") ? 1 : 0;
    if (ImGui::Combo("Target", &targetIndex, targets, 2)) {
        newGroupConfig_.target = (targetIndex == 1) ? "2D" : "3D";
    }

    int count = static_cast<int>(newGroupConfig_.count);
    if (ImGui::DragInt("Count", &count, 1.0f, 1, 10000)) {
        newGroupConfig_.count = static_cast<std::size_t>(std::max(1, count));
    }

    ImGui::DragFloat("Speed", &newGroupConfig_.speed, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("LifeTime", &newGroupConfig_.lifeTimeSec, 0.1f, 0.1f, 60.0f);
    ImGui::DragFloat3("BaseScale", &newGroupConfig_.baseScale.x, 0.01f, 0.0f, 100.0f);
    ImGui::ColorEdit4("Color", &newGroupConfig_.color.x);

    ImGui::SeparatorText("SpawnBox");
    ImGui::DragFloat3("Min", &newGroupConfig_.spawnBox.min.x, 0.1f, -10000.0f, 10000.0f);
    ImGui::DragFloat3("Max", &newGroupConfig_.spawnBox.max.x, 0.1f, -10000.0f, 10000.0f);

    if (ImGui::Button("Add Group")) {
        newGroupConfig_.name = nameBuffer_.data();
        newGroupConfig_.pipelineName = pipelineBuffer_.data();
        newGroupConfig_.textureName = textureBuffer_.data();
        AddGroup(newGroupConfig_);
    }

    ImGui::SeparatorText("Json IO");
    ImGui::InputText("Json Path", jsonPathBuffer_.data(), jsonPathBuffer_.size());
    if (ImGui::Button("Load")) {
        LoadFromJsonFile(std::string(jsonPathBuffer_.data()));
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        SaveToJsonFile(std::string(jsonPathBuffer_.data()));
    }

    ImGui::SeparatorText("Groups");
    if (groups_.empty()) {
        ImGui::TextUnformatted("(none)");
    } else {
        std::size_t removeIndex = static_cast<std::size_t>(-1);
        for (std::size_t i = 0; i < groups_.size(); ++i) {
            const auto& group = groups_[i];
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::TreeNode(group.config.name.c_str())) {
                ImGui::Text("Count: %zu", group.config.count);
                ImGui::Text("Target: %s", group.config.target.c_str());
                ImGui::Text("Pipeline: %s", group.config.pipelineName.c_str());
                ImGui::Text("Texture: %s", group.config.textureName.c_str());
                ImGui::Text("BatchKey: 0x%llX", static_cast<unsigned long long>(group.batchKey));
                ImGui::TreePop();
            }
            if (ImGui::SmallButton("Remove")) {
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
