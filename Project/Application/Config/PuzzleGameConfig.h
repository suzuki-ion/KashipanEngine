#pragma once
#include <KashipanEngine.h>
#include <string>

namespace Application {

struct PuzzleGameConfig {
	// ステージの大きさ（n x n）
	int stageSize = 6;
	// パネルのスケール（ピクセル）
	float panelScale = 80.0f;
	// パネルとパネルの間の隙間（ピクセル）
	float panelGap = 8.0f;
	// パズルパネルの種類数
	int panelTypeCount = 4;
	// パズルパネルの種類ごとの色 (RGBA)
	static constexpr int kMaxPanelTypes = 8;
	Vector4 panelColors[kMaxPanelTypes] = {
		Vector4(0.8f, 0.2f, 0.2f, 1.0f), // タイプ1: 赤
		Vector4(0.2f, 0.6f, 0.8f, 1.0f), // タイプ2: 青
		Vector4(0.2f, 0.8f, 0.3f, 1.0f), // タイプ3: 緑
		Vector4(0.8f, 0.8f, 0.2f, 1.0f), // タイプ4: 黄
		Vector4(0.8f, 0.4f, 0.8f, 1.0f), // タイプ5: 紫
		Vector4(0.9f, 0.6f, 0.2f, 1.0f), // タイプ6: 橙
		Vector4(0.5f, 0.5f, 0.5f, 1.0f), // タイプ7: 灰
		Vector4(0.9f, 0.9f, 0.9f, 1.0f), // タイプ8: 白
	};

	// パズルパネルが移動する際のイージング時間（秒）
	float panelMoveEasingDuration = 0.15f;
	// パズルパネルが消える際のイージング時間（秒）
	float panelClearEasingDuration = 0.3f;
	// パズルパネルが新しく出現する際のイージング時間（秒）
	float panelSpawnEasingDuration = 0.15f;
	// カーソルが動く際のイージング時間（秒）
	float cursorEasingDuration = 0.1f;

	// ノーマル消しの最低個数 (n)
	int normalMinCount = 3;
	// ストレート消しの最低個数 (m, m > n)
	int straightMinCount = 5;

	// ロックスプライトの色
	Vector4 lockColor = Vector4(0.0f, 0.0f, 0.0f, 0.5f);
	// カーソルの色
	Vector4 cursorColor = Vector4(1.0f, 1.0f, 0.0f, 0.6f);
	// ステージ背景の色
	Vector4 stageBackgroundColor = Vector4(0.2f, 0.2f, 0.2f, 1.0f);

	// プレイヤーのHP
	int playerHP = 100;
	// 制限時間（秒）
	float timeLimit = 10.0f;

	// ダメージ量
	int normalDamage = 1;
	int straightDamage = 2;
	int crossDamage = 3;
	int squareDamage = 5;

	// ロック時間（秒）
	float normalLockTime = 1.0f;
	float straightLockTime = 3.0f;
	float crossLockTime = 5.0f;
	float squareLockTime = 15.0f;

	// コンボ時のダメージ倍数
	float comboDamageMultiplier = 2.0f;
	// 『ブレイク』時のダメージ倍数
	float breakDamageMultiplier = 2.0f;
	// コンボ時のロック時間倍数
	float comboLockMultiplier = 2.0f;
	// 『ブレイク』時のロック時間倍数
	float breakLockMultiplier = 2.0f;

	// 制限時間の余った秒数ぶんのダメージ量加算
	float remainingTimeDamageBonus = 0.5f;
	// 制限時間の余った秒数ぶんのロック時間加算
	float remainingTimeLockBonus = 0.5f;

	void LoadFromJSON(const std::string& filepath);
	void SaveToJSON(const std::string& filepath) const;
};

} // namespace Application
