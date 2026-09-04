#pragma once
#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include "Debug/Logger.h"
#include "Objects/ObjectComponentHeader.h"
#include "Assets/BitmapFontManager.h"
#include "Assets/MaterialManager.h"
#include "Graphics/IRenderTarget.h"
#include "Graphics/PipelineManager.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Objects/Components/Transform.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/Translation.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#endif

namespace KashipanEngine {

/// @brief BMFont（.fnt、事前生成済みビットマップアトラス）によるテキスト描画コンポーネント
/// @details TextRenderer（TTF/OTFをSDFとしてランタイムベイクする方式）とは完全に独立した、
///          もう一つのテキスト描画手段。フォントはBitmapFontManager経由で読み込んだ.fntを使う。
///          リッチテキストタグ・太字/斜体/アウトラインには対応しない（ビットマップアトラスの
///          性質上、SDFのような形状加工ができないため）。1文字ずつ独立した矩形インスタンスとして
///          描画する点、共有の単位クアッド（Rect2D）メッシュを使う点、描画は
///          SceneRenderer::DrawEntry（1文字＝1エントリ）としてsortedDrawList_に乗る点、
///          文字ごとに位置オフセット・回転・スケールを個別に上書きできる点（Transformと同様の
///          調整。ランタイム専用でシーンJSONへは保存しない）はTextRendererと同じだが、
///          シェーダーはSpriteRenderer/MeshRendererと同じObject2D系パイプラインをそのまま使う
///          （文字ごとのUV矩形はDrawEntryのinstanceUvTranslate/instanceUvScale経由で渡す。
///          専用シェーダー・パイプラインは持たない）。
class BitmapTextRenderer final : public IObjectComponent {
public:
    /// @brief テキストブロックの横方向アライメント
    enum class HorizontalAlign { Left, Center, Right };
    /// @brief テキストブロックの縦方向アライメント
    enum class VerticalAlign { Top, Middle, Bottom };
    /// @brief インスタンスカラーをマテリアル色へ適用する方法（MeshRenderer/SpriteRendererと同じ値）
    enum class ColorBlendMode : int { Override = 0, Multiply, Add, Subtract };

    /// @brief 文字ごとの位置オフセット・回転・スケールの上書き情報（Transform的な調整用）
    struct CharacterOverride {
        Vector2 offset{ 0.0f, 0.0f };
        float rotation = 0.0f; // ラジアン
        Vector2 scale{ 1.0f, 1.0f };
    };

    /// @brief 描画用に確定した1インスタンス（1文字の矩形）分のデータ
    /// @details worldMatrixは所有オブジェクトのTransformまで合成済みのワールド行列
    struct RenderCharacterInstance {
        Matrix4x4 worldMatrix = Matrix4x4::Identity();
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
    };

    OBJECT_COMPONENT_CONSTRUCTOR(BitmapTextRenderer, 0xFF,
        SetUpdatePriority(900);
        ADD_MEMBER_VARIABLE(pipelineName_);
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(materialName_, [this] {
            materialHandle_ = MaterialManager::kInvalidHandle;
        });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(text_, [this] { MarkShapeDirty(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(fontName_, [this] {
            fontHandle_ = BitmapFontManager::kInvalidHandle;
            MarkShapeDirty();
        });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(fontSize_, [this] {
            fontSize_ = std::max(0.01f, fontSize_);
            MarkShapeDirty();
        });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(instanceColor_, [this] { MarkInstancesDirty(); });
        ADD_MEMBER_VARIABLE(instanceColorBlendMode_);
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(horizontalAlign_, [this] { MarkInstancesDirty(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(verticalAlign_, [this] { MarkInstancesDirty(); });
        ADD_MEMBER_VARIABLE(pointSampling_);
        ADD_MEMBER_VARIABLE(renderPriority_);
        ADD_MEMBER_VARIABLE(allowInstancing_);
    )
    COMPONENT_CATEGORY("Render")
    ~BitmapTextRenderer() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        LogScope scope;
        auto ptr = std::make_unique<BitmapTextRenderer>();
        ptr->targetObjectID_ = targetObjectID_;
        ptr->pipelineName_ = pipelineName_;
        ptr->materialName_ = materialName_;
        ptr->materialHandle_ = materialHandle_;
        ptr->excludedRenderTargetNames_ = excludedRenderTargetNames_;
        ptr->text_ = text_;
        ptr->fontName_ = fontName_;
        ptr->fontSize_ = fontSize_;
        ptr->instanceColor_ = instanceColor_;
        ptr->instanceColorBlendMode_ = instanceColorBlendMode_;
        ptr->horizontalAlign_ = horizontalAlign_;
        ptr->verticalAlign_ = verticalAlign_;
        ptr->pointSampling_ = pointSampling_;
        ptr->renderPriority_ = renderPriority_;
        ptr->allowInstancing_ = allowInstancing_;
        ptr->MarkShapeDirty();
        return ptr;
    }

    //==================================================
    // 描画先指定
    //==================================================

    void SetTargetObject(const EmptyObject *targetObject) {
        targetObjectID_ = targetObject ? targetObject->GetObjectID() : UUID128();
    }
    void SetTargetObject(const UUID128 &targetObjectID) { targetObjectID_ = targetObjectID; }
    const UUID128 &GetTargetObjectID() const noexcept { return targetObjectID_; }
    EmptyObject *GetTargetObject() const {
        LogScope scope;
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !targetObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(targetObjectID_);
    }
    bool IsRenderTargetIncluded(const IRenderTarget *target) const {
        LogScope scope;
        if (!target) return false;
        return !excludedRenderTargetNames_.contains(target->GetRenderTargetName());
    }

    //==================================================
    // パイプライン・フォント指定
    //==================================================

    void SetPipelineName(const std::string &pipelineName) { pipelineName_ = pipelineName; }
    const std::string &GetPipelineName() const noexcept { return pipelineName_; }

    void SetFontName(const std::string &fontName) {
        LogScope scope;
        fontName_ = fontName;
        fontHandle_ = BitmapFontManager::kInvalidHandle;
        MarkInstancesDirty();
    }
    const std::string &GetFontName() const noexcept { return fontName_; }
    /// @brief フォントハンドルを取得する（未解決の場合はフォント名から解決を試みる）
    BitmapFontManager::FontHandle GetFontHandle() const {
        LogScope scope;
        if (fontHandle_ == BitmapFontManager::kInvalidHandle && !fontName_.empty()) {
            fontHandle_ = BitmapFontManager::GetFontHandleFromName(fontName_);
        }
        return fontHandle_;
    }

    void SetMaterialName(const std::string &materialName) {
        LogScope scope;
        materialName_ = materialName;
        materialHandle_ = MaterialManager::kInvalidHandle;
    }
    void SetMaterialHandle(MaterialManager::MaterialHandle materialHandle) { materialHandle_ = materialHandle; }
    const std::string &GetMaterialName() const noexcept { return materialName_; }
    /// @brief 描画パラメーターとして使う任意マテリアルを取得する。テクスチャはフォントページで上書きされる
    MaterialManager::MaterialHandle GetMaterialHandle() const {
        LogScope scope;
        if (materialHandle_ == MaterialManager::kInvalidHandle && !materialName_.empty()) {
            materialHandle_ = MaterialManager::GetMaterialHandleFromName(materialName_);
        }
        if (materialHandle_ == MaterialManager::kInvalidHandle) {
            materialHandle_ = MaterialManager::GetMaterialHandleFromName("Default");
        }
        return materialHandle_;
    }

    //==================================================
    // テキスト内容・見た目
    //==================================================

    void SetText(const std::string &text) {
        LogScope scope;
        if (text_ == text) return;
        text_ = text;
        MarkShapeDirty();
    }
    const std::string &GetText() const noexcept { return text_; }

    void SetFontSize(float fontSize) {
        LogScope scope;
        fontSize_ = std::max(0.01f, fontSize);
        MarkShapeDirty();
    }
    float GetFontSize() const noexcept { return fontSize_; }

    void SetInstanceColor(const Vector4 &color) {
        LogScope scope;
        instanceColor_ = color;
        MarkInstancesDirty();
    }
    const Vector4 &GetInstanceColor() const noexcept { return instanceColor_; }
    void SetInstanceColorBlendMode(ColorBlendMode mode) noexcept { instanceColorBlendMode_ = mode; }
    ColorBlendMode GetInstanceColorBlendMode() const noexcept { return instanceColorBlendMode_; }

    void SetHorizontalAlign(HorizontalAlign align) { LogScope scope; horizontalAlign_ = align; MarkInstancesDirty(); }
    HorizontalAlign GetHorizontalAlign() const noexcept { return horizontalAlign_; }
    void SetVerticalAlign(VerticalAlign align) { LogScope scope; verticalAlign_ = align; MarkInstancesDirty(); }
    VerticalAlign GetVerticalAlign() const noexcept { return verticalAlign_; }

    /// @brief アトラスのサンプリング方式を設定する（true=ポイント/ニアレスト、false=リニア。既定true）
    /// @details ドット絵フォント等、拡大縮小時ににじませたくない場合はtrueのままにする
    void SetPointSampling(bool enable) noexcept { pointSampling_ = enable; }
    bool GetPointSampling() const noexcept { return pointSampling_; }

    //==================================================
    // 描画順・インスタンシング制御
    //==================================================

    void SetRenderPriority(std::int32_t priority) noexcept { renderPriority_ = priority; }
    std::int32_t GetRenderPriority() const noexcept { return renderPriority_; }
    void SetAllowInstancing(bool allow) noexcept { allowInstancing_ = allow; }
    bool GetAllowInstancing() const noexcept { return allowInstancing_; }

    //==================================================
    // 文字ごとのアクセス（Transform的な調整。ランタイム専用・非シリアライズ）
    //==================================================

    /// @brief 改行文字も1つとしてカウントした実際の文字数を取得する
    size_t GetCharacterCount() const {
        LogScope scope;
        RebuildShapeIfDirty();
        return characterOverrides_.size();
    }
    void SetCharacterOffset(size_t index, const Vector2 &offset) {
        LogScope scope;
        RebuildShapeIfDirty();
        if (index >= characterOverrides_.size()) return;
        characterOverrides_[index].offset = offset;
        MarkInstancesDirty();
    }
    Vector2 GetCharacterOffset(size_t index) const {
        LogScope scope;
        RebuildShapeIfDirty();
        return index < characterOverrides_.size() ? characterOverrides_[index].offset : Vector2(0.0f, 0.0f);
    }
    void SetCharacterRotation(size_t index, float rotation) {
        LogScope scope;
        RebuildShapeIfDirty();
        if (index >= characterOverrides_.size()) return;
        characterOverrides_[index].rotation = rotation;
        MarkInstancesDirty();
    }
    float GetCharacterRotation(size_t index) const {
        LogScope scope;
        RebuildShapeIfDirty();
        return index < characterOverrides_.size() ? characterOverrides_[index].rotation : 0.0f;
    }
    void SetCharacterScale(size_t index, const Vector2 &scale) {
        LogScope scope;
        RebuildShapeIfDirty();
        if (index >= characterOverrides_.size()) return;
        characterOverrides_[index].scale = scale;
        MarkInstancesDirty();
    }
    Vector2 GetCharacterScale(size_t index) const {
        LogScope scope;
        RebuildShapeIfDirty();
        return index < characterOverrides_.size() ? characterOverrides_[index].scale : Vector2(1.0f, 1.0f);
    }

    //==================================================
    // 描画情報取得（Renderer専用パスから呼ばれる）
    //==================================================

    /// @brief 現在のTransformワールド行列まで合成した、描画に使う文字ごとのインスタンス一覧を取得する
    std::vector<RenderCharacterInstance> GetRenderInstances() const {
        LogScope scope;
        RebuildInstancesIfDirty();
        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        const Matrix4x4 ownerWorld = transform ? transform->GetWorldMatrix() : Matrix4x4::Identity();

        std::vector<RenderCharacterInstance> result = localInstances_;
        for (auto &instance : result) {
            instance.worldMatrix = instance.worldMatrix * ownerWorld;
        }
        return result;
    }

protected:
    void Initialize() override {
        LogScope scope;
        auto *sceneRenderer = GetOrAddSceneRenderer();
        if (sceneRenderer) {
            sceneRenderer->RegisterBitmapTextRenderer(this);
        }
    }

    void Finalize() override {
        LogScope scope;
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
        if (sceneRenderer) {
            sceneRenderer->UnregisterBitmapTextRenderer(this);
        }
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        LogScope scope;
        TargetObjectSelector::ShowSelector(TranslationLabel("component.common.target"), GetOwnerSceneContext(), targetObjectID_);
        TargetObjectSelector::ShowRenderTargetFilters(GetOwnerSceneContext(), targetObjectID_, excludedRenderTargetNames_);

        if (ImGui::InputTextMultiline(TranslationLabel("component.bitmaptextrenderer.text"), &text_)) {
            MarkShapeDirty();
        }

        std::vector<std::string> fontNames;
        for (const auto &entry : BitmapFontManager::GetLoadedFontListEntries()) {
            fontNames.push_back(entry.name);
        }
        if (ImGuiCustom::SelectString(TranslationLabel("component.bitmaptextrenderer.font"), fontName_, fontNames, true)) {
            fontHandle_ = BitmapFontManager::kInvalidHandle;
            MarkShapeDirty();
        }
        if (ImGui::DragFloat(TranslationLabel("component.bitmaptextrenderer.font_size"), &fontSize_, 0.01f, 0.01f, 1000.0f)) MarkShapeDirty();
        if (ImGui::ColorEdit4(TranslationLabel("component.bitmaptextrenderer.instance_color"), &instanceColor_.x)) MarkInstancesDirty();
        const char *kColorBlendModeLabels[] = { TranslationC("component.common.blendmode.override"), TranslationC("component.common.blendmode.multiply"), TranslationC("component.common.blendmode.add"), TranslationC("component.common.blendmode.subtract") };
        int blendModeIndex = static_cast<int>(instanceColorBlendMode_);
        if (ImGui::Combo(TranslationLabel("component.bitmaptextrenderer.instance_color_blend_mode"), &blendModeIndex, kColorBlendModeLabels, IM_ARRAYSIZE(kColorBlendModeLabels))) {
            instanceColorBlendMode_ = static_cast<ColorBlendMode>(blendModeIndex);
        }

        const auto materialEntries = MaterialManager::GetLoadedMaterialListEntries();
        std::vector<std::string> materialNames;
        for (const auto &entry : materialEntries) materialNames.push_back(entry.material.name);
        if (ImGuiCustom::SelectString(TranslationLabel("component.bitmaptextrenderer.material"), materialName_, materialNames)) {
            materialHandle_ = MaterialManager::kInvalidHandle;
        }

        const char *kHAlignLabels[] = { TranslationC("component.textrenderer.halign.left"), TranslationC("component.textrenderer.halign.center"), TranslationC("component.textrenderer.halign.right") };
        int hAlign = static_cast<int>(horizontalAlign_);
        if (ImGui::Combo(TranslationLabel("component.textrenderer.horizontal_align"), &hAlign, kHAlignLabels, 3)) {
            horizontalAlign_ = static_cast<HorizontalAlign>(hAlign);
            MarkInstancesDirty();
        }
        const char *kVAlignLabels[] = { TranslationC("component.textrenderer.valign.top"), TranslationC("component.textrenderer.valign.middle"), TranslationC("component.textrenderer.valign.bottom") };
        int vAlign = static_cast<int>(verticalAlign_);
        if (ImGui::Combo(TranslationLabel("component.textrenderer.vertical_align"), &vAlign, kVAlignLabels, 3)) {
            verticalAlign_ = static_cast<VerticalAlign>(vAlign);
            MarkInstancesDirty();
        }

        ImGui::Checkbox(TranslationLabel("component.bitmaptextrenderer.point_sampling"), &pointSampling_);

        ImGuiCustom::SelectString(TranslationLabel("component.bitmaptextrenderer.pipeline"), pipelineName_, PipelineManager::GetLoadedRenderPipelineNames("2D"));

        ImGui::DragInt(TranslationLabel("component.common.render_priority"), &renderPriority_);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.common.desc_render_priority"));
        }
        ImGui::Checkbox(TranslationLabel("component.common.allow_instancing"), &allowInstancing_);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.common.desc_allow_instancing"));
        }

        RebuildShapeIfDirty();
        if (!characterOverrides_.empty() && ImGui::TreeNode(TranslationLabel("component.bitmaptextrenderer.character_overrides"))) {
            for (size_t i = 0; i < characterOverrides_.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                ImGui::Text("[%zu]", i);
                bool changed = false;
                changed |= ImGui::DragFloat2(TranslationLabel("component.textrenderer.offset"), &characterOverrides_[i].offset.x, 0.1f);
                changed |= ImGui::DragFloat(TranslationLabel("component.textrenderer.rotation"), &characterOverrides_[i].rotation, 0.01f);
                changed |= ImGui::DragFloat2(TranslationLabel("component.textrenderer.scale"), &characterOverrides_[i].scale.x, 0.01f);
                if (changed) MarkInstancesDirty();
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }
#endif

    JSON SaveToJson() const override {
        LogScope scope;
        JSON json = JSON::object();
        json["text"] = text_;
        json["fontName"] = fontName_;
        json["fontSize"] = fontSize_;
        json["instanceColor"] = ToJSON(instanceColor_);
        json["instanceColorBlendMode"] = static_cast<int>(instanceColorBlendMode_);
        json["materialName"] = materialName_;
        json["pipelineName"] = pipelineName_;
        json["horizontalAlign"] = static_cast<int>(horizontalAlign_);
        json["verticalAlign"] = static_cast<int>(verticalAlign_);
        json["pointSampling"] = pointSampling_;
        json["targetObjectID"] = ToJSON(targetObjectID_);
        for (const auto &name : excludedRenderTargetNames_) {
            json["excludedRenderTargetNames"].push_back(name);
        }
        json["renderPriority"] = renderPriority_;
        json["allowInstancing"] = allowInstancing_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        LogScope scope;
        text_ = json.value("text", std::string{});
        fontName_ = json.value("fontName", std::string{});
        fontHandle_ = BitmapFontManager::kInvalidHandle;
        fontSize_ = json.value("fontSize", 32.0f);
        instanceColor_ = json.contains("instanceColor") ? FromJSON<Vector4>(json["instanceColor"]) : Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        instanceColorBlendMode_ = static_cast<ColorBlendMode>(json.value("instanceColorBlendMode", static_cast<int>(ColorBlendMode::Multiply)));
        materialName_ = json.value("materialName", std::string{ "Default" });
        materialHandle_ = MaterialManager::kInvalidHandle;
        pipelineName_ = json.value("pipelineName", std::string{ "Object2D.DoubleSidedCulling.BlendNormal" });
        horizontalAlign_ = static_cast<HorizontalAlign>(json.value("horizontalAlign", 0));
        verticalAlign_ = static_cast<VerticalAlign>(json.value("verticalAlign", 0));
        pointSampling_ = json.value("pointSampling", true);
        if (json.contains("targetObjectID")) {
            targetObjectID_ = FromJSON<UUID128>(json["targetObjectID"]);
        } else {
            targetObjectID_ = UUID128();
        }
        excludedRenderTargetNames_.clear();
        for (const auto &name : json.value("excludedRenderTargetNames", std::vector<std::string>())) {
            excludedRenderTargetNames_.insert(name);
        }
        renderPriority_ = json.value("renderPriority", 0);
        allowInstancing_ = json.value("allowInstancing", true);
        MarkShapeDirty();
        return true;
    }

private:
    SceneRenderer *GetOrAddSceneRenderer() const {
        LogScope scope;
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return nullptr;
        auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
        if (!sceneRenderer) {
            sceneRenderer = sceneContext->AddComponent<SceneRenderer>();
        }
        return sceneRenderer;
    }

    void MarkShapeDirty() const { LogScope scope; shapeDirty_ = true; instancesDirty_ = true; }
    void MarkInstancesDirty() const { LogScope scope; instancesDirty_ = true; }

    /// @brief テキストをコードポイント列へ分解し、文字ごとのTransform上書き情報の器
    ///        （characterOverrides_）を現在の文字数に合わせて確保する。resize()を使うため、
    ///        テキストが少し変わっても既存の上書き値はインデックスが同じ限り保持される
    void RebuildShapeIfDirty() const {
        LogScope scope;
        if (!shapeDirty_) return;
        shapeDirty_ = false;
        shapedCodepoints_ = Utf8ToCodepoints(text_);
        characterOverrides_.resize(shapedCodepoints_.size());
    }

    void RebuildInstancesIfDirty() const {
        LogScope scope;
        RebuildShapeIfDirty();
        if (!instancesDirty_) return;
        instancesDirty_ = false;
        localInstances_.clear();

        const BitmapFontManager::FontHandle fontHandle = GetFontHandle();
        if (fontHandle == BitmapFontManager::kInvalidHandle || shapedCodepoints_.empty()) return;

        const float scale = BitmapFontManager::GetScaleForFontSize(fontHandle, fontSize_);
        const float lineHeight = BitmapFontManager::GetLineHeight(fontHandle, fontSize_);
        const float ascent = BitmapFontManager::GetBase(fontHandle, fontSize_);
        const float descent = -(lineHeight - ascent);

        struct PositionedChar {
            const BitmapFontManager::CharInfo *charInfo = nullptr;
            size_t shapedIndex = 0;
            float penX = 0.0f;
        };
        std::vector<std::vector<PositionedChar>> lines;
        std::vector<float> lineWidths;
        lines.emplace_back();
        lineWidths.push_back(0.0f);

        float penX = 0.0f;
        for (size_t i = 0; i < shapedCodepoints_.size(); ++i) {
            const char32_t codepoint = shapedCodepoints_[i];
            if (codepoint == U'\n') {
                lineWidths.back() = penX;
                lines.emplace_back();
                lineWidths.push_back(0.0f);
                penX = 0.0f;
                continue;
            }
            const auto *charInfo = BitmapFontManager::GetCharInfo(fontHandle, codepoint);
            if (!charInfo) continue;

            PositionedChar pc;
            pc.charInfo = charInfo;
            pc.shapedIndex = i;
            pc.penX = penX;
            lines.back().push_back(pc);
            penX += charInfo->xAdvance * scale;
        }
        lineWidths.back() = penX;

        const float lastBaselineOffset = static_cast<float>(lines.size() - 1) * lineHeight;
        float startY = -ascent;
        switch (verticalAlign_) {
            case VerticalAlign::Top:
                startY = -ascent;
                break;
            case VerticalAlign::Middle:
                startY = (lastBaselineOffset - ascent - descent) * 0.5f;
                break;
            case VerticalAlign::Bottom:
                startY = lastBaselineOffset - descent;
                break;
        }

        for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            float lineOffsetX = 0.0f;
            switch (horizontalAlign_) {
                case HorizontalAlign::Left:   lineOffsetX = 0.0f; break;
                case HorizontalAlign::Center: lineOffsetX = -lineWidths[lineIndex] * 0.5f; break;
                case HorizontalAlign::Right:  lineOffsetX = -lineWidths[lineIndex]; break;
            }
            const float lineBaselineY = startY - static_cast<float>(lineIndex) * lineHeight;
            // BMFontのxoffset/yoffsetは「ペン位置＝行の上端」からのオフセットであり、ベースライン
            // からのオフセットではない（TTF/SDF側のGlyphInfo::yoffはベースライン基準だが、
            // BMFontの.fnt仕様ではline topが基準となる点が異なる）。ascentはbaseそのもの
            // （行の上端からベースラインまでの距離）なので、baseline + ascent で行の上端に戻す
            const float lineTopY = lineBaselineY + ascent;

            for (const auto &pc : lines[lineIndex]) {
                const auto &ch = *pc.charInfo;
                const float quadW = ch.width * scale;
                const float quadH = ch.height * scale;
                if (quadW <= 0.0f || quadH <= 0.0f) continue;
                const float centerX = pc.penX + lineOffsetX + ch.xOffset * scale + quadW * 0.5f;
                // BMFontのyoffsetは行の上端からの距離（画像と同じくY下向き）、ワールドはY上向きのため符号を反転する
                const float centerY = lineTopY - (ch.yOffset * scale + quadH * 0.5f);

                const CharacterOverride &ov = characterOverrides_[pc.shapedIndex];
                const Vector3 centerWorld(centerX + ov.offset.x, centerY + ov.offset.y, 0.0f);

                Matrix4x4 scaleMat;
                scaleMat.MakeScale(Vector3(quadW, quadH, 1.0f));
                Matrix4x4 overrideScaleMat;
                overrideScaleMat.MakeScale(Vector3(ov.scale.x, ov.scale.y, 1.0f));
                Matrix4x4 rotateMat;
                rotateMat.MakeRotateZ(ov.rotation);
                Matrix4x4 translateMat;
                translateMat.MakeTranslate(centerWorld);

                RenderCharacterInstance instance;
                instance.worldMatrix = scaleMat * overrideScaleMat * rotateMat * translateMat;
                instance.color = instanceColor_;
                instance.u0 = ch.u0; instance.v0 = ch.v0; instance.u1 = ch.u1; instance.v1 = ch.v1;
                localInstances_.push_back(instance);
            }
        }
    }

    UUID128 targetObjectID_{};
    std::string pipelineName_ = "Object2D.DoubleSidedCulling.BlendNormal";
    std::string materialName_ = "Default";
    mutable MaterialManager::MaterialHandle materialHandle_ = MaterialManager::kInvalidHandle;
    std::unordered_set<std::string> excludedRenderTargetNames_;
    int renderPriority_ = 0;
    bool allowInstancing_ = true;

    std::string text_;
    std::string fontName_;
    mutable BitmapFontManager::FontHandle fontHandle_ = BitmapFontManager::kInvalidHandle;
    float fontSize_ = 32.0f;
    Vector4 instanceColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
    ColorBlendMode instanceColorBlendMode_ = ColorBlendMode::Multiply;

    HorizontalAlign horizontalAlign_ = HorizontalAlign::Left;
    VerticalAlign verticalAlign_ = VerticalAlign::Top;
    /// @brief アトラスのサンプリングをポイント（ニアレスト）にするか（既定true。ドット絵フォント向け）
    bool pointSampling_ = true;

    mutable bool shapeDirty_ = true;
    mutable bool instancesDirty_ = true;
    /// @brief 改行文字も含む、テキストを分解したコードポイント列（characterOverrides_の添字と対応）
    mutable std::vector<char32_t> shapedCodepoints_;
    /// @brief 文字ごとの位置/回転/スケール上書き（ランタイム専用。非シリアライズ）
    mutable std::vector<CharacterOverride> characterOverrides_;
    /// @brief 所有オブジェクトのTransformを合成する前の、文字ごとのローカル描画インスタンス
    mutable std::vector<RenderCharacterInstance> localInstances_;
};

REGISTER_COMPONENT_OBJECT(BitmapTextRenderer)

} // namespace KashipanEngine
