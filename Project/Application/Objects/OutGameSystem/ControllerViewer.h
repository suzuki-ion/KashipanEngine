#pragma once
#include <KashipanEngine.h>

namespace Application {
	class ControllerViewer final {
	public:
		void Initialize(std::function<KashipanEngine::Sprite* (const std::string&, const std::string&)> createSprite);
		void Update(float delta);

		// inputNameは"move", "a", "b", "x", "y", "lt", "rt"どれかで、checkFuncはその入力が現在されているかを返す関数を想定
		void SetInputCheckFunction(const std::string& inputName, std::function<bool()> checkFunc);

		void SetTranslate(const Vector3& translate);
		void SetScale(const Vector3& scale);
		void SetRotation(const Vector3& rotation);

		Vector3 GetTranslate() const;
		Vector3 GetScale() const;
		Vector3 GetRotation() const;

	private:
		std::map<std::string, std::function<bool()>> inputCheckFuncMap_;
		std::function<KashipanEngine::Sprite* (const std::string&, const std::string&)> createSpriteFunc_;

		KashipanEngine::Sprite* controllerSprite_ = nullptr;
		std::map<std::string,KashipanEngine::Sprite*> buttonSprites_;

	};
}
