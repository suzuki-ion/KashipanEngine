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
    }

#if defined(USE_IMGUI)
    void ShowPersistentImGui() override {
        RetryApplyToRendererIfNeeded();
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

    /// @brief 同一オブジェクトのSpriteRenderer/MeshRendererへ、指定テクスチャを適用する
    void ApplyToRenderer() {
        const auto handle = ResolveTextureHandle();
        if (handle == TextureManager::kInvalidHandle) return;

        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;
        auto *spriteRenderer = objectContext->GetComponent<SpriteRenderer>();
        auto *meshRenderer = spriteRenderer ? nullptr : objectContext->GetComponent<MeshRenderer>();
        if (!spriteRenderer && !meshRenderer) return;

        if (overrideMaterialHandle_ == MaterialManager::kInvalidHandle) {
            originalMaterialHandle_ = spriteRenderer ? spriteRenderer->GetMaterialHandle() : meshRenderer->GetMaterialHandle();

            MaterialManager::Material overrideMaterial{};
            if (auto *baseMaterial = MaterialManager::GetMaterial(originalMaterialHandle_)) {
                overrideMaterial = *baseMaterial;
            }
            overrideMaterial.name = overrideMaterialName_;
            overrideMaterialHandle_ = MaterialManager::RegisterMaterial(overrideMaterialName_, overrideMaterial);
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
