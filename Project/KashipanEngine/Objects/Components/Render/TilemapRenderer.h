#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Assets/MaterialManager.h"
#include "Assets/ModelManager.h"
#include "Assets/TextureManager.h"
#include "Graphics/PipelineManager.h"
#include "Math/Vector2.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#if defined(USE_IMGUI)
#include "Utilities/AssetDragDropPayload.h"
#include "Utilities/ImGuiCustom.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief RPGツクール風のオートタイル対応タイルマップ描画コンポーネント
/// @details 自身はSceneRendererへ描画登録せず、同一オブジェクトのMeshFilter/SpriteRendererを
///          自動追加して駆動する（ScreenBufferViewportと同じパターン）。MeshRendererではなく
///          SpriteRendererを使う理由: パイプライン名"Object2D.*"はシーンビュー（エディター）上で
///          gCamera3D投影のWorldバリアントへ差し替えられて初めて3Dシーンビュー内に正しく表示されるが、
///          この差し替え（SceneRenderer.cpp::ResolveEditorWorldPipelineName）はSpriteRenderer由来の
///          エントリにしか適用されない。MeshRendererで同じパイプラインを使うと、この差し替えが
///          行われずシーンビュー上で真っ黒・位置ズレして表示される（実ゲーム画面では問題ない）。
///          タイル配置(cells_)が変更されると、各非空セルについて上下左右の隣接セルを見て
///          「同じ接続グループのセルが存在するか」から4bit(0〜15)のビットマスクを求め、
///          タイル種類ごとにタイルセット画像上へ4x4=16パターン敷き詰めたブロックの中から
///          対応する1タイルを選んでUVを割り当てた結合メッシュを構築する。
///          ビットマスクのビットは 1=北(+Y方向), 2=東(+X方向), 4=南(-Y方向), 8=西(-X方向)で、
///          ブロック内の並びは (ビットマスク % 4, ビットマスク / 4) を (列, 行) として
///          tilesetOriginPx を左上原点に敷き詰められていることを前提とする。
class TilemapRenderer final : public IObjectComponent {
public:
    /// @brief タイル種類の定義（1種類につきタイルセット画像上に4x4=16パターンのオートタイルブロックを持つ）
    struct TileTypeDef {
        /// @brief 接続グループ。同じ値同士のセルだけを「繋がっている」とみなす（地形・海等の分離用）
        int connectionGroup = 0;
        /// @brief タイルセット画像上での16パターンブロックの左上原点（ピクセル）
        Vector2 tilesetOriginPx{ 0.0f, 0.0f };
    };

    OBJECT_COMPONENT_CONSTRUCTOR(TilemapRenderer, 0xFF,
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(materialName_, [this] {
            materialHandle_ = MaterialManager::kInvalidHandle;
            MarkMeshDirty();
            ApplyRendererSettings();
        });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(pipelineName_, [this] { ApplyRendererSettings(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(tileSize_, [this] { MarkMeshDirty(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(tilePixelSize_, [this] { MarkMeshDirty(); });
    )
    COMPONENT_CATEGORY("Render")
    ~TilemapRenderer() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<TilemapRenderer>();
        ptr->materialName_ = materialName_;
        ptr->pipelineName_ = pipelineName_;
        ptr->tileSize_ = tileSize_;
        ptr->tilePixelSize_ = tilePixelSize_;
        ptr->gridWidth_ = gridWidth_;
        ptr->gridHeight_ = gridHeight_;
        ptr->cells_ = cells_;
        ptr->tileTypes_ = tileTypes_;
        ptr->MarkMeshDirty();
        return ptr;
    }

    //==================================================
    // グリッド編集
    //==================================================

    int GetGridWidth() const noexcept { return gridWidth_; }
    int GetGridHeight() const noexcept { return gridHeight_; }

    /// @brief グリッドサイズを変更する（既存のタイル配置は(0,0)基準で可能な範囲まで維持される）
    void Resize(int width, int height) {
        width = std::max(0, width);
        height = std::max(0, height);
        if (width == gridWidth_ && height == gridHeight_) return;
        std::vector<int> newCells(static_cast<size_t>(width) * height, -1);
        for (int y = 0; y < std::min(height, gridHeight_); ++y) {
            for (int x = 0; x < std::min(width, gridWidth_); ++x) {
                newCells[static_cast<size_t>(y) * width + x] = cells_[static_cast<size_t>(y) * gridWidth_ + x];
            }
        }
        gridWidth_ = width;
        gridHeight_ = height;
        cells_ = std::move(newCells);
        MarkMeshDirty();
    }

    /// @brief 全セルを空にする
    void Clear() {
        std::fill(cells_.begin(), cells_.end(), -1);
        MarkMeshDirty();
    }

    /// @brief セルにタイル種類を設定する（範囲外は無視。tileTypeIndexに-1を渡すと空セルにする）
    void SetTile(int x, int y, int tileTypeIndex) {
        if (!IsInRange(x, y)) return;
        const size_t index = CellIndex(x, y);
        if (cells_[index] == tileTypeIndex) return;
        cells_[index] = tileTypeIndex;
        MarkMeshDirty();
    }
    /// @brief セルのタイル種類を取得する（範囲外・空セルは-1）
    int GetTile(int x, int y) const {
        if (!IsInRange(x, y)) return -1;
        return cells_[CellIndex(x, y)];
    }

    //==================================================
    // タイル種類
    //==================================================

    int GetTileTypeCount() const noexcept { return static_cast<int>(tileTypes_.size()); }
    /// @brief タイル種類を追加し、その新しいインデックスを返す
    int AddTileType(int connectionGroup, const Vector2 &tilesetOriginPx) {
        tileTypes_.push_back(TileTypeDef{ connectionGroup, tilesetOriginPx });
        MarkMeshDirty();
        return static_cast<int>(tileTypes_.size()) - 1;
    }
    /// @brief タイル種類を削除する。削除したインデックスを参照していたセルは空になり、
    ///        それより大きいインデックスを参照していたセルは1つ繰り上がる
    void RemoveTileType(int index) {
        if (index < 0 || index >= static_cast<int>(tileTypes_.size())) return;
        tileTypes_.erase(tileTypes_.begin() + index);
        for (auto &cell : cells_) {
            if (cell == index) cell = -1;
            else if (cell > index) --cell;
        }
        MarkMeshDirty();
    }
    const std::vector<TileTypeDef> &GetTileTypes() const noexcept { return tileTypes_; }
    void SetTileTypeConnectionGroup(int index, int connectionGroup) {
        if (index < 0 || index >= static_cast<int>(tileTypes_.size())) return;
        tileTypes_[index].connectionGroup = connectionGroup;
        MarkMeshDirty();
    }
    void SetTileTypeOriginPx(int index, const Vector2 &originPx) {
        if (index < 0 || index >= static_cast<int>(tileTypes_.size())) return;
        tileTypes_[index].tilesetOriginPx = originPx;
        MarkMeshDirty();
    }

    //==================================================
    // タイルセット・パイプライン指定
    //==================================================

    void SetMaterialName(const std::string &materialName) {
        materialName_ = materialName;
        materialHandle_ = MaterialManager::kInvalidHandle;
        MarkMeshDirty();
        ApplyRendererSettings();
    }
    const std::string &GetMaterialName() const noexcept { return materialName_; }
    void SetPipelineName(const std::string &pipelineName) {
        pipelineName_ = pipelineName;
        ApplyRendererSettings();
    }
    const std::string &GetPipelineName() const noexcept { return pipelineName_; }
    void SetTileSize(const Vector2 &tileSize) { tileSize_ = tileSize; MarkMeshDirty(); }
    const Vector2 &GetTileSize() const noexcept { return tileSize_; }
    void SetTilePixelSize(const Vector2 &tilePixelSize) { tilePixelSize_ = tilePixelSize; MarkMeshDirty(); }
    const Vector2 &GetTilePixelSize() const noexcept { return tilePixelSize_; }

protected:
    void Initialize() override {
        EnsureSiblingComponents();
        ApplyRendererSettings();
        MarkMeshDirty();
    }

    void Update() override {
        if (meshDirty_) RebuildMesh();
    }

#if defined(USE_IMGUI)
    /// @brief エディターでPlayしていない間もメッシュ再構築を行うためのフック
    /// @details Update()はScene::UpdateInterfaceがPlay中（isPlaying_）にしか呼ばないため
    ///          （Scene.h参照）、それだけに頼るとエディターでタイルを編集しても見た目に反映されない。
    ///          ShowPersistentImGuiInterfaceはPlay中かどうかに関わらず毎フレーム呼ばれるため
    ///          （TextureSource::ShowPersistentImGuiと同じ理由・同じ対処）、ここでも同じ再構築を行う。
    ///          RebuildMesh()側でmeshDirty_をfalseにするため、Update()と同一フレームで両方
    ///          呼ばれても二重に構築されることはない
    void ShowPersistentImGui() override {
        if (meshDirty_) RebuildMesh();
    }

    void ShowImGui() override {
        if (ImGuiCustom::SelectString(TranslationLabel("component.tilemaprenderer.pipeline"), pipelineName_, PipelineManager::GetLoadedRenderPipelineNames("2D"))) {
            ApplyRendererSettings();
        }
        const auto materialEntries = MaterialManager::GetLoadedMaterialListEntries();
        std::vector<std::string> materialNames;
        for (const auto &entry : materialEntries) materialNames.push_back(entry.material.name);
        if (ImGuiCustom::SelectString(TranslationLabel("component.tilemaprenderer.material"), materialName_, materialNames)) {
            materialHandle_ = MaterialManager::kInvalidHandle;
            MarkMeshDirty();
            ApplyRendererSettings();
        }
        if (std::string droppedPath; AcceptAssetDragDropTarget(kMaterialAssetDragDropType, droppedPath)) {
            for (const auto &entry : materialEntries) {
                if (entry.assetPath == droppedPath) {
                    materialName_ = entry.material.name;
                    materialHandle_ = MaterialManager::kInvalidHandle;
                    MarkMeshDirty();
                    ApplyRendererSettings();
                    break;
                }
            }
        }
        ImGui::TextUnformatted(TranslationC("component.tilemaprenderer.desc_1"));

        if (ImGui::DragFloat2(TranslationLabel("component.tilemaprenderer.tile_size"), &tileSize_.x, 0.01f, 0.01f, 1000.0f)) MarkMeshDirty();
        if (ImGui::DragFloat2(TranslationLabel("component.tilemaprenderer.tile_pixel_size"), &tilePixelSize_.x, 0.5f, 1.0f, 4096.0f)) MarkMeshDirty();

        int width = gridWidth_;
        int height = gridHeight_;
        if (ImGui::DragInt(TranslationLabel("component.tilemaprenderer.grid_width"), &width, 1.0f, 0, 4096)) Resize(width, height);
        if (ImGui::DragInt(TranslationLabel("component.tilemaprenderer.grid_height"), &height, 1.0f, 0, 4096)) Resize(width, height);

        ImGui::Separator();
        ImGui::TextUnformatted(TranslationC("component.tilemaprenderer.tile_types"));
        int removeIndex = -1;
        for (int i = 0; i < static_cast<int>(tileTypes_.size()); ++i) {
            ImGui::PushID(i);
            ImGui::Text("%d", i);
            ImGui::SameLine();
            if (ImGui::InputInt(TranslationLabel("component.tilemaprenderer.connection_group"), &tileTypes_[i].connectionGroup)) MarkMeshDirty();
            if (ImGui::DragFloat2(TranslationLabel("component.tilemaprenderer.tileset_origin_px"), &tileTypes_[i].tilesetOriginPx.x, 1.0f)) MarkMeshDirty();
            if (ImGui::Button(TranslationC("component.tilemaprenderer.remove_tile_type"))) removeIndex = i;
            ImGui::PopID();
        }
        if (removeIndex >= 0) RemoveTileType(removeIndex);
        if (ImGui::Button(TranslationC("component.tilemaprenderer.add_tile_type"))) {
            AddTileType(0, Vector2(0.0f, 0.0f));
        }

        ImGui::Separator();
        ImGui::TextUnformatted(TranslationC("component.tilemaprenderer.cells_bulk_edit"));
        if (ImGui::Button(TranslationC("component.tilemaprenderer.load_cells_to_text"))) {
            cellsEditBuffer_.clear();
            for (int y = gridHeight_ - 1; y >= 0; --y) {
                for (int x = 0; x < gridWidth_; ++x) {
                    cellsEditBuffer_ += std::to_string(cells_[CellIndex(x, y)]);
                    if (x + 1 < gridWidth_) cellsEditBuffer_ += ",";
                }
                cellsEditBuffer_ += "\n";
            }
        }
        ImGui::InputTextMultiline("##TilemapCellsBulkEdit", &cellsEditBuffer_, ImVec2(-1.0f, 150.0f));
        if (ImGui::Button(TranslationC("component.tilemaprenderer.apply_cells_from_text"))) {
            ApplyCellsFromText();
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["materialName"] = materialName_;
        json["pipelineName"] = pipelineName_;
        json["tileSize"] = ToJSON(tileSize_);
        json["tilePixelSize"] = ToJSON(tilePixelSize_);
        json["gridWidth"] = gridWidth_;
        json["gridHeight"] = gridHeight_;
        json["cells"] = cells_;
        JSON tileTypesJson = JSON::array();
        for (const auto &tileType : tileTypes_) {
            JSON tileTypeJson = JSON::object();
            tileTypeJson["connectionGroup"] = tileType.connectionGroup;
            tileTypeJson["tilesetOriginPx"] = ToJSON(tileType.tilesetOriginPx);
            tileTypesJson.push_back(tileTypeJson);
        }
        json["tileTypes"] = tileTypesJson;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        materialName_ = json.value("materialName", std::string{ "Default" });
        materialHandle_ = MaterialManager::kInvalidHandle;
        pipelineName_ = json.value("pipelineName", std::string{ "Object2D.DoubleSidedCulling.BlendNormal" });
        tileSize_ = json.contains("tileSize") ? FromJSON<Vector2>(json["tileSize"]) : Vector2(1.0f, 1.0f);
        tilePixelSize_ = json.contains("tilePixelSize") ? FromJSON<Vector2>(json["tilePixelSize"]) : Vector2(32.0f, 32.0f);
        gridWidth_ = json.value("gridWidth", 0);
        gridHeight_ = json.value("gridHeight", 0);
        cells_ = json.contains("cells")
            ? json["cells"].get<std::vector<int>>()
            : std::vector<int>(static_cast<size_t>(gridWidth_) * gridHeight_, -1);

        tileTypes_.clear();
        for (const auto &tileTypeJson : json.value("tileTypes", JSON::array())) {
            TileTypeDef tileType;
            tileType.connectionGroup = tileTypeJson.value("connectionGroup", 0);
            tileType.tilesetOriginPx = tileTypeJson.contains("tilesetOriginPx")
                ? FromJSON<Vector2>(tileTypeJson["tilesetOriginPx"]) : Vector2(0.0f, 0.0f);
            tileTypes_.push_back(tileType);
        }

        MarkMeshDirty();
        // シーン読み込み時はコンポーネント追加時点でInitialize()が読み込み前の既定値で
        // 呼ばれてしまっているため（TextureSource::LoadFromJsonと同じ理由）、アクティブなら
        // ここでMeshFilter/SpriteRendererへ改めて設定し直す。メッシュ本体はmeshDirty_経由でUpdate()が再構築する
        if (IsActive()) {
            EnsureSiblingComponents();
            ApplyRendererSettings();
        }
        return true;
    }

private:
    bool IsInRange(int x, int y) const noexcept { return x >= 0 && y >= 0 && x < gridWidth_ && y < gridHeight_; }
    size_t CellIndex(int x, int y) const noexcept { return static_cast<size_t>(y) * gridWidth_ + x; }
    void MarkMeshDirty() noexcept { meshDirty_ = true; }

    void EnsureSiblingComponents() {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;
        if (!objectContext->GetComponent<MeshFilter>()) objectContext->AddComponent<MeshFilter>();
        if (!objectContext->GetComponent<SpriteRenderer>()) objectContext->AddComponent<SpriteRenderer>();
    }

    /// @brief 同一オブジェクトのSpriteRendererへマテリアル・パイプライン名を反映する
    /// @details MeshRendererではなくSpriteRendererを使う理由はクラス冒頭のコメント参照
    void ApplyRendererSettings() {
        auto *objectContext = GetOwnerObjectContext();
        auto *spriteRenderer = objectContext ? objectContext->GetComponent<SpriteRenderer>() : nullptr;
        if (!spriteRenderer) return;
        spriteRenderer->SetPipelineName(pipelineName_);
        spriteRenderer->SetMaterialName(materialName_);
    }

    MaterialManager::MaterialHandle ResolveMaterialHandle() const {
        if (materialHandle_ == MaterialManager::kInvalidHandle && !materialName_.empty()) {
            materialHandle_ = MaterialManager::GetMaterialHandleFromName(materialName_);
        }
        return materialHandle_;
    }

    /// @brief セル(x,y)が非空で、指定接続グループと同じ種類か判定する（範囲外はfalse）
    bool ConnectsTo(int x, int y, int connectionGroup) const {
        if (!IsInRange(x, y)) return false;
        const int neighborType = cells_[CellIndex(x, y)];
        if (neighborType < 0 || neighborType >= static_cast<int>(tileTypes_.size())) return false;
        return tileTypes_[neighborType].connectionGroup == connectionGroup;
    }

    static ModelData::Vertex MakeVertex(float x, float y, float u, float v) {
        ModelData::Vertex vertex{};
        vertex.px = x; vertex.py = y; vertex.pz = 0.0f;
        vertex.nx = 0.0f; vertex.ny = 0.0f; vertex.nz = 1.0f;
        vertex.u = u; vertex.v = v;
        vertex.tx = 1.0f; vertex.ty = 0.0f; vertex.tz = 0.0f;
        return vertex;
    }

    /// @brief タイル配置から結合メッシュを再構築し、MeshFilterへ反映する
    /// @details テクスチャがまだ解決できない（読み込み中・未設定）場合は何もせずmeshDirty_を
    ///          立てたままにし、次回のUpdate()で再試行する
    void RebuildMesh() {
        auto *objectContext = GetOwnerObjectContext();
        auto *meshFilter = objectContext ? objectContext->GetComponent<MeshFilter>() : nullptr;
        if (!meshFilter) return;

        const auto materialHandle = ResolveMaterialHandle();
        const auto *material = MaterialManager::GetMaterial(materialHandle);
        const auto textureView = TextureManager::GetTextureView(material ? material->textureHandle : TextureManager::kInvalidHandle);
        const float texWidth = static_cast<float>(textureView.GetWidth());
        const float texHeight = static_cast<float>(textureView.GetHeight());
        if (texWidth <= 0.0f || texHeight <= 0.0f) return; // 未解決。meshDirty_はtrueのまま次回再試行

        meshDirty_ = false;

        std::vector<ModelData::Vertex> vertices;
        std::vector<std::uint32_t> indices;
        vertices.reserve(static_cast<size_t>(gridWidth_) * gridHeight_ * 4);
        indices.reserve(static_cast<size_t>(gridWidth_) * gridHeight_ * 6);

        for (int y = 0; y < gridHeight_; ++y) {
            for (int x = 0; x < gridWidth_; ++x) {
                const int tileType = cells_[CellIndex(x, y)];
                if (tileType < 0 || tileType >= static_cast<int>(tileTypes_.size())) continue;
                const TileTypeDef &def = tileTypes_[tileType];

                int bitmask = 0;
                if (ConnectsTo(x, y + 1, def.connectionGroup)) bitmask |= 1; // 北(+Y)
                if (ConnectsTo(x + 1, y, def.connectionGroup)) bitmask |= 2; // 東(+X)
                if (ConnectsTo(x, y - 1, def.connectionGroup)) bitmask |= 4; // 南(-Y)
                if (ConnectsTo(x - 1, y, def.connectionGroup)) bitmask |= 8; // 西(-X)

                const int subCol = bitmask % 4;
                const int subRow = bitmask / 4;
                const float u0 = (def.tilesetOriginPx.x + static_cast<float>(subCol) * tilePixelSize_.x) / texWidth;
                const float v0 = (def.tilesetOriginPx.y + static_cast<float>(subRow) * tilePixelSize_.y) / texHeight;
                const float u1 = u0 + tilePixelSize_.x / texWidth;
                const float v1 = v0 + tilePixelSize_.y / texHeight;

                const float px0 = static_cast<float>(x) * tileSize_.x;
                const float py0 = static_cast<float>(y) * tileSize_.y;
                const float px1 = px0 + tileSize_.x;
                const float py1 = py0 + tileSize_.y;

                // Rect2Dプリミティブ（AddQuad）と同じ規約: p0=左下 p1=左上 p2=右上 p3=右下、
                // 法線+Z、UVはVの上が0・下が1
                const auto base = static_cast<std::uint32_t>(vertices.size());
                vertices.push_back(MakeVertex(px0, py0, u0, v1));
                vertices.push_back(MakeVertex(px0, py1, u0, v0));
                vertices.push_back(MakeVertex(px1, py1, u1, v0));
                vertices.push_back(MakeVertex(px1, py0, u1, v1));
                indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
                indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
            }
        }

        if (meshHandle_ == ModelManager::kInvalidHandle) {
            meshHandle_ = ModelManager::RegisterProceduralMesh(meshRegistryName_, std::move(vertices), std::move(indices));
            meshFilter->SetMeshHandle(meshHandle_);
        } else {
            ModelManager::UpdateProceduralMesh(meshHandle_, std::move(vertices), std::move(indices));
        }
    }

#if defined(USE_IMGUI)
    /// @brief cellsEditBuffer_（行=Y、カンマ区切り=X、-1=空のテキスト）を解析してcells_へ反映する
    /// @details 行数・列数がgridHeight_/gridWidth_と異なる場合はResizeしてから反映する
    void ApplyCellsFromText() {
        std::vector<std::vector<int>> rows;
        size_t pos = 0;
        while (pos <= cellsEditBuffer_.size()) {
            size_t lineEnd = cellsEditBuffer_.find('\n', pos);
            if (lineEnd == std::string::npos) lineEnd = cellsEditBuffer_.size();
            std::string line = cellsEditBuffer_.substr(pos, lineEnd - pos);
            if (!line.empty()) {
                std::vector<int> row;
                size_t cellPos = 0;
                while (cellPos <= line.size()) {
                    size_t commaPos = line.find(',', cellPos);
                    if (commaPos == std::string::npos) commaPos = line.size();
                    const std::string token = line.substr(cellPos, commaPos - cellPos);
                    try {
                        row.push_back(std::stoi(token));
                    } catch (...) {
                        row.push_back(-1);
                    }
                    cellPos = commaPos + 1;
                }
                rows.push_back(std::move(row));
            }
            pos = lineEnd + 1;
        }
        if (rows.empty()) return;

        const int newHeight = static_cast<int>(rows.size());
        int newWidth = 0;
        for (const auto &row : rows) newWidth = std::max(newWidth, static_cast<int>(row.size()));
        Resize(newWidth, newHeight);

        // テキストは上の行から並んでいる（load_cells_to_textと対称）ため、Y座標は下から数えて割り当てる
        for (int lineIndex = 0; lineIndex < newHeight; ++lineIndex) {
            const int y = newHeight - 1 - lineIndex;
            const auto &row = rows[lineIndex];
            for (int x = 0; x < static_cast<int>(row.size()); ++x) {
                SetTile(x, y, row[x]);
            }
        }
    }
#endif

    std::string materialName_ = "Default";
    mutable MaterialManager::MaterialHandle materialHandle_ = MaterialManager::kInvalidHandle;
    std::string pipelineName_ = "Object2D.DoubleSidedCulling.BlendNormal";

    Vector2 tileSize_{ 1.0f, 1.0f };
    Vector2 tilePixelSize_{ 32.0f, 32.0f };

    int gridWidth_ = 0;
    int gridHeight_ = 0;
    /// @brief row-major（インデックス = y * gridWidth_ + x）。値はtileTypes_内のインデックス、-1=空
    std::vector<int> cells_;
    std::vector<TileTypeDef> tileTypes_;

    bool meshDirty_ = true;
    /// @brief RegisterProceduralMesh登録名（インスタンスごとに一意にするためthisのアドレスを使う。
    ///        TextureSource::overrideMaterialName_と同じ手法）
    const std::string meshRegistryName_ = "__TilemapRendererMesh_" + std::to_string(reinterpret_cast<std::uintptr_t>(this));
    ModelManager::ModelHandle meshHandle_ = ModelManager::kInvalidHandle;

#if defined(USE_IMGUI)
    /// @brief セル一括編集用のテキストバッファ（ShowImGuiの「セルをテキストへ読み込み」ボタンで生成される）
    std::string cellsEditBuffer_;
#endif
};

REGISTER_COMPONENT_OBJECT(TilemapRenderer)

} // namespace KashipanEngine
