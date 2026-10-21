#include "Utilities/StageGraphGenerator.h"

#include <algorithm>

namespace KashipanEngine {

void StageGraphGenerator::SetSeed(std::uint32_t seed) {
    seed_ = seed;
    randomEngine_.seed(seed_);
}

void StageGraphGenerator::SetGridSize(std::uint32_t width, std::uint32_t height, std::uint32_t depth) {
    width_ = width;
    height_ = height;
    depth_ = depth;
}

void StageGraphGenerator::SetBranchProbability(float probability) {
    branchProbability_ = std::clamp(probability, 0.0f, 1.0f);
}

bool StageGraphGenerator::AddSideRoomType(RoomType type, float weight) {
    if (weight <= 0.0f) {
        return false;
    }
    sideRoomTypes_.push_back({ type, weight });
    return true;
}

void StageGraphGenerator::ClearSideRoomTypes() {
    sideRoomTypes_.clear();
}

RoomType StageGraphGenerator::PickSideRoomType() {
    if (sideRoomTypes_.empty()) {
        return RoomType::Branch;
    }
    float totalWeight = 0.0f;
    for (const auto &entry : sideRoomTypes_) {
        totalWeight += entry.weight;
    }
    std::uniform_real_distribution<float> dist(0.0f, totalWeight);
    float roll = dist(randomEngine_);
    for (const auto &entry : sideRoomTypes_) {
        if (roll < entry.weight) {
            return entry.type;
        }
        roll -= entry.weight;
    }
    return sideRoomTypes_.back().type;
}

std::uint32_t StageGraphGenerator::AddRoom(RoomType type, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    RoomNode room;
    room.id = static_cast<std::uint32_t>(rooms_.size());
    room.type = type;
    room.x = x;
    room.y = y;
    room.z = z;
    rooms_.push_back(room);
    occupied_[x][y][z] = true;
    return room.id;
}

void StageGraphGenerator::Connect(std::uint32_t roomA, std::uint32_t roomB) {
    rooms_[roomA].connectedRoomIDs.push_back(roomB);
    rooms_[roomB].connectedRoomIDs.push_back(roomA);
}

bool StageGraphGenerator::IsOccupied(std::uint32_t x, std::uint32_t y, std::uint32_t z) const {
    return occupied_[x][y][z];
}

void StageGraphGenerator::Generate() {
    rooms_.clear();
    startRoomID_.reset();
    goalRoomID_.reset();

    if (width_ < 2 || height_ == 0 || depth_ == 0) {
        occupied_.clear();
        return;
    }

    occupied_.assign(width_, std::vector<std::vector<bool>>(height_, std::vector<bool>(depth_, false)));

    std::uniform_real_distribution<float> branchRoll(0.0f, 1.0f);

    const std::uint32_t startY = height_ / 2;
    std::uint32_t mainX = 0;
    const std::uint32_t mainY = startY;
    const std::uint32_t mainZ = 0;

    std::uint32_t prevRoomID = AddRoom(RoomType::Start, mainX, mainY, mainZ);
    startRoomID_ = prevRoomID;

    for (mainX = 1; mainX < width_; ++mainX) {
        // 1つ前のメインルート部屋から、寄り道部屋（行き止まり）を生やすかどうか判定する
        if (branchRoll(randomEngine_) < branchProbability_) {
            const std::uint32_t baseX = mainX - 1;

            struct Offset final { std::int32_t dx, dy, dz; };
            std::vector<Offset> candidates;
            if (mainY + 1 < height_ && !IsOccupied(baseX, mainY + 1, mainZ)) {
                candidates.push_back({ 0, 1, 0 });
            }
            if (mainY >= 1 && !IsOccupied(baseX, mainY - 1, mainZ)) {
                candidates.push_back({ 0, -1, 0 });
            }
            if (mainZ + 1 < depth_ && !IsOccupied(baseX, mainY, mainZ + 1)) {
                candidates.push_back({ 0, 0, 1 });
            }

            if (!candidates.empty()) {
                std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
                const Offset &offset = candidates[pick(randomEngine_)];
                const std::uint32_t sideX = static_cast<std::uint32_t>(static_cast<std::int32_t>(baseX) + offset.dx);
                const std::uint32_t sideY = static_cast<std::uint32_t>(static_cast<std::int32_t>(mainY) + offset.dy);
                const std::uint32_t sideZ = static_cast<std::uint32_t>(static_cast<std::int32_t>(mainZ) + offset.dz);
                const RoomType sideType = PickSideRoomType();
                const std::uint32_t sideRoomID = AddRoom(sideType, sideX, sideY, sideZ);
                Connect(prevRoomID, sideRoomID);
            }
        }

        // メインルートを1マス進める
        const RoomType mainType = (mainX == width_ - 1) ? RoomType::Goal : RoomType::Normal;
        const std::uint32_t newRoomID = AddRoom(mainType, mainX, mainY, mainZ);
        Connect(prevRoomID, newRoomID);
        prevRoomID = newRoomID;
    }

    goalRoomID_ = prevRoomID;
}

const RoomNode *StageGraphGenerator::GetRoomByIndex(std::size_t index) const {
    if (index >= rooms_.size()) {
        return nullptr;
    }
    return &rooms_[index];
}

const RoomNode *StageGraphGenerator::GetRoom(std::uint32_t roomID) const {
    return GetRoomByIndex(roomID);
}

} // namespace KashipanEngine
