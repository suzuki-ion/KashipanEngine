#pragma once
#include <KashipanEngine.h>

namespace Application {
	// スプライトアニメーションコンポーネント,基本的なテンプレートとして登録されたものを使用する
	class SpriteAnimationComponent : public KashipanEngine::IObjectComponent2D {
	public:
		/// @brief ローテーションアニメーションを再生する
		void PlayRotateAnimetion(const Vector3& rotation, float animationTime, EaseType easeType);
		/// @brief ローテーションアニメーションを強制的に再生する（現在のアニメーションがあっても即座に切り替える）
		void ForcePlayRotateAnimetion(const Vector3& startRotation,const Vector3& targetRotation, float animationTime, EaseType easeType);

		static const std::string& GetStaticComponentType() {
			static const std::string type = "SpriteAnimationComponent";
			return type;
		}
		SpriteAnimationComponent() : KashipanEngine::IObjectComponent2D("SpriteAnimationComponent",100) {}
		~SpriteAnimationComponent() override = default;
		std::unique_ptr<KashipanEngine::IObjectComponent> Clone() const override;
		std::optional<bool> Initialize() override;
		std::optional<bool> Update() override;
#if defined(USE_IMGUI)
		void ShowImGui() override {}
#endif

	private:
		void RotateAnimationUpdate();

		bool isPlaying_;

		float elapsedTime_;
		float animationDuration_;
		EaseType easeType_;

		Vector3 translate_;
		Vector3 scale_;
		Vector3 rotation_;
		std::function<void()> animationUpdateFunc_;
	};
} // namespace Application
