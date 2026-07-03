#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Assets/SamplerManager.h"
#include "Assets/TextureManager.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class MaterialManager final {
public:
    using MaterialHandle = std::uint32_t;
    static constexpr MaterialHandle kInvalidHandle = 0;

    struct Material {
        std::string name = "Default";
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        Matrix4x4 uvTransform = Matrix4x4::Identity();
        TextureManager::TextureHandle textureHandle = TextureManager::kInvalidHandle;
        TextureManager::TextureHandle environmentHandle = TextureManager::kInvalidHandle;
        SamplerManager::SamplerHandle samplerHandle = SamplerManager::kInvalidHandle;
        float shininess = 32.0f;
        Vector4 specularColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        float environmentCoefficient = 1.0f;
        bool enableLighting = true;
        bool enableShadowMapProjection = true;
    };

    MaterialManager() {
        materials_.push_back(Material{});
        nameToHandle_["Default"] = 1;
    }

    MaterialHandle RegisterMaterial(const Material &material) {
        if (material.name.empty()) return kInvalidHandle;
        if (auto handle = GetHandle(material.name); handle != kInvalidHandle) {
            materials_[handle - 1] = material;
            return handle;
        }

        materials_.push_back(material);
        const MaterialHandle handle = static_cast<MaterialHandle>(materials_.size());
        nameToHandle_[material.name] = handle;
        return handle;
    }

    bool RemoveMaterial(const std::string &name) {
        auto it = nameToHandle_.find(name);
        if (it == nameToHandle_.end() || it->second == kInvalidHandle) return false;
        nameToHandle_.erase(it);
        return true;
    }

    MaterialHandle GetHandle(const std::string &name) const {
        auto it = nameToHandle_.find(name);
        return it != nameToHandle_.end() ? it->second : kInvalidHandle;
    }

    Material *GetMaterial(MaterialHandle handle) {
        if (handle == kInvalidHandle || handle > materials_.size()) return nullptr;
        return &materials_[handle - 1];
    }

    const Material *GetMaterial(MaterialHandle handle) const {
        if (handle == kInvalidHandle || handle > materials_.size()) return nullptr;
        return &materials_[handle - 1];
    }

    Material *GetMaterial(const std::string &name) { return GetMaterial(GetHandle(name)); }
    const Material *GetMaterial(const std::string &name) const { return GetMaterial(GetHandle(name)); }

private:
    std::vector<Material> materials_;
    std::unordered_map<std::string, MaterialHandle> nameToHandle_;
};

} // namespace KashipanEngine
