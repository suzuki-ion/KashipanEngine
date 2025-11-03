#pragma once
// 既存のMath構造体
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Matrix3x3.h"
#include "Math/Matrix4x4.h"

// ユーティリティ
#include "Utilities/MathUtils.h"

// コアシステム
#include "Core/GameEngine.h"

namespace KashipanEngine {
int Execute(PasskeyForWinMain, const std::string &engineSettingsPath);
} // namespace KashipanEngine