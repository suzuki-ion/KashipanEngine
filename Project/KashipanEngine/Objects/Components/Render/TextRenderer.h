#pragma once
#include <algorithm>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Assets/FontManager.h"
#include "Graphics/IRenderTarget.h"
#include "Graphics/PipelineManager.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Objects/Components/Transform.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief SDFフォントによるテキスト描画コンポーネント
/// @details 指定フォント（FontManager経由でTTF/OTFから読み込み・SDFベイク）を使い、
///          Unity風のリッチテキストタグ（&lt;i&gt; &lt;b&gt; &lt;color=#RRGGBB(AA)&gt;
///          &lt;size=N|N%&gt; &lt;s&gt; &lt;u&gt; &lt;sub&gt; &lt;sup&gt;）を解釈しながら
///          1文字ずつ独立した矩形インスタンスとして描画する。文字ごとに位置オフセット・回転・
///          スケールを個別に上書きできる（Transformと同様の調整。ランタイム専用でシーンJSONへは保存しない）。
///          文字ごとにアトラス内UVが異なるため既存のMeshRenderer/SpriteRenderer用の描画バッチ
///          （CollectSortableEntries/DrawBatch）には乗せず、Rendererの専用描画パス
///          （RenderTextRenderers）がGetRenderInstances()の結果を直接バッチ化して描画する。
class TextRenderer final : public IObjectComponent {
public:
    /// @brief テキストブロックの横方向アライメント
    enum class HorizontalAlign { Left, Center, Right };
    /// @brief テキストブロックの縦方向アライメント
    enum class VerticalAlign { Top, Middle, Bottom };

    /// @brief 文字ごとの位置オフセット・回転・スケールの上書き情報（Transform的な調整用）
    struct CharacterOverride {
        Vector2 offset{ 0.0f, 0.0f };
        float rotation = 0.0f; // ラジアン
        Vector2 scale{ 1.0f, 1.0f };
    };

    /// @brief 描画用に確定した1インスタンス（1文字の矩形、または下線/取り消し線の装飾矩形）分のデータ
    /// @details worldMatrixは所有オブジェクトのTransformまで合成済みのワールド行列
    struct RenderCharacterInstance {
        Matrix4x4 worldMatrix = Matrix4x4::Identity();
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
        float boldWeight = 0.0f;
    };

    // 直接書き込み時もセッターと同様に形状/インスタンスの再構築を促す
    OBJECT_COMPONENT_CONSTRUCTOR(TextRenderer, 0xFF,
        SetUpdatePriority(900);
        ADD_MEMBER_VARIABLE(pipelineName_);
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(text_, [this] { MarkShapeDirty(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(fontName_, [this] {
            fontHandle_ = FontManager::kInvalidHandle;
            MarkShapeDirty();
        });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(fontSize_, [this] {
            fontSize_ = std::max(0.01f, fontSize_);
            MarkShapeDirty();
        });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(color_, [this] { MarkShapeDirty(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(defaultCharacterAnchor_, [this] { MarkInstancesDirty(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(defaultCharacterPivot_, [this] { MarkInstancesDirty(); });
    )
    COMPONENT_CATEGORY("Render")
    ~TextRenderer() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<TextRenderer>();
        ptr->targetObjectID_ = targetObjectID_;
        ptr->pipelineName_ = pipelineName_;
        ptr->excludedRenderTargetNames_ = excludedRenderTargetNames_;
        ptr->text_ = text_;
        ptr->fontName_ = fontName_;
        ptr->fontSize_ = fontSize_;
        ptr->color_ = color_;
        ptr->horizontalAlign_ = horizontalAlign_;
        ptr->verticalAlign_ = verticalAlign_;
        ptr->defaultCharacterAnchor_ = defaultCharacterAnchor_;
        ptr->defaultCharacterPivot_ = defaultCharacterPivot_;
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
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !targetObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(targetObjectID_);
    }
    bool IsRenderTargetIncluded(const IRenderTarget *target) const {
        if (!target) return false;
        return !excludedRenderTargetNames_.contains(target->GetRenderTargetName());
    }

    //==================================================
    // パイプライン・フォント指定
    //==================================================

    void SetPipelineName(const std::string &pipelineName) { pipelineName_ = pipelineName; }
    const std::string &GetPipelineName() const noexcept { return pipelineName_; }

    void SetFontName(const std::string &fontName) {
        fontName_ = fontName;
        fontHandle_ = FontManager::kInvalidHandle;
        MarkShapeDirty();
    }
    const std::string &GetFontName() const noexcept { return fontName_; }
    /// @brief フォントハンドルを取得する（未解決の場合はフォント名から解決を試みる）
    FontManager::FontHandle GetFontHandle() const {
        if (fontHandle_ == FontManager::kInvalidHandle && !fontName_.empty()) {
            fontHandle_ = FontManager::GetFontHandleFromName(fontName_);
        }
        return fontHandle_;
    }

    //==================================================
    // テキスト内容・見た目
    //==================================================

    void SetText(const std::string &text) {
        if (text_ == text) return;
        text_ = text;
        MarkShapeDirty();
    }
    const std::string &GetText() const noexcept { return text_; }

    void SetFontSize(float fontSize) {
        fontSize_ = std::max(0.01f, fontSize);
        MarkShapeDirty();
    }
    float GetFontSize() const noexcept { return fontSize_; }

    void SetColor(const Vector4 &color) {
        color_ = color;
        MarkShapeDirty();
    }
    const Vector4 &GetColor() const noexcept { return color_; }

    void SetHorizontalAlign(HorizontalAlign align) { horizontalAlign_ = align; MarkInstancesDirty(); }
    HorizontalAlign GetHorizontalAlign() const noexcept { return horizontalAlign_; }
    void SetVerticalAlign(VerticalAlign align) { verticalAlign_ = align; MarkInstancesDirty(); }
    VerticalAlign GetVerticalAlign() const noexcept { return verticalAlign_; }

    /// @brief 各文字のデフォルトアンカー（単位クアッド内の正規化座標。(0,0)=左下 ～ (1,1)=右上、既定(0.5,0.5)）
    void SetDefaultCharacterAnchor(const Vector2 &anchor) { defaultCharacterAnchor_ = anchor; MarkInstancesDirty(); }
    const Vector2 &GetDefaultCharacterAnchor() const noexcept { return defaultCharacterAnchor_; }
    /// @brief 各文字のデフォルトピボット（回転・拡縮の中心にする単位クアッド内の正規化座標、既定(0.5,0.5)）
    void SetDefaultCharacterPivot(const Vector2 &pivot) { defaultCharacterPivot_ = pivot; MarkInstancesDirty(); }
    const Vector2 &GetDefaultCharacterPivot() const noexcept { return defaultCharacterPivot_; }

    //==================================================
    // 文字ごとのアクセス（Transform的な調整。ランタイム専用・非シリアライズ）
    //==================================================

    /// @brief タグを除いた実際の文字数（改行文字も1つとしてカウントする）を取得する
    size_t GetCharacterCount() const {
        RebuildShapeIfDirty();
        return characterOverrides_.size();
    }
    void SetCharacterOffset(size_t index, const Vector2 &offset) {
        RebuildShapeIfDirty();
        if (index >= characterOverrides_.size()) return;
        characterOverrides_[index].offset = offset;
        MarkInstancesDirty();
    }
    Vector2 GetCharacterOffset(size_t index) const {
        RebuildShapeIfDirty();
        return index < characterOverrides_.size() ? characterOverrides_[index].offset : Vector2(0.0f, 0.0f);
    }
    void SetCharacterRotation(size_t index, float rotation) {
        RebuildShapeIfDirty();
        if (index >= characterOverrides_.size()) return;
        characterOverrides_[index].rotation = rotation;
        MarkInstancesDirty();
    }
    float GetCharacterRotation(size_t index) const {
        RebuildShapeIfDirty();
        return index < characterOverrides_.size() ? characterOverrides_[index].rotation : 0.0f;
    }
    void SetCharacterScale(size_t index, const Vector2 &scale) {
        RebuildShapeIfDirty();
        if (index >= characterOverrides_.size()) return;
        characterOverrides_[index].scale = scale;
        MarkInstancesDirty();
    }
    Vector2 GetCharacterScale(size_t index) const {
        RebuildShapeIfDirty();
        return index < characterOverrides_.size() ? characterOverrides_[index].scale : Vector2(1.0f, 1.0f);
    }

    //==================================================
    // 描画情報取得（Renderer専用パスから呼ばれる）
    //==================================================

    /// @brief 現在のTransformワールド行列まで合成した、描画に使う文字ごとのインスタンス一覧を取得する
    std::vector<RenderCharacterInstance> GetRenderInstances() const {
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
        auto *sceneRenderer = GetOrAddSceneRenderer();
        if (sceneRenderer) {
            sceneRenderer->RegisterTextRenderer(this);
        }
    }

    void Finalize() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
        if (sceneRenderer) {
            sceneRenderer->UnregisterTextRenderer(this);
        }
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        TargetObjectSelector::ShowSelector(TranslationLabel("component.common.target"), GetOwnerSceneContext(), targetObjectID_);
        TargetObjectSelector::ShowRenderTargetFilters(GetOwnerSceneContext(), targetObjectID_, excludedRenderTargetNames_);

        if (ImGui::InputTextMultiline(TranslationLabel("component.textrenderer.text"), &text_)) {
            MarkShapeDirty();
        }
        ImGui::TextDisabled(TranslationC("component.textrenderer.desc_1"));

        std::vector<std::string> fontNames;
        for (const auto &entry : FontManager::GetLoadedFontListEntries()) {
            fontNames.push_back(entry.name);
        }
        if (ImGuiCustom::SelectString(TranslationLabel("component.textrenderer.font"), fontName_, fontNames, true)) {
            fontHandle_ = FontManager::kInvalidHandle;
            MarkShapeDirty();
        }
        if (ImGui::DragFloat(TranslationLabel("component.textrenderer.font_size"), &fontSize_, 0.01f, 0.01f, 1000.0f)) MarkShapeDirty();
        if (ImGui::ColorEdit4(TranslationLabel("component.textrenderer.color"), &color_.x)) MarkShapeDirty();

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

        if (ImGui::DragFloat2(TranslationLabel("component.textrenderer.default_character_anchor"), &defaultCharacterAnchor_.x, 0.01f)) MarkInstancesDirty();
        if (ImGui::DragFloat2(TranslationLabel("component.textrenderer.default_character_pivot"), &defaultCharacterPivot_.x, 0.01f)) MarkInstancesDirty();

        ImGuiCustom::SelectString(TranslationLabel("component.textrenderer.pipeline"), pipelineName_, PipelineManager::GetLoadedRenderPipelineNames());

        RebuildShapeIfDirty();
        if (!characterOverrides_.empty() && ImGui::TreeNode(TranslationLabel("component.textrenderer.character_overrides"))) {
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
        JSON json = JSON::object();
        json["text"] = text_;
        json["fontName"] = fontName_;
        json["fontSize"] = fontSize_;
        json["color"] = ToJSON(color_);
        json["pipelineName"] = pipelineName_;
        json["horizontalAlign"] = static_cast<int>(horizontalAlign_);
        json["verticalAlign"] = static_cast<int>(verticalAlign_);
        json["defaultCharacterAnchor"] = ToJSON(defaultCharacterAnchor_);
        json["defaultCharacterPivot"] = ToJSON(defaultCharacterPivot_);
        json["targetObjectID"] = ToJSON(targetObjectID_);
        for (const auto &name : excludedRenderTargetNames_) {
            json["excludedRenderTargetNames"].push_back(name);
        }
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        text_ = json.value("text", std::string{});
        fontName_ = json.value("fontName", std::string{});
        fontHandle_ = FontManager::kInvalidHandle;
        fontSize_ = json.value("fontSize", 32.0f);
        color_ = json.contains("color") ? FromJSON<Vector4>(json["color"]) : Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        pipelineName_ = json.value("pipelineName", std::string{ "Text2D.BlendNormal" });
        horizontalAlign_ = static_cast<HorizontalAlign>(json.value("horizontalAlign", 0));
        verticalAlign_ = static_cast<VerticalAlign>(json.value("verticalAlign", 0));
        defaultCharacterAnchor_ = json.contains("defaultCharacterAnchor") ? FromJSON<Vector2>(json["defaultCharacterAnchor"]) : Vector2(0.5f, 0.5f);
        defaultCharacterPivot_ = json.contains("defaultCharacterPivot") ? FromJSON<Vector2>(json["defaultCharacterPivot"]) : Vector2(0.5f, 0.5f);
        if (json.contains("targetObjectID")) {
            targetObjectID_ = FromJSON<UUID128>(json["targetObjectID"]);
        } else {
            targetObjectID_ = UUID128();
        }
        excludedRenderTargetNames_.clear();
        for (const auto &name : json.value("excludedRenderTargetNames", std::vector<std::string>())) {
            excludedRenderTargetNames_.insert(name);
        }
        MarkShapeDirty();
        return true;
    }

private:
    /// @brief リッチテキストのタグを解釈した後の1文字分の情報（レイアウト前）
    struct ShapedCharacter {
        char32_t codepoint = 0;
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        bool italic = false;
        bool bold = false;
        /// @brief fontSize_に対する追加倍率（<size>指定時のみ1.0以外になる）
        float sizeScale = 1.0f;
        bool strikethrough = false;
        bool underline = false;
        /// @brief 0=通常 / 1=下付き(<sub>) / 2=上付き(<sup>)
        int scriptMode = 0;
    };

    /// @brief 行内配置が確定した1文字分の情報（レイアウト計算の中間データ）
    struct PositionedChar {
        size_t shapedIndex = 0;
        float penX = 0.0f;    ///< 行内ローカルX（ベイクピクセル単位、行頭=0）
        float penY = 0.0f;    ///< ベースラインからの追加オフセット（sub/sup用、ベイクピクセル単位）
        float scale = 1.0f;   ///< sizeScale × sub/sup分のスケール
        float advance = 0.0f; ///< このグリフの送り幅（ベイクピクセル単位、追加スケール適用後）
    };

    SceneRenderer *GetOrAddSceneRenderer() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return nullptr;
        auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
        if (!sceneRenderer) {
            sceneRenderer = sceneContext->AddComponent<SceneRenderer>();
        }
        return sceneRenderer;
    }

    void MarkShapeDirty() const { shapeDirty_ = true; instancesDirty_ = true; }
    void MarkInstancesDirty() const { instancesDirty_ = true; }

    /// @brief "#RRGGBB"または"#RRGGBBAA"形式の色文字列を解釈する（不正な場合はfallbackを返す）
    static Vector4 ParseColorTagValue(const std::string &value, const Vector4 &fallback) {
        if (value.size() < 7 || value[0] != '#') return fallback;
        auto hexNibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        auto byteAt = [&](size_t idx) -> int {
            if (idx + 1 >= value.size()) return -1;
            const int hi = hexNibble(value[idx]);
            const int lo = hexNibble(value[idx + 1]);
            if (hi < 0 || lo < 0) return -1;
            return hi * 16 + lo;
        };
        const int r = byteAt(1);
        const int g = byteAt(3);
        const int b = byteAt(5);
        if (r < 0 || g < 0 || b < 0) return fallback;
        int a = 255;
        if (value.size() >= 9) {
            const int parsedAlpha = byteAt(7);
            if (parsedAlpha >= 0) a = parsedAlpha;
        }
        return Vector4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }

    /// @brief "N"（ワールド単位の絶対値）または"N%"（fontSize_に対する割合）形式のサイズ指定を倍率へ変換する
    float ParseSizeTagValue(const std::string &value, float fallbackScale) const {
        if (value.empty()) return fallbackScale;
        if (value.back() == '%') {
            const float percent = static_cast<float>(std::atof(value.substr(0, value.size() - 1).c_str()));
            return percent / 100.0f;
        }
        const float absolute = static_cast<float>(std::atof(value.c_str()));
        if (fontSize_ <= 0.0f) return fallbackScale;
        return absolute / fontSize_;
    }

    /// @brief リッチテキストを解析し、タグを除いた文字ごとの見た目情報を求める
    std::vector<ShapedCharacter> ParseRichText(const std::string &text, const Vector4 &baseColor) const {
        std::vector<ShapedCharacter> result;
        const auto codepoints = Utf8ToCodepoints(text);
        result.reserve(codepoints.size());

        struct StyleState {
            Vector4 color;
            bool italic = false;
            bool bold = false;
            float sizeScale = 1.0f;
            bool strikethrough = false;
            bool underline = false;
            int scriptMode = 0;
        };
        std::vector<StyleState> styleStack;
        styleStack.push_back(StyleState{ baseColor });

        size_t i = 0;
        while (i < codepoints.size()) {
            if (codepoints[i] == U'<') {
                size_t j = i + 1;
                std::string tagBuffer;
                bool closedProperly = false;
                while (j < codepoints.size() && tagBuffer.size() < 40) {
                    if (codepoints[j] == U'>') { closedProperly = true; break; }
                    if (codepoints[j] > 0x7Fu) break;
                    tagBuffer.push_back(static_cast<char>(codepoints[j]));
                    ++j;
                }
                if (closedProperly) {
                    const bool isClosing = !tagBuffer.empty() && tagBuffer[0] == '/';
                    const std::string body = isClosing ? tagBuffer.substr(1) : tagBuffer;
                    std::string tagName = body;
                    std::string tagValue;
                    if (const auto eq = body.find('='); eq != std::string::npos) {
                        tagName = body.substr(0, eq);
                        tagValue = body.substr(eq + 1);
                    }

                    const bool recognized = (tagName == "i" || tagName == "b" || tagName == "color" ||
                        tagName == "size" || tagName == "s" || tagName == "u" ||
                        tagName == "sub" || tagName == "sup");

                    if (recognized) {
                        if (isClosing) {
                            if (styleStack.size() > 1) styleStack.pop_back();
                        } else {
                            StyleState next = styleStack.back();
                            if (tagName == "i") next.italic = true;
                            else if (tagName == "b") next.bold = true;
                            else if (tagName == "color") next.color = ParseColorTagValue(tagValue, next.color);
                            else if (tagName == "size") next.sizeScale = ParseSizeTagValue(tagValue, next.sizeScale);
                            else if (tagName == "s") next.strikethrough = true;
                            else if (tagName == "u") next.underline = true;
                            else if (tagName == "sub") next.scriptMode = 1;
                            else if (tagName == "sup") next.scriptMode = 2;
                            styleStack.push_back(next);
                        }
                        i = j + 1;
                        continue;
                    }
                }
            }

            const StyleState &style = styleStack.back();
            ShapedCharacter ch;
            ch.codepoint = codepoints[i];
            ch.color = style.color;
            ch.italic = style.italic;
            ch.bold = style.bold;
            ch.sizeScale = style.sizeScale;
            ch.strikethrough = style.strikethrough;
            ch.underline = style.underline;
            ch.scriptMode = style.scriptMode;
            result.push_back(ch);
            ++i;
        }

        return result;
    }

    void RebuildShapeIfDirty() const {
        if (!shapeDirty_) return;
        shapeDirty_ = false;
        shapedCharacters_ = ParseRichText(text_, color_);
        characterOverrides_.resize(shapedCharacters_.size());
    }

    /// @brief アンカー/ピボット補正（SpriteRenderer::GetWorldMatrixと同じ考え方。単位クアッド内の正規化座標）
    void ApplyAnchorPivot(Matrix4x4 &mat) const {
        const float pivotOffsetX = 0.5f - defaultCharacterPivot_.x;
        const float pivotOffsetY = 0.5f - defaultCharacterPivot_.y;
        if (pivotOffsetX != 0.0f || pivotOffsetY != 0.0f) {
            mat.m[3][0] += pivotOffsetX * mat.m[0][0] + pivotOffsetY * mat.m[1][0];
            mat.m[3][1] += pivotOffsetX * mat.m[0][1] + pivotOffsetY * mat.m[1][1];
            mat.m[3][2] += pivotOffsetX * mat.m[0][2] + pivotOffsetY * mat.m[1][2];
        }
        const float anchorDeltaX = defaultCharacterAnchor_.x - defaultCharacterPivot_.x;
        const float anchorDeltaY = defaultCharacterAnchor_.y - defaultCharacterPivot_.y;
        if (anchorDeltaX != 0.0f || anchorDeltaY != 0.0f) {
            mat.m[3][0] -= anchorDeltaX * mat.m[0][0] + anchorDeltaY * mat.m[1][0];
            mat.m[3][1] -= anchorDeltaX * mat.m[0][1] + anchorDeltaY * mat.m[1][1];
            mat.m[3][2] -= anchorDeltaX * mat.m[0][2] + anchorDeltaY * mat.m[1][2];
        }
    }

    void AppendGlyphInstance(const PositionedChar &pc, float lineOffsetX, float lineBaselineY, float worldScale,
        const FontManager::GlyphInfo &glyph) const {
        const ShapedCharacter &shaped = shapedCharacters_[pc.shapedIndex];
        const CharacterOverride &ov = characterOverrides_[pc.shapedIndex];

        const float centerXBake = pc.penX + lineOffsetX + (glyph.xoff + glyph.width * 0.5f) * pc.scale;
        const float centerYBake = pc.penY + lineBaselineY - (glyph.yoff + glyph.height * 0.5f) * pc.scale;
        const float quadW = glyph.width * pc.scale * worldScale;
        const float quadH = glyph.height * pc.scale * worldScale;
        const Vector3 centerWorld(centerXBake * worldScale + ov.offset.x, centerYBake * worldScale + ov.offset.y, 0.0f);

        Matrix4x4 scaleMat;
        scaleMat.MakeScale(Vector3(quadW, quadH, 1.0f));

        Matrix4x4 shearMat = Matrix4x4::Identity();
        if (shaped.italic) {
            constexpr float kItalicShear = 0.25f;
            shearMat.m[1][0] = kItalicShear;
        }

        Matrix4x4 overrideScaleMat;
        overrideScaleMat.MakeScale(Vector3(ov.scale.x, ov.scale.y, 1.0f));

        Matrix4x4 rotateMat;
        rotateMat.MakeRotateZ(ov.rotation);

        Matrix4x4 translateMat;
        translateMat.MakeTranslate(centerWorld);

        Matrix4x4 local = scaleMat * shearMat * overrideScaleMat * rotateMat * translateMat;
        ApplyAnchorPivot(local);

        constexpr float kBoldWeight = 0.10f;

        RenderCharacterInstance instance;
        instance.worldMatrix = local;
        instance.color = shaped.color;
        instance.u0 = glyph.u0; instance.v0 = glyph.v0; instance.u1 = glyph.u1; instance.v1 = glyph.v1;
        instance.boldWeight = shaped.bold ? kBoldWeight : 0.0f;
        localInstances_.push_back(instance);
    }

    void AppendDecorationRuns(const std::vector<PositionedChar> &line, float lineOffsetX, float lineBaselineY,
        float worldScale, FontManager::FontHandle fontHandle, bool strikethroughMode) const {
        const auto *solid = FontManager::GetSolidGlyph(fontHandle);
        if (!solid) return;

        size_t i = 0;
        while (i < line.size()) {
            const ShapedCharacter &first = shapedCharacters_[line[i].shapedIndex];
            const bool active = strikethroughMode ? first.strikethrough : first.underline;
            if (!active) { ++i; continue; }
            size_t j = i;
            while (j < line.size()) {
                const ShapedCharacter &cur = shapedCharacters_[line[j].shapedIndex];
                if ((strikethroughMode ? cur.strikethrough : cur.underline) != active) break;
                ++j;
            }

            const float startX = line[i].penX;
            const float endX = line[j - 1].penX + line[j - 1].advance;
            const float thicknessBake = FontManager::kBakePixelHeight * 0.06f;
            const float yBake = strikethroughMode
                ? FontManager::kBakePixelHeight * 0.28f
                : -FontManager::kBakePixelHeight * 0.08f;

            const float centerXBake = (startX + endX) * 0.5f + lineOffsetX;
            const float centerYBake = line[i].penY + lineBaselineY + yBake;
            const float widthWorld = std::max((endX - startX) * worldScale, 0.0f);
            const float thicknessWorld = thicknessBake * worldScale;

            Matrix4x4 scaleMat;
            scaleMat.MakeScale(Vector3(widthWorld, thicknessWorld, 1.0f));
            Matrix4x4 translateMat;
            translateMat.MakeTranslate(Vector3(centerXBake * worldScale, centerYBake * worldScale, 0.0f));
            Matrix4x4 local = scaleMat * translateMat;
            ApplyAnchorPivot(local);

            RenderCharacterInstance instance;
            instance.worldMatrix = local;
            instance.color = first.color;
            instance.u0 = solid->u0; instance.v0 = solid->v0; instance.u1 = solid->u1; instance.v1 = solid->v1;
            instance.boldWeight = 0.0f;
            localInstances_.push_back(instance);

            i = j;
        }
    }

    void RebuildInstancesIfDirty() const {
        RebuildShapeIfDirty();
        if (!instancesDirty_) return;
        instancesDirty_ = false;
        localInstances_.clear();

        const FontManager::FontHandle fontHandle = GetFontHandle();
        if (fontHandle == FontManager::kInvalidHandle || shapedCharacters_.empty()) return;

        constexpr float kSubScale = 0.6f;
        constexpr float kSupScale = 0.6f;
        constexpr float kSubOffsetRatio = -0.15f;
        constexpr float kSupOffsetRatio = 0.35f;

        std::vector<std::vector<PositionedChar>> lines;
        std::vector<float> lineWidths;
        lines.emplace_back();
        lineWidths.push_back(0.0f);

        float penX = 0.0f;
        const float lineHeightPixels = FontManager::GetLineHeight(fontHandle, FontManager::kBakePixelHeight);

        for (size_t i = 0; i < shapedCharacters_.size(); ++i) {
            const ShapedCharacter &ch = shapedCharacters_[i];
            if (ch.codepoint == U'\n') {
                lineWidths.back() = penX;
                lines.emplace_back();
                lineWidths.push_back(0.0f);
                penX = 0.0f;
                continue;
            }

            const auto *glyph = FontManager::GetOrBakeGlyph(fontHandle, ch.codepoint);
            float scriptScale = 1.0f;
            float baselineOffset = 0.0f;
            if (ch.scriptMode == 1) { scriptScale = kSubScale; baselineOffset = kSubOffsetRatio * FontManager::kBakePixelHeight; }
            else if (ch.scriptMode == 2) { scriptScale = kSupScale; baselineOffset = kSupOffsetRatio * FontManager::kBakePixelHeight; }
            const float totalScale = ch.sizeScale * scriptScale;
            const float advance = glyph ? glyph->advance * totalScale : 0.0f;

            PositionedChar pc;
            pc.shapedIndex = i;
            pc.penX = penX;
            pc.penY = baselineOffset * ch.sizeScale;
            pc.scale = totalScale;
            pc.advance = advance;
            lines.back().push_back(pc);

            penX += advance;
        }
        lineWidths.back() = penX;

        const float totalBlockHeight = static_cast<float>(lines.size()) * lineHeightPixels;
        float startY = 0.0f;
        switch (verticalAlign_) {
            case VerticalAlign::Top:    startY = 0.0f; break;
            case VerticalAlign::Middle: startY = totalBlockHeight * 0.5f; break;
            case VerticalAlign::Bottom: startY = totalBlockHeight; break;
        }

        const float worldScale = fontSize_ / FontManager::kBakePixelHeight;

        for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            float lineOffsetX = 0.0f;
            switch (horizontalAlign_) {
                case HorizontalAlign::Left:   lineOffsetX = 0.0f; break;
                case HorizontalAlign::Center: lineOffsetX = -lineWidths[lineIndex] * 0.5f; break;
                case HorizontalAlign::Right:  lineOffsetX = -lineWidths[lineIndex]; break;
            }
            const float lineBaselineY = startY - static_cast<float>(lineIndex) * lineHeightPixels;

            for (const auto &pc : lines[lineIndex]) {
                const auto *glyph = FontManager::GetOrBakeGlyph(fontHandle, shapedCharacters_[pc.shapedIndex].codepoint);
                if (!glyph || !glyph->isValid) continue;
                AppendGlyphInstance(pc, lineOffsetX, lineBaselineY, worldScale, *glyph);
            }

            AppendDecorationRuns(lines[lineIndex], lineOffsetX, lineBaselineY, worldScale, fontHandle, true);
            AppendDecorationRuns(lines[lineIndex], lineOffsetX, lineBaselineY, worldScale, fontHandle, false);
        }
    }

    UUID128 targetObjectID_{};
    std::string pipelineName_ = "Text2D.BlendNormal";
    std::unordered_set<std::string> excludedRenderTargetNames_;

    std::string text_;
    std::string fontName_;
    mutable FontManager::FontHandle fontHandle_ = FontManager::kInvalidHandle;
    float fontSize_ = 32.0f;
    Vector4 color_{ 1.0f, 1.0f, 1.0f, 1.0f };

    HorizontalAlign horizontalAlign_ = HorizontalAlign::Left;
    VerticalAlign verticalAlign_ = VerticalAlign::Top;
    /// @brief 各文字のデフォルトアンカー（単位クアッド内の正規化座標、既定(0.5,0.5)=中心）
    Vector2 defaultCharacterAnchor_{ 0.5f, 0.5f };
    /// @brief 各文字のデフォルトピボット（単位クアッド内の正規化座標、既定(0.5,0.5)=中心）
    Vector2 defaultCharacterPivot_{ 0.5f, 0.5f };

    mutable bool shapeDirty_ = true;
    mutable bool instancesDirty_ = true;
    mutable std::vector<ShapedCharacter> shapedCharacters_;
    /// @brief 文字ごとの位置/回転/スケール上書き（ランタイム専用。非シリアライズ）
    mutable std::vector<CharacterOverride> characterOverrides_;
    /// @brief 所有オブジェクトのTransformを合成する前の、文字ごとのローカル描画インスタンス
    mutable std::vector<RenderCharacterInstance> localInstances_;
};

REGISTER_COMPONENT_OBJECT(TextRenderer)

} // namespace KashipanEngine
