#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Utilities/ShortTextBehavior.h"

namespace KashipanEngine {

// 言語モデルを使用しない実験的なレシピ。シーンに保存し、生成ファイルやAPIキーは使用しない。
class ShortTextBehavior final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(ShortTextBehavior, 1, )
    COMPONENT_CATEGORY("Experimental")
    std::unique_ptr<IObjectComponent> Clone() const override;
    const ShortText::Recipe &GetRecipe() const { return recipe_; }
    const std::string &GetSourceText() const { return sourceText_; }
protected:
    void Update() override;
    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif
private:
    ShortText::Recipe recipe_;
    std::string sourceText_;
    double elapsed_ = 0;
    ShortText::ProgramMemory programMemory_;
    std::string executionError_;
};
REGISTER_COMPONENT_OBJECT(ShortTextBehavior)

// Add/ComponentEditCommandが受け取る既存形式のコンポーネントスナップショットを生成する。
JSON MakeShortTextBehaviorState(const ShortText::Recipe &recipe, const std::string &sourceText);
void ReadShortTextSceneInput(const EmptyObject *owner, SceneContext *scene, const ShortText::Recipe &recipe,
    ShortText::ProgramState &state, ShortText::InputSnapshot &input, ShortText::ProgramMemory *memory = nullptr, double dt = 0);
void ApplyShortTextSceneEffects(const EmptyObject *owner, SceneContext *scene, const ShortText::ProgramState &state,
    ShortText::ProgramMemory *memory = nullptr, std::string *error = nullptr);
} // namespace KashipanEngine
