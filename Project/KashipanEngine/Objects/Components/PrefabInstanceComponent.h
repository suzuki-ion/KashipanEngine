#pragma once
#include <string>

#include "Objects/ObjectComponentHeader.h"
#include "Utilities/UUID128.h"

namespace KashipanEngine {

/// @brief Prefabインスタンスのサブツリー根にのみ付与される、Prefabとの対応関係を示すマーカーコンポーネント
/// @details エディター上でPrefabをシーンへ配置した際に自動で付与される。ユーザーがこのコンポーネントを
///          手動で削除した場合、そのオブジェクトはPrefabとのリンクを失う（Unityの「Unpack Prefab Instance」相当）
class PrefabInstanceComponent final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(PrefabInstanceComponent, 1,)
    COMPONENT_CATEGORY("Prefab")
    ~PrefabInstanceComponent() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<PrefabInstanceComponent>();
        ptr->prefabID_ = prefabID_;
        return ptr;
    }

    void SetPrefabID(const UUID128 &id) { prefabID_ = id; }
    const UUID128 &GetPrefabID() const noexcept { return prefabID_; }

    /// @brief PrefabAssetManager経由でソースファイルパスを取得する（USE_IMGUI限定。未登録時は空文字）
    /// @details 定義は EmptyObject/Scene/Editor の完全な型定義が必要なため PrefabInstanceComponent.cpp にある
    std::string GetPrefabPath() const;

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::TextUnformatted(("Prefab: " + GetPrefabPath()).c_str());
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["prefabID"] = prefabID_.ToString();
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        prefabID_ = UUID128(json.value("prefabID", std::string{}));
        return true;
    }

private:
    UUID128 prefabID_;
};

REGISTER_COMPONENT_OBJECT(PrefabInstanceComponent)

} // namespace KashipanEngine
