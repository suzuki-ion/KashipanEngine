#include "JsonManager.h"
#include <fstream>
#include <filesystem>

JsonManager::JsonManager() {
	input = std::make_unique<JsonInput>();
	output = std::make_unique<JsonOutput>();
}

JsonManager::~JsonManager() {}

void JsonManager::Write(std::string fileName) {
	// ファイル名に.json拡張子がない場合は追加
	if (fileName.size() < 5 || fileName.substr(fileName.size() - 5) != ".json") {
		fileName += ".json";
	}

	// ディレクトリが存在しない場合は作成
	std::filesystem::path fullPath = basePath + fileName;
	std::filesystem::path directory = fullPath.parent_path();

	if (!directory.empty() && !std::filesystem::exists(directory)) {
		try {
			std::filesystem::create_directories(directory);
		}
		catch (const std::filesystem::filesystem_error&) {
			return;
		}
	}

	std::ofstream file(basePath + fileName);

	if (!file.is_open()) {
		return;
	}

	output->WriteJson(file, values);

	values.clear();

	file.close();
}

std::vector<std::shared_ptr<ValueBase>> JsonManager::Read(std::string fileName) {
	// ファイル名に.json拡張子がない場合は追加
	if (fileName.size() < 5 || fileName.substr(fileName.size() - 5) != ".json") {
		fileName += ".json";
	}

	std::ifstream file(basePath + fileName);
	if (!file.is_open()) {
		return {};
	}

	std::vector<std::shared_ptr<ValueBase>> ans = input->ReadJson(file);

	file.close();

	return ans;
}
