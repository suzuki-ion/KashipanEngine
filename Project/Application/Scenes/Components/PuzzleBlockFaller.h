#pragma once

#include "Scenes/Components/PuzzleBlockNextContainer.h"
#include "Scenes/Components/PuzzleBlockPlacePositions.h"

#include <array>
#include <numbers>
#include <vector>

namespace Application {

class PuzzleBlockFaller final : public KashipanEngine::ISceneComponent {
public:
    PuzzleBlockFaller(PuzzleBoard *board, PuzzleBlockPlacePositions *placePositions, PuzzleBlockNextContainer *nextContainer)
        : ISceneComponent("PuzzleBlockFaller", 2), board_(board), placePositions_(placePositions), nextContainer_(nextContainer) {}
    ~PuzzleBlockFaller() override = default;

    void Initialize() override {
        currentColumn_ = 0;
        currentRotationStep_ = 0;
        currentBlockData_.clear();
        placedPreviewTriangles_.clear();
        ghostTriangles_.clear();
        SpawnNext();
    }

    void Finalize() override {
        DestroyTriangles(placedPreviewTriangles_);
        DestroyTriangles(ghostTriangles_);
    }

    void Update() override {
        const auto *ctx = GetOwnerContext();
        if (!ctx) return;
        if (ctx->GetSceneVariableOr<bool>("IsPuzzleStop", true)) return;
        RefreshPlacePreview();
    }

    bool MoveLeft() {
        if (!board_) return false;
        if (currentBlockData_.empty() || currentBlockData_.front().empty()) return false;
        if (currentColumn_ <= 0) return false;
        --currentColumn_;
        RefreshPlacePreview();
        return true;
    }

    bool MoveRight() {
        if (!board_) return false;
        if (currentBlockData_.empty() || currentBlockData_.front().empty()) return false;

        const int maxColumn = board_->GetBoardWidth() - static_cast<int>(currentBlockData_.front().size());
        if (currentColumn_ >= maxColumn) return false;

        ++currentColumn_;
        RefreshPlacePreview();
        return true;
    }

    bool RotateLeft() {
        if (currentRotationStep_ <= -1) return false;
        --currentRotationStep_;
        RebuildCurrentBlockByRotation();
        ClampColumnByShape();
        RefreshPlacePreview();
        return true;
    }

    bool RotateRight() {
        if (currentRotationStep_ >= 1) return false;
        ++currentRotationStep_;
        RebuildCurrentBlockByRotation();
        ClampColumnByShape();
        RefreshPlacePreview();
        return true;
    }

    bool PlaceCurrentBlock() {
        if (!board_ || !placePositions_) return false;
        if (currentBlockData_.empty() || currentBlockData_.front().empty()) return false;

        placePositions_->EvaluatePlaceablePositions(currentBlockData_);
        auto nearest = placePositions_->GetNearestPlaceableIndices(static_cast<size_t>(currentColumn_), static_cast<size_t>(board_->GetBoardHeight()));
        if (nearest.empty()) {
            return false;
        }

        if (nearest.size() != CountCurrentBlockCells()) {
            return false;
        }

        size_t index = 0;
        for (size_t y = 0; y < currentBlockData_.size(); ++y) {
            for (size_t x = 0; x < currentBlockData_[y].size(); ++x) {
                const auto [bx, by] = nearest[index++];
                auto obj = std::make_unique<KashipanEngine::Triangle2D>();
                auto *trianglePtr = obj.get();
                obj->SetName("PuzzlePlacedBlock");
                obj->SetUniqueBatchKey();
                obj->AttachToRenderer(board_->GetScreenBuffer2D(), "Object2D.DoubleSidedCulling.BlendNormal");
                PuzzleBoard::ApplyEquilateralTriangleVertices(obj.get());

                if (auto *mat = obj->GetComponent2D<KashipanEngine::Material2D>()) {
                    mat->SetColor(PuzzleBoard::GetColorByType(currentBlockData_[y][x].type, 1.0f));
                }
                if (auto *tr = obj->GetComponent2D<KashipanEngine::Transform2D>()) {
                    tr->SetTranslate(board_->GetCellLocalPosition(static_cast<int>(bx), static_cast<int>(by)));
                    tr->SetScale(Vector3(board_->GetTriangleRadius(), board_->GetTriangleRadius(), 1.0f));
                    tr->SetRotate(Vector3(0.0f, 0.0f, currentBlockData_[y][x].direction == PuzzleBlockDirection::Up ? 0.0f : std::numbers::pi_v<float>));
                    tr->SetParentTransform(board_->GetBoardRootTransform());
                }

                if (!GetOwnerContext()->AddObject2D(std::move(obj))) {
                    return false;
                }

                PuzzleBlockData placed = currentBlockData_[y][x];
                placed.triangle = trianglePtr;
                if (!board_->SetBlockData(static_cast<int>(bx), static_cast<int>(by), placed)) {
                    return false;
                }
            }
        }

        SpawnNext();
        return true;
    }

    const std::vector<std::vector<PuzzleBlockData>> &GetCurrentBlockData() const { return currentBlockData_; }

private:
    void SpawnNext() {
        DestroyTriangles(placedPreviewTriangles_);
        DestroyTriangles(ghostTriangles_);

        if (!nextContainer_ || !board_) {
            currentBlockData_.clear();
            return;
        }

        auto block = nextContainer_->PopNextBlock();
        if (block.empty() || block.front().empty()) {
            currentBlockData_.clear();
            return;
        }

        baseTypes_[0] = block[0][0].type;
        baseTypes_[1] = block[1][0].type;

        currentRotationStep_ = 0;
        RebuildCurrentBlockByRotation();
        currentColumn_ = (board_->GetBoardWidth() - static_cast<int>(currentBlockData_.front().size())) / 2;
        ClampColumnByShape();

        CreateVisualTriangles(currentBlockData_, placedPreviewTriangles_, 1.0f);
        CreateVisualTriangles(currentBlockData_, ghostTriangles_, 0.5f);
        RefreshPlacePreview();
    }

    void RebuildCurrentBlockByRotation() {
        if (currentRotationStep_ == 0) {
            currentBlockData_.assign(2, std::vector<PuzzleBlockData>(1));
            currentBlockData_[0][0].type = baseTypes_[0];
            currentBlockData_[0][0].direction = PuzzleBlockDirection::Up;
            currentBlockData_[1][0].type = baseTypes_[1];
            currentBlockData_[1][0].direction = PuzzleBlockDirection::Down;
            return;
        }

        currentBlockData_.assign(1, std::vector<PuzzleBlockData>(2));
        if (currentRotationStep_ > 0) {
            currentBlockData_[0][0].type = baseTypes_[0];
            currentBlockData_[0][0].direction = PuzzleBlockDirection::Down;
            currentBlockData_[0][1].type = baseTypes_[1];
            currentBlockData_[0][1].direction = PuzzleBlockDirection::Up;
        } else {
            currentBlockData_[0][0].type = baseTypes_[0];
            currentBlockData_[0][0].direction = PuzzleBlockDirection::Up;
            currentBlockData_[0][1].type = baseTypes_[1];
            currentBlockData_[0][1].direction = PuzzleBlockDirection::Down;
        }
    }

    void ClampColumnByShape() {
        if (!board_ || currentBlockData_.empty() || currentBlockData_.front().empty()) return;

        const int shapeW = static_cast<int>(currentBlockData_.front().size());
        const int maxColumn = board_->GetBoardWidth() - shapeW;
        if (currentColumn_ < 0) currentColumn_ = 0;
        if (currentColumn_ > maxColumn) currentColumn_ = maxColumn;
    }

    void RefreshPlacePreview() {
        if (!board_ || !placePositions_ || currentBlockData_.empty()) return;
        if (placedPreviewTriangles_.empty() || ghostTriangles_.empty()) return;

        placePositions_->EvaluatePlaceablePositions(currentBlockData_);
        const auto place = placePositions_->GetNearestPlaceableIndices(static_cast<size_t>(currentColumn_), static_cast<size_t>(board_->GetBoardHeight()));
        if (place.empty()) return;

        size_t idx = 0;
        for (size_t y = 0; y < currentBlockData_.size(); ++y) {
            for (size_t x = 0; x < currentBlockData_[y].size(); ++x) {
                if (idx >= placedPreviewTriangles_.size() || idx >= ghostTriangles_.size() || idx >= place.size()) return;

                auto *fallingTr = placedPreviewTriangles_[idx]->GetComponent2D<KashipanEngine::Transform2D>();
                auto *ghostTr = ghostTriangles_[idx]->GetComponent2D<KashipanEngine::Transform2D>();

                if (fallingTr) {
                    const int fx = currentColumn_ + static_cast<int>(x);
                    const int fy = static_cast<int>(y);
                    fallingTr->SetTranslate(board_->GetCellLocalPosition(fx, fy));
                    fallingTr->SetRotate(Vector3(0.0f, 0.0f, currentBlockData_[y][x].direction == PuzzleBlockDirection::Up ? 0.0f : std::numbers::pi_v<float>));
                }

                if (ghostTr) {
                    ghostTr->SetTranslate(board_->GetCellLocalPosition(static_cast<int>(place[idx].first), static_cast<int>(place[idx].second)));
                    ghostTr->SetRotate(Vector3(0.0f, 0.0f, currentBlockData_[y][x].direction == PuzzleBlockDirection::Up ? 0.0f : std::numbers::pi_v<float>));
                }
                ++idx;
            }
        }
    }

    void CreateVisualTriangles(const std::vector<std::vector<PuzzleBlockData>> &shape,
        std::vector<KashipanEngine::Triangle2D *> &out,
        float alpha) {
        out.clear();
        if (!board_ || !board_->GetScreenBuffer2D() || !board_->GetBoardRootTransform()) return;

        for (size_t y = 0; y < shape.size(); ++y) {
            for (size_t x = 0; x < shape[y].size(); ++x) {
                auto obj = std::make_unique<KashipanEngine::Triangle2D>();
                auto *trianglePtr = obj.get();
                obj->SetName(alpha < 1.0f ? "PuzzleGhostBlock" : "PuzzleFallingBlock");
                obj->SetUniqueBatchKey();
                obj->AttachToRenderer(board_->GetScreenBuffer2D(), "Object2D.DoubleSidedCulling.BlendNormal");
                PuzzleBoard::ApplyEquilateralTriangleVertices(obj.get());

                if (auto *mat = obj->GetComponent2D<KashipanEngine::Material2D>()) {
                    mat->SetColor(PuzzleBoard::GetColorByType(shape[y][x].type, alpha));
                }
                if (auto *tr = obj->GetComponent2D<KashipanEngine::Transform2D>()) {
                    tr->SetScale(Vector3(board_->GetTriangleRadius(), board_->GetTriangleRadius(), 1.0f));
                    tr->SetRotate(Vector3(0.0f, 0.0f, shape[y][x].direction == PuzzleBlockDirection::Up ? 0.0f : std::numbers::pi_v<float>));
                    tr->SetParentTransform(board_->GetBoardRootTransform());
                }

                if (GetOwnerContext()->AddObject2D(std::move(obj))) {
                    out.push_back(trianglePtr);
                }
            }
        }
    }

    void DestroyTriangles(std::vector<KashipanEngine::Triangle2D *> &targets) {
        auto *ctx = GetOwnerContext();
        if (!ctx) {
            targets.clear();
            return;
        }

        for (auto *obj : targets) {
            if (obj) {
                ctx->RemoveObject2D(obj);
            }
        }
        targets.clear();
    }

    size_t CountCurrentBlockCells() const {
        size_t count = 0;
        for (const auto &row : currentBlockData_) {
            count += row.size();
        }
        return count;
    }

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
