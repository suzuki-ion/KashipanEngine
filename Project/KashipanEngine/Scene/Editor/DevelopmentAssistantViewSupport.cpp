#include "Scene/Editor/SceneEditorView.h"
#ifdef USE_IMGUI
#include "Graphics/ScreenBuffer.h"
#include <cmath>
namespace KashipanEngine {
JSON SceneEditorView::GetApiCamera() const {
    return {{"target", ToJSON(target_)}, {"eye", ToJSON(eye_)}, {"distance", distance_}, {"yaw", yaw_}, {"pitch", pitch_},
        {"fly", flyMode_}, {"fly_speed", flySpeed_}, {"fov", fovY_}, {"near", nearClip_}, {"far", farClip_},
        {"pan_2d", ToJSON(pan2D_)}, {"zoom_2d", zoom2D_}, {"display_mode", static_cast<int>(displayMode_)}};
}
bool SceneEditorView::SetApiCamera(const JSON &settings) {
    try {
        if (!settings.is_object()) return false;
        EnsureSceneViewObject(); EnsureCameraSettingsComponent();
        auto next = GetApiCamera();
        for (auto it = settings.begin(); it != settings.end(); ++it) { if (!next.contains(it.key())) return false; next[it.key()] = it.value(); }
        auto number = [&](const char *name) { const float v = next.at(name).get<float>(); if (!std::isfinite(v) || std::abs(v) > 1e6f) throw std::runtime_error("Invalid camera number"); return v; };
        const auto target = FromJSON<Vector3>(next.at("target")), eye = FromJSON<Vector3>(next.at("eye"));
        const auto pan = FromJSON<Vector2>(next.at("pan_2d"));
        for (float value : {target.x, target.y, target.z, eye.x, eye.y, eye.z, pan.x, pan.y}) if (!std::isfinite(value) || std::abs(value) > 1e6f) return false;
        const float distance = number("distance"), yaw = number("yaw"), pitch = number("pitch"), speed = number("fly_speed"), fov = number("fov"), nearValue = number("near"), farValue = number("far"), zoom = number("zoom_2d");
        const bool fly = next.at("fly").get<bool>(); const int mode = next.at("display_mode").get<int>();
        if (distance <= 0 || speed <= 0 || fov <= 0 || fov >= 3.14f || nearValue <= 0 || farValue <= nearValue || zoom <= 0 || mode < 0 || mode > 2) return false;
        target_ = target; eye_ = eye; pan2D_ = pan; distance_ = distance; yaw_ = yaw; pitch_ = pitch; flySpeed_ = speed; flyMode_ = fly;
        fovY_ = fov; nearClip_ = nearValue; farClip_ = farValue; zoom2D_ = zoom; displayMode_ = static_cast<SceneRenderer::EditorDisplayMode>(mode);
        PushCameraSettingsToComponent();
        return true;
    } catch (...) { return false; }
}
bool SceneEditorView::SaveApiImage(const std::string &path) const { return screenBuffer_ && screenBuffer_->SaveToFile(path); }
}
#endif
