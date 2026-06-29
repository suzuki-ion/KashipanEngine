#include "Text3D.h"

#include <algorithm>
#include <filesystem>
#include <utf8.h>

#include "Assets/TextureManager.h"
#include "Font/FontLoader.h"
#include "Objects/Components/3D/Material3D.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Objects/GameObjects/3D/VertexData3D.h"
#include "Utilities/RandomValue.h"

namespace KashipanEngine {

Text3D::Text3D(uint32_t textCount)
    : EmptyObject("Text3D") {
    planes_.reserve(textCount);
    textCodePoints_.resize(textCount, -1);
    basePositions_.resize(textCount, Vector3{ 0.0f, 0.0f, 0.0f });

    spriteBatchKey_ = GetRandomValue<uint64_t>(0, UINT64_MAX);

    for (uint32_t i = 0; i < textCount; ++i) {
        auto plane = std::make_unique<Plane3D>();
        plane->SetBatchKey(spriteBatchKey_);
        plane->SetName(std::string("TextChar_") + std::to_string(i));
        auto vertexData = plane->GetVertexData<VertexData3D>();
        for (auto &v : vertexData) {
            v.position += Vector4{ 0.5f, -0.5f, 0.0f, 0.0f };
        }

        if (auto *tr = plane->GetComponent3D<Transform3D>()) {
            tr->SetParentObject(this);
            tr->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
            tr->SetTranslate(Vector3{ 0.0f, 0.0f, 0.0f });
        }

        planes_.push_back(std::move(plane));
    }
}

void Text3D::SetFont(const char *fontFilePath) {
    fontData_ = LoadFNT(fontFilePath);
    CalculateFontSizeScale();
    ResolveFontTextures();
    RebuildTextLayout();
}

void Text3D::SetFont(const FontData &fontData) {
    fontData_ = fontData;
    CalculateFontSizeScale();
    ResolveFontTextures();
    RebuildTextLayout();
}

void Text3D::SetText(const std::u8string &text) {
    if (text_ == text) {
        return;
    }

    text_ = text;
    RebuildTextLayout();
}

void Text3D::SetTextAlign(TextAlignX textAlignX, TextAlignY textAlignY) {
    if (textAlignX_ == textAlignX && textAlignY_ == textAlignY) return;
    textAlignX_ = textAlignX;
    textAlignY_ = textAlignY;
    ApplyTextAlign();
}

Plane3D *Text3D::operator[](size_t index) {
    if (index >= planes_.size()) return nullptr;
    return planes_[index].get();
}

const Plane3D *Text3D::operator[](size_t index) const {
    if (index >= planes_.size()) return nullptr;
    return planes_[index].get();
}

void Text3D::AttachToRenderer(Window *targetWindow, const std::string &pipelineName) {
    for (auto &plane : planes_) {
        if (!plane) continue;
        plane->AttachToRenderer(targetWindow, pipelineName);
    }
}

void Text3D::AttachToRenderer(ScreenBuffer *targetBuffer, const std::string &pipelineName) {
    for (auto &plane : planes_) {
        if (!plane) continue;
        plane->AttachToRenderer(targetBuffer, pipelineName);
    }
}

void Text3D::DetachFromRenderer() {
    for (auto &plane : planes_) {
        if (!plane) continue;
        plane->DetachFromRenderer();
    }
}

void Text3D::OnUpdate() {
    for (auto &plane : planes_) {
        if (plane) plane->Update();
    }
}

void Text3D::CalculateFontSizeScale() {
    const float size = static_cast<float>(fontData_.info.size);
    fontSizeScale_ = (size > 0.0f) ? (1.0f / size) : 1.0f;
}

void Text3D::RebuildTextLayout() {
    lineInfos_.clear();

    if (planes_.empty()) return;

    lineInfos_.emplace_back(LineInfo{});
    lineInfos_.back().beginPlaneIndex = 0;
    lineInfos_.back().height = fontData_.common.lineHeight * fontSizeScale_;

    float cursorX = 0.0f;
    float cursorY = 0.0f;
    size_t charIndex = 0;

    auto it = text_.begin();
    auto end = text_.end();

    while (it != end) {
        if (charIndex >= planes_.size()) break;

        const auto codePoint = utf8::next(it, end);

        if (codePoint == '\n') {
            lineInfos_.back().endPlaneIndex = static_cast<uint32_t>(charIndex);
            cursorX = 0.0f;
            cursorY += fontData_.common.lineHeight * fontSizeScale_;
            lineInfos_.emplace_back(LineInfo{});
            lineInfos_.back().beginPlaneIndex = static_cast<uint32_t>(charIndex);
            lineInfos_.back().height = fontData_.common.lineHeight * fontSizeScale_;
            continue;
        }

        auto charIt = fontData_.chars.find(codePoint);
        if (charIt == fontData_.chars.end()) {
            continue;
        }

        const auto &charData = charIt->second;
        UpdatePlaneForChar(charIndex, charData);

        textCodePoints_[charIndex] = codePoint;
        basePositions_[charIndex] = Vector3{ cursorX + charData.xOffset * fontSizeScale_, cursorY + charData.yOffset * fontSizeScale_, 0.0f };
        lineInfos_.back().width += charData.xAdvance * fontSizeScale_;
        cursorX += charData.xAdvance * fontSizeScale_;
        ++charIndex;
    }

    lineInfos_.back().endPlaneIndex = static_cast<uint32_t>(charIndex);

    for (size_t i = charIndex; i < planes_.size(); ++i) {
        textCodePoints_[i] = -1;
        basePositions_[i] = Vector3{ 0.0f, 0.0f, 0.0f };
        HidePlane(i);
    }

    ApplyTextAlign();
}

void Text3D::ApplyTextAlign() {
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

        for (uint32_t i = line.beginPlaneIndex; i < line.endPlaneIndex; ++i) {
            if (i >= planes_.size()) continue;
            auto *plane = planes_[i].get();
            if (!plane) continue;
            if (auto *tr = plane->GetComponent3D<Transform3D>()) {
                const auto &base = basePositions_[i];
                tr->SetTranslate(Vector3{ base.x + offsetX, -(base.y + offsetY), 0.0f });
            }
        }
    }
}

void Text3D::CalculateTextAlignX(TextAlignX /*newTextAlignX*/) {
    ApplyTextAlign();
}

void Text3D::CalculateTextAlignY(TextAlignY /*newTextAlignY*/) {
    ApplyTextAlign();
}

void Text3D::ResolveFontTextures() {
    for (auto &page : fontData_.pages) {
        const auto fileName = std::filesystem::path(page.file).filename().string();
        if (fileName.empty()) {
            page.textureIndex = TextureManager::kInvalidHandle;
            continue;
        }
        page.textureIndex = static_cast<int>(TextureManager::GetTextureFromFileName(fileName));
    }
}

void Text3D::HidePlane(size_t index) {
    if (index >= planes_.size()) return;
    auto *plane = planes_[index].get();
    if (!plane) return;
    if (auto *tr = plane->GetComponent3D<Transform3D>()) {
        tr->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
        tr->SetTranslate(Vector3{ 0.0f, 0.0f, 0.0f });
    }
}

void Text3D::UpdatePlaneForChar(size_t index, const CharInfo &charData) {
    if (index >= planes_.size()) return;
    auto *plane = planes_[index].get();
    if (!plane) return;

    if (auto *tr = plane->GetComponent3D<Transform3D>()) {
        tr->SetScale(Vector3{ charData.width * fontSizeScale_, charData.height * fontSizeScale_, 1.0f });
    }

    if (charData.page >= 0 && static_cast<size_t>(charData.page) < fontData_.pages.size()) {
        auto texHandle = static_cast<TextureManager::TextureHandle>(fontData_.pages[charData.page].textureIndex);
        if (texHandle != TextureManager::kInvalidHandle) {
            if (auto *mat = plane->GetComponent3D<Material3D>()) {
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

    if (auto *mat = plane->GetComponent3D<Material3D>()) {
        Material3D::UVTransform uvTransform;
        uvTransform.scale = Vector3{ u1 - u0, v1 - v0, 1.0f };
        uvTransform.rotate = Vector3{ 0.0f, 0.0f, 0.0f };
        uvTransform.translate = Vector3{ u0, v0, 0.0f };
        mat->SetUVTransform(uvTransform);
    }
}

std::u8string Text3D::ToU8String(const std::string &text) {
    return std::u8string(reinterpret_cast<const char8_t *>(text.data()), text.size());
}

} // namespace KashipanEngine