#include "Scenes/Components/PuzzleBlockPlacePositions.h"

#include <limits>

namespace Application {

PuzzleBlockPlacePositions::PuzzleBlockPlacePositions(PuzzleBoard *board)
    : ISceneComponent("PuzzleBlockPlacePositions", 2), board_(board) {}

PuzzleBlockPlacePositions::~PuzzleBlockPlacePositions() = default;

void PuzzleBlockPlacePositions::Initialize() {
    placeablePositions_.clear();
}

void PuzzleBlockPlacePositions::Update() {
    const auto *ctx = GetOwnerContext();
    if (!ctx) return;
    if (ctx->GetSceneVariableOr<bool>("IsPuzzleStop", true)) return;
}

void PuzzleBlockPlacePositions::EvaluatePlaceablePositions(const std::vector<std::vector<PuzzleBlockData>> &placingBlocks) {
    placeablePositions_.clear();
    if (!board_ || placingBlocks.empty() || placingBlocks.front().empty()) {
        return;
    }

    const int shapeH = static_cast<int>(placingBlocks.size());
    const int shapeW = static_cast<int>(placingBlocks.front().size());

    for (int baseY = 0; baseY <= board_->GetBoardHeight() - shapeH; ++baseY) {
        for (int baseX = 0; baseX <= board_->GetBoardWidth() - shapeW; ++baseX) {
            std::vector<std::pair<size_t, size_t>> cells;
            if (!CanFit(placingBlocks, baseX, baseY, cells)) continue;
            if (!HasSupport(cells)) continue;
            if (HasRoof(cells)) continue;
            placeablePositions_.push_back(cells);
        }
    }
}

const std::vector<std::vector<std::pair<size_t, size_t>>> &PuzzleBlockPlacePositions::GetPlaceablePositions() const {
    return placeablePositions_;
}

std::vector<std::pair<size_t, size_t>> PuzzleBlockPlacePositions::GetNearestPlaceableIndices(size_t x, size_t y) const {
    if (placeablePositions_.empty()) {
        return {};
    }

    size_t bestIndex = 0;
    float bestScore = std::numeric_limits<float>::max();

    for (size_t i = 0; i < placeablePositions_.size(); ++i) {
        if (placeablePositions_[i].empty()) continue;
        const auto &head = placeablePositions_[i].front();
        const float dx = static_cast<float>(head.first) - static_cast<float>(x);
        const float dy = static_cast<float>(head.second) - static_cast<float>(y);
        const float score = dx * dx + dy * dy;
        if (score < bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    return placeablePositions_[bestIndex];
}

bool PuzzleBlockPlacePositions::CanFit(const std::vector<std::vector<PuzzleBlockData>> &shape, int baseX, int baseY,
    std::vector<std::pair<size_t, size_t>> &outCells) const {
    outCells.clear();
    for (int sy = 0; sy < static_cast<int>(shape.size()); ++sy) {
        for (int sx = 0; sx < static_cast<int>(shape[sy].size()); ++sx) {
            const int bx = baseX + sx;
            const int by = baseY + sy;
            if (!board_->IsInside(bx, by)) {
                return false;
            }
            if (!board_->IsCellEmpty(bx, by)) {
                return false;
            }
            if (shape[sy][sx].direction != board_->GetCellDirection(bx, by)) {
                return false;
            }
            outCells.emplace_back(static_cast<size_t>(bx), static_cast<size_t>(by));
        }
    }
    return true;
}

bool PuzzleBlockPlacePositions::HasSupport(const std::vector<std::pair<size_t, size_t>> &cells) const {
    if (!board_ || cells.empty()) return false;

    for (const auto &[x, y] : cells) {
        if (static_cast<int>(y) == board_->GetBoardHeight() - 1) {
            return true;
        }

        const int xi = static_cast<int>(x);
        const int yi = static_cast<int>(y);

        const auto hasBlock = [this](int tx, int ty) {
            return board_->IsInside(tx, ty) && !board_->IsCellEmpty(tx, ty);
        };

        if (hasBlock(xi - 1, yi) || hasBlock(xi + 1, yi) || hasBlock(xi, yi + 1)) {
            return true;
        }
    }
    return false;
}

bool PuzzleBlockPlacePositions::HasRoof(const std::vector<std::pair<size_t, size_t>> &cells) const {
    if (!board_) return false;

    for (const auto &[x, y] : cells) {
        const int xi = static_cast<int>(x);
        for (int ty = 0; ty < static_cast<int>(y); ++ty) {
            if (!board_->IsCellEmpty(xi, ty)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace Application
