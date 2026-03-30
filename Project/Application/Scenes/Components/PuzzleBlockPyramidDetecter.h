#pragma once

#include "Scenes/Components/PuzzleBoard.h"

#include <utility>
#include <vector>

namespace Application {

struct DetectPyramidData {
    size_t pyramidLevel = 0;
    std::vector<std::pair<size_t, size_t>> outerIndices;
    std::vector<std::pair<size_t, size_t>> innerIndices;
};

class PuzzleBlockPyramidDetecter final : public KashipanEngine::ISceneComponent {
public:
    explicit PuzzleBlockPyramidDetecter(PuzzleBoard *board);
    ~PuzzleBlockPyramidDetecter() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;

    const std::vector<DetectPyramidData> &GetDetectedPyramids() const;
    const std::vector<std::vector<std::pair<size_t, size_t>>> &GetDetectedIndexGroups() const;

private:
    struct WorkPyramidData {
        DetectPyramidData data;
        std::vector<std::pair<size_t, size_t>> allIndices;
    };

    void DetectPyramids();
    bool TryBuildPyramidUp(int apexX, int apexY, int level, PuzzleBlockType type, WorkPyramidData &out) const;
    bool TryBuildPyramidDown(int apexX, int apexY, int level, PuzzleBlockType type, WorkPyramidData &out) const;

    static bool IsSubset(
        const std::vector<std::pair<size_t, size_t>> &subsetIndices,
        const std::vector<std::pair<size_t, size_t>> &supersetIndices);

    void RefreshHighlights();
    void ClearHighlightObjects();

    PuzzleBoard *board_ = nullptr;

    std::vector<DetectPyramidData> detectedPyramids_;
    std::vector<std::vector<std::pair<size_t, size_t>>> detectedIndexGroups_;
    std::vector<KashipanEngine::Triangle2D *> highlightTriangles_;
};

} // namespace Application
