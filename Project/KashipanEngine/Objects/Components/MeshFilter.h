#pragma once
#include <string>
#include <vector>
#include "Objects/ObjectComponentHeader.h"
#include "Assets/ModelManager.h"
#include "Scene/Components/Render/SceneRenderer.h"

namespace KashipanEngine {

class MeshFilter final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(MeshFilter, 1, )
    COMPONENT_CATEGORY("Render")
    explicit MeshFilter(ModelManager::ModelHandle meshHandle)
        : MeshFilter() {
        meshHandle_ = meshHandle;
    }
    ~MeshFilter() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<MeshFilter>(meshHandle_);
    }

    /// @brief メッシュハンドルを設定する
    /// @details MeshRenderer/SpriteRendererはこのハンドルを描画リストのソートキーに使うため、
    ///          変更時はSceneRendererのキャッシュを再構築させる
    void SetMeshHandle(ModelManager::ModelHandle handle) {
        meshHandle_ = handle;
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
        if (sceneRenderer) sceneRenderer->MarkDrawListDirty();
    }
    ModelManager::ModelHandle GetMeshHandle() const noexcept { return meshHandle_; }
    bool HasMesh() const noexcept { return meshHandle_ != ModelManager::kInvalidHandle; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        // 読み込み済みモデルの中から選択する（重複名を避けるためAssetsルートからの相対パスをキーにする）
        std::vector<std::string> modelPaths;
        std::string currentPath;
        for (const auto &entry : ModelManager::GetLoadedModelListEntries()) {
            modelPaths.push_back(entry.assetPath);
            if (entry.handle == meshHandle_) currentPath = entry.assetPath;
        }
        if (ImGuiCustom::SelectString("Mesh", currentPath, modelPaths, true)) {
            SetMeshHandle(currentPath.empty() ? ModelManager::kInvalidHandle : ModelManager::GetModelHandleFromAssetPath(currentPath));
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
