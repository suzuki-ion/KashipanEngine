#pragma once
#include <string>
#include <vector>

#include "Objects/Components/Collider/ICollider.h"
#include "Objects/Components/MeshFilter.h"
#include "Assets/ModelManager.h"
#include "Math/Vector3.h"

namespace KashipanEngine {

/// @brief UnityのMeshColliderに相当するコンポーネント
/// @details Convexフラグをオンにした場合のみ凸包として扱われ、他のMeshColliderと衝突できるようになる
///          （物理エンジン側の制約により、非Convexなメッシュ同士の衝突判定には対応していない）。
///          メッシュを明示的に指定しない場合は、同オブジェクトのMeshFilterコンポーネントのメッシュを参照する。
class MeshCollider final : public ICollider {
public:
    MeshCollider() : ICollider("MeshCollider", Shape::Mesh, false, GetComponentTypeID<MeshCollider>()) {}
    ~MeshCollider() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<MeshCollider>();
        ptr->convex_ = convex_;
        ptr->explicitMeshHandle_ = explicitMeshHandle_;
        ptr->SetTrigger(IsTrigger());
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
        if (explicitMeshHandle_ != ModelManager::kInvalidHandle) return explicitMeshHandle_;
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return ModelManager::kInvalidHandle;
        auto *meshFilter = objectContext->GetComponent<MeshFilter>();
        return meshFilter ? meshFilter->GetMeshHandle() : ModelManager::kInvalidHandle;
    }

    std::optional<ColliderInfo3D> BuildColliderInfo3D() const override {
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
        if (convex_) {
            ColliderInfo3D::ConvexMeshShape3D meshShape;
            meshShape.vertices = std::move(vertices);
            meshShape.indices = data.GetIndices();
            info.shape = std::move(meshShape);
        } else {
            ColliderInfo3D::ConcaveMeshShape3D meshShape;
            meshShape.vertices = std::move(vertices);
            meshShape.indices = data.GetIndices();
            info.shape = std::move(meshShape);
        }
        auto *objectContext = GetOwnerObjectContext();
        info.ownerObject = const_cast<EmptyObject *>(objectContext ? objectContext->GetOwner() : nullptr);
        return info;
    }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ICollider::ShowImGui();
        ImGui::Checkbox("Convex", &convex_);
        ImGui::TextDisabled("Convexオフの場合、他のMeshColliderとは衝突しません");

        // 読み込み済みモデルの中から選択する（未指定の場合はMeshFilterのメッシュを使用）
        std::vector<std::string> modelPaths;
        std::string currentPath;
        for (const auto &entry : ModelManager::GetLoadedModelListEntries()) {
            modelPaths.push_back(entry.assetPath);
            if (entry.handle == explicitMeshHandle_) currentPath = entry.assetPath;
        }
        if (ImGuiCustom::SelectString("Mesh (optional)", currentPath, modelPaths, true)) {
            explicitMeshHandle_ = currentPath.empty() ? ModelManager::kInvalidHandle : ModelManager::GetModelHandleFromAssetPath(currentPath);
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = ICollider::SaveToJson();
        json["convex"] = convex_;
        json["meshHandle"] = explicitMeshHandle_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        ICollider::LoadFromJson(json);
        convex_ = json.value("convex", true);
        explicitMeshHandle_ = json.value("meshHandle", ModelManager::kInvalidHandle);
        return true;
    }

private:
    bool convex_ = true;
    ModelManager::ModelHandle explicitMeshHandle_ = ModelManager::kInvalidHandle;
};

REGISTER_COMPONENT_OBJECT(MeshCollider)

} // namespace KashipanEngine
