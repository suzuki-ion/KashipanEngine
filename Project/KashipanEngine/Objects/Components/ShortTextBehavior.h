#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Utilities/ShortTextBehavior.h"

namespace KashipanEngine {

// Experimental, model-free recipe. Saved in the scene; no generated files or API keys.
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
};
REGISTER_COMPONENT_OBJECT(ShortTextBehavior)

// Produces the existing component snapshot envelope expected by Add/ComponentEditCommand.
JSON MakeShortTextBehaviorState(const ShortText::Recipe &recipe, const std::string &sourceText);
} // namespace KashipanEngine
