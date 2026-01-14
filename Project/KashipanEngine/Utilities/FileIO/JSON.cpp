#include <fstream>
#include <sstream>
#include <iostream>
#include "JSON.h"

namespace KashipanEngine {

JSON LoadJSON(const std::string &filename) {
    JSON jsonData;
    std::fstream jsonFile(filename);
    jsonData = JSON::parse(jsonFile, nullptr, false, true);
    return jsonData;
}

bool SaveJSON(const JSON &jsonData, const std::string &filepath, int indent) {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        file << jsonData.dump(indent);
        file.close();
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

template<typename T>
std::optional<T> GetJSONValue(const JSON &json, const std::string &key) {
    try {
        if (json.contains(key) && !json[key].is_null()) {
            return json[key].get<T>();
        }
    } catch (const std::exception &) {
        // 型変換エラーなどを捕捉
    }
    return std::nullopt;
}

template<typename T>
T GetJSONValueOrDefault(const JSON &json, const std::string &key, const T &defaultValue) {
    auto value = GetJSONValue<T>(json, key);
    return value.has_value() ? value.value() : defaultValue;
}

std::optional<JSON> GetNestedJSONValue(const JSON &json, const std::string &path) {
    try {
        JSON current = json;
        std::stringstream ss(path);
        std::string segment;

        while (std::getline(ss, segment, '.')) {
            // 配列インデックスをチェック（例: "array[0]"）
            size_t bracketStart = segment.find('[');
            if (bracketStart != std::string::npos) {
                std::string arrayName = segment.substr(0, bracketStart);
                size_t bracketEnd = segment.find(']', bracketStart);
                if (bracketEnd == std::string::npos) {
                    return std::nullopt;
                }

                std::string indexStr = segment.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
                int index = std::stoi(indexStr);

                if (!current.contains(arrayName) || !current[arrayName].is_array() ||
                    index < 0 || index >= static_cast<int>(current[arrayName].size())) {
                    return std::nullopt;
                }
                current = current[arrayName][index];
            } else {
                if (!current.contains(segment)) {
                    return std::nullopt;
                }
                current = current[segment];
            }
        }
        return current;
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

bool ValidateJSONStructure(const JSON &json, const std::vector<std::string> &requiredKeys) {
    for (const auto &key : requiredKeys) {
        if (!json.contains(key)) {
            return false;
        }
    }
    return true;
}

bool IsJSONFileValid(const std::string &filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        JSON parsedJSON = JSON::parse(file);
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

JSON MergeJSON(const JSON &base, const JSON &overlay, bool deepMerge) {
    if (!deepMerge) {
        JSON result = base;
        result.update(overlay);
        return result;
    }

    JSON result = base;
    for (auto it = overlay.begin(); it != overlay.end(); ++it) {
        if (result.contains(it.key()) && result[it.key()].is_object() && it.value().is_object()) {
            result[it.key()] = MergeJSON(result[it.key()], it.value(), true);
        } else {
            result[it.key()] = it.value();
        }
    }
    return result;
}

bool AppendToJSONArray(JSON &json, const std::string &arrayKey, const JSON &value) {
    try {
        if (!json.contains(arrayKey)) {
            json[arrayKey] = JSON::array();
        } else if (!json[arrayKey].is_array()) {
            return false;
        }
        json[arrayKey].push_back(value);
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

bool RemoveFromJSONArray(JSON &json, const std::string &arrayKey, size_t index) {
    try {
        if (!json.contains(arrayKey) || !json[arrayKey].is_array()) {
            return false;
        }

        auto &array = json[arrayKey];
        if (index >= array.size()) {
            return false;
        }

        array.erase(array.begin() + index);
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

std::string JSONToFormattedString(const JSON &json, int indent) {
    try {
        return json.dump(indent);
    } catch (const std::exception &) {
        return "{}";
    }
}

void PrintJSON(const JSON &json, const std::string &title) {
    std::cout << "=== " << title << " ===" << std::endl;
    std::cout << JSONToFormattedString(json, 2) << std::endl;
    std::cout << "===================" << std::endl;
}

// テンプレートの明示的なインスタンス化
template std::optional<int> GetJSONValue<int>(const JSON &json, const std::string &key);
template std::optional<float> GetJSONValue<float>(const JSON &json, const std::string &key);
template std::optional<double> GetJSONValue<double>(const JSON &json, const std::string &key);
template std::optional<bool> GetJSONValue<bool>(const JSON &json, const std::string &key);
template std::optional<std::string> GetJSONValue<std::string>(const JSON &json, const std::string &key);

template int GetJSONValueOrDefault<int>(const JSON &json, const std::string &key, const int &defaultValue);
template float GetJSONValueOrDefault<float>(const JSON &json, const std::string &key, const float &defaultValue);
template double GetJSONValueOrDefault<double>(const JSON &json, const std::string &key, const double &defaultValue);
template bool GetJSONValueOrDefault<bool>(const JSON &json, const std::string &key, const bool &defaultValue);
template std::string GetJSONValueOrDefault<std::string>(const JSON &json, const std::string &key, const std::string &defaultValue);

} // namespace KashipanEngine