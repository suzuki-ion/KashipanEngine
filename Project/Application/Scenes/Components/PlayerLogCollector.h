#pragma once
#include <KashipanEngine.h>
#include <fstream>
#include <vector>
#include <cmath>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "Scenes/Components/StageGoalPlaneController.h"

namespace KashipanEngine {
	/// @brief プレイヤーの記録データを保持する構造体
	struct PlayerLogData {
		float stageProgress;
		float radius;
		float angle;
	};

	/// @brief プレイヤーの情報を収集してファイルに保存するコンポーネント
	class PlayerLogCollector final : public ISceneComponent {
	public:
		PlayerLogCollector(Object3DBase* player)
			: ISceneComponent("PlayerLogCollector", 1), player_(player) {}
		~PlayerLogCollector() override {
			try
			{
				// 何も記録されていなければ終了
				if (recordedData_.empty()) return;

				// 出力時のみJSONを構築する（メモリ効率の改善）
				nlohmann::json logs;
				logs["fps"] = 60.0f; // 固定値
				logs["frameInterval"] = logIntervalFrames_;
				
				// 記録したデータからJSON配列を作成
				logs["logs"] = nlohmann::json::array();
				for (const auto& data : recordedData_) {
					nlohmann::json logJson;
					logJson["stageProgress"] = data.stageProgress;
					logJson["radius"] = data.radius;
					logJson["angle"] = data.angle;
					logs["logs"].push_back(logJson);
				}

				// 現在時刻を取得してファイル名に組み込む
				auto now = std::chrono::system_clock::now();
				std::time_t t = std::chrono::system_clock::to_time_t(now);
				struct tm tm_buf;
#if defined(_WIN32)
				localtime_s(&tm_buf, &t);
#else
				localtime_r(&t, &tm_buf);
#endif
				std::ostringstream oss;
				oss << "PlayerLogs_" << std::put_time(&tm_buf, "%Y%m%d_%H%M%S") << ".json";
				const std::string logFilePath = oss.str();

				std::ofstream ofs(logFilePath);
				if (!ofs) {
					Log("Failed to open log file for writing: " + logFilePath, LogSeverity::Error);
					return;
				}
				
				ofs << logs.dump(4); // インデント4スペースで整形して出力
				Log("Player logs saved to: " + logFilePath);
			}
			catch (const std::exception&){}
		}

		void Initialize() override {
			recordedData_.clear();
			frameCount_ = 0;

			startZ_ = -2.0f; // StageGroundGeneratorのspawnGroundCenterZ_と同値
			goalZ_ = -8192.0f;

			auto* ctx = GetOwnerContext();
			if (!ctx) return;

			// StageGroundGeneratorと同様の方法でゴールZを取得
			if (auto* gpc = ctx->GetComponent<StageGoalPlaneController>()) {
				goalZ_ = gpc->GetGoalZ();
			}

			float editStageLengthData = 100.0f;
			std::string stageDataFilePath = ctx->GetSceneVariableOr<std::string>("TargetStageFilePath", "Assets/Application/StageData/stage.json");
			std::ifstream ifs(stageDataFilePath);
			if (ifs.is_open()) {
				try {
					nlohmann::json j;
					ifs >> j;
					if (j.contains("stageLength")) {
						editStageLengthData = j["stageLength"].get<float>();
					}
				}
				catch (...) {}
			}

			stageLengthRate_ = 1.0f;
			if (editStageLengthData != 0.0f) {
				stageLengthRate_ = std::fabsf(goalZ_ - startZ_) / editStageLengthData;
			}
		}
		
		void Update() override {
			if (!player_) return;

			frameCount_++;
			if (frameCount_ < logIntervalFrames_) return;
			frameCount_ = 0;

			auto *tr = player_->GetComponent3D<Transform3D>();
			if (!tr) return;

			// プレイヤーの座標を取得
			Vector3 pos = tr->GetTranslate();
			PlayerLogData data{};
			
			// 進捗度 (実際のZ座標から、0.0〜1.0など元のstageProgressスケールに逆算)
			if (goalZ_ != startZ_) {
				data.stageProgress = (pos.z - startZ_) / (goalZ_ - startZ_);
			} else {
				data.stageProgress = 0.0f;
			}

			// 半径を計算 (x^2 + y^2 の平方根) し、stageLengthRate で割って元のスケールに戻す
			float worldRadius = std::sqrt(pos.x * pos.x + pos.y * pos.y);
			if (stageLengthRate_ > 0.0f) {
				data.radius = worldRadius / stageLengthRate_;
			} else {
				data.radius = worldRadius;
			}

			// 生成時の式(-sin(angle)*radius, -cos(angle)*radius)の逆算
			data.angle = std::atan2(-pos.x, -pos.y);

			recordedData_.push_back(data);
		}

	private:
		Object3DBase* player_ = nullptr;
		
		// 記録用データを保持するコンテナ
		std::vector<PlayerLogData> recordedData_;

		int frameCount_ = 0;
		int logIntervalFrames_ = 30;

		float startZ_ = -2.0f;
		float goalZ_ = -8192.0f;
		float stageLengthRate_ = 1.0f;
	};
} // namespace KashipanEngine