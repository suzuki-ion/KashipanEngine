#pragma once
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Font/FontStructs.h"
#include "Math/Vector2.h"
#include "Objects/Object2DBase.h"
#include "Objects/GameObjects/2D/Sprite.h"

namespace KashipanEngine {

enum class TextAlignX {
    Left,   ///< 左揃え
    Center, ///< 中央揃え
    Right,  ///< 右揃え
};

enum class TextAlignY {
    Top,    ///< 上揃え
    Center, ///< 中央揃え
    Bottom, ///< 下揃え
};

class ScreenBuffer;
class Window;

class Text : public Object2DBase {
public:
    struct LineInfo {
        float width = 0.0f;
        float height = 0.0f;
        uint32_t beginSpriteIndex = 0;
        uint32_t endSpriteIndex = 0;
    };

    Text() = delete;
    /// @brief テキストのコンストラクタ
    /// @param textCount テキストの文字数
    explicit Text(uint32_t textCount = 128);
    ~Text() override = default;

    /// @brief テキストのフォントの設定
    /// @param fontFilePath フォントファイル(.fnt)のパス
    void SetFont(const char *fontFilePath);

    /// @brief テキストのフォントの設定
    /// @param fontData フォントデータ
    void SetFont(const FontData &fontData);

    /// @brief テキストの設定(UTF8文字列)
    /// @param text テキストの文字列(""の前にu8と付けたUTF8形式の文字列。例: u8"Hello, World!")
    void SetText(const std::u8string &text);

    /// @brief テキストの設定
    /// @param text テキストの文字列
    void SetText(const std::string &text) {
        SetText(ToU8String(text));
    }

    /// @brief テキストの設定（`std::format` スタイル）
    template<typename... Args>
    void SetTextFormat(std::format_string<Args...> format, Args&&... args) {
        const std::string formatted = std::format(format, std::forward<Args>(args)...);
        SetText(ToU8String(formatted));
    }

    /// @brief テキストの揃え方を設定する
    /// @param textAlignX 水平方向の揃え方
    /// @param textAlignY 垂直方向の揃え方
    void SetTextAlign(TextAlignX textAlignX, TextAlignY textAlignY);

    /// @brief テキストのフォントデータを取得する
    /// @return テキストのフォントデータ
    const FontData &GetFontData() const { return fontData_; }

    /// @brief テキストの文字列を取得する
    /// @return テキストの文字列
    const std::u8string &GetText() const { return text_; }

    /// @brief テキストの文字数を取得する
    /// @return テキストの文字数
    int GetTextCount() const { return static_cast<int>(text_.size()); }

    /// @brief 文字スプライトへのアクセス
    Sprite *operator[](size_t index);
    const Sprite *operator[](size_t index) const;

    /// @brief 文字スプライトを描画対象に登録
    void AttachToRenderer(Window *targetWindow, const std::string &pipelineName);
    /// @brief 文字スプライトを描画対象に登録
    void AttachToRenderer(ScreenBuffer *targetBuffer, const std::string &pipelineName);
    /// @brief 文字スプライトの描画登録を解除
    void DetachFromRenderer();

protected:
    void OnUpdate() override;

private:
    void RebuildTextLayout();
    void ApplyTextAlign();
    void CalculateTextAlignX(TextAlignX newTextAlignX);
    void CalculateTextAlignY(TextAlignY newTextAlignY);
    void ResolveFontTextures();
    void HideSprite(size_t index);
    void UpdateSpriteForChar(size_t index, const CharInfo &charData);
    static std::u8string ToU8String(const std::string &text);

    FontData fontData_;
    std::u8string text_;
    std::vector<int> textCodePoints_;
    std::vector<LineInfo> lineInfos_;
    std::vector<std::unique_ptr<Sprite>> sprites_;
    std::vector<Vector2> basePositions_;
    Transform2D *parentTransform_ = nullptr;
    TextAlignX textAlignX_ = TextAlignX::Left;
    TextAlignY textAlignY_ = TextAlignY::Top;

    uint64_t spriteBatchKey_ = 0;
};

} // namespace KashipanEngine