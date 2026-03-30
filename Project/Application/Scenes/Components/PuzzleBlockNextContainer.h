#pragma once

#include "Scenes/Components/PuzzleBoard.h"

#include <array>
#include <random>
#include <vector>

namespace Application {

class PuzzleBlockNextContainer final : public KashipanEngine::ISceneComponent {
public:
    PuzzleBlockNextContainer();
    ~PuzzleBlockNextContainer() override;

    void Initialize() override;
    void Update() override;

    const std::vector<std::vector<PuzzleBlockData>> &GetNextBlock();
    std::vector<std::vector<PuzzleBlockData>> PopNextBlock();

private:
    using TypePair = std::array<PuzzleBlockType, 2>;

    static std::vector<std::vector<PuzzleBlockData>> BuildBlock(const TypePair &pair);
    void RefillIfNeeded();

    std::mt19937 randomEngine_{};
    std::vector<std::vector<std::vector<PuzzleBlockData>>> nextBlocks_;
    const std::vector<std::vector<PuzzleBlockData>> kEmptyBlock_{};
};

} // namespace Application
