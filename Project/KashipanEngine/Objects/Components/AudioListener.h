#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Transform.h"
#include "Scene/Components/Audio/SceneAudioPlayer.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 音声の受聴位置となるコンポーネント
/// @details 使用フラグ(used)のみを持つ。使用フラグをオンにすると、
///          同一シーン内の他のAudioListenerの使用フラグは自動でオフになる
///          （シーン内で同時に使用されるAudioListenerは常に高々1つ）。
class AudioListener final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(AudioListener, 1, )
    COMPONENT_CATEGORY("Audio")
    ~AudioListener() override = default;
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<AudioListener>();
        ptr->used_ = used_;
        return ptr;
    }

    /// @brief 使用フラグを設定する
    /// @details trueを設定すると、同一シーン内の他のAudioListenerの使用フラグをオフにする
    void SetUsed(bool used) {
        used_ = used;
        if (used_) {
            if (auto *player = GetOrAddSceneAudioPlayer()) {
                player->NotifyListenerUsed(Passkey<AudioListener>(), this);
            }
        }
    }
    /// @brief 使用フラグを取得する
    bool GetUsed() const noexcept { return used_; }

    /// @brief ワールド座標を取得（Transform が無い場合は原点）
    Vector3 GetWorldPosition() const {
        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        if (!transform) return Vector3(0.0f, 0.0f, 0.0f);
        const Matrix4x4 &world = transform->GetWorldMatrix();
        return Vector3(world.m[3][0], world.m[3][1], world.m[3][2]);
    }

    /// @brief ワールド右方向（Transform の +X）を取得（パン計算の基準に使用）
    Vector3 GetWorldRightDirection() const {
        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        Vector3 right(1.0f, 0.0f, 0.0f);
        if (transform) {
            const Matrix4x4 &world = transform->GetWorldMatrix();
            const Vector3 axis(world.m[0][0], world.m[0][1], world.m[0][2]);
            const float length = axis.Length();
            if (length > 0.0f) right = axis * (1.0f / length);
        }
        return right;
    }

    /// @brief ワールド前方向（Transform の +Z）を取得（前後判定の基準に使用）
    Vector3 GetWorldForwardDirection() const {
        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        Vector3 forward(0.0f, 0.0f, 1.0f);
        if (transform) {
            const Matrix4x4 &world = transform->GetWorldMatrix();
            const Vector3 axis(world.m[2][0], world.m[2][1], world.m[2][2]);
            const float length = axis.Length();
            if (length > 0.0f) forward = axis * (1.0f / length);
        }
        return forward;
    }

protected:
    void Initialize() override {
        auto *player = GetOrAddSceneAudioPlayer();
        if (player) {
            player->RegisterListener(this);
            // 使用フラグが立っている場合はもちろん、シーン内に他に使用中のリスナーが
            // 1つも無い場合も自動的にこのリスナーを使用中にする（明示操作なしでも動作するように）
            if (used_ || !player->GetUsedListener()) {
                SetUsed(true);
            }
        }
    }

    void Finalize() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *player = sceneContext ? sceneContext->GetComponent<SceneAudioPlayer>() : nullptr;
        if (player) player->UnregisterListener(this);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        bool used = used_;
        if (ImGui::Checkbox(TranslationLabel("component.audiolistener.used"), &used)) {
            SetUsed(used);
        }
    }
#endif
    JSON SaveToJson() const override { return JSON{ {"used", used_} }; }
    bool LoadFromJson(const JSON &json) override { used_ = json.value("used", false); return true; }

private:
    SceneAudioPlayer *GetOrAddSceneAudioPlayer() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return nullptr;
        auto *player = sceneContext->GetComponent<SceneAudioPlayer>();
        if (!player) {
            player = sceneContext->AddComponent<SceneAudioPlayer>();
        }
        return player;
    }

    bool used_ = false;
};

REGISTER_COMPONENT_OBJECT(AudioListener)

} // namespace KashipanEngine
