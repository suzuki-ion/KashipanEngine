#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/GroundDefined.h"
#include "Objects/Components/SlowGroundDefined.h"
#include "StageGoalPlaneController.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <fstream>

namespace KashipanEngine {

// スポーンリクエスト構造体。
struct SpawnRequest {
    float stageProgress = 0.0f;// 0.0f～1.0fの範囲で、ステージのどのあたりにスポーンするか（0.0fが最初、1.0fがゴール）
    float radius = 0.0f;// スポーン位置の半径（中心からの距離）
    float angle = 0.0f;// スポーン位置の角度（ラジアン）
    float panelWidth = 0.0f;// パネルの幅
    float panelThickness = 0.0f;// パネルの厚み
    float panelLength = 0.0f;// パネルの長さ
};

/// ステージの地面を生成・管理するコンポーネント。JSONデータを元に一括で地面オブジェクトを生成します。
class StageGroundGenerator final : public ISceneComponent {
public:
    StageGroundGenerator() : ISceneComponent("StageGroundGenerator", 1) {}
    ~StageGroundGenerator() override = default;

    void Initialize() override {
		// 初期化
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

		// シーンに含まれるSceneDefaultVariablesコンポーネントを取得し、コライダーを初期化
        defaultVars_ = ctx->GetComponent<SceneDefaultVariables>();
        if (!defaultVars_) return;

        auto *colliderComp = defaultVars_->GetColliderComp();
        if (!colliderComp) return;

        collider_ = colliderComp->GetCollider();

        // リクエストリストを初期化
        spawnRequests_.clear();

        // ステージの読み込み
        nlohmann::json j;
        std::ifstream ifs(stageDataFilePath_);
        if (ifs.is_open()) {
            try {
                ifs >> j;
            }
            catch (const nlohmann::json::parse_error& e) {
                e;
                assert(false && "JSONファイルのパースに失敗しました。フォーマットを確認してください。");
            }
        } else {
            assert(false && "Failed to open stage data file.");
        }

        // jsonから制作時に想定したステージの長さを格納
        if (j.contains("stageLength")) {
            editStageLengthData = j["stageLength"].get<float>();
        }

        // jsonから配置リクエストを積む
		float maxStageProgress = 0.0f;
        if (j.contains("spawn_reqests") && j["spawn_reqests"].is_array()) {
            for (const auto &groundData : j["spawn_reqests"]) {
				SpawnRequest req{};
                if (groundData.contains("stageProgress") && groundData["stageProgress"].is_number()) {
                    req.stageProgress = groundData["stageProgress"].get<float>();
					if (req.stageProgress > maxStageProgress) {
						maxStageProgress = req.stageProgress;
					}
                } else {
					assert(false && "spawn_reqestsの各要素にはstageProgressが必要です。");
					throw std::runtime_error("spawn_reqestsの各要素にはstageProgressが必要です。");
                }
                if (groundData.contains("radius") && groundData["radius"].is_number()) {
                    req.radius = groundData["radius"].get<float>();
                } else {
					assert(false && "spawn_reqestsの各要素にはradiusが必要です。");
					throw std::runtime_error("spawn_reqestsの各要素にはradiusが必要です。");
                }
                if (groundData.contains("angle") && groundData["angle"].is_number()) {
					req.angle = groundData["angle"].get<float>();
                } else {
					assert(false && "spawn_reqestsの各要素にはangleが必要です。");
					throw std::runtime_error("spawn_reqestsの各要素にはangleが必要です。");
                }
                if (groundData.contains("panelWidth") && groundData["panelWidth"].is_number()) {
                    req.panelWidth = groundData["panelWidth"].get<float>();
                } else {
					assert(false && "spawn_reqestsの各要素にはpanelWidthが必要です。");
					throw std::runtime_error("spawn_reqestsの各要素にはpanelWidthが必要です。");
                }
                if (groundData.contains("panelThickness") && groundData["panelThickness"].is_number()) {
                    req.panelThickness = groundData["panelThickness"].get<float>();
				} else {
					assert(false && "spawn_reqestsの各要素にはpanelThicknessが必要です。");
					throw std::runtime_error("spawn_reqestsの各要素にはpanelThicknessが必要です。");
				}
                if (groundData.contains("panelLength") && groundData["panelLength"].is_number()) {
                    req.panelLength = groundData["panelLength"].get<float>();
                } else {
					assert(false && "spawn_reqestsの各要素にはpanelLengthが必要です。");
					throw std::runtime_error("spawn_reqestsの各要素にはpanelLengthが必要です。");
                }
                spawnRequests_.push_back(req);
            }
		}

        // ステージの進み具合が1.0fを超えている場合、壊れたデータとして修正する
        if(maxStageProgress > 1.0f) {
            // すべてのリクエストのstageProgressをmaxStageProgressで割って正規化する
            for (auto &req : spawnRequests_) {
				// stageProgressをmaxStageProgressで割って正規化する
                req.stageProgress /= maxStageProgress;

				// radiusもmaxStageProgressで割って正規化する
				req.radius /= maxStageProgress;

				// パネルもmaxStageProgressで割って正規化する
                req.panelLength /= maxStageProgress;
				req.panelThickness /= maxStageProgress;
				req.panelWidth /= maxStageProgress;
			}

            // 修正版を出力する
			nlohmann::json outputJson;
            outputJson["stageLength"] = editStageLengthData;
            outputJson["spawn_reqests"] = nlohmann::json::array();
            for (const auto &req : spawnRequests_) {
                nlohmann::json reqJson;
                reqJson["stageProgress"] = req.stageProgress;
                reqJson["radius"] = req.radius;
                reqJson["angle"] = req.angle;
                reqJson["panelWidth"] = req.panelWidth;
                reqJson["panelThickness"] = req.panelThickness;
                reqJson["panelLength"] = req.panelLength;
                outputJson["spawn_reqests"].push_back(reqJson);
            }
            std::ofstream ofs("Assets/Application/StageData/corrected_stage_data.json");
            if (ofs.is_open()) {
                ofs << outputJson.dump(4); // インデント幅4で整形して出力
            } else {
                assert(false && "Failed to open file for writing corrected stage data.");
			}
        }

		// スポーン要求の順番をstageProgressの昇順にソートする
        std::sort(spawnRequests_.begin(), spawnRequests_.end(), [](const SpawnRequest &a, const SpawnRequest &b) {
            return a.stageProgress < b.stageProgress;
			});

        // 生成要求
        TryGenerate();
    }

    void Update() override {
		// 生成されていない、または地面が存在しない場合は何もしない
        if (!generated_ || grounds_.empty()) return;

        // シーンコンテキストを取得
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

		// プレイヤーオブジェクトを取得
        if (!player_) {
            player_ = ctx->GetObject3D("PlayerRoot");
        }
        if (!player_) return;

        // 再スポーン処理を削除し、タッチ判定だけを残す
        for (auto &g : grounds_) {
            if (!g.object) continue;
			// プレイヤーが地面に触れたかどうかを判定し、触れていればカウントを増やす
            if (auto *ground = g.object->GetComponent3D<GroundDefined>()) {
                if (ground->ConsumePlayerTouchEvent()) {
                    ++touchedGroundCount_;
                }
            }

			// プレイヤーより後ろの地面オブジェクトを非アクティブにする
            auto *tr = g.object->GetComponent3D<Transform3D>();
            if (!tr) continue;
            if ((tr->GetTranslate().z - g.length) > player_->GetComponent3D<Transform3D>()->GetTranslate().z) {
                g.isActive = false;
			}
        }

		// スポーン要求のインデックスがまだリクエストの数より少ない場合、次のスポーン要求を処理して地面を生成する
        if (spawnRequests_.size() > currentSpawnRequestIndex_) {
            SpawnGroundFromStageData();
		}  
    }

    void RequestGenerate() {
        requested_ = true;
        TryGenerate();
    }

    void TriggerGroundReaction(const Vector3 &center, float radius) {
        const float radiusSq = std::max(0.0f, radius) * std::max(0.0f, radius);
        for (auto &g : grounds_) {
            if (!g.object) continue;
            auto *tr = g.object->GetComponent3D<Transform3D>();
            auto *ground = g.object->GetComponent3D<GroundDefined>();
            if (!tr || !ground) continue;
            const Vector3 d = tr->GetTranslate() - center;
            if (d.LengthSquared() <= radiusSq) {
                ground->TriggerTouchColorAnimation();
            }
        }
    }

    int GetTouchedGroundCount() const { return touchedGroundCount_; }

    /// @brief スポーン時の地面をプレイヤーの下に再配置する
    void RespawnStartGroundUnderPlayer() {
        if (!player_) return;
        if (!spawnGround_) return;

        auto *tr = spawnGround_->GetComponent3D<Transform3D>();
        if (!tr) return;

        auto *playerTr = player_->GetComponent3D<Transform3D>();
        if (!playerTr) return;

        tr->SetTranslate(Vector3{ playerTr->GetTranslate().x, playerTr->GetTranslate().y - panelThickness_, playerTr->GetTranslate().z });
    }

private:
    static constexpr float kTwoPi = 3.14159265358979323846f * 2.0f;
    static constexpr std::uint64_t kGroundBatchKey = 0x1101000000000001ull;

    struct GroundRuntime {
        Object3DBase *object = nullptr;
        float centerZ = 0.0f;
        float length = 0.0f;
		bool isActive = false;
    };

    void TryGenerate() {
		// すでに生成されているか、生成要求がない場合は何もしない
        if (generated_ || !requested_) return;

		// 必要なコンポーネントや変数が揃っているか確認し、足りない場合は生成を開始しない
        auto *ctx = GetOwnerContext();
        if (!ctx || !defaultVars_ || !collider_) return;

		// スポーン用の地面を生成
        CreateSpawnGround(ctx);

		// 全地面オブジェクトを一括生成
        CreateAllGrounds(ctx);

        generated_ = true;
    }

	/// @brief JSONから読み込んだらプールの限界まで地面オブジェクトを生成する。
    void CreateAllGrounds(SceneContext *ctx) {
		// 地面オブジェクトのプールを生成
		CreateGroundPool(ctx);

        // 今のスポーンリクエストインデックスをリセット
		currentSpawnRequestIndex_ = 0;
		// プールの容量すべて使って生成できるところまで生成する
        for(auto &g : grounds_) {
            if (!g.object) continue;
            if (!g.isActive) {
                g.isActive = true;

				// 最初から最後まで生成できるスポーンリクエストがあれば生成する
				SpawnGroundFromStageData();
            }
		}
    }

	/// @brief プレイヤーのスポーン位置に地面を生成する。これは通常の地面と同じ見た目・当たり判定だが、スポーン位置に固定され、ステージの進行に応じて再利用されない。
    void CreateSpawnGround(SceneContext *ctx) {
        if (!ctx || !defaultVars_ || !collider_) return;
		// プレイヤーが最初に立つための地面を生成
        auto obj = std::make_unique<Box>();
        obj->SetName("Ground");
        obj->SetBatchKey(kGroundBatchKey, RenderType::Instancing);
		// スクリーンバッファがあれば描画に登録
        if (defaultVars_->GetScreenBuffer3D()) {
            obj->AttachToRenderer(defaultVars_->GetScreenBuffer3D(), "Object3D.Solid.BlendNormal");
        }
		// トランスフォームを設定
        auto *tr = obj->GetComponent3D<Transform3D>();
        if (!tr) return;
		// スポーン位置に配置
        tr->SetTranslate(Vector3{spawnGroundCenterX_, spawnGroundCenterY_, spawnGroundCenterZ_});
        tr->SetRotate(Vector3{0.0f, 0.0f, 0.0f});
        tr->SetScale(Vector3{spawnGroundWidth_, panelThickness_, spawnGroundDepth_});
		// このオブジェクトを地面として定義するコンポーネントを登録
        obj->RegisterComponent<GroundDefined>(collider_);
        spawnGround_ = obj.get();
        (void)ctx->AddObject3D(std::move(obj));
    }

    /// @brief 地面のプールを生成する
    void CreateGroundPool(SceneContext *ctx) {
        grounds_.clear();
        for(int i = 0; i < poolSize_; ++i) {
			// Boxオブジェクトを生成し、名前とバッチキーを設定
            auto obj = std::make_unique<Box>();
            obj->SetName("GroundPool");
            obj->SetBatchKey(kGroundBatchKey, RenderType::Instancing);
            if (defaultVars_ && defaultVars_->GetScreenBuffer3D()) {
                obj->AttachToRenderer(defaultVars_->GetScreenBuffer3D(), "Object3D.Solid.BlendNormal");
            }
			// 地面としての定義
            obj->RegisterComponent<GroundDefined>(collider_);
            Object3DBase *objPtr = obj.get();
            if (ctx->AddObject3D(std::move(obj)) && objPtr) {
                GroundRuntime runtime{};
                runtime.object = objPtr;
                grounds_.push_back(runtime);
            }
		}
	}

	/// @brief 地面をスポーン位置に生成する。プールの空きがない場合は何もしない
    void SpawnGround(const Vector3& pos, const Vector3& rot, const Vector3& scale) {
        for(auto &g : grounds_) {
            if (!g.object) continue;
            if (g.isActive) continue;
            if (auto *tr = g.object->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(pos);
                tr->SetRotate(rot);
                tr->SetScale(scale);
                g.isActive = true;

				g.length = scale.z; // 長さを保存しておく（Z方向のスケールを長さとみなす）
                break;
            }
		}
	}

	/// @brief 地面プールの空きがあるかどうか
    bool HasGroundPoolSpace() const {
        for (const auto &g : grounds_) {
            if (!g.object) continue;
            if (!g.isActive) {
                return true;
            }
        }
        return false;
	}

    void SpawnGroundFromStageData() {
        if (!HasGroundPoolSpace()) return;

        // 現在のスポーンリクエストを取得
        if (currentSpawnRequestIndex_ >= spawnRequests_.size()) return;
        const auto& req = spawnRequests_[currentSpawnRequestIndex_];

        // 0.0(スタート)と1.0(ゴール)のZ座標を取得
        const float startZ = spawnGroundCenterZ_;
        float goalZ = -8192.0f;
        if (auto* gpc = GetOwnerContext()->GetComponent<StageGoalPlaneController>()) {
            goalZ = gpc->GetGoalZ();
        }
        const float endZ = goalZ;

		// JSONデータ上のstageProgressを元に、ステージのZ方向の位置を計算
        float stageLengthRate = 1.0f;
        if (editStageLengthData != 0.0f) {
            stageLengthRate = std::fabsf(endZ - startZ) / editStageLengthData;
        } else {
            assert(false);
        }
        const float centerZ = startZ + (endZ - startZ) * req.stageProgress;
        // 角度と半径からXY座標を計算
        const float x = -std::sin(req.angle) * req.radius * stageLengthRate;
        const float y = -std::cos(req.angle) * req.radius * stageLengthRate;

        // 地面オブジェクトを配置
        SpawnGround(
            Vector3{ x, y, centerZ },
            Vector3{ 0.0f, 0.0f, -req.angle },
            Vector3{ req.panelWidth * stageLengthRate, req.panelThickness * stageLengthRate, req.panelLength * stageLengthRate });
		++currentSpawnRequestIndex_;
    }

    bool requested_ = false;
    bool generated_ = false;

    float editStageLengthData = 100.0f;
    float panelThickness_ = 2.0f;
    float spawnGroundCenterX_ = 0.0f;
    float spawnGroundCenterY_ = -panelThickness_;
    float spawnGroundCenterZ_ = -2.0f;
    float spawnGroundWidth_ = 16.0f;
    float spawnGroundDepth_ = 256.0f;

    int panelWidthSplitCount_ = 3;

    float minPanelLength_ = 64.0f;
    int panelLengthSplitCount_ = 3;

    int minPanelsPerSegment_ = 6;

	std::vector<SpawnRequest> spawnRequests_; // JSONから読み込んだすべてのスポーンリクエスト
	int currentSpawnRequestIndex_ = 0; // 次にスポーンすべきリクエストのインデックス
	const std::string stageDataFilePath_ = "Assets/Application/StageData/stage.json";

    float nextSpawnZ_ = 0.0f;
    float currentSegmentCenterZ_ = 0.0f;
    float currentSegmentLength_ = 0.0f;
    int remainingPanelsInCurrentSegment_ = 0;

    SceneDefaultVariables *defaultVars_ = nullptr;
    Collider *collider_ = nullptr;
    Object3DBase *player_ = nullptr;
    Object3DBase *spawnGround_ = nullptr;
    std::vector<GroundRuntime> grounds_{};
	const int poolSize_ = 200;
    int touchedGroundCount_ = 0;
    bool hasMinSpawnZ_ = false;
};

} // namespace KashipanEngine
