#include "Scenes/Components/PuzzleBlockNextContainer.h"

#include <algorithm>

namespace Application {

PuzzleBlockNextContainer::PuzzleBlockNextContainer()
    : ISceneComponent("PuzzleBlockNextContainer", 2) {}

PuzzleBlockNextContainer::~PuzzleBlockNextContainer() = default;

void PuzzleBlockNextContainer::Initialize() {
    randomEngine_.seed(std::random_device{}());
    nextBlocks_.clear();
    RefillIfNeeded();
}

void PuzzleBlockNextContainer::Update() {
    const auto *ctx = GetOwnerContext();
    if (!ctx) return;
    if (ctx->GetSceneVariableOr<bool>("IsPuzzleStop", true)) return;
}

const std::vector<std::vector<PuzzleBlockData>> &PuzzleBlockNextContainer::GetNextBlock() {
    RefillIfNeeded();
    if (nextBlocks_.empty()) return kEmptyBlock_;
    return nextBlocks_.back();
}

std::vector<std::vector<PuzzleBlockData>> PuzzleBlockNextContainer::PopNextBlock() {
    RefillIfNeeded();
    if (nextBlocks_.empty()) return {};
    auto result = nextBlocks_.back();
    nextBlocks_.pop_back();
    RefillIfNeeded();
    return result;
}

std::vector<std::vector<PuzzleBlockData>> PuzzleBlockNextContainer::BuildBlock(const TypePair &pair) {
    std::vector<std::vector<PuzzleBlockData>> block(2, std::vector<PuzzleBlockData>(1));
    block[0][0].type = pair[0];
    block[0][0].direction = PuzzleBlockDirection::Up;
    block[1][0].type = pair[1];
    block[1][0].direction = PuzzleBlockDirection::Down;
    return block;
}

void PuzzleBlockNextContainer::RefillIfNeeded() {
    if (!nextBlocks_.empty()) return;

    std::array<TypePair, 4> table = {
        TypePair{ PuzzleBlockType::Red, PuzzleBlockType::Red },
        TypePair{ PuzzleBlockType::Red, PuzzleBlockType::Blue },
        TypePair{ PuzzleBlockType::Blue, PuzzleBlockType::Red },
        TypePair{ PuzzleBlockType::Blue, PuzzleBlockType::Blue },
    };

    std::shuffle(table.begin(), table.end(), randomEngine_);
    for (const auto &entry : table) {
        nextBlocks_.push_back(BuildBlock(entry));
    }
}

} // namespace Application
