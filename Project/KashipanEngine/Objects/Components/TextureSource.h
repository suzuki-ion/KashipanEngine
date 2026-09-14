#pragma once
#include <cstdint>
#include <string>

#include "Assets/MaterialManager.h"
#include "Assets/TextureManager.h"
#include "Assets/TextureRef.h"
#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Utilities/ImGuiCustom.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 同一オブジェクトのSpriteRenderer/MeshRendererが表示するテクスチャを、
///        マテリアルアセット自体を編集せずに簡単に差し替えられるようにするコンポーネント
/// @details GifSourceと同じ方式（レンダラーが使用しているマテリアルを複製し、複製先のテクスチャだけ
///          差し替えてレンダラーへ割り当てる）で実現する。複製するのは、複数オブジェクトで共有されて
///          いる可能性のある元のマテリアルアセット自体を書き換えないため。コンポーネントが無効化・
///          破棄された場合は元のマテリアルへ戻す。
class TextureSource final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(TextureSource, 0xFF,
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(textureAssetPath_, [this] {
            textureHandle_ = TextureManager::kInvalidHandle;
            ApplyToRenderer();
        });
    )
    COMPONENT_CATEGORY("Render")
    ~TextureSource() override { RevertRenderer(); }

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<TextureSource>();
        ptr->textureAssetPath_ = textureAssetPath_;
        return ptr;
    }

    //==================================================
    // プロパティ
    //==================================================

    /// @brief 使用するテクスチャをAssetsルートからの相対パスで設定する（切り替えると即座にレンダラーへ反映される）
    void SetTextureAssetPath(const std::string &textureAssetPath) {
        if (textureAssetPath_ == textureAssetPath) return;
        textureAssetPath_ = textureAssetPath;
        textureHandle_ = TextureManager::kInvalidHandle;
        ApplyToRenderer();
    }
    const std::string &GetTextureAssetPath() const noexcept { return textureAssetPath_; }

protected:
    void Initialize() override {
        ApplyToRenderer();
    }

    void Finalize() override {
        RevertRenderer();
    }

    void Update() override {
        RetryApplyToRendererIfNeeded();
        SyncOverrideMaterialFields();
    }

#if defined(USE_IMGUI)
    void ShowPersistentImGui() override {
        RetryApplyToRendererIfNeeded();
        SyncOverrideMaterialFields();
    }

    void ShowImGui() override {
        TextureRef texture(textureAssetPath_);
        if (ImGuiCustom::EditValue(TranslationLabel("component.texturesource.texture"), texture)) {
            SetTextureAssetPath(texture.assetPath);
        }
    }
#endif

    JSON SaveToJson() const override {
        return JSON{ {"textureAssetPath", textureAssetPath_} };
    }

    bool LoadFromJson(const JSON &json) override {
        textureAssetPath_ = json.value("textureAssetPath", std::string{});
        textureHandle_ = TextureManager::kInvalidHandle;
        // シーン読み込み時はコンポーネント追加時点でInitialize()が読み込み前の
        // textureAssetPath_（空文字）で呼ばれてしまっているため、ここで読み込んだ実際の値を
        // 使って改めて適用し直す（非アクティブな場合はSetActive(true)時のInitialize()に任せる）
        if (IsActive()) ApplyToRenderer();
        return true;
    }

private:
    TextureManager::TextureHandle ResolveTextureHandle() const {
        if (textureHandle_ == TextureManager::kInvalidHandle && !textureAssetPath_.empty()) {
            textureHandle_ = TextureManager::GetTextureFromAssetPath(textureAssetPath_);
        }
        return textureHandle_;
    }

    /// @brief レンダラーへの反映がまだ・または外れてしまっている場合に再試行する
    /// @details GifSource::RetryApplyToRendererIfNeededと同じ理由（追加順・読み込み順の都合による保険）
    void RetryApplyToRendererIfNeeded() {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;
        auto *spriteRenderer = objectContext->GetComponent<SpriteRenderer>();
        auto *meshRenderer = spriteRenderer ? nullptr : objectContext->GetComponent<MeshRenderer>();
        if (!spriteRenderer && !meshRenderer) return;

        const auto currentHandle = spriteRenderer ? spriteRenderer->GetMaterialHandle() : meshRenderer->GetMaterialHandle();
        if (overrideMaterialHandle_ == MaterialManager::kInvalidHandle || currentHandle != overrideMaterialHandle_) {
            ApplyToRenderer();
        }
    }

    /// @brief 複製先マテリアル（テクスチャ以外の全フィールド）を元マテリアルの最新値へ同期する
    /// @details 複製先マテリアル自体はApplyToRenderer初回適用時の一度きりの生成のため、
    ///          以後にマテリアルエディター等で元マテリアル（サンプラー・UV変換・色等）を編集しても
    ///          複製先（実際にレンダラーが参照している方）へ反映されないままになっていた。
    ///          そのためテクスチャ差し替え自体とは独立して毎フレーム同期する
    void SyncOverrideMaterialFields() {
        if (overrideMaterialHandle_ == MaterialManager::kInvalidHandle) return;
        auto *overrideMaterial = MaterialManager::GetMaterial(overrideMaterialHandle_);
        auto *baseMaterial = MaterialManager::GetMaterial(originalMaterialHandle_);
        if (!overrideMaterial || !baseMaterial) return;

        const auto name = overrideMaterial->name;
        const auto textureHandle = overrideMaterial->textureHandle;
        const auto textureFileName = overrideMaterial->textureFileName;
        *overrideMaterial = *baseMaterial;
        overrideMaterial->name = name;
        overrideMaterial->textureHandle = textureHandle;
        overrideMaterial->textureFileName = textureFileName;
    }

    /// @brief 同一オブジェクトのSpriteRenderer/MeshRendererへ、指定テクスチャを適用する
    void ApplyToRenderer() {
        const auto handle = ResolveTextureHandle();
        if (handle == TextureManager::kInvalidHandle) return;

        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;
        auto *spriteRenderer = objectContext->GetComponent<SpriteRenderer>();
        auto *meshRenderer = spriteRenderer ? nullptr : objectContext->GetComponent<MeshRenderer>();
        if (!spriteRenderer && !meshRenderer) return;

        const auto currentHandle = spriteRenderer ? spriteRenderer->GetMaterialHandle() : meshRenderer->GetMaterialHandle();
        // レンダラーが今参照しているマテリアルが複製先（overrideMaterialHandle_）自身でない場合、
        // それは初回適用か、Inspector等で別のベースマテリアルへ切り替えられたことを意味する。
        // どちらの場合もその値を新しいベースマテリアルとして採用し直し、複製先へコピーする
        // （これをしないと、一度複製した後にベースマテリアルを切り替えても、次にこの関数が
        // 呼ばれた時点で古い複製が無条件に再適用されてしまい、切り替えが反映されなかった）
        if (currentHandle != overrideMaterialHandle_) {
            originalMaterialHandle_ = currentHandle;

            MaterialManager::Material overrideMaterial{};
            if (auto *baseMaterial = MaterialManager::GetMaterial(originalMaterialHandle_)) {
                overrideMaterial = *baseMaterial;
            }
            overrideMaterial.name = overrideMaterialName_;
            overrideMaterial.textureHandle = handle;

            if (overrideMaterialHandle_ != MaterialManager::kInvalidHandle) {
                // 複製先マテリアルは使い回す。RegisterMaterialを呼び直すと同名でも別ハンドルが
                // 発行され、古い複製がMaterialManagerにゴミとして残ってしまうため
                if (auto *existing = MaterialManager::GetMaterial(overrideMaterialHandle_)) {
                    *existing = overrideMaterial;
                }
            } else {
                overrideMaterialHandle_ = MaterialManager::RegisterMaterial(overrideMaterialName_, overrideMaterial);
            }
        }
        if (overrideMaterialHandle_ == MaterialManager::kInvalidHandle) return;

        if (auto *material = MaterialManager::GetMaterial(overrideMaterialHandle_)) {
            material->textureHandle = handle;
        }

        if (spriteRenderer) spriteRenderer->SetMaterialHandle(overrideMaterialHandle_);
        if (meshRenderer) meshRenderer->SetMaterialHandle(overrideMaterialHandle_);
    }

    /// @brief 適用していたレンダラーのマテリアルを元へ戻し、複製した専用マテリアルを破棄する
    void RevertRenderer() {
        if (overrideMaterialHandle_ == MaterialManager::kInvalidHandle) return;

        if (auto *objectContext = GetOwnerObjectContext()) {
            if (auto *spriteRenderer = objectContext->GetComponent<SpriteRenderer>()) {
                spriteRenderer->SetMaterialHandle(originalMaterialHandle_);
            } else if (auto *meshRenderer = objectContext->GetComponent<MeshRenderer>()) {
                meshRenderer->SetMaterialHandle(originalMaterialHandle_);
            }
        }

        MaterialManager::RemoveMaterial(overrideMaterialName_);
        overrideMaterialHandle_ = MaterialManager::kInvalidHandle;
        originalMaterialHandle_ = MaterialManager::kInvalidHandle;
    }

    std::string textureAssetPath_;
    mutable TextureManager::TextureHandle textureHandle_ = TextureManager::kInvalidHandle;

    // このコンポーネント専用の複製マテリアル（他オブジェクトと共有される元のマテリアルアセットは書き換えない）
    const std::string overrideMaterialName_ = "__TextureSourceMaterial_" + std::to_string(reinterpret_cast<std::uintptr_t>(this));
    MaterialManager::MaterialHandle overrideMaterialHandle_ = MaterialManager::kInvalidHandle;
    MaterialManager::MaterialHandle originalMaterialHandle_ = MaterialManager::kInvalidHandle;
};

REGISTER_COMPONENT_OBJECT(TextureSource)

} // namespace KashipanEngine
