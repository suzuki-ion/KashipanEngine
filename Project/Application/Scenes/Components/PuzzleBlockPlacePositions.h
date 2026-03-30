#pragma once

#include "Scenes/Components/PuzzleBoard.h"

#include <utility>
#include <vector>

namespace Application {

class PuzzleBlockPlacePositions final : public KashipanEngine::ISceneComponent {
public:
    explicit PuzzleBlockPlacePositions(PuzzleBoard *board);
    ~PuzzleBlockPlacePositions() override;

    void Initialize() override;
    void Update() override;

    void EvaluatePlaceablePositions(const std::vector<std::vector<PuzzleBlockData>> &placingBlocks);
    const std::vector<std::vector<std::pair<size_t, size_t>>> &GetPlaceablePositions() const;
    std::vector<std::pair<size_t, size_t>> GetNearestPlaceableIndices(size_t x, size_t y) const;

private:
    bool CanFit(const std::vector<std::vector<PuzzleBlockData>> &shape, int baseX, int baseY,
        std::vector<std::pair<size_t, size_t>> &outCells) const;
    bool HasSupport(const std::vector<std::pair<size_t, size_t>> &cells) const;
    bool HasRoof(const std::vector<std::pair<size_t, size_t>> &cells) const;

    PuzzleBoard *board_ = nullptr;
    std::vector<std::vector<std::pair<size_t, size_t>>> placeablePositions_;
};

} // namespace Application
