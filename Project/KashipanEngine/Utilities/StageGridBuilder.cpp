#include "Utilities/StageGridBuilder.h"

#include <algorithm>
#include <utility>

#include "Debug/Logger.h"

namespace KashipanEngine {

void StageGridBuilder::SetRoomSize(std::uint32_t sizeX, std::uint32_t sizeY, std::uint32_t sizeZ) {
    LogScope scope;
    roomSizeX_ = sizeX;
    roomSizeY_ = sizeY;
    roomSizeZ_ = sizeZ;
}

void StageGridBuilder::SetRoomSpacing(std::uint32_t spacing) {
    LogScope scope;
    roomSpacing_ = spacing;
}

void StageGridBuilder::SetCorridorWidth(std::uint32_t width) {
    LogScope scope;
    corridorWidth_ = width;
}

void StageGridBuilder::SetTileWorldSize(float size) {
    LogScope scope;
    tileWorldSize_ = size;
}

void StageGridBuilder::SetRoomTileName(RoomType type, const std::string &tileName) {
    LogScope scope;
    roomTileNames_[type] = tileName;
}

void StageGridBuilder::SetDefaultRoomTileName(const std::string &tileName) {
    LogScope scope;
    defaultRoomTileName_ = tileName;
}

void StageGridBuilder::SetCorridorTileName(const std::string &tileName) {
    LogScope scope;
    corridorTileName_ = tileName;
}

std::uint32_t StageGridBuilder::OriginOf(std::uint32_t slot, std::uint32_t cellSize) const {
    LogScope scope;
    return slot * (cellSize + roomSpacing_);
}

void StageGridBuilder::GetRequiredGridSize(const StageGraphGenerator &graph,
    std::uint32_t &outWidth, std::uint32_t &outHeight, std::uint32_t &outDepth) const {
    LogScope scope;
    const std::uint32_t gw = graph.GetGridWidth();
    const std::uint32_t gh = graph.GetGridHeight();
    const std::uint32_t gd = graph.GetGridDepth();
    outWidth = gw > 0 ? gw * roomSizeX_ + (gw - 1) * roomSpacing_ : 0;
    outHeight = gh > 0 ? gh * roomSizeY_ + (gh - 1) * roomSpacing_ : 0;
    outDepth = gd > 0 ? gd * roomSizeZ_ + (gd - 1) * roomSpacing_ : 0;
}

bool StageGridBuilder::Build(const StageGraphGenerator &graph, WaveFunctionCollapse &wfc) const {
    LogScope scope;
    if (!defaultRoomTileName_ || !corridorTileName_ || graph.GetRoomCount() == 0) {
        return false;
    }

    std::uint32_t totalWidth = 0, totalHeight = 0, totalDepth = 0;
    GetRequiredGridSize(graph, totalWidth, totalHeight, totalDepth);
    if (totalWidth == 0 || totalHeight == 0 || totalDepth == 0) {
        return false;
    }

    wfc.SetGridSize(totalWidth, totalHeight, totalDepth);

    auto tileNameForType = [&](RoomType type) -> std::optional<std::string> {
        auto it = roomTileNames_.find(type);
        if (it != roomTileNames_.end()) {
            return it->second;
        }
        return defaultRoomTileName_;
    };

    auto fixBox = [&](std::uint32_t x0, std::uint32_t x1, std::uint32_t y0, std::uint32_t y1,
        std::uint32_t z0, std::uint32_t z1, const std::string &tileName) -> bool {
        for (std::uint32_t x = x0; x < x1; ++x) {
            for (std::uint32_t y = y0; y < y1; ++y) {
                for (std::uint32_t z = z0; z < z1; ++z) {
                    if (!wfc.FixTile(x, y, z, tileName)) {
                        return false;
                    }
                }
            }
        }
        return true;
    };

    for (const RoomNode &room : graph.GetRooms()) {
        const auto tileName = tileNameForType(room.type);
        if (!tileName) {
            return false;
        }
        const std::uint32_t x0 = OriginOf(room.x, roomSizeX_);
        const std::uint32_t y0 = OriginOf(room.y, roomSizeY_);
        const std::uint32_t z0 = OriginOf(room.z, roomSizeZ_);
        if (!fixBox(x0, x0 + roomSizeX_, y0, y0 + roomSizeY_, z0, z0 + roomSizeZ_, *tileName)) {
            return false;
        }
    }

    // 部屋の隙間に区切られた1本分の断面を、対象軸の範囲内で中央寄せして求める
    auto centeredRange = [](std::uint32_t origin, std::uint32_t size, std::uint32_t width) -> std::pair<std::uint32_t, std::uint32_t> {
        const std::uint32_t w = std::min(width, size);
        const std::uint32_t offset = (size - w) / 2;
        return { origin + offset, origin + offset + w };
    };

    for (const RoomNode &room : graph.GetRooms()) {
        for (const std::uint32_t neighborID : room.connectedRoomIDs) {
            if (neighborID <= room.id) {
                continue; // 逆側からの重複処理を防ぐ（両側から1回ずつ辺が登録されているため）
            }
            const RoomNode *neighbor = graph.GetRoom(neighborID);
            if (!neighbor) {
                return false;
            }

            const std::int32_t dx = static_cast<std::int32_t>(neighbor->x) - static_cast<std::int32_t>(room.x);
            const std::int32_t dy = static_cast<std::int32_t>(neighbor->y) - static_cast<std::int32_t>(room.y);
            const std::int32_t dz = static_cast<std::int32_t>(neighbor->z) - static_cast<std::int32_t>(room.z);
            const int axisCount = (dx != 0 ? 1 : 0) + (dy != 0 ? 1 : 0) + (dz != 0 ? 1 : 0);
            if (axisCount != 1) {
                return false; // 抽象グリッド上で隣接していない部屋同士の接続には対応しない
            }

            if (roomSpacing_ == 0) {
                continue; // 隙間が無く部屋同士が直接接しているため、通路は不要
            }
            if (corridorWidth_ == 0) {
                return false; // 隙間があるのに通路幅0では物理的に繋がらない
            }

            const std::uint32_t ax0 = OriginOf(room.x, roomSizeX_);
            const std::uint32_t ay0 = OriginOf(room.y, roomSizeY_);
            const std::uint32_t az0 = OriginOf(room.z, roomSizeZ_);
            const std::uint32_t bx0 = OriginOf(neighbor->x, roomSizeX_);
            const std::uint32_t by0 = OriginOf(neighbor->y, roomSizeY_);
            const std::uint32_t bz0 = OriginOf(neighbor->z, roomSizeZ_);

            std::uint32_t x0, x1, y0, y1, z0, z1;
            if (dx != 0) {
                if (dx > 0) { x0 = ax0 + roomSizeX_; x1 = bx0; } else { x0 = bx0 + roomSizeX_; x1 = ax0; }
                std::tie(y0, y1) = centeredRange(ay0, roomSizeY_, corridorWidth_);
                std::tie(z0, z1) = centeredRange(az0, roomSizeZ_, corridorWidth_);
            } else if (dy != 0) {
                if (dy > 0) { y0 = ay0 + roomSizeY_; y1 = by0; } else { y0 = by0 + roomSizeY_; y1 = ay0; }
                std::tie(x0, x1) = centeredRange(ax0, roomSizeX_, corridorWidth_);
                std::tie(z0, z1) = centeredRange(az0, roomSizeZ_, corridorWidth_);
            } else {
                if (dz > 0) { z0 = az0 + roomSizeZ_; z1 = bz0; } else { z0 = bz0 + roomSizeZ_; z1 = az0; }
                std::tie(x0, x1) = centeredRange(ax0, roomSizeX_, corridorWidth_);
                std::tie(y0, y1) = centeredRange(ay0, roomSizeY_, corridorWidth_);
            }

            if (!fixBox(x0, x1, y0, y1, z0, z1, *corridorTileName_)) {
                return false;
            }
        }
    }

    return true;
}

bool StageGridBuilder::GetRoomGridCenter(const StageGraphGenerator &graph, std::uint32_t roomID,
    std::uint32_t &outX, std::uint32_t &outY, std::uint32_t &outZ) const {
    LogScope scope;
    const RoomNode *room = graph.GetRoom(roomID);
    if (!room) {
        return false;
    }
    outX = OriginOf(room->x, roomSizeX_) + roomSizeX_ / 2;
    outY = OriginOf(room->y, roomSizeY_) + roomSizeY_ / 2;
    outZ = OriginOf(room->z, roomSizeZ_) + roomSizeZ_ / 2;
    return true;
}

bool StageGridBuilder::GetRoomWorldCenter(const StageGraphGenerator &graph, std::uint32_t roomID, Vector3 &outPosition) const {
    LogScope scope;
    std::uint32_t gx = 0, gy = 0, gz = 0;
    if (!GetRoomGridCenter(graph, roomID, gx, gy, gz)) {
        return false;
    }
    outPosition = Vector3(
        static_cast<float>(gx) * tileWorldSize_,
        static_cast<float>(gy) * tileWorldSize_,
        static_cast<float>(gz) * tileWorldSize_);
    return true;
}

} // namespace KashipanEngine
