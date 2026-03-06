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

	// お邪魔パネルの色
	Vector4 garbageColor = Vector4(0.3f, 0.3f, 0.3f, 1.0f);
	// お邪魔パネル予告の色
	Vector4 garbageWarningColor = Vector4(0.5f, 0.5f, 0.5f, 0.5f);

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

	// ロック時間（秒）
	float normalLockTime = 1.0f;
	float straightLockTime = 3.0f;
	float crossLockTime = 5.0f;
	float squareLockTime = 15.0f;

	// コンボ時のロック時間倍数
	float comboLockMultiplier = 2.0f;
	// 『ブレイク』時のロック時間倍数
	float breakLockMultiplier = 2.0f;

	// お邪魔パネルが出現するまでの移動回数
	int movesPerGarbage = 5;
	// 攻撃時に消したパネル数→お邪魔パネル個数の倍率
	float attackGarbageMultiplier = 0.5f;
	// 未使用ステージのお邪魔パネルが1個消えるまでの秒数
	float inactiveGarbageDecayInterval = 1.0f;

	// パズルパネルの形状ごとのお邪魔パネル出現量
	float normalGarbageCount = 1.0f;
	float straightGarbageCount = 3.0f;
	float crossGarbageCount = 5.0f;
	float squareGarbageCount = 9.0f;

	// コンボ時のお邪魔パネル出現量倍数（1コンボごとにm倍）
	float comboGarbageMultiplier = 1.5f;

	// 敗北とみなす崩壊度のパーセンテージ (0.0~1.0)
	float defeatCollapseRatio = 0.7f;

	// お邪魔パネル1個あたりの出現量（お邪魔パネルが消えたとき攻撃量に加算される値）
	float garbageClearedBonus = 1.0f;

	// お邪魔パネル出現遅延時間の倍率（出現量 × この値 = 出現までの時間(秒)）
	float garbageDelayTimeMultiplier = 0.5f;

	// 時間経過によるお邪魔パネル出現量の倍率増加間隔（秒）
	float garbageEscalationInterval = 60.0f;
	// 時間経過によるお邪魔パネル出現量の倍率増加値
	float garbageEscalationIncrement = 0.1f;

	void LoadFromJSON(const std::string& filepath);
	void SaveToJSON(const std::string& filepath) const;
};

} // namespace Application
