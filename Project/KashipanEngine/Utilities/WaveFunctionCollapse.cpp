#include "Utilities/WaveFunctionCollapse.h"

#include <algorithm>
#include <iterator>

#include "Debug/Logger.h"

namespace KashipanEngine {

namespace {

/// @brief Direction列挙の並び順（Up, Down, Left, Right, Front, Back）に対応する座標オフセット
constexpr std::array<std::array<std::int32_t, 3>, WaveFunctionCollapse::kDirectionCount> kDirectionOffsets = { {
    { 0, 1, 0 },   // Up
    { 0, -1, 0 },  // Down
    { -1, 0, 0 },  // Left
    { 1, 0, 0 },   // Right
    { 0, 0, 1 },   // Front
    { 0, 0, -1 },  // Back
} };

/// @brief JSON保存/読込で使うDirection列挙の並び順に対応するキー名
constexpr std::array<const char *, WaveFunctionCollapse::kDirectionCount> kDirectionKeys = {
    "up", "down", "left", "right", "front", "back",
};

} // namespace

void WaveFunctionCollapse::SetSeed(std::uint32_t seed) {
    seed_ = seed;
    randomEngine_.seed(seed_);
}

void WaveFunctionCollapse::SetGridSize(std::uint32_t width, std::uint32_t height, std::uint32_t depth) {
    width_ = width;
    height_ = height;
    depth_ = depth;
    grid_.assign(width_, std::vector<std::vector<Cell>>(height_, std::vector<Cell>(depth_)));
    startPosition_.reset();
}

bool WaveFunctionCollapse::RegisterTile(const Tile &tile) {
    if (tile.name.empty()) {
        return false;
    }
    return tiles_.emplace(tile.name, tile).second;
}

bool WaveFunctionCollapse::RegisterTile(const std::string &name) {
    Tile tile;
    tile.name = name;
    return RegisterTile(tile);
}

bool WaveFunctionCollapse::AddTileConnection(const std::string &tileName, Direction direction, const std::string &connectedTileName) {
    auto it = tiles_.find(tileName);
    if (it == tiles_.end()) {
        return false;
    }
    auto &connections = it->second.connections[static_cast<std::size_t>(direction)];
    if (std::find(connections.begin(), connections.end(), connectedTileName) == connections.end()) {
        connections.push_back(connectedTileName);
    }
    return true;
}

bool WaveFunctionCollapse::RemoveTile(const std::string &tileName) {
    if (tiles_.erase(tileName) == 0) {
        return false;
    }

    for (auto &plane : grid_) {
        for (auto &row : plane) {
            for (auto &cell : row) {
                if (cell.fixedTileName == tileName) {
                    cell.fixedTileName.reset();
                }
                if (cell.resolvedTileName == tileName) {
                    cell.resolvedTileName.reset();
                }
            }
        }
    }
    return true;
}

bool WaveFunctionCollapse::FixTile(std::uint32_t x, std::uint32_t y, std::uint32_t z, const std::string &tileName) {
    if (!IsInBounds(x, y, z) || !tiles_.contains(tileName)) {
        return false;
    }
    grid_[x][y][z].fixedTileName = tileName;
    return true;
}

std::optional<std::string> WaveFunctionCollapse::GetFixedTile(std::uint32_t x, std::uint32_t y, std::uint32_t z) const {
    if (!IsInBounds(x, y, z)) {
        return std::nullopt;
    }
    return grid_[x][y][z].fixedTileName;
}

bool WaveFunctionCollapse::SetStartPosition(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    if (!IsInBounds(x, y, z)) {
        return false;
    }
    startPosition_ = { x, y, z };
    return true;
}

std::optional<std::string> WaveFunctionCollapse::GetResolvedTile(std::uint32_t x, std::uint32_t y, std::uint32_t z) const {
    if (!IsInBounds(x, y, z)) {
        return std::nullopt;
    }
    return grid_[x][y][z].resolvedTileName;
}

bool WaveFunctionCollapse::Solve() {
    if (width_ == 0 || height_ == 0 || depth_ == 0 || tiles_.empty()) {
        return false;
    }

    for (auto &plane : grid_) {
        for (auto &row : plane) {
            for (auto &cell : row) {
                cell.resolvedTileName.reset();
            }
        }
    }

    // 候補集合の演算（ソート・積集合）を文字列比較で行うと遅いため、
    // Solve内ではタイル名を一時的な整数インデックスへ変換して処理する
    std::vector<const Tile *> tileList;
    std::unordered_map<std::string, std::uint32_t> nameToIndex;
    tileList.reserve(tiles_.size());
    nameToIndex.reserve(tiles_.size());
    for (const auto &[name, tile] : tiles_) {
        nameToIndex.emplace(name, static_cast<std::uint32_t>(tileList.size()));
        tileList.push_back(&tile);
    }
    const std::uint32_t tileCount = static_cast<std::uint32_t>(tileList.size());

    // 各タイルの方向別接続リストをインデックスへ変換しておく（未登録名の接続先は無視する）
    std::vector<std::array<std::vector<std::uint32_t>, kDirectionCount>> connectionIndices(tileCount);
    for (std::uint32_t t = 0; t < tileCount; ++t) {
        for (std::size_t dir = 0; dir < kDirectionCount; ++dir) {
            auto &indices = connectionIndices[t][dir];
            for (const auto &connectedName : tileList[t]->connections[dir]) {
                auto it = nameToIndex.find(connectedName);
                if (it != nameToIndex.end()) {
                    indices.push_back(it->second);
                }
            }
            std::sort(indices.begin(), indices.end());
            indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
        }
    }

    std::vector<std::uint32_t> allTileIndices(tileCount);
    for (std::uint32_t t = 0; t < tileCount; ++t) allTileIndices[t] = t;

    // 各セルの残り候補タイルインデックス一覧（波動関数の重ね合わせ状態）
    using Possibilities = std::vector<std::vector<std::vector<std::vector<std::uint32_t>>>>;
    Possibilities possibilities(width_,
        std::vector<std::vector<std::vector<std::uint32_t>>>(height_,
            std::vector<std::vector<std::uint32_t>>(depth_)));

    std::vector<std::array<std::uint32_t, 3>> queue;
    for (std::uint32_t x = 0; x < width_; ++x) {
        for (std::uint32_t y = 0; y < height_; ++y) {
            for (std::uint32_t z = 0; z < depth_; ++z) {
                if (const auto &fixed = grid_[x][y][z].fixedTileName) {
                    auto it = nameToIndex.find(*fixed);
                    if (it == nameToIndex.end()) {
                        // FixTile/RemoveTileの整合性維持により通常起こらないが、念のため未登録名は全候補として扱う
                        possibilities[x][y][z] = allTileIndices;
                        continue;
                    }
                    possibilities[x][y][z] = { it->second };
                    queue.push_back({ x, y, z });
                } else {
                    possibilities[x][y][z] = allTileIndices;
                }
            }
        }
    }

    // 指定セルの候補集合から、各方向の隣接セルの候補を絞り込み、変化があれば伝播キューへ積む
    auto propagateFrom = [&](std::uint32_t x, std::uint32_t y, std::uint32_t z) -> bool {
        for (std::size_t dir = 0; dir < kDirectionCount; ++dir) {
            const auto &offset = kDirectionOffsets[dir];
            const std::int64_t nx = static_cast<std::int64_t>(x) + offset[0];
            const std::int64_t ny = static_cast<std::int64_t>(y) + offset[1];
            const std::int64_t nz = static_cast<std::int64_t>(z) + offset[2];
            if (nx < 0 || ny < 0 || nz < 0 ||
                nx >= static_cast<std::int64_t>(width_) ||
                ny >= static_cast<std::int64_t>(height_) ||
                nz >= static_cast<std::int64_t>(depth_)) {
                continue;
            }

            std::vector<std::uint32_t> allowed;
            for (std::uint32_t candidate : possibilities[x][y][z]) {
                const auto &connections = connectionIndices[candidate][dir];
                allowed.insert(allowed.end(), connections.begin(), connections.end());
            }
            std::sort(allowed.begin(), allowed.end());
            allowed.erase(std::unique(allowed.begin(), allowed.end()), allowed.end());

            auto &neighborPossibilities = possibilities[static_cast<std::size_t>(nx)][static_cast<std::size_t>(ny)][static_cast<std::size_t>(nz)];
            std::vector<std::uint32_t> filtered;
            filtered.reserve(neighborPossibilities.size());
            std::set_intersection(neighborPossibilities.begin(), neighborPossibilities.end(),
                allowed.begin(), allowed.end(), std::back_inserter(filtered));

            if (filtered.size() == neighborPossibilities.size()) {
                continue;
            }
            if (filtered.empty()) {
                return false; // 矛盾（候補が0件になった）
            }
            neighborPossibilities = std::move(filtered);
            queue.push_back({ static_cast<std::uint32_t>(nx), static_cast<std::uint32_t>(ny), static_cast<std::uint32_t>(nz) });
        }
        return true;
    };

    std::size_t head = 0;
    while (head < queue.size()) {
        const auto [x, y, z] = queue[head++];
        if (!propagateFrom(x, y, z)) {
            return false;
        }
    }

    bool firstCollapse = true;
    while (true) {
        std::optional<std::array<std::uint32_t, 3>> target;

        if (firstCollapse && startPosition_) {
            const auto &[sx, sy, sz] = *startPosition_;
            if (possibilities[sx][sy][sz].size() > 1) {
                target = *startPosition_;
            }
        }
        firstCollapse = false;

        if (!target) {
            std::size_t bestCount = 0;
            std::vector<std::array<std::uint32_t, 3>> candidates;
            for (std::uint32_t x = 0; x < width_; ++x) {
                for (std::uint32_t y = 0; y < height_; ++y) {
                    for (std::uint32_t z = 0; z < depth_; ++z) {
                        const std::size_t count = possibilities[x][y][z].size();
                        if (count <= 1) {
                            continue;
                        }
                        if (candidates.empty() || count < bestCount) {
                            bestCount = count;
                            candidates.clear();
                            candidates.push_back({ x, y, z });
                        } else if (count == bestCount) {
                            candidates.push_back({ x, y, z });
                        }
                    }
                }
            }
            if (candidates.empty()) {
                break; // 全セル確定済み
            }
            std::uniform_int_distribution<std::size_t> tieBreak(0, candidates.size() - 1);
            target = candidates[tieBreak(randomEngine_)];
        }

        const auto &[tx, ty, tz] = *target;
        auto &options = possibilities[tx][ty][tz];
        std::uniform_int_distribution<std::size_t> pick(0, options.size() - 1);
        const std::uint32_t chosen = options[pick(randomEngine_)];
        options = { chosen };

        queue.clear();
        queue.push_back({ tx, ty, tz });
        head = 0;
        while (head < queue.size()) {
            const auto [x, y, z] = queue[head++];
            if (!propagateFrom(x, y, z)) {
                return false;
            }
        }
    }

    for (std::uint32_t x = 0; x < width_; ++x) {
        for (std::uint32_t y = 0; y < height_; ++y) {
            for (std::uint32_t z = 0; z < depth_; ++z) {
                grid_[x][y][z].resolvedTileName = tileList[possibilities[x][y][z].front()]->name;
            }
        }
    }
    return true;
}

JSON WaveFunctionCollapse::SaveToJson() const {
    JSON json = JSON::object();
    json["seed"] = seed_;
    json["gridWidth"] = width_;
    json["gridHeight"] = height_;
    json["gridDepth"] = depth_;

    JSON tilesJson = JSON::array();
    for (const auto &[name, tile] : tiles_) {
        JSON tileJson = JSON::object();
        tileJson["name"] = tile.name;
        JSON connectionsJson = JSON::object();
        for (std::size_t dir = 0; dir < kDirectionCount; ++dir) {
            connectionsJson[kDirectionKeys[dir]] = tile.connections[dir];
        }
        tileJson["connections"] = connectionsJson;
        tilesJson.push_back(tileJson);
    }
    json["tiles"] = tilesJson;

    JSON fixedTilesJson = JSON::array();
    for (std::uint32_t x = 0; x < width_; ++x) {
        for (std::uint32_t y = 0; y < height_; ++y) {
            for (std::uint32_t z = 0; z < depth_; ++z) {
                if (const auto &fixed = grid_[x][y][z].fixedTileName) {
                    JSON entry = JSON::object();
                    entry["x"] = x;
                    entry["y"] = y;
                    entry["z"] = z;
                    entry["tileName"] = *fixed;
                    fixedTilesJson.push_back(entry);
                }
            }
        }
    }
    json["fixedTiles"] = fixedTilesJson;

    if (startPosition_) {
        json["startPosition"] = JSON{
            {"x", (*startPosition_)[0]}, {"y", (*startPosition_)[1]}, {"z", (*startPosition_)[2]}
        };
    } else {
        json["startPosition"] = nullptr;
    }

    return json;
}

bool WaveFunctionCollapse::LoadFromJson(const JSON &json) {
    if (!json.contains("gridWidth") || !json.contains("gridHeight") || !json.contains("gridDepth")) {
        return false;
    }

    seed_ = json.value("seed", std::uint32_t{ 0 });
    randomEngine_.seed(seed_);

    SetGridSize(
        json.at("gridWidth").get<std::uint32_t>(),
        json.at("gridHeight").get<std::uint32_t>(),
        json.at("gridDepth").get<std::uint32_t>());

    tiles_.clear();
    if (json.contains("tiles")) {
        for (const auto &tileJson : json.at("tiles")) {
            Tile tile;
            tile.name = tileJson.value("name", std::string{});
            if (tileJson.contains("connections")) {
                const auto &connectionsJson = tileJson.at("connections");
                for (std::size_t dir = 0; dir < kDirectionCount; ++dir) {
                    if (connectionsJson.contains(kDirectionKeys[dir])) {
                        tile.connections[dir] = connectionsJson.at(kDirectionKeys[dir]).get<std::vector<std::string>>();
                    }
                }
            }
            RegisterTile(tile);
        }
    }

    if (json.contains("fixedTiles")) {
        for (const auto &entry : json.at("fixedTiles")) {
            FixTile(
                entry.at("x").get<std::uint32_t>(),
                entry.at("y").get<std::uint32_t>(),
                entry.at("z").get<std::uint32_t>(),
                entry.value("tileName", std::string{}));
        }
    }

    if (json.contains("startPosition") && !json.at("startPosition").is_null()) {
        const auto &startPositionJson = json.at("startPosition");
        SetStartPosition(
            startPositionJson.at("x").get<std::uint32_t>(),
            startPositionJson.at("y").get<std::uint32_t>(),
            startPositionJson.at("z").get<std::uint32_t>());
    }

    return true;
}

bool WaveFunctionCollapse::IsInBounds(std::uint32_t x, std::uint32_t y, std::uint32_t z) const noexcept {
    return x < width_ && y < height_ && z < depth_;
}

} // namespace KashipanEngine
