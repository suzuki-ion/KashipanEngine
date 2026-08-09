#pragma once
#include <string>
#include <vector>
#include "Objects/ObjectComponentHeader.h"
#include "Assets/ModelManager.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

class MeshFilter final : public IObjectComponent {
public:
    // meshHandle_の直接書き込み時は、セッターと同様にSceneRendererの描画リスト再構築を促す
    OBJECT_COMPONENT_CONSTRUCTOR(MeshFilter, 1,
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(meshHandle_, [this] { SetMeshHandle(meshHandle_); });
    )
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
        if (ImGuiCustom::SelectString(TranslationLabel("component.meshfilter.mesh"), currentPath, modelPaths, true)) {
            SetMeshHandle(currentPath.empty() ? ModelManager::kInvalidHandle : ModelManager::GetModelHandleFromAssetPath(currentPath));
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        // ハンドル値はモデルの読み込み順や数で変わってしまうため、
        // Assetsルートからの相対パス（文字列）で保存する
        json["meshAssetPath"] = HasMesh() ? ModelManager::GetModelData(meshHandle_).GetAssetRelativePath() : std::string();
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        if (json.contains("meshAssetPath")) {
            const std::string assetPath = json.value("meshAssetPath", std::string());
            meshHandle_ = assetPath.empty() ? ModelManager::kInvalidHandle : ModelManager::GetModelHandleFromAssetPath(assetPath);
        } else {
            // 旧形式（ハンドル値の直接保存）との後方互換
            meshHandle_ = json.value("meshHandle", ModelManager::kInvalidHandle);
        }
        return true;
    }

private:
    ModelManager::ModelHandle meshHandle_ = ModelManager::kInvalidHandle;
};

REGISTER_COMPONENT_OBJECT(MeshFilter)

} // namespace KashipanEngine
