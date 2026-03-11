#pragma once
#include <KashipanEngine.h>
#include <Objects/ResultSystem/ResultSelector.h>
#include "Scenes/Components/ResultSceneAnimator.h"

namespace KashipanEngine {

    class ResultScene final : public SceneBase {
    public:
        ResultScene();
        ~ResultScene() override;

        void Initialize() override;

    protected:
        void OnUpdate() override;

    private:
        SceneDefaultVariables* sceneDefaultVariables_ = nullptr;

        Application::ResultSelector resultSelector_;
        ResultSceneAnimator* resultSceneAnimator_ = nullptr;

        std::map<std::string, Sprite*> spriteMap_;

    };

}
