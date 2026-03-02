#include "Config/PuzzleGameConfig.h"

namespace Application {

void PuzzleGameConfig::LoadFromJSON(const std::string& filepath) {
	if (!KashipanEngine::IsFileExist(filepath)) return;
	auto json = KashipanEngine::LoadJSON(filepath);
	if (json.is_null()) return;

	stageSize = KashipanEngine::GetJSONValueOrDefault(json, "stageSize", stageSize);
	// 奇数に補正
	if (stageSize % 2 == 0) stageSize++;

	panelTypeCount = KashipanEngine::GetJSONValueOrDefault(json, "panelTypeCount", panelTypeCount);
	panelEasingDuration = KashipanEngine::GetJSONValueOrDefault(json, "panelEasingDuration", panelEasingDuration);
	cursorEasingDuration = KashipanEngine::GetJSONValueOrDefault(json, "cursorEasingDuration", cursorEasingDuration);
	cameraHeight = KashipanEngine::GetJSONValueOrDefault(json, "cameraHeight", cameraHeight);
	cameraZDistance = KashipanEngine::GetJSONValueOrDefault(json, "cameraZDistance", cameraZDistance);
	cameraFov = KashipanEngine::GetJSONValueOrDefault(json, "cameraFov", cameraFov);
	goalMoveCount = KashipanEngine::GetJSONValueOrDefault(json, "goalMoveCount", goalMoveCount);

	if (json.contains("goalPanelOffset") && json["goalPanelOffset"].is_array() && json["goalPanelOffset"].size() >= 3) {
		goalPanelOffset.x = json["goalPanelOffset"][0].get<float>();
		goalPanelOffset.y = json["goalPanelOffset"][1].get<float>();
		goalPanelOffset.z = json["goalPanelOffset"][2].get<float>();
	}

	if (json.contains("groundColor") && json["groundColor"].is_array() && json["groundColor"].size() >= 4) {
		groundColor.x = json["groundColor"][0].get<float>();
		groundColor.y = json["groundColor"][1].get<float>();
		groundColor.z = json["groundColor"][2].get<float>();
		groundColor.w = json["groundColor"][3].get<float>();
	}

	if (json.contains("emptyGroundColor") && json["emptyGroundColor"].is_array() && json["emptyGroundColor"].size() >= 4) {
		emptyGroundColor.x = json["emptyGroundColor"][0].get<float>();
		emptyGroundColor.y = json["emptyGroundColor"][1].get<float>();
		emptyGroundColor.z = json["emptyGroundColor"][2].get<float>();
		emptyGroundColor.w = json["emptyGroundColor"][3].get<float>();
	}

	if (json.contains("cursorColor") && json["cursorColor"].is_array() && json["cursorColor"].size() >= 4) {
		cursorColor.x = json["cursorColor"][0].get<float>();
		cursorColor.y = json["cursorColor"][1].get<float>();
		cursorColor.z = json["cursorColor"][2].get<float>();
		cursorColor.w = json["cursorColor"][3].get<float>();
	}

	if (json.contains("panelColors") && json["panelColors"].is_array()) {
		int count = static_cast<int>(json["panelColors"].size());
		if (count > 8) count = 8;
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
	json["panelTypeCount"] = panelTypeCount;
	json["panelEasingDuration"] = panelEasingDuration;
	json["cursorEasingDuration"] = cursorEasingDuration;
	json["cameraHeight"] = cameraHeight;
	json["cameraZDistance"] = cameraZDistance;
	json["cameraFov"] = cameraFov;
	json["goalMoveCount"] = goalMoveCount;
	json["goalPanelOffset"] = { goalPanelOffset.x, goalPanelOffset.y, goalPanelOffset.z };
	json["groundColor"] = { groundColor.x, groundColor.y, groundColor.z, groundColor.w };
	json["emptyGroundColor"] = { emptyGroundColor.x, emptyGroundColor.y, emptyGroundColor.z, emptyGroundColor.w };
	json["cursorColor"] = { cursorColor.x, cursorColor.y, cursorColor.z, cursorColor.w };

	KashipanEngine::JSON colorsArr = KashipanEngine::JSON::array();
	for (int i = 0; i < 8; i++) {
		colorsArr.push_back({ panelColors[i].x, panelColors[i].y, panelColors[i].z, panelColors[i].w });
	}
	json["panelColors"] = colorsArr;

	KashipanEngine::SaveJSON(json, filepath);
}

} // namespace Application
