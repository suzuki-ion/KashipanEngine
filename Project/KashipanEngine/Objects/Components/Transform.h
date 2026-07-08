#pragma once
#include "Objects/ObjectComponentHeader.h"
#include "Scene/Scene.h"

namespace KashipanEngine {

class Transform : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(Transform, 1,
        ADD_MEMBER_VARIABLE(translate_);
        ADD_MEMBER_VARIABLE(rotate_);
        ADD_MEMBER_VARIABLE(scale_);
        ADD_MEMBER_VARIABLE(worldMatrix_);
    )
    ~Transform() override = default;

    /// @brief コンポーネントのクローンを作成
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<Transform>();
        ptr->translate_ = translate_;
        ptr->rotate_ = rotate_;
        ptr->rotateQuat_ = rotateQuat_;
        ptr->scale_ = scale_;
        ptr->isWorldMatrixCalculated_ = false;
        ptr->worldMatrix_ = Matrix4x4::Identity();
        ptr->worldMatrixVersion_ = 0;
        ptr->cachedParentVersion_ = 0;
        return ptr;
    }

    /// @brief 親オブジェクトをポインタから設定する
    /// @param parent 親オブジェクトのポインタ（nullptrの場合は親を解除）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool SetParentObject(EmptyObject *parent) {
        if (!parent) {
            parentObject_ = nullptr;
            isWorldMatrixCalculated_ = false;
            cachedParentVersion_ = 0;
            return true;
        }
        // 親にTransformを持たないオブジェクトは親にできない
        if (!parent->GetComponent<Transform>()) return false;

        auto *objectCtx = GetOwnerObjectContext();
        const auto *ownerObject = objectCtx ? objectCtx->GetOwner() : nullptr;
        if (parent == ownerObject) return false;
        for (auto *p = parent; p != nullptr; p = p->GetComponent<Transform>()->parentObject_) {
            if (p == ownerObject) return false;
        }
        parentObject_ = parent;
        // 親が変わったのでキャッシュは無効
        isWorldMatrixCalculated_ = false;
        cachedParentVersion_ = 0;
        return true;
    }
    /// @brief 親オブジェクトをUUIDから設定する
    /// @param parentUUID 親オブジェクトのUUID（無効な場合は親を解除）
    /// @return 成功した場合はtrue、失敗した場合はfalseを返す
    bool SetParentObject(const UUID128 &parentUUID) {
        if (!parentUUID.IsValid()) {
            parentObject_ = nullptr;
            isWorldMatrixCalculated_ = false;
            cachedParentVersion_ = 0;
            return true;
        }
        auto *sceneCtx = GetOwnerSceneContext();
        if (!sceneCtx) return false;
        auto *parent = sceneCtx->GetSceneObject(parentUUID);
        if (!parent) return false;
        return SetParentObject(parent);
    }

    EmptyObject *GetParentObject() const { return TryGetParentObject(); }

    void SetTranslate(const Vector3 &translate) {
        if (translate_ == translate) return;
        translate_ = translate;
        isWorldMatrixCalculated_ = false;
    }

    void SetTranslateY(const float &translateY) {
        if (translate_.y == translateY) return;
        translate_.y = translateY;
        isWorldMatrixCalculated_ = false;
    }

    /// @brief オイラー角で回転を設定する（互換用）
    /// @param rotate オイラー角（ラジアン）
    void SetRotate(const Vector3 &rotate) {
        if (rotate_ == rotate) return;
        rotate_ = rotate;
        rotateQuat_ = Quaternion::MakeRotateEuler(rotate);
        isWorldMatrixCalculated_ = false;
    }

    /// @brief クォータニオンで回転を設定する
    /// @param quat クォータニオン
    void SetRotateQuaternion(const Quaternion &quat) {
        rotateQuat_ = quat.Normalize();
        rotate_ = rotateQuat_.MakeEuler();
        isWorldMatrixCalculated_ = false;
    }

    void SetScale(const Vector3 &scale) {
        if (scale_ == scale) return;
        scale_ = scale;
        isWorldMatrixCalculated_ = false;
    }

    const Vector3 &GetTranslate() const { return translate_; }

    /// @brief オイラー角で回転を取得する（互換用）
    /// @return オイラー角（ラジアン）
    const Vector3 &GetRotate() const { return rotate_; }

    /// @brief クォータニオンで回転を取得する
    /// @return クォータニオン
    const Quaternion &GetRotateQuaternion() const { return rotateQuat_; }

    const Vector3 &GetScale() const { return scale_; }

    const Matrix4x4 &GetWorldMatrix() {
        if (IsWorldMatrixDirty()) {
            // クォータニオンから回転行列を生成してワールド行列を構築
            Matrix4x4 scaleMat;
            scaleMat.MakeScale(scale_);

            Matrix4x4 rotateMat = rotateQuat_.MakeRotateMatrix();

            Matrix4x4 translateMat;
            translateMat.MakeTranslate(translate_);

            Matrix4x4 local = scaleMat * rotateMat * translateMat;

            auto *parentObj = TryGetParentObject();
            auto *parentTransform = parentObj ? parentObj->GetComponent<Transform>() : nullptr;
            if (parentTransform) {
                // 親のワールド行列を取得（必要なら親が再計算される）
                const Matrix4x4 &pw = parentTransform->GetWorldMatrix();
                worldMatrix_ = local * pw;
                // 親のバージョンをキャッシュ
                cachedParentVersion_ = parentTransform->GetWorldMatrixVersion();
            } else {
                worldMatrix_ = local;
                cachedParentVersion_ = 0;
            }
            isWorldMatrixCalculated_ = true;
            // 自身のワールド行列が更新されたことを示すバージョンを進める
            ++worldMatrixVersion_;
        }
        return worldMatrix_;
    }

    // ワールド行列の現在バージョンを取得（外からは const にして参照可能）
    std::uint64_t GetWorldMatrixVersion() const { return worldMatrixVersion_; }

    bool IsWorldMatrixCalculated() const {
        // 自分のキャッシュがあること、かつ親がいる場合は親が計算済みで
        // 親のバージョンが子がキャッシュしたものと一致していることを要求する
        if (!isWorldMatrixCalculated_) return false;
        auto *parentTransform = parentObject_ ? parentObject_->GetComponent<Transform>() : nullptr;
        if (!parentTransform) return true;
        if (!parentTransform->IsWorldMatrixCalculated()) return false;
        return (cachedParentVersion_ == parentTransform->GetWorldMatrixVersion());
    }
    bool IsWorldMatrixDirty() const { return !IsWorldMatrixCalculated(); }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::TextUnformatted(Translation("engine.imgui.component.transform3d").c_str());

        Vector3 t = translate_;
        Vector3 r = rotate_;
        Vector3 s = scale_;
        Vector3 rDeg{ r.x * 180.0f / 3.14159265f, r.y * 180.0f / 3.14159265f, r.z * 180.0f / 3.14159265f };

        ImGuiCustom::EditValue(Translation("engine.imgui.transform.translate").c_str(), t, { .vSpeed = 0.01f });
        if (ImGuiCustom::EditValue(Translation("engine.imgui.transform.rotate").c_str(), rDeg, { .vSpeed = 0.01f, .vMin = -180.0f, .vMax = 180.0f })) {
            r.x = rDeg.x * 3.14159265f / 180.0f;
            r.y = rDeg.y * 3.14159265f / 180.0f;
            r.z = rDeg.z * 3.14159265f / 180.0f;
        }
        ImGuiCustom::EditValue(Translation("engine.imgui.transform.scale").c_str(), s, { .vSpeed = 0.01f });

        ImGui::Spacing();
        ImGui::TextUnformatted(Translation("engine.imgui.transform.parent").c_str());
        std::string parentName = parentObject_ ? parentObject_->GetName() : "(None)";
        ImGui::TextUnformatted(parentName.c_str());

        SetTranslate(t);
        SetRotate(r);
        SetScale(s);
    }
#endif

    JSON SaveToJson() const override {
        JSON json;
        json["translate"] = ToJSON(translate_);
        json["rotate"] = ToJSON(rotateQuat_);
        json["scale"] = ToJSON(scale_);
        auto *parentObj = TryGetParentObject();
        if (parentObj) {
            json["parent"] = ToJSON(parentObj->GetObjectID());
        }
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        if (!json.contains("translate") || !json.contains("rotate") || !json.contains("scale")) {
            return false;
        }
        translate_ = FromJSON<Vector3>(json["translate"]);
        rotateQuat_ = FromJSON<Quaternion>(json["rotate"]);
        scale_ = FromJSON<Vector3>(json["scale"]);
        if (json.contains("parent")) {
            UUID128 parentUUID = FromJSON<UUID128>(json["parent"]);
            SetParentObject(parentUUID);
        } else {
            SetParentObject(nullptr);
        }
        return true;
    }

private:
    EmptyObject *TryGetParentObject() const {
        if (!parentObject_) return nullptr;
        auto *sceneCtx = GetOwnerSceneContext();
        auto *parentObj = sceneCtx ? sceneCtx->GetSceneObject(parentObject_) : nullptr;
        return parentObj;
    }

    Vector3 translate_{ 0.0f, 0.0f, 0.0f };
    Vector3 rotate_{ 0.0f, 0.0f, 0.0f };
    Quaternion rotateQuat_ = Quaternion::Identity();
    Vector3 scale_{ 1.0f, 1.0f, 1.0f };

    EmptyObject *parentObject_ = nullptr;
    Matrix4x4 worldMatrix_ = Matrix4x4::Identity();
    bool isWorldMatrixCalculated_ = false;

    // ワールド行列のバージョン（再計算ごとに++）
    std::uint64_t worldMatrixVersion_ = 0;
    // この Transform が最後に計算したときの親のバージョン
    std::uint64_t cachedParentVersion_ = 0;
};

REGISTER_COMPONENT_OBJECT(Transform);

} // namespace KashipanEngine
