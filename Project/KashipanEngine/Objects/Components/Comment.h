#pragma once
#include <string>

#include "Objects/ObjectComponentHeader.h"

namespace KashipanEngine {

/// @brief デバッグ用のコメントを保持するだけのコンポーネント
/// @details 実行時の動作には一切関与しない。保持するコメントの内容は、エディターの
///          オブジェクトヒエラルキーで対象オブジェクトにカーソルを合わせた際にツールチップとして表示される
class Comment final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(Comment, 1, )
    COMPONENT_CATEGORY("Debug")
    ~Comment() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Comment>();
        ptr->comment_ = comment_;
        return ptr;
    }

    void SetComment(const std::string &comment) { comment_ = comment; }
    const std::string &GetComment() const noexcept { return comment_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::InputTextMultiline("Comment", &comment_);
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["comment"] = comment_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        comment_ = json.value("comment", std::string{});
        return true;
    }

private:
    std::string comment_;
};

REGISTER_COMPONENT_OBJECT(Comment)

} // namespace KashipanEngine
