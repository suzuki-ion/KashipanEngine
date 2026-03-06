#include "Config/PuzzleGameConfig.h"

namespace Application {

void PuzzleGameConfig::LoadFromJSON(const std::string& filepath) {
	if (!KashipanEngine::IsFileExist(filepath)) return;
	auto json = KashipanEngine::LoadJSON(filepath);
	if (json.is_null()) return;

	stageSize = KashipanEngine::GetJSONValueOrDefault(json, "stageSize", stageSize);
	panelScale = KashipanEngine::GetJSONValueOrDefault(json, "panelScale", panelScale);
	panelGap = KashipanEngine::GetJSONValueOrDefault(json, "panelGap", panelGap);
	panelTypeCount = KashipanEngine::GetJSONValueOrDefault(json, "panelTypeCount", panelTypeCount);
	panelMoveEasingDuration = KashipanEngine::GetJSONValueOrDefault(json, "panelMoveEasingDuration", panelMoveEasingDuration);
	panelClearEasingDuration = KashipanEngine::GetJSONValueOrDefault(json, "panelClearEasingDuration", panelClearEasingDuration);
	panelSpawnEasingDuration = KashipanEngine::GetJSONValueOrDefault(json, "panelSpawnEasingDuration", panelSpawnEasingDuration);
	cursorEasingDuration = KashipanEngine::GetJSONValueOrDefault(json, "cursorEasingDuration", cursorEasingDuration);
	normalMinCount = KashipanEngine::GetJSONValueOrDefault(json, "normalMinCount", normalMinCount);
	straightMinCount = KashipanEngine::GetJSONValueOrDefault(json, "straightMinCount", straightMinCount);
	normalLockTime = KashipanEngine::GetJSONValueOrDefault(json, "normalLockTime", normalLockTime);
	straightLockTime = KashipanEngine::GetJSONValueOrDefault(json, "straightLockTime", straightLockTime);
	crossLockTime = KashipanEngine::GetJSONValueOrDefault(json, "crossLockTime", crossLockTime);
	squareLockTime = KashipanEngine::GetJSONValueOrDefault(json, "squareLockTime", squareLockTime);
	comboLockMultiplier = KashipanEngine::GetJSONValueOrDefault(json, "comboLockMultiplier", comboLockMultiplier);
	breakLockMultiplier = KashipanEngine::GetJSONValueOrDefault(json, "breakLockMultiplier", breakLockMultiplier);
	movesPerGarbage = KashipanEngine::GetJSONValueOrDefault(json, "movesPerGarbage", movesPerGarbage);
	attackGarbageMultiplier = KashipanEngine::GetJSONValueOrDefault(json, "attackGarbageMultiplier", attackGarbageMultiplier);
	inactiveGarbageDecayInterval = KashipanEngine::GetJSONValueOrDefault(json, "inactiveGarbageDecayInterval", inactiveGarbageDecayInterval);
	normalGarbageCount = KashipanEngine::GetJSONValueOrDefault(json, "normalGarbageCount", normalGarbageCount);
	straightGarbageCount = KashipanEngine::GetJSONValueOrDefault(json, "straightGarbageCount", straightGarbageCount);
	crossGarbageCount = KashipanEngine::GetJSONValueOrDefault(json, "crossGarbageCount", crossGarbageCount);
	squareGarbageCount = KashipanEngine::GetJSONValueOrDefault(json, "squareGarbageCount", squareGarbageCount);
	comboGarbageMultiplier = KashipanEngine::GetJSONValueOrDefault(json, "comboGarbageMultiplier", comboGarbageMultiplier);
	defeatCollapseRatio = KashipanEngine::GetJSONValueOrDefault(json, "defeatCollapseRatio", defeatCollapseRatio);
	garbageClearedBonus = KashipanEngine::GetJSONValueOrDefault(json, "garbageClearedBonus", garbageClearedBonus);
	garbageDelayTimeMultiplier = KashipanEngine::GetJSONValueOrDefault(json, "garbageDelayTimeMultiplier", garbageDelayTimeMultiplier);
	garbageEscalationInterval = KashipanEngine::GetJSONValueOrDefault(json, "garbageEscalationInterval", garbageEscalationInterval);
	garbageEscalationIncrement = KashipanEngine::GetJSONValueOrDefault(json, "garbageEscalationIncrement", garbageEscalationIncrement);

	auto loadColor = [&](const char* key, Vector4& color) {
		if (json.contains(key) && json[key].is_array() && json[key].size() >= 4) {
			color.x = json[key][0].get<float>();
			color.y = json[key][1].get<float>();
			color.z = json[key][2].get<float>();
			color.w = json[key][3].get<float>();
		}
	};

	loadColor("lockColor", lockColor);
	loadColor("cursorColor", cursorColor);
	loadColor("stageBackgroundColor", stageBackgroundColor);
	loadColor("garbageColor", garbageColor);
	loadColor("garbageWarningColor", garbageWarningColor);

	if (json.contains("panelColors") && json["panelColors"].is_array()) {
		int count = static_cast<int>(json["panelColors"].size());
		if (count > kMaxPanelTypes) count = kMaxPanelTypes;
		for (int i = 0; i < count; i++) {
			auto& c = json["panelColors"][i];
			if (c.is_array() && c.size() >= 4) {
				panelColors[i].x = c[0].get<float>();
				panelColors[i].y = c[1].get<float>();
				panelColors[i].z = c[2].get<float>();
				panelColors[i].w = c[3].get<float>();
			}
		}
	}
}

void PuzzleGameConfig::SaveToJSON(const std::string& filepath) const {
	KashipanEngine::JSON json;
	json["stageSize"] = stageSize;
	json["panelScale"] = panelScale;
	json["panelGap"] = panelGap;
	json["panelTypeCount"] = panelTypeCount;
	json["panelMoveEasingDuration"] = panelMoveEasingDuration;
	json["panelClearEasingDuration"] = panelClearEasingDuration;
	json["panelSpawnEasingDuration"] = panelSpawnEasingDuration;
	json["cursorEasingDuration"] = cursorEasingDuration;
	json["normalMinCount"] = normalMinCount;
	json["straightMinCount"] = straightMinCount;
	json["normalLockTime"] = normalLockTime;
	json["straightLockTime"] = straightLockTime;
	json["crossLockTime"] = crossLockTime;
	json["squareLockTime"] = squareLockTime;
	json["comboLockMultiplier"] = comboLockMultiplier;
	json["breakLockMultiplier"] = breakLockMultiplier;
	json["movesPerGarbage"] = movesPerGarbage;
	json["attackGarbageMultiplier"] = attackGarbageMultiplier;
	json["inactiveGarbageDecayInterval"] = inactiveGarbageDecayInterval;
	json["normalGarbageCount"] = normalGarbageCount;
	json["straightGarbageCount"] = straightGarbageCount;
	json["crossGarbageCount"] = crossGarbageCount;
	json["squareGarbageCount"] = squareGarbageCount;
	json["comboGarbageMultiplier"] = comboGarbageMultiplier;
	json["defeatCollapseRatio"] = defeatCollapseRatio;
	json["garbageClearedBonus"] = garbageClearedBonus;
	json["garbageDelayTimeMultiplier"] = garbageDelayTimeMultiplier;
	json["garbageEscalationInterval"] = garbageEscalationInterval;
	json["garbageEscalationIncrement"] = garbageEscalationIncrement;

	auto saveColor = [](const Vector4& c) {
		return KashipanEngine::JSON::array({ c.x, c.y, c.z, c.w });
	};

	json["lockColor"] = saveColor(lockColor);
	json["cursorColor"] = saveColor(cursorColor);
	json["stageBackgroundColor"] = saveColor(stageBackgroundColor);
	json["garbageColor"] = saveColor(garbageColor);
	json["garbageWarningColor"] = saveColor(garbageWarningColor);

	KashipanEngine::JSON colorsArr = KashipanEngine::JSON::array();
	for (int i = 0; i < kMaxPanelTypes; i++) {
		colorsArr.push_back({ panelColors[i].x, panelColors[i].y, panelColors[i].z, panelColors[i].w });
	}
	json["panelColors"] = colorsArr;

	KashipanEngine::SaveJSON(json, filepath);
}

} // namespace Application
