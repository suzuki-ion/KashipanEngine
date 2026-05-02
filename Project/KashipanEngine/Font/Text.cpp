#include "Text.h"

#include <algorithm>
#include <filesystem>
#include <utf8.h>

#include "Assets/TextureManager.h"
#include "Font/FontLoader.h"
#include "Objects/Components/2D/Material2D.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Objects/GameObjects/2D/VertexData2D.h"
#include "Utilities/RandomValue.h"

namespace KashipanEngine {

Text::Text(uint32_t textCount)
    : Object2DBase("Text") {
    sprites_.reserve(textCount);
    textCodePoints_.resize(textCount, -1);
    basePositions_.resize(textCount, Vector2{0.0f, 0.0f});

    spriteBatchKey_ = GetRandomValue<uint64_t>(0, UINT64_MAX);

    parentTransform_ = GetComponent2D<Transform2D>();

    for (uint32_t i = 0; i < textCount; ++i) {
        auto sprite = std::make_unique<Sprite>();
        sprite->SetBatchKey(spriteBatchKey_);
        sprite->SetName(std::string("TextChar_") + std::to_string(i));
        sprite->SetPivotPoint(0.0f, 0.0f);

        if (auto *tr = sprite->GetComponent2D<Transform2D>()) {
            if (parentTransform_) {
                tr->SetParentTransform(parentTransform_);
            }
            tr->SetScale(Vector2{0.0f, 0.0f});
            tr->SetTranslate(Vector2{0.0f, 0.0f});
        }

        sprites_.push_back(std::move(sprite));
    }
}

void Text::SetFont(const char *fontFilePath) {
    fontData_ = LoadFNT(fontFilePath);
    ResolveFontTextures();
    RebuildTextLayout();
}

void Text::SetFont(const FontData &fontData) {
    fontData_ = fontData;
    ResolveFontTextures();
    RebuildTextLayout();
}

void Text::SetText(const std::u8string &text) {
    if (text_ == text) {
        return;
    }

    text_ = text;
    RebuildTextLayout();
}

void Text::SetTextAlign(TextAlignX textAlignX, TextAlignY textAlignY) {
    if (textAlignX_ == textAlignX && textAlignY_ == textAlignY) return;
    textAlignX_ = textAlignX;
    textAlignY_ = textAlignY;
    ApplyTextAlign();
}

Sprite *Text::operator[](size_t index) {
    if (index >= sprites_.size()) return nullptr;
    return sprites_[index].get();
}

const Sprite *Text::operator[](size_t index) const {
    if (index >= sprites_.size()) return nullptr;
    return sprites_[index].get();
}

void Text::AttachToRenderer(Window *targetWindow, const std::string &pipelineName) {
    for (auto &sprite : sprites_) {
        if (!sprite) continue;
        sprite->AttachToRenderer(targetWindow, pipelineName);
    }
}

void Text::AttachToRenderer(ScreenBuffer *targetBuffer, const std::string &pipelineName) {
    for (auto &sprite : sprites_) {
        if (!sprite) continue;
        sprite->AttachToRenderer(targetBuffer, pipelineName);
    }
}

void Text::DetachFromRenderer() {
    for (auto &sprite : sprites_) {
        if (!sprite) continue;
        sprite->DetachFromRenderer();
    }
}

void Text::OnUpdate() {
    for (auto &sprite : sprites_) {
        if (sprite) sprite->Update();
    }
}

void Text::RebuildTextLayout() {
    lineInfos_.clear();

    if (sprites_.empty()) return;

    lineInfos_.emplace_back(LineInfo{});
    lineInfos_.back().beginSpriteIndex = 0;
    lineInfos_.back().height = fontData_.common.lineHeight;

    float cursorX = 0.0f;
    float cursorY = 0.0f;
    size_t charIndex = 0;

    auto it = text_.begin();
    auto end = text_.end();

    while (it != end) {
        if (charIndex >= sprites_.size()) break;

        const auto codePoint = utf8::next(it, end);

        if (codePoint == '\n') {
            lineInfos_.back().endSpriteIndex = static_cast<uint32_t>(charIndex);
            cursorX = 0.0f;
            cursorY += fontData_.common.lineHeight;
            lineInfos_.emplace_back(LineInfo{});
            lineInfos_.back().beginSpriteIndex = static_cast<uint32_t>(charIndex);
            lineInfos_.back().height = fontData_.common.lineHeight;
            continue;
        }

        auto charIt = fontData_.chars.find(codePoint);
        if (charIt == fontData_.chars.end()) {
            continue;
        }

        const auto &charData = charIt->second;
        UpdateSpriteForChar(charIndex, charData);

        textCodePoints_[charIndex] = codePoint;
        basePositions_[charIndex] = Vector2{cursorX + charData.xOffset, cursorY + charData.yOffset};
        lineInfos_.back().width += charData.xAdvance;
        cursorX += charData.xAdvance;
        ++charIndex;
    }

    lineInfos_.back().endSpriteIndex = static_cast<uint32_t>(charIndex);

    for (size_t i = charIndex; i < sprites_.size(); ++i) {
        textCodePoints_[i] = -1;
        basePositions_[i] = Vector2{0.0f, 0.0f};
        HideSprite(i);
    }

    ApplyTextAlign();
}

void Text::ApplyTextAlign() {
    if (lineInfos_.empty()) return;

    float totalHeight = 0.0f;
    for (const auto &line : lineInfos_) {
        totalHeight += line.height;
    }

    float offsetY = 0.0f;
    if (textAlignY_ == TextAlignY::Center) {
        offsetY = -totalHeight * 0.5f;
    } else if (textAlignY_ == TextAlignY::Bottom) {
        offsetY = -totalHeight;
    }

    for (const auto &line : lineInfos_) {
        float offsetX = 0.0f;
        if (textAlignX_ == TextAlignX::Center) {
            offsetX = -line.width * 0.5f;
        } else if (textAlignX_ == TextAlignX::Right) {
            offsetX = -line.width;
        }

        for (uint32_t i = line.beginSpriteIndex; i < line.endSpriteIndex; ++i) {
            if (i >= sprites_.size()) continue;
            auto *sprite = sprites_[i].get();
            if (!sprite) continue;
            if (auto *tr = sprite->GetComponent2D<Transform2D>()) {
                const auto &base = basePositions_[i];
                tr->SetTranslate(Vector2{base.x + offsetX, -(base.y + offsetY)});
            }
        }
    }
}

void Text::CalculateTextAlignX(TextAlignX /*newTextAlignX*/) {
    ApplyTextAlign();
}

void Text::CalculateTextAlignY(TextAlignY /*newTextAlignY*/) {
    ApplyTextAlign();
}

void Text::ResolveFontTextures() {
    for (auto &page : fontData_.pages) {
        const auto fileName = std::filesystem::path(page.file).filename().string();
        if (fileName.empty()) {
            page.textureIndex = TextureManager::kInvalidHandle;
            continue;
        }
        page.textureIndex = static_cast<int>(TextureManager::GetTextureFromFileName(fileName));
    }
}

void Text::HideSprite(size_t index) {
    if (index >= sprites_.size()) return;
    auto *sprite = sprites_[index].get();
    if (!sprite) return;
    if (auto *tr = sprite->GetComponent2D<Transform2D>()) {
        tr->SetScale(Vector2{0.0f, 0.0f});
        tr->SetTranslate(Vector2{0.0f, 0.0f});
    }
}

void Text::UpdateSpriteForChar(size_t index, const CharInfo &charData) {
    if (index >= sprites_.size()) return;
    auto *sprite = sprites_[index].get();
    if (!sprite) return;

    if (auto *tr = sprite->GetComponent2D<Transform2D>()) {
        tr->SetScale(Vector2{charData.width, charData.height});
    }

    if (charData.page >= 0 && static_cast<size_t>(charData.page) < fontData_.pages.size()) {
        auto texHandle = static_cast<TextureManager::TextureHandle>(fontData_.pages[charData.page].textureIndex);
        if (texHandle != TextureManager::kInvalidHandle) {
            if (auto *mat = sprite->GetComponent2D<Material2D>()) {
                mat->SetTexture(texHandle);
            }
        }
    }

    const float invW = (fontData_.common.scaleW == 0.0f) ? 0.0f : (1.0f / fontData_.common.scaleW);
    const float invH = (fontData_.common.scaleH == 0.0f) ? 0.0f : (1.0f / fontData_.common.scaleH);

    const float u0 = charData.x * invW;
    const float v0 = charData.y * invH;
    const float u1 = (charData.x + charData.width) * invW;
    const float v1 = (charData.y + charData.height) * invH;

    if (auto *mat = sprite->GetComponent2D<Material2D>()) {
        Material2D::UVTransform uvTransform;
        uvTransform.scale = Vector3{u1 - u0, v1 - v0, 1.0f};
        uvTransform.rotate = Vector3{0.0f, 0.0f, 0.0f};
        uvTransform.translate = Vector3{u0, v0, 0.0f};
        mat->SetUVTransform(uvTransform);
    }
}

std::u8string Text::ToU8String(const std::string &text) {
    return std::u8string(reinterpret_cast<const char8_t *>(text.data()), text.size());
}

} // namespace KashipanEngine