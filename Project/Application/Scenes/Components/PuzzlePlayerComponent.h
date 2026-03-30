#pragma once

#include "Scenes/Components/PuzzleBlockFaller.h"

#include <string>

namespace Application::ScenePuzzle {

class PuzzlePlayer final : public KashipanEngine::ISceneComponent {
public:
    PuzzlePlayer(
        Application::PuzzleBlockFaller *faller,
        const std::string &moveHorizontalCommand,
        const std::string &rotateCommand,
        const std::string &placeCommand);

    ~PuzzlePlayer() override;

    void Update() override;

private:
    Application::PuzzleBlockFaller *faller_ = nullptr;

    std::string moveHorizontalCommand_;
    std::string rotateCommand_;
    std::string placeCommand_;
};

} // namespace Application::ScenePuzzle
