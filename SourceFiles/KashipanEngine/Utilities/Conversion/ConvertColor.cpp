#include "ConvertColor.h"
#include <algorithm>

namespace KashipanEngine {

namespace {
constexpr unsigned int kRedShift = 24;
constexpr unsigned int kGreenShift = 16;
constexpr unsigned int kBlueShift = 8;
constexpr unsigned int kColorChannelMask = 0xFF;
constexpr float kMaxColorChannel = static_cast<float>(kColorChannelMask);
} // namespace

Vector4 ConvertColor(unsigned int color) {
    return Vector4(
        static_cast<float>((color >> kRedShift) & kColorChannelMask) / kMaxColorChannel,
        static_cast<float>((color >> kGreenShift) & kColorChannelMask) / kMaxColorChannel,
        static_cast<float>((color >> kBlueShift) & kColorChannelMask) / kMaxColorChannel,
        static_cast<float>(color & kColorChannelMask) / kMaxColorChannel
    );
}

Vector4 ConvertColor(const Vector4 &color) {
    Vector4 convertColor;
    // 渡された値を0.0f～255.0fの間に収める
    convertColor.x = std::clamp(color.x, 0.0f, kMaxColorChannel);
    convertColor.y = std::clamp(color.y, 0.0f, kMaxColorChannel);
    convertColor.z = std::clamp(color.z, 0.0f, kMaxColorChannel);
    convertColor.w = std::clamp(color.w, 0.0f, kMaxColorChannel);
    // 0.0f～1.0fの間に収める
    convertColor.x /= kMaxColorChannel;
    convertColor.y /= kMaxColorChannel;
    convertColor.z /= kMaxColorChannel;
    convertColor.w /= kMaxColorChannel;
    return convertColor;
}

}