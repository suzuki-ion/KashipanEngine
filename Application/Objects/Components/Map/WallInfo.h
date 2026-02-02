#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

    /// @brief 壁情報を保持する構造体
    struct WallInfo {
        Object3DBase* object = nullptr;
        bool isActive = false;

		GameTimer moveTimer{};
        float moveTime = 1.0f;
		bool isMoving = false;

		int hp = 1;
        int spawnAgainCount = 8; // 壁が壊れてから再度設置できるようになるまでの拍数
        int currentSpawnAgainCount = 0; // 壊れてからの経過拍数カウンター
        bool isWaitingRespawn = false; // 再生成待機中フラグ
    };

} // namespace KashipanEngine