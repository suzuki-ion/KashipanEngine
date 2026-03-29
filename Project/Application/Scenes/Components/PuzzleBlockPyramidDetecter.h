#pragma once

#include "Scenes/Components/PuzzleBoard.h"

#include <algorithm>
#include <numbers>
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
    explicit PuzzleBlockPyramidDetecter(PuzzleBoard *board)
        : ISceneComponent("PuzzleBlockPyramidDetecter", 2), board_(board) {}
    ~PuzzleBlockPyramidDetecter() override = default;

    void Initialize() override {
        detectedPyramids_.clear();
        detectedIndexGroups_.clear();
        ClearHighlightObjects();
    }

    void Finalize() override {
        ClearHighlightObjects();
        detectedPyramids_.clear();
        detectedIndexGroups_.clear();
    }

    void Update() override {
        const auto *ctx = GetOwnerContext();
        if (!ctx) return;
        if (ctx->GetSceneVariableOr<bool>("IsPuzzleStop", true)) return;

        DetectPyramids();
        RefreshHighlights();
    }

    const std::vector<DetectPyramidData> &GetDetectedPyramids() const { return detectedPyramids_; }
    const std::vector<std::vector<std::pair<size_t, size_t>>> &GetDetectedIndexGroups() const { return detectedIndexGroups_; }

private:
    struct WorkPyramidData {
        DetectPyramidData data;
        std::vector<std::pair<size_t, size_t>> allIndices;
    };

    void DetectPyramids() {
        detectedPyramids_.clear();
        detectedIndexGroups_.clear();

        if (!board_) return;

        std::vector<WorkPyramidData> found;
        const auto &boardData = board_->GetBoardData();

        for (int y = 0; y < board_->GetBoardHeight(); ++y) {
            for (int x = 0; x < board_->GetBoardWidth(); ++x) {
                if (board_->IsCellEmpty(x, y)) continue;

                const auto type = boardData[static_cast<size_t>(y)][static_cast<size_t>(x)].type;
                const auto apexDir = board_->GetCellDirection(x, y);

                for (size_t level = 2;; ++level) {
                    WorkPyramidData work;
                    work.data.pyramidLevel = level;

                    const bool ok = (apexDir == PuzzleBlockDirection::Up)
                        ? TryBuildPyramidUp(x, y, static_cast<int>(level), type, work)
                        : TryBuildPyramidDown(x, y, static_cast<int>(level), type, work);
                    if (!ok) {
                        break;
                    }

                    if (work.data.outerIndices.size() >= 3) {
                        found.push_back(std::move(work));
                    }
                }
            }
        }

        std::sort(found.begin(), found.end(), [](const WorkPyramidData &a, const WorkPyramidData &b) {
            return a.data.pyramidLevel > b.data.pyramidLevel;
        });

        std::vector<WorkPyramidData> filtered;
        for (const auto &candidate : found) {
            boolean included = false;
            for (const auto &accepted : filtered) {
                if (candidate.data.pyramidLevel >= accepted.data.pyramidLevel) continue;
                if (IsSubset(candidate.allIndices, accepted.allIndices)) {
                    included = true;
                    break;
                }
            }
            if (!included) {
                filtered.push_back(candidate);
            }
        }

        for (const auto &v : filtered) {
            detectedPyramids_.push_back(v.data);
            detectedIndexGroups_.push_back(v.data.outerIndices);
        }
    }

    bool TryBuildPyramidUp(int apexX, int apexY, int level, PuzzleBlockType type, WorkPyramidData &out) const {
        out.data.outerIndices.clear();
        out.data.innerIndices.clear();
        out.allIndices.clear();

        for (int r = 0; r < level; ++r) {
            const int y = apexY + r;
            const int minX = apexX - r;
            const int maxX = apexX + r;

            for (int x = minX; x <= maxX; x += 2) {
                if (!board_->IsInside(x, y)) {
                    return false;
                }

                const bool isOuter = (r == 0) || (r == level - 1) || (x == minX) || (x == maxX);
                if (isOuter) {
                    if (board_->IsCellEmpty(x, y)) {
                        return false;
                    }
                    const auto &cell = board_->GetBoardData()[static_cast<size_t>(y)][static_cast<size_t>(x)];
                    if (cell.type != type) {
                        return false;
                    }
                    out.data.outerIndices.emplace_back(static_cast<size_t>(x), static_cast<size_t>(y));
                } else {
                    out.data.innerIndices.emplace_back(static_cast<size_t>(x), static_cast<size_t>(y));
                }
                out.allIndices.emplace_back(static_cast<size_t>(x), static_cast<size_t>(y));
            }
        }

        return true;
    }

    bool TryBuildPyramidDown(int apexX, int apexY, int level, PuzzleBlockType type, WorkPyramidData &out) const {
        out.data.outerIndices.clear();
        out.data.innerIndices.clear();
        out.allIndices.clear();

        for (int r = 0; r < level; ++r) {
            const int y = apexY - r;
            const int minX = apexX - r;
            const int maxX = apexX + r;

            for (int x = minX; x <= maxX; x += 2) {
                if (!board_->IsInside(x, y)) {
                    return false;
                }

                const bool isOuter = (r == 0) || (r == level - 1) || (x == minX) || (x == maxX);
                if (isOuter) {
                    if (board_->IsCellEmpty(x, y)) {
                        return false;
                    }
                    const auto &cell = board_->GetBoardData()[static_cast<size_t>(y)][static_cast<size_t>(x)];
                    if (cell.type != type) {
                        return false;
                    }
                    out.data.outerIndices.emplace_back(static_cast<size_t>(x), static_cast<size_t>(y));
                } else {
                    out.data.innerIndices.emplace_back(static_cast<size_t>(x), static_cast<size_t>(y));
                }
                out.allIndices.emplace_back(static_cast<size_t>(x), static_cast<size_t>(y));
            }
        }

        return true;
    }

    static bool IsSubset(
        const std::vector<std::pair<size_t, size_t>> &subsetIndices,
        const std::vector<std::pair<size_t, size_t>> &supersetIndices) {
        for (const auto &index : subsetIndices) {
            if (std::find(supersetIndices.begin(), supersetIndices.end(), index) == supersetIndices.end()) {
                return false;
            }
        }
        return true;
    }

    void RefreshHighlights() {
        ClearHighlightObjects();
        if (!board_ || !board_->GetBoardRootTransform() || !board_->GetScreenBuffer2D()) {
            return;
        }

        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        for (const auto &pyramid : detectedPyramids_) {
            std::vector<std::pair<size_t, size_t>> all = pyramid.outerIndices;
            all.insert(all.end(), pyramid.innerIndices.begin(), pyramid.innerIndices.end());

            for (const auto &[x, y] : all) {
                auto tri = std::make_unique<KashipanEngine::Triangle2D>();
                auto *triPtr = tri.get();
                tri->SetName("PuzzlePyramidHighlight");
                tri->SetUniqueBatchKey();
                tri->AttachToRenderer(board_->GetScreenBuffer2D(), "Object2D.DoubleSidedCulling.BlendNormal");
                PuzzleBoard::ApplyEquilateralTriangleVertices(tri.get());

                if (auto *mat = tri->GetComponent2D<KashipanEngine::Material2D>()) {
                    mat->SetColor(Vector4(0.4f, 1.0f, 0.4f, 0.28f));
                }
                if (auto *tr = tri->GetComponent2D<KashipanEngine::Transform2D>()) {
                    tr->SetTranslate(board_->GetCellLocalPosition(static_cast<int>(x), static_cast<int>(y)));
                    tr->SetScale(Vector3(board_->GetTriangleRadius(), board_->GetTriangleRadius(), 1.0f));
                    tr->SetRotate(Vector3(0.0f, 0.0f, board_->GetCellDirection(static_cast<int>(x), static_cast<int>(y)) == PuzzleBlockDirection::Up ? 0.0f : std::numbers::pi_v<float>));
                    tr->SetParentTransform(board_->GetBoardRootTransform());
                }

                if (ctx->AddObject2D(std::move(tri))) {
                    highlightTriangles_.push_back(triPtr);
                }
            }
        }
    }

    void ClearHighlightObjects() {
        auto *ctx = GetOwnerContext();
        if (!ctx) {
            highlightTriangles_.clear();
            return;
        }

        for (auto *obj : highlightTriangles_) {
            if (obj) {
                ctx->RemoveObject2D(obj);
            }
        }
        highlightTriangles_.clear();
    }

    PuzzleBoard *board_ = nullptr;

    std::vector<DetectPyramidData> detectedPyramids_;
    std::vector<std::vector<std::pair<size_t, size_t>>> detectedIndexGroups_;
    std::vector<KashipanEngine::Triangle2D *> highlightTriangles_;
};

} // namespace Application
