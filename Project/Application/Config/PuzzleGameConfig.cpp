#include "Config/PuzzleGameConfig.h"

namespace Application {

void PuzzleGameConfig::LoadFromJSON(const std::string& filepath) {
	if (!KashipanEngine::IsFileExist(filepath)) return;
	auto json = KashipanEngine::LoadJSON(filepath);
	if (json.is_null()) return;

	stageSize = KashipanEngine::GetJSONValueOrDefault(json, "stageSize", stageSize);
	minMatchCount = KashipanEngine::GetJSONValueOrDefault(json, "minMatchCount", minMatchCount);
	panelEasingDuration = KashipanEngine::GetJSONValueOrDefault(json, "panelEasingDuration", panelEasingDuration);
	cursorEasingDuration = KashipanEngine::GetJSONValueOrDefault(json, "cursorEasingDuration", cursorEasingDuration);
	cameraHeight = KashipanEngine::GetJSONValueOrDefault(json, "cameraHeight", cameraHeight);
	cameraZDistance = KashipanEngine::GetJSONValueOrDefault(json, "cameraZDistance", cameraZDistance);
	cameraFov = KashipanEngine::GetJSONValueOrDefault(json, "cameraFov", cameraFov);
	panelClearRiseHeight = KashipanEngine::GetJSONValueOrDefault(json, "panelClearRiseHeight", panelClearRiseHeight);
	panelClearDuration = KashipanEngine::GetJSONValueOrDefault(json, "panelClearDuration", panelClearDuration);

	if (json.contains("groundColor") && json["groundColor"].is_array() && json["groundColor"].size() >= 4) {
		groundColor.x = json["groundColor"][0].get<float>();
		groundColor.y = json["groundColor"][1].get<float>();
		groundColor.z = json["groundColor"][2].get<float>();
		groundColor.w = json["groundColor"][3].get<float>();
	}

	if (json.contains("cursorColor") && json["cursorColor"].is_array() && json["cursorColor"].size() >= 4) {
		cursorColor.x = json["cursorColor"][0].get<float>();
		cursorColor.y = json["cursorColor"][1].get<float>();
		cursorColor.z = json["cursorColor"][2].get<float>();
		cursorColor.w = json["cursorColor"][3].get<float>();
	}

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
	json["minMatchCount"] = minMatchCount;
	json["panelEasingDuration"] = panelEasingDuration;
	json["cursorEasingDuration"] = cursorEasingDuration;
	json["cameraHeight"] = cameraHeight;
	json["cameraZDistance"] = cameraZDistance;
	json["cameraFov"] = cameraFov;
	json["panelClearRiseHeight"] = panelClearRiseHeight;
	json["panelClearDuration"] = panelClearDuration;
	json["groundColor"] = { groundColor.x, groundColor.y, groundColor.z, groundColor.w };
	json["cursorColor"] = { cursorColor.x, cursorColor.y, cursorColor.z, cursorColor.w };

	KashipanEngine::JSON colorsArr = KashipanEngine::JSON::array();
	for (int i = 0; i < kMaxPanelTypes; i++) {
		colorsArr.push_back({ panelColors[i].x, panelColors[i].y, panelColors[i].z, panelColors[i].w });
	}
	json["panelColors"] = colorsArr;

	KashipanEngine::SaveJSON(json, filepath);
}

} // namespace Application
