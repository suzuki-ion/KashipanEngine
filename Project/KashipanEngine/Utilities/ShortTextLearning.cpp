#include "Utilities/ShortTextLearning.h"
#include "Utilities/ShortTextRecipeJson.h"
#include <Windows.h>
#include <json.hpp>
#include <fstream>
#include <chrono>

namespace KashipanEngine::ShortText {
namespace {
constexpr size_t kMaxEntries = 256;
constexpr std::uintmax_t kMaxFileBytes = 1024 * 1024;
using Json = nlohmann::json;

bool Read(const std::filesystem::path &path, bool &exists, std::string &text, std::string &error) {
    std::error_code ec;
    exists = std::filesystem::exists(path, ec);
    if (ec) { error = "学習ファイルの状態を確認できません。"; return false; }
    text.clear();
    if (!exists) return true;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > kMaxFileBytes) { error = "学習ファイルを読み取れないか、1MBを超えています。"; return false; }
    std::ifstream file(path, std::ios::binary);
    if (!file) { error = "学習ファイルを開けません。"; return false; }
    text.resize(static_cast<size_t>(size));
    file.read(text.data(), static_cast<std::streamsize>(size));
    if (!file || file.peek() != std::char_traits<char>::eof()) { error = "読み込み中に学習ファイルが変わりました。再読み込みしてください。"; return false; }
    return true;
}

bool Validate(const LearnedEntry &entry, std::string &error) {
    if (entry.replacement.size() > 512) { error = "言い換え先は512バイト以下で指定してください。"; return false; }
    if (entry.phrase.empty() || entry.phrase.size() > 512 || Normalize(entry.phrase).empty()) {
        error = "教える言葉は1〜512バイトで入力してください。"; return false;
    }
    if ((entry.mode != LearningMode::Recipe || entry.recipe.kind != Kind::Rules) && HasUnsupportedGrammar(Normalize(entry.phrase))) {
        error = "条件・否定・停止・複数動作は学習対象にできません。"; return false;
    }
    if (entry.mode == LearningMode::Recipe) {
        if (!IsValid(entry.recipe)) { error = "挙動の設定値が範囲外です。"; return false; }
    } else if (entry.mode == LearningMode::Alias) {
        if (!Parse(entry.replacement).success) {
            error = "言い換え先は標準の解釈器が理解できる動きにしてください。例：ふわふわ、回転、右に移動"; return false;
        }
    } else { error = "未対応の学習形式です。"; return false; }
    return true;
}

int Integer(const Json &value, int min, int max) {
    if (!value.is_number_integer()) throw std::runtime_error("invalid integer");
    if (value.is_number_unsigned()) {
        if (value.get<std::uint64_t>() > static_cast<std::uint64_t>(max)) throw std::runtime_error("integer overflow");
    } else {
        const auto number = value.get<std::int64_t>();
        if (number < min || number > max) throw std::runtime_error("integer range");
    }
    return value.get<int>();
}
Json Encode(const std::vector<LearnedEntry> &entries) {
    Json items = Json::array();
    for (const auto &entry : entries) {
        Json item{{"mode", entry.mode == LearningMode::Recipe ? "recipe" : "alias"}, {"phrase", entry.phrase}};
        if (entry.mode == LearningMode::Alias) item["replacement"] = entry.replacement;
        else {
            item["recipe"] = EncodeRecipe(entry.recipe);
        }
        items.push_back(std::move(item));
    }
    return Json{{"version", 1}, {"entries", items}};
}
}

bool LearningStore::Reload(std::string &error) {
    error.clear();
    try {
        bool exists;
        std::string text;
        if (!Read(path_, exists, text, error)) return false;
        std::vector<LearnedEntry> next;
        if (exists) {
            const auto doc = Json::parse(text);
            if (!doc.is_object() || Integer(doc.at("version"), 1, 1) != 1 || !doc.at("entries").is_array() || doc.at("entries").size() > kMaxEntries)
                throw std::runtime_error("invalid schema");
            for (const auto &item : doc.at("entries")) {
                LearnedEntry entry;
                entry.phrase = item.at("phrase").get<std::string>();
                const auto mode = item.at("mode").get<std::string>();
                if (mode == "alias") { entry.mode = LearningMode::Alias; entry.replacement = item.at("replacement").get<std::string>(); }
                else if (mode == "recipe") {
                    entry.recipe = DecodeRecipe(item.at("recipe"));
                } else throw std::runtime_error("invalid mode");
                if (!Validate(entry, error)) return false;
                for (const auto &previous : next) if (Normalize(previous.phrase) == Normalize(entry.phrase))
                    throw std::runtime_error("duplicate phrase");
                next.push_back(std::move(entry));
            }
        }
        entries_ = std::move(next);
        diskContents_ = std::move(text);
        diskExists_ = exists;
        loaded_ = true;
        return true;
    } catch (const std::exception &) { error = "学習ファイルの形式が不正です。現在の辞書は保持しました。"; return false; }
}

bool LearningStore::Persist(const std::vector<LearnedEntry> &next, std::string &error) {
    error.clear();
    if (!loaded_) { error = "学習ファイルを正常に読み込んでから登録してください。"; return false; }
    std::filesystem::path temp;
    try {
        bool exists;
        std::string current;
        if (!Read(path_, exists, current, error)) return false;
        if (exists != diskExists_ || current != diskContents_) {
            error = "学習ファイルが外部で変更されました。再読み込みしてから編集してください。"; return false;
        }
        auto text = Encode(next).dump(2) + "\n";
        auto savedEntries = next;
        if (text.size() > kMaxFileBytes) { error = "学習内容が1MBを超えています。"; return false; }
        if (!path_.parent_path().empty()) std::filesystem::create_directories(path_.parent_path());
        temp = path_;
        temp += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
            std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
        {
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            file.write(text.data(), static_cast<std::streamsize>(text.size()));
            file.flush();
            if (!file) throw std::runtime_error("write failed");
        }
        if (!MoveFileExW(temp.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("replace failed (Windows " + std::to_string(GetLastError()) + ")");
        entries_.swap(savedEntries);
        diskContents_ = std::move(text);
        diskExists_ = true;
        return true;
    } catch (const std::exception &exception) {
        std::error_code ec;
        if (!temp.empty()) std::filesystem::remove(temp, ec);
        error = "学習内容を保存できませんでした。現在の辞書は変更していません。詳細：" + std::string(exception.what());
        return false;
    }
}
bool LearningStore::Upsert(const LearnedEntry &entry, std::string &error) {
    error.clear();
    if (!Validate(entry, error)) return false;
    auto next = entries_;
    const auto key = Normalize(entry.phrase);
    auto found = std::find_if(next.begin(), next.end(), [&](const auto &other) { return Normalize(other.phrase) == key; });
    if (found != next.end()) *found = entry;
    else {
        if (next.size() >= kMaxEntries) { error = "登録できる学習内容は256件までです。"; return false; }
        next.push_back(entry);
    }
    return Persist(next, error);
}
bool LearningStore::Remove(size_t index, std::string &error) {
    if (index >= entries_.size()) { error = "削除する学習内容を選択してください。"; return false; }
    auto next = entries_;
    next.erase(next.begin() + static_cast<std::ptrdiff_t>(index));
    return Persist(next, error);
}
Result LearningStore::Interpret(const std::string &input) const {
    const auto standard = Parse(input);
    if (input.empty() || input.size() > 512) return standard;
    const auto text = Normalize(input);
    for (const auto &entry : entries_) {
        if (entry.mode == LearningMode::Recipe && Normalize(entry.phrase) == text)
            return {true, entry.recipe, "学習した挙動：" + Describe(entry.recipe)};
    }
    if (HasUnsupportedGrammar(text)) return standard;
    if (standard.success) return standard;
    const LearnedEntry *chosen = nullptr;
    size_t position = 0, length = 0;
    for (const auto &entry : entries_) {
        if (entry.mode != LearningMode::Alias) continue;
        const auto key = Normalize(entry.phrase);
        const auto pos = text.find(key);
        if (pos != std::string::npos && key.size() > length) { chosen = &entry; position = pos; length = key.size(); }
    }
    if (!chosen) return standard;
    std::string remaining = text;
    remaining.erase(position, length);
    for (const auto &entry : entries_) {
        if (entry.mode == LearningMode::Alias && remaining.find(Normalize(entry.phrase)) != std::string::npos)
            return {false, {}, "複数の学習した言い回しは、分けて入力してください。"};
    }
    auto expanded = text;
    expanded.replace(position, length, Normalize(chosen->replacement));
    auto result = Parse(expanded);
    if (result.success) result.message = "学習した言い換え「" + chosen->phrase + "」：" + result.message;
    return result;
}
} // namespace KashipanEngine::ShortText
