#pragma once
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

/// @brief クラッシュ発生直前のシーン状態をJSONファイルへ保存する
/// @details 保存に失敗しても（シーンが存在しない、書き込みに失敗する等）例外は投げない。
void ExportCrashSceneBackup(PasskeyForCrashHandler);

} // namespace KashipanEngine
