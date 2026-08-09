#pragma once
#include "Utilities/FileIO/JSON.h"

#include <string>

namespace KashipanEngine {

/// @brief シーンJSONをパスへ保存する
/// @details パスが ".json" で終わる場合は単一ファイル形式（既存互換）、
///          ".scene" で終わる場合はフォルダ形式（1オブジェクト1フォルダ）で保存する。
///          どちらの形式でも `Scene::SaveToJSON()` の出力をそのまま渡せる。
bool SaveSceneToPath(const JSON &sceneJson, const std::string &path);

/// @brief パスからシーンJSONを読み込む
/// @details パスの末尾（".json"/".scene"）で形式を自動判別する。
///          戻り値は `Scene::LoadFromJSON()` にそのまま渡せる形状。読み込みに失敗した場合は空のJSONを返す
JSON LoadSceneFromPath(const std::string &path);

} // namespace KashipanEngine
