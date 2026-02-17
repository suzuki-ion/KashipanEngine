#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

    /// @brief 壁情報を保持する構造体
    struct WallInfo {
        Object3DBase* object = nullptr;
        bool isActive = false;

		GameTimer moveTimer{};
        float moveTime = 0.1f;
		bool isMoving = false;

		int hp = 1;
        int spawnAgainCount = 8; // 壁が壊れてから再度設置できるようになるまでの拍数
        int currentSpawnAgainCount = 0; // 壊れてからの経過拍数カウンター
        bool isWaitingRespawn = false; // 再生成待機中フラグ

        // パーティクル生成（壁が設置された）時の拍数（自動非アクティブ化用）
        int particleSpawnBeat = -1; // -1 = 未設定
        int autoDeactivateBeatCount = 16; // パーティクル生成から自動非アクティブ化までの拍数
    };

} // namespace KashipanEngine