#pragma once
#ifdef USE_IMGUI
#include "Utilities/ImGuiCustom.h"
#endif
#include "Utilities/FileIO.h"

namespace KashipanEngine {

// SaveJSONPropertyVisitor と LoadJSONPropertyVisitor で UiOptions は必要ない
// ImGuiPropertyVisitor では UiOptions が必要なので、共通のテンプレート引数として受け取る

#ifdef USE_IMGUI
class ImGuiPropertyVisitor {
public:
    template <typename T>
    void Visit(const std::string &name, T &value, const ImGuiCustom::UiOptions &opts = {}) {
        ImGuiCustom::EditValue(name.c_str(), value, opts);
    }
    template <typename T>
    void Visit(const std::string &name, std::vector<T> &values, const ImGuiCustom::UiOptions &opts = {}) {
        ImGuiCustom::EditValue(name.c_str(), values, opts);
    }
    template <typename T>
    void Visit(const std::string &name, std::unordered_map<std::string, T> &values, const ImGuiCustom::UiOptions &opts = {}) {
        ImGuiCustom::EditValue(name.c_str(), values, opts);
    }
};
#endif

class SaveJSONPropertyVisitor {
public:
    template <typename T>
    void Visit(const std::string &name, const T &value, const ImGuiCustom::UiOptions &opts = {}) {
        jsonObject_[name] = ToJSON(value);
    }
    template <typename T>
    void Visit(const std::string &name, const std::vector<T> &values, const ImGuiCustom::UiOptions &opts = {}) {
        jsonObject_[name] = ToJSON(values);
    }
    template <typename T>
    void Visit(const std::string &name, const std::unordered_map<std::string, T> &values, const ImGuiCustom::UiOptions &opts = {}) {
        jsonObject_[name] = ToJSON(values);
    }
    bool SaveToFile(const std::string &filePath) const {
        return SaveJSON(jsonObject_, filePath);
    }
private:
    JSON jsonObject_;
};

class LoadJSONPropertyVisitor {
public:
    template <typename T>
    void Visit(const std::string &name, T &value, const ImGuiCustom::UiOptions &opts = {}) {
        if (jsonObject_.contains(name)) {
            value = FromJSON<T>(jsonObject_[name]);
        }
    }
    template <typename T>
    void Visit(const std::string &name, std::vector<T> &values, const ImGuiCustom::UiOptions &opts = {}) {
        if (jsonObject_.contains(name)) {
            values = FromJSON<std::vector<T>>(jsonObject_[name]);
        }
    }
    template <typename K, typename V>
    void Visit(const std::string &name, std::unordered_map<K, V> &values, const ImGuiCustom::UiOptions &opts = {}) {
        if (jsonObject_.contains(name)) {
            values = FromJSON<std::unordered_map<K, V>>(jsonObject_[name]);
        }
    }
    bool LoadFromFile(const std::string &filePath) {
        return LoadJSON(filePath);
    }
private:
    JSON jsonObject_;
};

} // namespace KashipanEngine