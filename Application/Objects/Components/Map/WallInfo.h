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
    };

} // namespace KashipanEngine