#pragma once
#include <string>
#include <vector>

#include "Debug/Logger.h"
#include "Objects/Components/Collider/ICollider.h"
#include "Objects/Components/MeshFilter.h"
#include "Assets/ModelManager.h"
#include "Math/Vector3.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief UnityのMeshColliderに相当するコンポーネント
/// @details Convexフラグをオンにした場合は頂点群から凸包を生成する。オフの場合は描画メッシュの
///          三角形形状をそのまま使用する（静的な地形・壁向け）。物理エンジン側の制約により、
///          非Convexなメッシュ同士の衝突、およびDynamic/Kinematic RigidBodyへの取り付けには対応していない。
///          メッシュを明示的に指定しない場合は、同オブジェクトのMeshFilterコンポーネントのメッシュを参照する。
class MeshCollider final : public ICollider {
public:
    MeshCollider() : ICollider("MeshCollider", Shape::Mesh, false, GetComponentTypeID<MeshCollider>()) {
        LogScope scope;
        ADD_MEMBER_VARIABLE(convex_);
    }
    ~MeshCollider() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        LogScope scope;
        auto ptr = std::make_unique<MeshCollider>();
        ptr->convex_ = convex_;
        ptr->explicitMeshHandle_ = explicitMeshHandle_;
        ptr->SetTrigger(IsTrigger());
        ptr->CopySyncSettingsFrom(*this);
        return ptr;
    }

    void SetConvex(bool convex) noexcept { convex_ = convex; }
    bool IsConvex() const noexcept { return convex_; }

    /// @brief 使用するメッシュを明示的に指定する（未指定の場合はMeshFilterのメッシュを使用する）
    void SetMeshHandle(ModelManager::ModelHandle handle) noexcept { explicitMeshHandle_ = handle; }
    ModelManager::ModelHandle GetMeshHandle() const noexcept { return explicitMeshHandle_; }

    /// @brief 実際に衝突判定に使用するメッシュハンドルを取得する
    /// @details 明示的な指定が無い場合は同オブジェクトのMeshFilterコンポーネントのメッシュにフォールバックする
    ModelManager::ModelHandle GetEffectiveMeshHandle() const {
        LogScope scope;
        if (explicitMeshHandle_ != ModelManager::kInvalidHandle) return explicitMeshHandle_;
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return ModelManager::kInvalidHandle;
        auto *meshFilter = objectContext->GetComponent<MeshFilter>();
        return meshFilter ? meshFilter->GetMeshHandle() : ModelManager::kInvalidHandle;
    }

    std::optional<ColliderInfo3D> BuildColliderInfo3D() const override {
        LogScope scope;
        const auto meshHandle = GetEffectiveMeshHandle();
        if (meshHandle == ModelManager::kInvalidHandle) return std::nullopt;

        const ModelData &data = ModelManager::GetModelData(meshHandle);
        if (data.GetVertexCount() == 0 || data.GetIndexCount() == 0) return std::nullopt;

        std::vector<Vector3> vertices;
        vertices.reserve(data.GetVertexCount());
        for (const auto &v : data.GetVertices()) {
            vertices.emplace_back(v.px, v.py, v.pz);
        }

        ColliderInfo3D info;
        const Vector3 scale = GetSyncedOwnerScale();
        if (convex_) {
            ColliderInfo3D::ConvexMeshShape3D meshShape;
            meshShape.vertices = std::move(vertices);
            meshShape.indices = data.GetIndices();
            meshShape.scale = scale;
            info.shape = std::move(meshShape);
        } else {
            ColliderInfo3D::ConcaveMeshShape3D meshShape;
            meshShape.vertices = std::move(vertices);
            meshShape.indices = data.GetIndices();
            meshShape.scale = scale;
            info.shape = std::move(meshShape);
        }
        auto *objectContext = GetOwnerObjectContext();
        info.ownerObject = const_cast<EmptyObject *>(objectContext ? objectContext->GetOwner() : nullptr);
        return info;
    }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        LogScope scope;
        ICollider::ShowImGui();
        ImGui::Checkbox(TranslationLabel("component.meshcollider.convex"), &convex_);
        ImGui::TextDisabled("%s", TranslationC("component.meshcollider.convex_meshcollider"));

        // 読み込み済みモデルの中から選択する（未指定の場合はMeshFilterのメッシュを使用）
        std::vector<std::string> modelPaths;
        std::string currentPath;
        for (const auto &entry : ModelManager::GetLoadedModelListEntries()) {
            modelPaths.push_back(entry.assetPath);
            if (entry.handle == explicitMeshHandle_) currentPath = entry.assetPath;
        }
        if (ImGuiCustom::SelectString(TranslationLabel("component.meshcollider.mesh_optional"), currentPath, modelPaths, true)) {
            explicitMeshHandle_ = currentPath.empty() ? ModelManager::kInvalidHandle : ModelManager::GetModelHandleFromAssetPath(currentPath);
        }
    }
#endif

    JSON SaveToJson() const override {
        LogScope scope;
        JSON json = ICollider::SaveToJson();
        json["convex"] = convex_;
        // ハンドル値はモデルの読み込み順や数で変わってしまうため、
        // Assetsルートからの相対パス（文字列）で保存する
        json["meshAssetPath"] = (explicitMeshHandle_ != ModelManager::kInvalidHandle)
            ? ModelManager::GetModelData(explicitMeshHandle_).GetAssetRelativePath()
            : std::string();
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        LogScope scope;
        ICollider::LoadFromJson(json);
        convex_ = json.value("convex", true);
        if (json.contains("meshAssetPath")) {
            const std::string assetPath = json.value("meshAssetPath", std::string());
            explicitMeshHandle_ = assetPath.empty() ? ModelManager::kInvalidHandle : ModelManager::GetModelHandleFromAssetPath(assetPath);
        } else {
            // 旧形式（ハンドル値の直接保存）との後方互換
            explicitMeshHandle_ = json.value("meshHandle", ModelManager::kInvalidHandle);
        }
        return true;
    }

private:
    bool convex_ = true;
    ModelManager::ModelHandle explicitMeshHandle_ = ModelManager::kInvalidHandle;
};

REGISTER_COMPONENT_OBJECT(MeshCollider)

} // namespace KashipanEngine
