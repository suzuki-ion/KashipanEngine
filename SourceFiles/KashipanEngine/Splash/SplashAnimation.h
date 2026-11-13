#pragma once
#include <algorithm>
#include <cmath>

namespace KashipanEngine::SplashAnimation {
// World coordinates: bottom-left origin, +X right, +Y up, +rotation counterclockwise.
inline constexpr float kCanvasWidth = 1920, kCanvasHeight = 1080;
inline constexpr float kLogoX = kCanvasWidth * 0.5f, kLogoY = kCanvasHeight * 0.5f;
inline constexpr float kTextY = kCanvasHeight * 0.2f, kTextWidth = 1024, kTextHeight = 256;
struct Point { float x, y; };
inline Point ToCanvas(float x, float y) { return {x, kCanvasHeight - y}; }
inline float ToCanvasRotation(float degrees) { return -degrees; }
struct Viewport { float scale, x, y; };
inline Viewport FitViewport(int width, int height) {
    const float scale = std::min(std::max(1, width) / kCanvasWidth, std::max(1, height) / kCanvasHeight);
    return {scale, (width - kCanvasWidth * scale) * 0.5f, (height - kCanvasHeight * scale) * 0.5f};
}
struct ClientSize { int width, height; };
// The window occupies at most 2/3 of each monitor dimension, with its frame outside the client.
inline ClientSize ChooseClientSize(int monitorWidth, int monitorHeight, int availableWidth, int availableHeight) {
    const double scale = std::max(0.0, std::min({
        monitorWidth * (2.0 / 3.0) / kCanvasWidth,
        monitorHeight * (2.0 / 3.0) / kCanvasHeight,
        availableWidth / static_cast<double>(kCanvasWidth),
        availableHeight / static_cast<double>(kCanvasHeight)}));
    return {std::max(1, static_cast<int>(std::floor(kCanvasWidth * scale + 1e-6))),
        std::max(1, static_cast<int>(std::floor(kCanvasHeight * scale + 1e-6)))};
}
// TD2_3 EngineLogoScene: 1s lead-in, logo reveal/hold through 3s, 1s fade.
inline constexpr double kIntroEnd = 3.0;
inline constexpr double kFadeDuration = 1.0;
inline constexpr double kRotationDuration = 1.0;
inline float Normalize(double time, double start, double end) {
    return static_cast<float>(std::clamp((time - start) / (end - start), 0.0, 1.0));
}
inline float EaseOutCubic(float t) {
    const float p = 1.0f - t;
    return 1.0f - p * p * p;
}
inline float EaseOutBack(float t) {
    const float p = t - 1.0f;
    return 1.0f + 2.70158f * p * p * p + 1.70158f * p * p;
}
struct Frame {
    float gearScale = 0, gearRotation = 0, breadScale = 0;
    float textAlpha = 0, wipeCenter = kLogoX, wipeWidth = 0, fade = 0;
    bool loading = false, finished = false;
};
// readyAt is measured from the first splash paint; -1 means still loading.
inline Frame Sample(double seconds, double readyAt = -1.0) {
    Frame frame;
    const double t = seconds - 1.0;
    frame.gearScale = 512 * EaseOutBack(Normalize(t, 0, 0.5));
    frame.gearRotation = 360 * EaseOutCubic(Normalize(t, 0, 0.5));
    frame.breadScale = 512 * EaseOutCubic(Normalize(t, 0.25, 0.75));
    frame.textAlpha = t >= 0.75 ? 1.0f : 0.0f;
    if (t >= 0.5 && t < 0.75) {
        const float p = EaseOutCubic(Normalize(t, 0.5, 0.75));
        frame.wipeCenter = kLogoX - kTextWidth * 0.5f + kTextWidth * 0.5f * p;
        frame.wipeWidth = kTextWidth * p;
    } else if (t >= 0.75) {
        const float p = EaseOutCubic(Normalize(t, 0.75, 1));
        frame.wipeCenter = kLogoX + kTextWidth * 0.5f * p;
        frame.wipeWidth = kTextWidth * (1 - p);
    }
    double fadeAt = -1;
    if (readyAt >= 0) {
        fadeAt = kIntroEnd + std::ceil(std::max(0.0, readyAt - kIntroEnd) / kRotationDuration) * kRotationDuration;
    }
    if (seconds >= kIntroEnd) {
        frame.loading = readyAt < 0 || readyAt > kIntroEnd;
        const double rotationTime = std::max(0.0, std::min(seconds, fadeAt >= 0 ? fadeAt : seconds) - kIntroEnd);
        frame.gearRotation = 360 * EaseOutBack(static_cast<float>(std::fmod(rotationTime, kRotationDuration) / kRotationDuration));
    }
    if (fadeAt >= 0) {
        frame.fade = Normalize(seconds, fadeAt, fadeAt + kFadeDuration);
        frame.finished = seconds >= fadeAt + kFadeDuration;
    }
    return frame;
}
} // namespace KashipanEngine::SplashAnimation
