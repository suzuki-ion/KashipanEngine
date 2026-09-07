#pragma once
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Assets/MaterialManager.h"
#include "Assets/ModelManager.h"
#include "Assets/TextureManager.h"
#include "Graphics/PipelineManager.h"
#include "Math/Vector2.h"
#include "Objects/EmptyObject.h"
#include "Objects/Components/Collider/Box2DCollider.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Objects/Components/Transform.h"
#include "Scene/SceneContext.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/AssetDragDropPayload.h"
#include "Utilities/ImGuiCustom.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief RPGツクール風のオートタイル対応タイルマップ描画コンポーネント
/// @details 自身はSceneRendererへ描画登録せず、タイル配置を一定サイズ（kChunkSize四方）の
///          チャンクへ分割し、チャンクごとに自動生成した子オブジェクト（Transform+MeshFilter+
///          SpriteRenderer、ChunkState参照）が実際の描画を担う。1マスタイルを書き換えるたびに
///          グリッド全体を再構築していた旧実装と異なり、変更されたチャンクとその隣接チャンク
///          （オートタイル判定が参照しうる範囲）だけをdirty化して再構築するため、大きなマップで
///          実行時に頻繁にタイルを書き換えても負荷が抑えられる。MeshRendererではなく
///          SpriteRendererを使う理由: パイプライン名"Object2D.*"はシーンビュー（エディター）上で
///          gCamera3D投影のWorldバリアントへ差し替えられて初めて3Dシーンビュー内に正しく表示されるが、
///          この差し替え（SceneRenderer.cpp::ResolveEditorWorldPipelineName）はSpriteRenderer由来の
///          エントリにしか適用されない。MeshRendererで同じパイプラインを使うと、この差し替えが
///          行われずシーンビュー上で真っ黒・位置ズレして表示される（実ゲーム画面では問題ない）。
///          タイル配置(cells_)が変更されると、各非空セルについて隣接セルを見て「自分のタイル種類が
///          connectsToTileTypesに隣接セルのタイル種類インデックスを含んでいるか」（片方向の判定。
///          相手側が自分を含んでいなくても自分側の判定には影響しない）を調べ、UVを割り当てた
///          結合メッシュを構築する。判定方向はGetAutotileMode()で切り替えられる:
///          - FourDirection（既定）: 上下左右4方向のみを見て4bit(0〜15)のビットマスクを求め、
///            タイルセット画像上へ4x4=16パターン敷き詰めたブロック（(ビットマスク%4, ビットマスク/4)
///            を(列,行)として tilesetOriginPx を左上原点に敷き詰め）から1タイルを選ぶ。
///            ビットマスクのビットは 1=北(+Y方向), 2=東(+X方向), 4=南(-Y方向), 8=西(-X方向)。
///          - EightDirection: 斜め4方向も含めて判定し、1タイルを4つの角（左上/右上/左下/右下）に
///            分割してそれぞれ合成する（4分割合成方式）。タイルセット画像は1つのタイル種類につき
///            2列×3行のブロックを前提とする: (0,0)=Isolated（孤立時にまるごと使う）、
///            (1,0)=Cross（上下左右4方向のみ接続・斜め4つとも非接続の時にまるごと使う）、
///            (0,1)/(1,1)/(0,2)/(1,2)=2×2ブロック（それぞれ角の位置に対応）。
///            各角は常に「出力する角と同じ位置の1/4」を参照し、状態に応じて参照元だけが変わる:
///            隣接2方向とも非接続=2×2ブロックの自分と同じ位置のマス、縦方向だけ接続=上下反転した
///            位置のマス、横方向だけ接続=左右反転した位置のマス、縦横・斜め全部接続=対角のマス、
///            縦横は接続・斜めのみ非接続=Crossタイル。反転・入れ替えは一切行わない
///            （詳細はAppendEightDirCorner参照）
class TilemapRenderer final : public IObjectComponent {
public:
    /// @brief オートタイル判定方向。FourDirectionは既定値で既存シーンとの互換性を保つ
    enum class AutotileMode {
        FourDirection = 0,
        EightDirection = 1,
    };

    /// @brief EightDirectionモードで使う、Isolated/Crossタイルの位置（タイル単位。
    ///        tilesetOriginPxからのオフセット）。2×2ブロックの位置は列0-1×行1-2に固定
    struct EightDirLayout {
        /// @brief 孤立時（8方向すべて非接続）にまるごと1枚使うタイルの位置（タイル単位）
        Vector2 isolatedTilePos{ 0.0f, 0.0f };
        /// @brief 上下左右4方向のみ接続時（斜めは4つとも非接続）にまるごと1枚使うタイルの位置（タイル単位）
        Vector2 crossTilePos{ 1.0f, 0.0f };
    };

    /// @brief タイル種類の定義（1種類につきタイルセット画像上にオートタイル用ブロックを持つ。
    ///        ブロックのレイアウトはAutotileModeにより異なる）
    struct TileTypeDef {
        /// @brief 接続先タイル種類インデックスの一覧（片方向）。隣接セルのタイル種類インデックスが
        ///        この一覧に含まれていれば、自分から見てそのセルへ「繋がっている」とみなす。
        ///        相互に繋げたい場合は双方のconnectsToTileTypesに互いのインデックスを入れる必要がある
        std::vector<int> connectsToTileTypes;
        /// @brief タイルセット画像上での16パターンブロックの左上原点（ピクセル）
        Vector2 tilesetOriginPx{ 0.0f, 0.0f };
        /// @brief 2D当たり判定自動生成の対象にするか（GetGenerateColliders()がtrueの時のみ意味を持つ）
        bool isSolid = true;
    };

    OBJECT_COMPONENT_CONSTRUCTOR(TilemapRenderer, 0xFF,
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(materialName_, [this] {
            materialHandle_ = MaterialManager::kInvalidHandle;
            MarkAllChunksDirty();
            ApplyRendererSettings();
        });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(pipelineName_, [this] { ApplyRendererSettings(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(tileSize_, [this] { MarkAllChunksDirty(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(tilePixelSize_, [this] { MarkAllChunksDirty(); });
    )
    COMPONENT_CATEGORY("Render")
    ~TilemapRenderer() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<TilemapRenderer>();
        ptr->targetObjectID_ = targetObjectID_;
        ptr->excludedRenderTargetNames_ = excludedRenderTargetNames_;
        ptr->materialName_ = materialName_;
        ptr->pipelineName_ = pipelineName_;
        ptr->tileSize_ = tileSize_;
        ptr->tilePixelSize_ = tilePixelSize_;
        ptr->gridWidth_ = gridWidth_;
        ptr->gridHeight_ = gridHeight_;
        ptr->cells_ = cells_;
        ptr->tileTypes_ = tileTypes_;
        ptr->generateColliders_ = generateColliders_;
        ptr->autotileMode_ = autotileMode_;
        ptr->eightDirLayout_ = eightDirLayout_;
        return ptr;
    }

    //==================================================
    // 描画先指定
    //==================================================

    /// @brief 描画先オブジェクトを設定する（描画先コンポーネントが付与されたオブジェクト。
    ///        SpriteRenderer::SetTargetObjectと同じ意味。全チャンクのSpriteRendererへ反映される）
    void SetTargetObject(const EmptyObject *targetObject) {
        targetObjectID_ = targetObject ? targetObject->GetObjectID() : UUID128();
        ApplyRendererSettings();
    }
    /// @brief 描画先オブジェクトをUUIDから設定する
    void SetTargetObject(const UUID128 &targetObjectID) {
        targetObjectID_ = targetObjectID;
        ApplyRendererSettings();
    }
    /// @brief 描画先オブジェクトのUUIDを取得する
    const UUID128 &GetTargetObjectID() const noexcept { return targetObjectID_; }
    /// @brief 描画先オブジェクトを取得（存在しない場合はnullptr）
    EmptyObject *GetTargetObject() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !targetObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(targetObjectID_);
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
        // チャンク数自体が変わらないリサイズ（例: 31→32）だとEnsureChunkTopology側の
        // 「チャンク数が変わらなければ何もしない」早期returnに引っかかり、新しく増えた
        // セルが再構築されないまま取り残されてしまう。そのため常に全チャンクdirty化しておく
        MarkAllChunksDirty();
    }

    /// @brief 全セルを空にする
    void Clear() {
        std::fill(cells_.begin(), cells_.end(), -1);
        MarkAllChunksDirty();
    }

    /// @brief セルにタイル種類を設定する（範囲外は無視。tileTypeIndexに-1を渡すと空セルにする）
    void SetTile(int x, int y, int tileTypeIndex) {
        if (!IsInRange(x, y)) return;
        const size_t index = CellIndex(x, y);
        if (cells_[index] == tileTypeIndex) return;
        cells_[index] = tileTypeIndex;
        MarkCellDirty(x, y);
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
    /// @brief タイル種類を追加し、その新しいインデックスを返す（接続先は空で始まる。
    ///        AddTileTypeConnectionで自分自身を含め接続先タイル種類を追加すること）
    int AddTileType(const Vector2 &tilesetOriginPx) {
        TileTypeDef tileType;
        tileType.tilesetOriginPx = tilesetOriginPx;
        tileTypes_.push_back(tileType);
        MarkAllChunksDirty();
        return static_cast<int>(tileTypes_.size()) - 1;
    }
    /// @brief タイル種類を削除する。削除したインデックスを参照していたセルは空になり、
    ///        それより大きいインデックスを参照していたセルは1つ繰り上がる。他のタイル種類が
    ///        connectsToTileTypesで削除対象を参照していた場合も同様に削除・繰り上げる
    void RemoveTileType(int index) {
        if (index < 0 || index >= static_cast<int>(tileTypes_.size())) return;
        tileTypes_.erase(tileTypes_.begin() + index);
        for (auto &cell : cells_) {
            if (cell == index) cell = -1;
            else if (cell > index) --cell;
        }
        for (auto &tileType : tileTypes_) {
            auto &targets = tileType.connectsToTileTypes;
            targets.erase(std::remove(targets.begin(), targets.end(), index), targets.end());
            for (auto &target : targets) {
                if (target > index) --target;
            }
        }
        MarkAllChunksDirty();
    }
    const std::vector<TileTypeDef> &GetTileTypes() const noexcept { return tileTypes_; }
    /// @brief タイル種類indexの接続先へtargetTileTypeIndexを1つ追加する（片方向。既に含まれている
    ///        場合は何もしない。自分自身のindexを追加すると自分自身と接続するようになる）
    void AddTileTypeConnection(int index, int targetTileTypeIndex) {
        if (index < 0 || index >= static_cast<int>(tileTypes_.size())) return;
        auto &targets = tileTypes_[index].connectsToTileTypes;
        if (std::find(targets.begin(), targets.end(), targetTileTypeIndex) != targets.end()) return;
        targets.push_back(targetTileTypeIndex);
        MarkAllChunksDirty();
    }
    /// @brief タイル種類indexの接続先からtargetTileTypeIndexを1つ削除する
    void RemoveTileTypeConnection(int index, int targetTileTypeIndex) {
        if (index < 0 || index >= static_cast<int>(tileTypes_.size())) return;
        auto &targets = tileTypes_[index].connectsToTileTypes;
        const auto it = std::find(targets.begin(), targets.end(), targetTileTypeIndex);
        if (it == targets.end()) return;
        targets.erase(it);
        MarkAllChunksDirty();
    }
    int GetTileTypeConnectionCount(int index) const {
        if (index < 0 || index >= static_cast<int>(tileTypes_.size())) return 0;
        return static_cast<int>(tileTypes_[index].connectsToTileTypes.size());
    }
    /// @brief タイル種類indexのconnectsToTileTypes[connectionIndex]を取得する（範囲外は0）
    int GetTileTypeConnectionAt(int index, int connectionIndex) const {
        if (index < 0 || index >= static_cast<int>(tileTypes_.size())) return 0;
        const auto &targets = tileTypes_[index].connectsToTileTypes;
        if (connectionIndex < 0 || connectionIndex >= static_cast<int>(targets.size())) return 0;
        return targets[connectionIndex];
    }
    void SetTileTypeOriginPx(int index, const Vector2 &originPx) {
        if (index < 0 || index >= static_cast<int>(tileTypes_.size())) return;
        tileTypes_[index].tilesetOriginPx = originPx;
        MarkAllChunksDirty();
    }
    /// @brief タイル種類が2D当たり判定自動生成の対象かを設定する（GetGenerateColliders()参照）
    void SetTileTypeSolid(int index, bool solid) {
        if (index < 0 || index >= static_cast<int>(tileTypes_.size())) return;
        tileTypes_[index].isSolid = solid;
        MarkCollidersDirty(); // 見た目のメッシュには影響しないため、コライダー再生成のみ要求する
    }
    bool GetTileTypeSolid(int index) const {
        if (index < 0 || index >= static_cast<int>(tileTypes_.size())) return false;
        return tileTypes_[index].isSolid;
    }

    //==================================================
    // タイルセット・パイプライン指定
    //==================================================

    void SetMaterialName(const std::string &materialName) {
        materialName_ = materialName;
        materialHandle_ = MaterialManager::kInvalidHandle;
        MarkAllChunksDirty();
        ApplyRendererSettings();
    }
    const std::string &GetMaterialName() const noexcept { return materialName_; }
    /// @brief マテリアルハンドルを取得（未解決の場合はマテリアル名から解決を試みる。ブラシサムネイル表示等に使う）
    MaterialManager::MaterialHandle GetMaterialHandle() const noexcept { return ResolveMaterialHandle(); }
    void SetPipelineName(const std::string &pipelineName) {
        pipelineName_ = pipelineName;
        ApplyRendererSettings();
    }
    const std::string &GetPipelineName() const noexcept { return pipelineName_; }
    void SetTileSize(const Vector2 &tileSize) { tileSize_ = tileSize; MarkAllChunksDirty(); }
    const Vector2 &GetTileSize() const noexcept { return tileSize_; }
    void SetTilePixelSize(const Vector2 &tilePixelSize) { tilePixelSize_ = tilePixelSize; MarkAllChunksDirty(); }
    const Vector2 &GetTilePixelSize() const noexcept { return tilePixelSize_; }
    /// @brief オートタイルの判定方向を設定する（クラス冒頭のコメント参照。タイルセット画像の
    ///        レイアウトもモードにより異なるため、切り替えると見た目が変わる）
    void SetAutotileMode(AutotileMode mode) { autotileMode_ = mode; MarkAllChunksDirty(); }
    AutotileMode GetAutotileMode() const noexcept { return autotileMode_; }
    /// @brief EightDirectionモードで使う参照位置一式をまとめて設定する
    void SetEightDirLayout(const EightDirLayout &layout) { eightDirLayout_ = layout; MarkAllChunksDirty(); }
    const EightDirLayout &GetEightDirLayout() const noexcept { return eightDirLayout_; }

    //==================================================
    // 2D当たり判定自動生成
    //==================================================

    /// @brief タイル配置から2D当たり判定（Box2DCollider）を自動生成するかを設定する
    /// @details 隣接するisSolidなセルを貪欲法で矩形へ結合してから生成する（RegenerateColliders参照）。
    ///          falseにすると、これまで自動生成していたBox2DColliderは全て削除される
    void SetGenerateColliders(bool enabled) { generateColliders_ = enabled; MarkCollidersDirty(); }
    bool GetGenerateColliders() const noexcept { return generateColliders_; }

    /// @brief タイル編集をまとめて行う間、コライダーの再生成を保留する
    /// @details 1マス変更するたびにRegenerateColliders()（グリッド全体を毎回スキャンする）が
    ///          走ってしまうと、エディターのペイントツールで連続的にSetTile()する場面で
    ///          大きなグリッドほど重くなる。保留中はSetTile()等でdirty化されても実際の
    ///          再生成を行わず、保留を解除（false）した時点でまだ再生成が必要な変更が
    ///          残っていれば、そこで1回だけまとめて再生成する。通常のゲームプレイ中に
    ///          単発でSetTile()する分にはこれを使わず、即座にコライダーへ反映されたままでよい
    void SetCollidersRegenerationSuspended(bool suspended) {
        collidersRegenerationSuspended_ = suspended;
        if (!suspended && collidersDirty_) {
            RegenerateColliders();
            collidersDirty_ = false;
        }
    }
    bool IsCollidersRegenerationSuspended() const noexcept { return collidersRegenerationSuspended_; }

protected:
    void Initialize() override {
        // ここではチャンク子オブジェクトの生成（EnsureChunkTopology）を呼ばない。
        // 所有オブジェクトがシーンへ完全に登録・アタッチされきる前にSceneContext経由で
        // 子オブジェクトを生成しようとすると、シーン読み込み中の状態次第で不安定になりうる
        // （ScreenBufferViewport::Initialize()と同じ理由・同じ対処）。実際の生成は
        // Update()/ShowPersistentImGui()の初回（＝同オブジェクトの全コンポーネントが
        // 読み込まれきった後）に遅延する。dirtyフラグは既定でtrueのため、ここで何もしなくても
        // 初回のRebuildMesh()で正しく構築される
    }

    void Update() override {
        RebuildMesh();
    }

#if defined(USE_IMGUI)
    /// @brief エディターでPlayしていない間もメッシュ再構築を行うためのフック
    /// @details Update()はScene::UpdateInterfaceがPlay中（isPlaying_）にしか呼ばないため
    ///          （Scene.h参照）、それだけに頼るとエディターでタイルを編集しても見た目に反映されない。
    ///          ShowPersistentImGuiInterfaceはPlay中かどうかに関わらず毎フレーム呼ばれるため
    ///          （TextureSource::ShowPersistentImGuiと同じ理由・同じ対処）、ここでも同じ再構築を行う。
    ///          RebuildMesh()側で各dirtyフラグをfalseにするため、Update()と同一フレームで両方
    ///          呼ばれても二重に構築されることはない
    void ShowPersistentImGui() override {
        RebuildMesh();
    }

    void ShowImGui() override {
        // 描画先はシーン上のオブジェクトから選択（ヒエラルキーからのD&Dも受け付ける。SpriteRendererと同じUI）。
        // ここで設定した内容は全チャンクのSpriteRendererへApplyRendererSettings()経由で反映される
        if (TargetObjectSelector::ShowSelector(TranslationLabel("component.common.target"), GetOwnerSceneContext(), targetObjectID_)) {
            ApplyRendererSettings();
        }
        // 対象オブジェクトが持つ描画先ごとに描画する/しないを選択する（戻り値が無いため、直前の値と
        // 比較して実際に変更があった場合のみApplyRendererSettings()を呼ぶ。ApplyRendererSettings()は
        // 全チャンク（最大数百個）のSpriteRendererへSetXXXを呼び直し、それぞれがMarkDrawListDirty()で
        // SceneRenderer側の描画リストキャッシュ全体を無効化するため、Inspector表示中に無条件で毎フレーム
        // 呼ぶと選択している間ずっと毎フレーム描画リストが再構築される重大な性能劣化になる
        // （Inspectorでオブジェクトを選択した途端に重くなる不具合として発現した）
        const auto excludedRenderTargetNamesBefore = excludedRenderTargetNames_;
        TargetObjectSelector::ShowRenderTargetFilters(GetOwnerSceneContext(), targetObjectID_, excludedRenderTargetNames_);
        if (excludedRenderTargetNames_ != excludedRenderTargetNamesBefore) {
            ApplyRendererSettings();
        }

        if (ImGuiCustom::SelectString(TranslationLabel("component.tilemaprenderer.pipeline"), pipelineName_, PipelineManager::GetLoadedRenderPipelineNames("2D"))) {
            ApplyRendererSettings();
        }
        const auto materialEntries = MaterialManager::GetLoadedMaterialListEntries();
        std::vector<std::string> materialNames;
        for (const auto &entry : materialEntries) materialNames.push_back(entry.material.name);
        if (ImGuiCustom::SelectString(TranslationLabel("component.tilemaprenderer.material"), materialName_, materialNames)) {
            materialHandle_ = MaterialManager::kInvalidHandle;
            MarkAllChunksDirty();
            ApplyRendererSettings();
        }
        if (std::string droppedPath; AcceptAssetDragDropTarget(kMaterialAssetDragDropType, droppedPath)) {
            for (const auto &entry : materialEntries) {
                if (entry.assetPath == droppedPath) {
                    materialName_ = entry.material.name;
                    materialHandle_ = MaterialManager::kInvalidHandle;
                    MarkAllChunksDirty();
                    ApplyRendererSettings();
                    break;
                }
            }
        }
        ImGui::TextUnformatted(TranslationC("component.tilemaprenderer.desc_1"));

        {
            int modeIndex = static_cast<int>(autotileMode_);
            if (ImGui::RadioButton(TranslationLabel("component.tilemaprenderer.mode_4dir"), modeIndex == 0)) {
                SetAutotileMode(AutotileMode::FourDirection);
            }
            ImGui::SameLine();
            if (ImGui::RadioButton(TranslationLabel("component.tilemaprenderer.mode_8dir"), modeIndex == 1)) {
                SetAutotileMode(AutotileMode::EightDirection);
            }
            if (ImGui::IsItemHovered()) ImGuiCustom::SetTooltipWrapped("%s", TranslationC("component.tilemaprenderer.desc_mode"));
        }

        if (autotileMode_ == AutotileMode::EightDirection && ImGui::CollapsingHeader(TranslationC("component.tilemaprenderer.eight_dir_layout"))) {
            ImGui::Indent();
            ImGui::TextUnformatted(TranslationC("component.tilemaprenderer.desc_eight_dir_layout"));
            if (ImGui::DragFloat2(TranslationLabel("component.tilemaprenderer.layout_isolated"), &eightDirLayout_.isolatedTilePos.x, 0.1f)) MarkAllChunksDirty();
            if (ImGui::DragFloat2(TranslationLabel("component.tilemaprenderer.layout_cross"), &eightDirLayout_.crossTilePos.x, 0.1f)) MarkAllChunksDirty();
            ImGui::Unindent();
        }

        if (ImGui::DragFloat2(TranslationLabel("component.tilemaprenderer.tile_size"), &tileSize_.x, 0.01f, 0.01f, 1000.0f)) MarkAllChunksDirty();
        if (ImGui::DragFloat2(TranslationLabel("component.tilemaprenderer.tile_pixel_size"), &tilePixelSize_.x, 0.5f, 1.0f, 4096.0f)) MarkAllChunksDirty();

        int width = gridWidth_;
        int height = gridHeight_;
        if (ImGui::DragInt(TranslationLabel("component.tilemaprenderer.grid_width"), &width, 1.0f, 0, 4096)) Resize(width, height);
        if (ImGui::DragInt(TranslationLabel("component.tilemaprenderer.grid_height"), &height, 1.0f, 0, 4096)) Resize(width, height);

        ImGui::Separator();
        ImGui::TextUnformatted(TranslationC("component.tilemaprenderer.tile_types"));
        int removeIndex = -1;
        for (int i = 0; i < static_cast<int>(tileTypes_.size()); ++i) {
            ImGui::PushID(i);
            // タイル種類が増えても見やすいよう、1種類ごとに折りたたみ可能なセクションへ分ける
            const std::string headerLabel = TranslationC("component.tilemaprenderer.tile_type_header") + std::to_string(i);
            if (ImGui::CollapsingHeader(headerLabel.c_str())) {
                ImGui::Indent();
                if (ImGui::DragFloat2(TranslationLabel("component.tilemaprenderer.tileset_origin_px"), &tileTypes_[i].tilesetOriginPx.x, 1.0f)) MarkAllChunksDirty();
                if (ImGui::Checkbox(TranslationLabel("component.tilemaprenderer.solid"), &tileTypes_[i].isSolid)) MarkCollidersDirty();

                // 接続先タイル種類インデックスの小さなリスト（追加・削除ボタン付き、片方向）。
                // このタイル種類から見て、リストに含まれるインデックスのセルへ「繋がっている」と判定される
                // （ConnectsTo参照）。相互に接続させたい場合は双方のリストへ互いのインデックスを追加すること
                ImGui::Spacing();
                ImGui::TextUnformatted(TranslationC("component.tilemaprenderer.connections"));
                auto &connections = tileTypes_[i].connectsToTileTypes;
                int removeConnectionAt = -1;
                for (int c = 0; c < static_cast<int>(connections.size()); ++c) {
                    ImGui::PushID(c);
                    ImGui::SetNextItemWidth(120.0f);
                    if (ImGui::InputInt("##connection", &connections[c])) MarkAllChunksDirty();
                    ImGui::SameLine();
                    if (ImGui::Button(TranslationC("component.tilemaprenderer.remove_connection"))) removeConnectionAt = c;
                    ImGui::PopID();
                }
                if (removeConnectionAt >= 0) {
                    connections.erase(connections.begin() + removeConnectionAt);
                    MarkAllChunksDirty();
                }
                if (ImGui::Button(TranslationC("component.tilemaprenderer.add_connection"))) {
                    connections.push_back(i);
                    MarkAllChunksDirty();
                }

                ImGui::Spacing();
                if (ImGui::Button(TranslationC("component.tilemaprenderer.remove_tile_type"))) removeIndex = i;
                ImGui::Unindent();
            }
            ImGui::PopID();
        }
        if (removeIndex >= 0) RemoveTileType(removeIndex);
        if (ImGui::Button(TranslationC("component.tilemaprenderer.add_tile_type"))) {
            AddTileType(Vector2(0.0f, 0.0f));
        }

        if (ImGui::Checkbox(TranslationLabel("component.tilemaprenderer.generate_colliders"), &generateColliders_)) MarkCollidersDirty();
        if (ImGui::IsItemHovered()) ImGuiCustom::SetTooltipWrapped("%s", TranslationC("component.tilemaprenderer.desc_generate_colliders"));

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
        json["targetObjectID"] = ToJSON(targetObjectID_);
        for (const auto &name : excludedRenderTargetNames_) {
            json["excludedRenderTargetNames"].push_back(name);
        }
        json["materialName"] = materialName_;
        json["pipelineName"] = pipelineName_;
        json["tileSize"] = ToJSON(tileSize_);
        json["tilePixelSize"] = ToJSON(tilePixelSize_);
        json["gridWidth"] = gridWidth_;
        json["gridHeight"] = gridHeight_;
        // cells_を1マス1要素のJSON配列でそのまま書き出すと、大きなグリッド（例: 800x800なら64万要素）で
        // nlohmann::jsonのノード構築コストが無視できなくなる。SaveToJson()はUndoスナップショットや
        // クラッシュ復元用スナップショット（SceneManager、1秒間隔で編集有無に関わらず常時実行）等、
        // 実際の変更頻度よりずっと高い頻度で呼ばれるため、行ごとにカンマ区切り文字列へエンコードして
        // 1行1要素（＝1マス1要素よりずっと少ない要素数）に圧縮する。1マス変更しただけでもその行の文字列
        // 全体が変わって見えるためGit差分の粒度はマス単位から行単位へ粗くなるが、シーンファイルの
        // 行数・パース対象ノード数を大幅に減らせる。旧形式（フラットな数値配列）のシーンファイルは
        // LoadFromJson()側で読み込めるようにしてある
        JSON cellsJson = JSON::array();
        for (int y = 0; y < gridHeight_; ++y) {
            cellsJson.push_back(EncodeCellsRow(&cells_[CellIndex(0, y)], gridWidth_));
        }
        json["cells"] = std::move(cellsJson);
        JSON tileTypesJson = JSON::array();
        for (const auto &tileType : tileTypes_) {
            JSON tileTypeJson = JSON::object();
            tileTypeJson["connectsToTileTypes"] = tileType.connectsToTileTypes;
            tileTypeJson["tilesetOriginPx"] = ToJSON(tileType.tilesetOriginPx);
            tileTypeJson["isSolid"] = tileType.isSolid;
            tileTypesJson.push_back(tileTypeJson);
        }
        json["tileTypes"] = tileTypesJson;
        json["generateColliders"] = generateColliders_;
        json["autotileMode"] = static_cast<int>(autotileMode_);
        JSON layoutJson = JSON::object();
        layoutJson["isolatedTilePos"] = ToJSON(eightDirLayout_.isolatedTilePos);
        layoutJson["crossTilePos"] = ToJSON(eightDirLayout_.crossTilePos);
        json["eightDirLayout"] = layoutJson;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        targetObjectID_ = json.contains("targetObjectID") ? FromJSON<UUID128>(json["targetObjectID"]) : UUID128();
        excludedRenderTargetNames_.clear();
        for (const auto &name : json.value("excludedRenderTargetNames", std::vector<std::string>())) {
            excludedRenderTargetNames_.insert(name);
        }
        materialName_ = json.value("materialName", std::string{ "Default" });
        materialHandle_ = MaterialManager::kInvalidHandle;
        pipelineName_ = json.value("pipelineName", std::string{ "Object2D.DoubleSidedCulling.BlendNormal" });
        tileSize_ = json.contains("tileSize") ? FromJSON<Vector2>(json["tileSize"]) : Vector2(1.0f, 1.0f);
        tilePixelSize_ = json.contains("tilePixelSize") ? FromJSON<Vector2>(json["tilePixelSize"]) : Vector2(32.0f, 32.0f);
        gridWidth_ = json.value("gridWidth", 0);
        gridHeight_ = json.value("gridHeight", 0);
        cells_.assign(static_cast<size_t>(gridWidth_) * gridHeight_, -1);
        if (json.contains("cells") && json["cells"].is_array()) {
            const JSON &cellsJson = json["cells"];
            if (!cellsJson.empty() && cellsJson[0].is_string()) {
                // 新形式: 行ごとにカンマ区切り文字列でエンコードされている（SaveToJson参照）
                const int rowCount = std::min(gridHeight_, static_cast<int>(cellsJson.size()));
                for (int y = 0; y < rowCount; ++y) {
                    DecodeCellsRow(cellsJson[y].get<std::string>(), &cells_[CellIndex(0, y)], gridWidth_);
                }
            } else {
                // 旧形式（1マス1要素のフラットな数値配列）との後方互換
                const auto flat = cellsJson.get<std::vector<int>>();
                std::copy_n(flat.begin(), std::min(flat.size(), cells_.size()), cells_.begin());
            }
        }

        tileTypes_.clear();
        for (const auto &tileTypeJson : json.value("tileTypes", JSON::array())) {
            TileTypeDef tileType;
            tileType.connectsToTileTypes = tileTypeJson.value("connectsToTileTypes", std::vector<int>{});
            tileType.tilesetOriginPx = tileTypeJson.contains("tilesetOriginPx")
                ? FromJSON<Vector2>(tileTypeJson["tilesetOriginPx"]) : Vector2(0.0f, 0.0f);
            tileType.isSolid = tileTypeJson.value("isSolid", true);
            tileTypes_.push_back(tileType);
        }
        generateColliders_ = json.value("generateColliders", false);
        autotileMode_ = static_cast<AutotileMode>(json.value("autotileMode", 0));
        if (json.contains("eightDirLayout")) {
            const JSON &layoutJson = json["eightDirLayout"];
            eightDirLayout_.isolatedTilePos = layoutJson.contains("isolatedTilePos") ? FromJSON<Vector2>(layoutJson["isolatedTilePos"]) : Vector2(0.0f, 0.0f);
            eightDirLayout_.crossTilePos = layoutJson.contains("crossTilePos") ? FromJSON<Vector2>(layoutJson["crossTilePos"]) : Vector2(1.0f, 0.0f);
        } else {
            eightDirLayout_ = EightDirLayout{};
        }

        MarkAllChunksDirty();
        // シーン読み込み時はコンポーネント追加時点でInitialize()が読み込み前の既定値で
        // 呼ばれてしまっているため（TextureSource::LoadFromJsonと同じ理由）、アクティブなら
        // ここでチャンク子オブジェクトへ改めて設定し直す。メッシュ本体は各dirtyフラグ経由で
        // Update()が再構築する
        if (IsActive()) {
            EnsureChunkTopology();
            ApplyRendererSettings();
        }
        return true;
    }

private:
    /// @brief チャンク1つ分の実行時状態（シーンJSONには保存しない。EnsureChunkTopology参照）
    struct ChunkState {
        /// @brief このチャンク専用の子オブジェクト（Transform+MeshFilter+SpriteRenderer持ち）
        EmptyObject *object = nullptr;
        ModelManager::ModelHandle meshHandle = ModelManager::kInvalidHandle;
        /// @brief RegisterProceduralMesh登録名（遅延生成。RebuildChunkMesh参照）
        std::string meshRegistryName;
        bool dirty = true;
    };
    /// @brief 1チャンクの一辺のセル数
    static constexpr int kChunkSize = 32;

    bool IsInRange(int x, int y) const noexcept { return x >= 0 && y >= 0 && x < gridWidth_ && y < gridHeight_; }
    size_t CellIndex(int x, int y) const noexcept { return static_cast<size_t>(y) * gridWidth_ + x; }

    /// @brief セル1行分（width個のint、row[0]がx=0）をカンマ区切り文字列へエンコードする（SaveToJson参照）
    static std::string EncodeCellsRow(const int *row, int width) {
        std::string result;
        result.reserve(static_cast<size_t>(width) * 4);
        char buffer[16];
        for (int x = 0; x < width; ++x) {
            if (x != 0) result += ',';
            const auto res = std::to_chars(buffer, buffer + sizeof(buffer), row[x]);
            result.append(buffer, res.ptr);
        }
        return result;
    }
    /// @brief EncodeCellsRow()の逆変換。encodedのトークン数がwidthに満たない場合、残りは空セル(-1)として扱う
    static void DecodeCellsRow(const std::string &encoded, int *outRow, int width) {
        size_t pos = 0;
        for (int x = 0; x < width; ++x) {
            int value = -1;
            if (pos <= encoded.size()) {
                size_t comma = encoded.find(',', pos);
                if (comma == std::string::npos) comma = encoded.size();
                std::from_chars(encoded.data() + pos, encoded.data() + comma, value);
                pos = comma + 1;
            }
            outRow[x] = value;
        }
    }

    /// @brief 全チャンクをdirty化する（グリッド全体・タイル種類定義に影響する変更で使う）
    void MarkAllChunksDirty() noexcept {
        for (auto &chunk : chunks_) chunk.dirty = true;
        anyChunkDirty_ = !chunks_.empty();
        collidersDirty_ = true;
    }
    /// @brief コライダーの再生成だけを要求する（見た目のメッシュには影響しない変更で使う）
    void MarkCollidersDirty() noexcept { collidersDirty_ = true; }
    /// @brief セル(x,y)を含むチャンクと、オートタイル判定が参照しうる周囲8方向の隣接チャンクを
    ///        dirty化する
    /// @details オートタイルはEightDirectionモードで斜め含め周囲8方向を参照しうるため、
    ///          チャンク境界付近のセルを変更すると、変更したセル自身のチャンクだけでなく
    ///          隣接チャンクの境界タイルの見た目も変わりうる。安全のため常に3x3セル分の
    ///          範囲にかかるチャンクをまとめてdirty化しておく（FourDirectionモードでも
    ///          過剰マージンなだけで害はない）
    void MarkCellDirty(int x, int y) noexcept {
        collidersDirty_ = true;
        if (chunks_.empty()) return; // トポロジ未構築。初回EnsureChunkTopologyで全チャンクがdirty=trueとして生成される
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int nx = x + dx;
                const int ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= gridWidth_ || ny >= gridHeight_) continue;
                chunks_[ChunkIndex(nx / kChunkSize, ny / kChunkSize)].dirty = true;
            }
        }
        anyChunkDirty_ = true;
    }

    int ChunkIndex(int cx, int cy) const noexcept { return cy * chunkCountX_ + cx; }

    /// @brief gridWidth_/gridHeight_から必要なチャンク数を算出し、chunks_の構成をそれに合わせる
    /// @details チャンク数が変化する場合（Resize()直後等）は既存チャンクオブジェクトを全て破棄して
    ///          作り直す（Resize自体が頻繁には起きない操作のため、差分更新の複雑さより
    ///          単純さを優先する）。チャンク数が変わらない場合は何もせず即returnするため、
    ///          毎フレーム呼んでもコストはごく小さい
    void EnsureChunkTopology() {
        const int newCountX = gridWidth_ > 0 ? (gridWidth_ + kChunkSize - 1) / kChunkSize : 0;
        const int newCountY = gridHeight_ > 0 ? (gridHeight_ + kChunkSize - 1) / kChunkSize : 0;
        if (newCountX == chunkCountX_ && newCountY == chunkCountY_) return;

        DestroyAllChunkObjects();
        chunkCountX_ = newCountX;
        chunkCountY_ = newCountY;
        chunks_.assign(static_cast<size_t>(chunkCountX_) * chunkCountY_, ChunkState{});
        anyChunkDirty_ = !chunks_.empty();

        for (int cy = 0; cy < chunkCountY_; ++cy) {
            for (int cx = 0; cx < chunkCountX_; ++cx) {
                CreateChunkObject(cx, cy);
            }
        }
    }

    /// @brief チャンク(cx,cy)専用の子オブジェクト（Transform+MeshFilter+SpriteRenderer）を生成する
    /// @details MeshFilterは1オブジェクトにつき1個までのため（MeshFilter.h参照）、チャンクごとに
    ///          独立した描画データを持たせるにはBox2DColliderの自動生成のようにコンポーネントを
    ///          増やすだけでは足りず、子オブジェクトそのものを分ける必要がある。生成した子は
    ///          SetSaveEnabled(false)でシーンJSONへ保存せず、次回シーン読み込み時はEnsureChunkTopology
    ///          経由で毎回作り直す想定にする。頂点はこれまで通り自身のローカル座標系でそのまま
    ///          生成するため、子オブジェクトのTransformは既定（恒等）のまま親
    ///          （このコンポーネントの所有オブジェクト）へぶら下げるだけで元と同じ位置に描画される
    void CreateChunkObject(int cx, int cy) {
        auto *sceneContext = GetOwnerSceneContext();
        const auto *ownerObject = GetOwnerObject();
        if (!sceneContext || !ownerObject) return;
        auto *parent = sceneContext->GetSceneObject(ownerObject->GetObjectID());
        if (!parent) return;

        auto *chunkObj = sceneContext->CreateEmptyObject("TilemapChunk_" + std::to_string(cx) + "_" + std::to_string(cy));
        if (!chunkObj) return;
        chunkObj->SetSaveEnabled(false);
        if (auto *chunkTransform = chunkObj->GetComponent<Transform>()) {
            chunkTransform->SetParentObject(parent);
        }
        chunkObj->AddComponent<MeshFilter>();
        if (auto *spriteRenderer = chunkObj->AddComponent<SpriteRenderer>()) {
            spriteRenderer->SetPipelineName(pipelineName_);
            spriteRenderer->SetMaterialName(materialName_);
            spriteRenderer->SetTargetObject(targetObjectID_);
            spriteRenderer->SetExcludedRenderTargetNames(excludedRenderTargetNames_);
        }

        ChunkState &state = chunks_[ChunkIndex(cx, cy)];
        state.object = chunkObj;
        state.dirty = true;
    }

    /// @brief 全チャンクの子オブジェクトを破棄する
    /// @details EnsureChunkTopology()（Update()/ShowPersistentImGui()経由、通常の実行タイミング）
    ///          からのみ呼び出すこと。Finalize()から呼んではいけない: シーン全体の破棄
    ///          （Scene::ClearSceneObjects）はobjects_を直接ループしながら各オブジェクトの
    ///          Finalize()を呼ぶため、その最中にDeleteObjectでobjects_を変更するとイテレーターが
    ///          壊れて危険（Scene.cpp参照）。シーン全体の破棄時はScene側が全オブジェクト
    ///          （このチャンク子オブジェクトも含む）を一括で破棄するため、このコンポーネント側で
    ///          明示的に破棄しなくてもリークしない
    void DestroyAllChunkObjects() {
        auto *sceneContext = GetOwnerSceneContext();
        if (sceneContext) {
            for (auto &chunk : chunks_) {
                if (chunk.object) sceneContext->DeleteObject(chunk.object);
            }
        }
        chunks_.clear();
    }

    /// @brief 全チャンクのSpriteRendererへマテリアル・パイプライン名・描画先指定を反映する
    /// @details MeshRendererではなくSpriteRendererを使う理由はクラス冒頭のコメント参照。
    ///          描画先（targetObjectID_/excludedRenderTargetNames_）を反映し忘れると、
    ///          各チャンクのSpriteRendererが既定の描画先（＝TilemapRenderer自身が本来
    ///          意図した描画先とは異なる場合がある）のままになり、実際の画面には何も
    ///          描画されないという不具合になる
    void ApplyRendererSettings() {
        for (auto &chunk : chunks_) {
            if (!chunk.object) continue;
            auto *spriteRenderer = chunk.object->GetComponent<SpriteRenderer>();
            if (!spriteRenderer) continue;
            spriteRenderer->SetPipelineName(pipelineName_);
            spriteRenderer->SetMaterialName(materialName_);
            spriteRenderer->SetTargetObject(targetObjectID_);
            spriteRenderer->SetExcludedRenderTargetNames(excludedRenderTargetNames_);
        }
    }

    MaterialManager::MaterialHandle ResolveMaterialHandle() const {
        if (materialHandle_ == MaterialManager::kInvalidHandle && !materialName_.empty()) {
            materialHandle_ = MaterialManager::GetMaterialHandleFromName(materialName_);
        }
        return materialHandle_;
    }

    /// @brief セル(x,y)が非空で、そのタイル種類インデックスがmyTypeIndexのconnectsToTileTypesに
    ///        含まれているか判定する（範囲外・空セルはfalse）。片方向の判定であり、逆方向（隣接セルの
    ///        タイル種類がmyTypeIndexを含むか）は別途そちら側のConnectsTo呼び出しで判定される
    bool ConnectsTo(int x, int y, int myTypeIndex) const {
        if (!IsInRange(x, y)) return false;
        const int neighborType = cells_[CellIndex(x, y)];
        if (neighborType < 0) return false;
        if (myTypeIndex < 0 || myTypeIndex >= static_cast<int>(tileTypes_.size())) return false;
        const auto &targets = tileTypes_[myTypeIndex].connectsToTileTypes;
        return std::find(targets.begin(), targets.end(), neighborType) != targets.end();
    }

    static ModelData::Vertex MakeVertex(float x, float y, float u, float v) {
        ModelData::Vertex vertex{};
        vertex.px = x; vertex.py = y; vertex.pz = 0.0f;
        vertex.nx = 0.0f; vertex.ny = 0.0f; vertex.nz = 1.0f;
        vertex.u = u; vertex.v = v;
        vertex.tx = 1.0f; vertex.ty = 0.0f; vertex.tz = 0.0f;
        return vertex;
    }

    /// @brief ワールド矩形(wx0,wy0)-(wx1,wy1)に、4頂点それぞれ個別のUV(bl/tl/tr/br)を割り当てた
    ///        クアッドを積む（Rect2Dプリミティブと同じ規約: 頂点順=左下/左上/右上/右下、法線+Z）
    static void AppendQuadUV(std::vector<ModelData::Vertex> &vertices, std::vector<std::uint32_t> &indices,
        float wx0, float wy0, float wx1, float wy1, const Vector2 &bl, const Vector2 &tl, const Vector2 &tr, const Vector2 &br) {
        const auto base = static_cast<std::uint32_t>(vertices.size());
        vertices.push_back(MakeVertex(wx0, wy0, bl.x, bl.y));
        vertices.push_back(MakeVertex(wx0, wy1, tl.x, tl.y));
        vertices.push_back(MakeVertex(wx1, wy1, tr.x, tr.y));
        vertices.push_back(MakeVertex(wx1, wy0, br.x, br.y));
        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
    }

    /// @brief FourDirectionモード: 上下左右4方向のビットマスク(0〜15)から、タイルセット画像上の
    ///        4x4パターンブロックの中の1タイル分をそのままUVとして割り当てたクアッドを1枚積む
    void AppendFourDirectionTile(int x, int y, int tileType, const TileTypeDef &def,
        float px0, float py0, float px1, float py1, float texWidth, float texHeight,
        std::vector<ModelData::Vertex> &vertices, std::vector<std::uint32_t> &indices) const {
        int bitmask = 0;
        if (ConnectsTo(x, y + 1, tileType)) bitmask |= 1; // 北(+Y)
        if (ConnectsTo(x + 1, y, tileType)) bitmask |= 2; // 東(+X)
        if (ConnectsTo(x, y - 1, tileType)) bitmask |= 4; // 南(-Y)
        if (ConnectsTo(x - 1, y, tileType)) bitmask |= 8; // 西(-X)

        const int subCol = bitmask % 4;
        const int subRow = bitmask / 4;
        const float u0 = (def.tilesetOriginPx.x + static_cast<float>(subCol) * tilePixelSize_.x) / texWidth;
        const float v0 = (def.tilesetOriginPx.y + static_cast<float>(subRow) * tilePixelSize_.y) / texHeight;
        const float u1 = u0 + tilePixelSize_.x / texWidth;
        const float v1 = v0 + tilePixelSize_.y / texHeight;

        // UVはVの上が0・下が1
        AppendQuadUV(vertices, indices, px0, py0, px1, py1, Vector2(u0, v1), Vector2(u0, v0), Vector2(u1, v0), Vector2(u1, v1));
    }

    /// @brief EightDirectionモード: タイルセット画像上の(col,row)（タイル単位）のタイルを
    ///        丸ごと1枚として使う（孤立時のisolatedTilePos、全接続時のcrossTilePos専用）
    void AppendEightDirWholeCell(const Vector2 &originPx, const Vector2 &tilePos,
        float px0, float py0, float px1, float py1, float texWidth, float texHeight,
        std::vector<ModelData::Vertex> &vertices, std::vector<std::uint32_t> &indices) const {
        const float u0 = (originPx.x + tilePos.x * tilePixelSize_.x) / texWidth;
        const float v0 = (originPx.y + tilePos.y * tilePixelSize_.y) / texHeight;
        const float u1 = u0 + tilePixelSize_.x / texWidth;
        const float v1 = v0 + tilePixelSize_.y / texHeight;
        AppendQuadUV(vertices, indices, px0, py0, px1, py1, Vector2(u0, v1), Vector2(u0, v0), Vector2(u1, v0), Vector2(u1, v1));
    }

    /// @brief EightDirectionモード: 1つの角（左上/右上/左下/右下のいずれか）1枚分のクアッドを積む。
    ///        サンプリング元のタイル（2x2ブロックの4マスのいずれか、またはCrossタイル）は
    ///        この角自身の縦方向接続(vConn)・横方向接続(hConn)・斜め接続(diagConn)から選び、
    ///        選んだタイル内の(cornerCol, cornerRow)のクォーター（＝この角自身のラベルと同じ位置。
    ///        反転や入れ替えは行わない）をそのまま貼り付ける
    /// @param cornerCol 0=左, 1=右
    /// @param cornerRow 0=上, 1=下
    /// @param vConn この角に隣接する縦方向（上コーナーなら北、下コーナーなら南）の接続有無
    /// @param hConn この角に隣接する横方向（左コーナーなら西、右コーナーなら東）の接続有無
    /// @param diagConn この角の斜め方向の接続有無
    void AppendEightDirCorner(const Vector2 &originPx, int cornerCol, int cornerRow, bool vConn, bool hConn, bool diagConn,
        float wx0, float wy0, float wx1, float wy1, float texWidth, float texHeight,
        std::vector<ModelData::Vertex> &vertices, std::vector<std::uint32_t> &indices) const {
        Vector2 tilePos;
        if (!vConn && !hConn) {
            // どちらも非接続: 同じ位置の2x2ブロックマス（角パーツ）
            tilePos = Vector2(static_cast<float>(cornerCol), 1.0f + static_cast<float>(cornerRow));
        } else if (vConn && !hConn) {
            // 縦方向のみ接続: 行を反転した2x2ブロックマス（辺パーツ）
            tilePos = Vector2(static_cast<float>(cornerCol), 1.0f + static_cast<float>(1 - cornerRow));
        } else if (!vConn && hConn) {
            // 横方向のみ接続: 列を反転した2x2ブロックマス（辺パーツ）
            tilePos = Vector2(static_cast<float>(1 - cornerCol), 1.0f + static_cast<float>(cornerRow));
        } else if (diagConn) {
            // 縦横斜め全て接続（完全に囲まれている）: 行列とも反転した2x2ブロックマス（内側パーツ）
            tilePos = Vector2(static_cast<float>(1 - cornerCol), 1.0f + static_cast<float>(1 - cornerRow));
        } else {
            // 縦横は接続しているが斜めだけ非接続（凹み）: Crossタイルのこの角部分を使う
            tilePos = eightDirLayout_.crossTilePos;
        }

        const float quarterWidth = tilePixelSize_.x * 0.5f;
        const float quarterHeight = tilePixelSize_.y * 0.5f;
        const float u0 = (originPx.x + tilePos.x * tilePixelSize_.x + static_cast<float>(cornerCol) * quarterWidth) / texWidth;
        const float v0 = (originPx.y + tilePos.y * tilePixelSize_.y + static_cast<float>(cornerRow) * quarterHeight) / texHeight;
        const float u1 = u0 + quarterWidth / texWidth;
        const float v1 = v0 + quarterHeight / texHeight;
        AppendQuadUV(vertices, indices, wx0, wy0, wx1, wy1, Vector2(u0, v1), Vector2(u0, v0), Vector2(u1, v0), Vector2(u1, v1));
    }

    /// @brief EightDirectionモード: 1セル分（孤立・4方向接続の特別扱い、またはそれ以外は4つの角を
    ///        個別に合成）のクアッドを積む
    void AppendEightDirectionTile(int x, int y, int tileType, const TileTypeDef &def,
        float px0, float py0, float px1, float py1, float texWidth, float texHeight,
        std::vector<ModelData::Vertex> &vertices, std::vector<std::uint32_t> &indices) const {
        const bool n = ConnectsTo(x, y + 1, tileType);
        const bool e = ConnectsTo(x + 1, y, tileType);
        const bool s = ConnectsTo(x, y - 1, tileType);
        const bool w = ConnectsTo(x - 1, y, tileType);
        const bool ne = ConnectsTo(x + 1, y + 1, tileType);
        const bool se = ConnectsTo(x + 1, y - 1, tileType);
        const bool sw = ConnectsTo(x - 1, y - 1, tileType);
        const bool nw = ConnectsTo(x - 1, y + 1, tileType);

        if (!n && !e && !s && !w && !ne && !se && !sw && !nw) {
            AppendEightDirWholeCell(def.tilesetOriginPx, eightDirLayout_.isolatedTilePos, px0, py0, px1, py1, texWidth, texHeight, vertices, indices);
            return;
        }
        if (n && e && s && w && !ne && !se && !sw && !nw) {
            // 上下左右4方向のみ接続（斜めは4つとも非接続）の場合だけCrossタイルをそのまま使う。
            // 斜めが1つでも接続している場合（3x3配置の中央など）はここに該当させず、
            // 下の4隅個別合成へ進める（各隅がInner判定になり、継ぎ目のない内側の見た目になる）
            AppendEightDirWholeCell(def.tilesetOriginPx, eightDirLayout_.crossTilePos, px0, py0, px1, py1, texWidth, texHeight, vertices, indices);
            return;
        }

        const float hx = (px0 + px1) * 0.5f;
        const float hy = (py0 + py1) * 0.5f;
        // 各角(cornerCol, cornerRow): 0=左/上, 1=右/下。vConn=縦方向接続、hConn=横方向接続、diagConn=斜め接続
        AppendEightDirCorner(def.tilesetOriginPx, 0, 0, n, w, nw, px0, hy, hx, py1, texWidth, texHeight, vertices, indices);
        AppendEightDirCorner(def.tilesetOriginPx, 1, 0, n, e, ne, hx, hy, px1, py1, texWidth, texHeight, vertices, indices);
        AppendEightDirCorner(def.tilesetOriginPx, 0, 1, s, w, sw, px0, py0, hx, hy, texWidth, texHeight, vertices, indices);
        AppendEightDirCorner(def.tilesetOriginPx, 1, 1, s, e, se, hx, py0, px1, hy, texWidth, texHeight, vertices, indices);
    }

    /// @brief cells_/tileTypes_[].isSolidから貪欲法で矩形結合した2D当たり判定(Box2DCollider)を
    ///        同一オブジェクトへ生成し直す
    /// @details 自分が過去に生成したBox2DColliderだけを、各コライダー自身が持つ
    ///          Box2DCollider::IsAutoGenerated()フラグで識別・削除するため、ユーザーが手動で
    ///          追加した無関係なBox2DColliderには一切触れない。このフラグはJSONへ永続化される
    ///          （ランタイムのみのComponentRef一覧で追跡していた旧実装では、シーンの
    ///          フルリロード（Scene::PlayStop等）でTilemapRenderer自身のインスタンスが
    ///          作り直されて追跡用リストが空にリセットされる一方、JSONに保存済みの生成済み
    ///          コライダーはそのまま兄弟コンポーネントとして再読込されるため孤立してしまい、
    ///          再生・停止を繰り返すたびにコライダーが二重・三重に増え続ける不具合があった）。
    ///          generateColliders_がfalseの場合は削除するだけで新規生成は行わない
    ///          （＝オフにすると全て消える）。チャンク分割の対象外（メッシュとは独立に、collidersDirty_
    ///          が立っている時だけRebuildMesh()から呼ばれる）
    void RegenerateColliders() {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;

        std::vector<Box2DCollider *> toRemove;
        for (const auto &pair : objectContext->GetAllComponents()) {
            if (auto *box = dynamic_cast<Box2DCollider *>(pair.first)) {
                if (box->IsAutoGenerated()) toRemove.push_back(box);
            }
        }
        for (auto *box : toRemove) {
            objectContext->RemoveComponent(box);
        }

        if (!generateColliders_) return;

        std::vector<bool> solid(static_cast<size_t>(gridWidth_) * gridHeight_, false);
        for (int y = 0; y < gridHeight_; ++y) {
            for (int x = 0; x < gridWidth_; ++x) {
                const int tileType = cells_[CellIndex(x, y)];
                solid[CellIndex(x, y)] = (tileType >= 0 && tileType < static_cast<int>(tileTypes_.size()) && tileTypes_[tileType].isSolid);
            }
        }

        // 貪欲法による矩形分割: 未訪問のtrueセルを左上として右方向へ最大幅を求め、
        // その幅ぶんの行が全てtrue・未訪問である限り下方向へ高さを伸ばして1矩形を確定する
        std::vector<bool> visited(solid.size(), false);
        for (int y0 = 0; y0 < gridHeight_; ++y0) {
            for (int x0 = 0; x0 < gridWidth_; ++x0) {
                const size_t startIndex = CellIndex(x0, y0);
                if (!solid[startIndex] || visited[startIndex]) continue;

                int x1 = x0 + 1;
                while (x1 < gridWidth_ && solid[CellIndex(x1, y0)] && !visited[CellIndex(x1, y0)]) ++x1;

                int y1 = y0 + 1;
                while (y1 < gridHeight_) {
                    bool rowOk = true;
                    for (int x = x0; x < x1; ++x) {
                        const size_t idx = CellIndex(x, y1);
                        if (!solid[idx] || visited[idx]) { rowOk = false; break; }
                    }
                    if (!rowOk) break;
                    ++y1;
                }

                for (int y = y0; y < y1; ++y) {
                    for (int x = x0; x < x1; ++x) {
                        visited[CellIndex(x, y)] = true;
                    }
                }

                auto *collider = objectContext->AddComponent<Box2DCollider>();
                if (!collider) continue;
                const Vector2 size(static_cast<float>(x1 - x0) * tileSize_.x, static_cast<float>(y1 - y0) * tileSize_.y);
                const Vector2 center(static_cast<float>(x0) * tileSize_.x + size.x * 0.5f, static_cast<float>(y0) * tileSize_.y + size.y * 0.5f);
                collider->SetSize(size);
                collider->SetCenter(center);
                collider->SetAutoGenerated(true);
            }
        }
    }

    /// @brief dirtyなチャンクだけメッシュを再構築し、対応するチャンク子オブジェクトのMeshFilterへ
    ///        反映する
    /// @details テクスチャがまだ解決できない（読み込み中・未設定）場合は何もせずdirtyフラグを
    ///          立てたままにし、次回のUpdate()で再試行する（2D当たり判定はテクスチャの解決状態と
    ///          無関係のため、この判定より前にRegenerateColliders()を呼ぶ）。1マスの変更でも
    ///          グリッド全体を作り直していた旧実装と異なり、変更のあったチャンク（kChunkSize四方）
    ///          だけを再構築する
    void RebuildMesh() {
        EnsureChunkTopology();

        if (collidersDirty_ && !collidersRegenerationSuspended_) {
            RegenerateColliders();
            collidersDirty_ = false;
        }

        if (!anyChunkDirty_) return;

        const auto materialHandle = ResolveMaterialHandle();
        const auto *material = MaterialManager::GetMaterial(materialHandle);
        const auto textureView = TextureManager::GetTextureView(material ? material->textureHandle : TextureManager::kInvalidHandle);
        const float texWidth = static_cast<float>(textureView.GetWidth());
        const float texHeight = static_cast<float>(textureView.GetHeight());
        if (texWidth <= 0.0f || texHeight <= 0.0f) return; // 未解決。dirtyのまま次回再試行

        for (int cy = 0; cy < chunkCountY_; ++cy) {
            for (int cx = 0; cx < chunkCountX_; ++cx) {
                ChunkState &chunk = chunks_[ChunkIndex(cx, cy)];
                if (!chunk.dirty) continue;
                RebuildChunkMesh(cx, cy, chunk, texWidth, texHeight);
                chunk.dirty = false;
            }
        }
        anyChunkDirty_ = false;
    }

    /// @brief チャンク(cx,cy)が担当するセル範囲だけを走査してメッシュを構築し、そのチャンク専用の
    ///        MeshFilterへ反映する
    void RebuildChunkMesh(int cx, int cy, ChunkState &chunk, float texWidth, float texHeight) {
        if (!chunk.object) return;
        auto *meshFilter = chunk.object->GetComponent<MeshFilter>();
        if (!meshFilter) return;

        const int xBegin = cx * kChunkSize;
        const int xEnd = std::min(xBegin + kChunkSize, gridWidth_);
        const int yBegin = cy * kChunkSize;
        const int yEnd = std::min(yBegin + kChunkSize, gridHeight_);

        std::vector<ModelData::Vertex> vertices;
        std::vector<std::uint32_t> indices;
        // EightDirectionモードは1セル最大4枚（4頂点×4）のクアッドになりうるため、その分を見込んで確保する
        const size_t verticesPerCell = (autotileMode_ == AutotileMode::EightDirection) ? 16 : 4;
        const size_t indicesPerCell = (autotileMode_ == AutotileMode::EightDirection) ? 24 : 6;
        const size_t cellCount = static_cast<size_t>(std::max(0, xEnd - xBegin)) * static_cast<size_t>(std::max(0, yEnd - yBegin));
        vertices.reserve(cellCount * verticesPerCell);
        indices.reserve(cellCount * indicesPerCell);

        for (int y = yBegin; y < yEnd; ++y) {
            for (int x = xBegin; x < xEnd; ++x) {
                const int tileType = cells_[CellIndex(x, y)];
                if (tileType < 0 || tileType >= static_cast<int>(tileTypes_.size())) continue;
                const TileTypeDef &def = tileTypes_[tileType];

                const float px0 = static_cast<float>(x) * tileSize_.x;
                const float py0 = static_cast<float>(y) * tileSize_.y;
                const float px1 = px0 + tileSize_.x;
                const float py1 = py0 + tileSize_.y;

                if (autotileMode_ == AutotileMode::EightDirection) {
                    AppendEightDirectionTile(x, y, tileType, def, px0, py0, px1, py1, texWidth, texHeight, vertices, indices);
                } else {
                    AppendFourDirectionTile(x, y, tileType, def, px0, py0, px1, py1, texWidth, texHeight, vertices, indices);
                }
            }
        }

        if (chunk.meshHandle == ModelManager::kInvalidHandle) {
            if (chunk.meshRegistryName.empty()) {
                const auto *owner = GetOwnerObject();
                chunk.meshRegistryName = "__TilemapRendererMesh_" +
                    (owner ? owner->GetObjectID().ToString() : std::to_string(reinterpret_cast<std::uintptr_t>(this))) +
                    "_" + std::to_string(cx) + "_" + std::to_string(cy);
            }
            // RegisterProceduralMeshは登録名が既存の場合、頂点・インデックスを更新せず既存ハンドルを
            // そのまま返す仕様（ModelManager.h参照）。名前が衝突していても常に自分自身のセルデータが
            // 反映されるよう、登録直後に必ずUpdateProceduralMeshで上書きする
            chunk.meshHandle = ModelManager::RegisterProceduralMesh(chunk.meshRegistryName, vertices, indices);
            meshFilter->SetMeshHandle(chunk.meshHandle);
        }
        ModelManager::UpdateProceduralMesh(chunk.meshHandle, std::move(vertices), std::move(indices));
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

    UUID128 targetObjectID_{};
    /// @brief 除外する描画先の名前（GetRenderTargetName()）の集合。SpriteRenderer::excludedRenderTargetNames_と同じ意味
    std::unordered_set<std::string> excludedRenderTargetNames_;

    std::string materialName_ = "Default";
    mutable MaterialManager::MaterialHandle materialHandle_ = MaterialManager::kInvalidHandle;
    std::string pipelineName_ = "Object2D.DoubleSidedCulling.BlendNormal";

    Vector2 tileSize_{ 1.0f, 1.0f };
    Vector2 tilePixelSize_{ 32.0f, 32.0f };
    AutotileMode autotileMode_ = AutotileMode::FourDirection;
    EightDirLayout eightDirLayout_;

    int gridWidth_ = 0;
    int gridHeight_ = 0;
    /// @brief row-major（インデックス = y * gridWidth_ + x）。値はtileTypes_内のインデックス、-1=空
    std::vector<int> cells_;
    std::vector<TileTypeDef> tileTypes_;

    /// @brief タイル配置からBox2DColliderを自動生成するか（既定false。RegenerateColliders参照）
    bool generateColliders_ = false;

    /// @brief チャンク実行時状態（インデックス = cy * chunkCountX_ + cx）。シーンJSONには保存しない
    std::vector<ChunkState> chunks_;
    int chunkCountX_ = 0;
    int chunkCountY_ = 0;
    /// @brief chunks_内に1つでもdirty=trueなチャンクがあるか（RebuildMesh()の早期判定用キャッシュ）
    bool anyChunkDirty_ = true;
    /// @brief RegenerateColliders()の再実行が必要か
    bool collidersDirty_ = true;
    /// @brief trueの間はcollidersDirty_が立ってもRegenerateColliders()を実行しない
    ///        （SetCollidersRegenerationSuspended参照）
    bool collidersRegenerationSuspended_ = false;

#if defined(USE_IMGUI)
    /// @brief セル一括編集用のテキストバッファ（ShowImGuiの「セルをテキストへ読み込み」ボタンで生成される）
    std::string cellsEditBuffer_;
#endif
};

REGISTER_COMPONENT_OBJECT(TilemapRenderer)

} // namespace KashipanEngine
