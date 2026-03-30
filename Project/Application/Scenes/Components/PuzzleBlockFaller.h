#pragma once

#include "Scenes/Components/PuzzleBlockNextContainer.h"
#include "Scenes/Components/PuzzleBlockPlacePositions.h"

#include <array>
#include <vector>

namespace Application {

class PuzzleBlockFaller final : public KashipanEngine::ISceneComponent {
public:
    PuzzleBlockFaller(PuzzleBoard *board, PuzzleBlockPlacePositions *placePositions, PuzzleBlockNextContainer *nextContainer);
    ~PuzzleBlockFaller() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;

    bool MoveLeft();
    bool MoveRight();
    bool RotateLeft();
    bool RotateRight();

    bool PlaceCurrentBlock();

    const std::vector<std::vector<PuzzleBlockData>> &GetCurrentBlockData() const;

private:
    void SpawnNext();
    void RebuildCurrentBlockByRotation();
    void ClampColumnByShape();
    void RefreshPlacePreview();

    void CreateVisualTriangles(const std::vector<std::vector<PuzzleBlockData>> &shape,
        std::vector<KashipanEngine::Triangle2D *> &out,
        float alpha);
    void DestroyTriangles(std::vector<KashipanEngine::Triangle2D *> &targets);
    size_t CountCurrentBlockCells() const;

    PuzzleBoard *board_ = nullptr;
    PuzzleBlockPlacePositions *placePositions_ = nullptr;
    PuzzleBlockNextContainer *nextContainer_ = nullptr;

    std::array<PuzzleBlockType, 2> baseTypes_{ PuzzleBlockType::Red, PuzzleBlockType::Blue };
    int currentColumn_ = 0;
    int currentRotationStep_ = 0;

    std::vector<std::vector<PuzzleBlockData>> currentBlockData_;
    std::vector<KashipanEngine::Triangle2D *> placedPreviewTriangles_;
    std::vector<KashipanEngine::Triangle2D *> ghostTriangles_;
};

} // namespace Application
