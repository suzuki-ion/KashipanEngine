#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Assets/ModelManager.h"

namespace KashipanEngine {

class MeshFilter final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(MeshFilter, 1, )
    explicit MeshFilter(ModelManager::ModelHandle meshHandle)
        : MeshFilter() {
        meshHandle_ = meshHandle;
    }
    ~MeshFilter() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<MeshFilter>(meshHandle_);
    }

    void SetMeshHandle(ModelManager::ModelHandle handle) { meshHandle_ = handle; }
    ModelManager::ModelHandle GetMeshHandle() const noexcept { return meshHandle_; }
    bool HasMesh() const noexcept { return meshHandle_ != ModelManager::kInvalidHandle; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::Text("MeshHandle: %u", static_cast<unsigned>(meshHandle_));
        int value = static_cast<int>(meshHandle_);
        if (ImGui::InputInt("##MeshHandle", &value)) {
            meshHandle_ = value <= 0 ? ModelManager::kInvalidHandle : static_cast<ModelManager::ModelHandle>(value);
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["meshHandle"] = meshHandle_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        meshHandle_ = json.value("meshHandle", ModelManager::kInvalidHandle);
        return true;
    }

private:
    ModelManager::ModelHandle meshHandle_ = ModelManager::kInvalidHandle;
};

REGISTER_COMPONENT_OBJECT(MeshFilter)

} // namespace KashipanEngine
