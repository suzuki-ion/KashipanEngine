#pragma once
#include <KashipanEngine.h>
#include <string>

namespace Application {

struct PuzzleGameConfig {
	// ステージの大きさ（必ず奇数）
	int stageSize = 5;
	// パズルパネルの種類数
	int panelTypeCount = 3;
	// パズルパネルの各種類の色 (RGBA)
	Vector4 panelColors[8] = {
		Vector4(0.8f, 0.2f, 0.2f, 1.0f), // タイプ1: 赤
		Vector4(0.2f, 0.6f, 0.8f, 1.0f), // タイプ2: 青
		Vector4(0.2f, 0.8f, 0.3f, 1.0f), // タイプ3: 緑
		Vector4(0.8f, 0.8f, 0.2f, 1.0f), // タイプ4: 黄
		Vector4(0.8f, 0.4f, 0.8f, 1.0f), // タイプ5: 紫
		Vector4(0.9f, 0.6f, 0.2f, 1.0f), // タイプ6: 橙
		Vector4(0.5f, 0.5f, 0.5f, 1.0f), // タイプ7: 灰
		Vector4(0.9f, 0.9f, 0.9f, 1.0f), // タイプ8: 白
	};
	// パズルパネルが動く際のイージング時間（秒）
	float panelEasingDuration = 0.15f;
	// カーソルが動く際のイージング時間（秒）
	float cursorEasingDuration = 0.1f;
	// カメラの高さ
	float cameraHeight = 20.0f;
	// カメラのZ軸方向の距離
	float cameraZDistance = -12.0f;
	// カメラのfov（度数）
	float cameraFov = 45.0f;
	// 目標パネルの配置位置（ステージ左隣）
	Vector3 goalPanelOffset = Vector3(-10.0f, 0.01f, 0.0f);
	// 地面パネルの色
	Vector4 groundColor = Vector4(0.3f, 0.3f, 0.3f, 1.0f);
	// 空きマスの地面色
	Vector4 emptyGroundColor = Vector4(0.15f, 0.15f, 0.15f, 1.0f);
	// カーソルの色
	Vector4 cursorColor = Vector4(1.0f, 1.0f, 0.0f, 0.6f);
	// 目標パネル生成時の移動回数
	int goalMoveCount = 5;

	void LoadFromJSON(const std::string& filepath);
	void SaveToJSON(const std::string& filepath) const;
};

} // namespace Application
