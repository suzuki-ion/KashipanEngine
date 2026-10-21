#include "Utilities/ShortTextLearning.h"
#include <json.hpp>
#include <fstream>
#include <iostream>
#include <chrono>
#include <stdexcept>

using namespace KashipanEngine::ShortText;
namespace fs = std::filesystem;
void Require(bool value, const std::string &message) { if (!value) throw std::runtime_error(message); }
void Write(const fs::path &path, const std::string &text) { std::ofstream out(path, std::ios::binary); out << text; Require(out.good(), "fixture write"); }
std::string Read(const fs::path &path) { std::ifstream in(path, std::ios::binary); return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()}; }
int main() {
    const auto sandbox = fs::temp_directory_path() / ("KashipanLearning-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(sandbox);
    int code = 0;
    try {
        const auto path = sandbox / L"学習内容" / L"ShortTextLearning.json";
        LearningStore store(path);
        std::string error;
        Require(store.Reload(error) && store.IsReady() && store.Entries().empty(), "first use without a file");
        LearnedEntry alias{LearningMode::Alias, "ぷかぷか", "ふわふわ", {}};
        Require(store.Upsert(alias, error), error);
        auto interpreted = store.Interpret("周期３秒でぷかぷかさせて");
        Require(interpreted.success && interpreted.recipe.kind == Kind::Oscillate && interpreted.recipe.period == 3, "learned alias with full-width numeric modifier");
        Require(!store.Interpret("ぷかぷかさせないで").success, "alias cannot hide negation");
        Require(!store.Interpret("触れたらぷかぷかさせて").success, "alias cannot hide conditions");
        Require(!store.Interpret("ぷかぷかさせて消して").success, "unknown text stays rejected");
        Require(!store.Interpret("ぷかぷかぷかぷか").success, "repeated alias rejected");
        LearnedEntry preset;
        preset.phrase = "宇宙遊泳させて";
        preset.recipe.amplitude = 2; preset.recipe.period = 5;
        Require(store.Upsert(preset, error), error);
        interpreted = store.Interpret(" 宇宙遊泳させて。 ");
        Require(interpreted.success && interpreted.recipe.amplitude == 2 && interpreted.recipe.period == 5, "exact recipe and normalization");
        Require(!store.Interpret("宇宙遊泳させて何かして").success, "exact recipe is not a substring match");
        preset.phrase = "宇宙遊泳させて。"; preset.recipe.amplitude = 3;
        Require(store.Upsert(preset, error) && store.Entries().size() == 2, "normalized phrase updates rather than duplicates");
        Require(store.Interpret("宇宙遊泳させて").recipe.amplitude == 3, "updated preset");
        LearnedEntry longer{LearningMode::Alias, "のんびりぷかぷか", "ゆっくりふわふわ", {}};
        Require(store.Upsert(longer, error), error);
        Require(store.Interpret("のんびりぷかぷかさせて").success && store.Interpret("のんびりぷかぷかさせて").recipe.period == 4, "longest overlapping alias wins");
        LearnedEntry spin{LearningMode::Alias, "ぐるぐる", "回転", {}};
        Require(store.Upsert(spin, error), error);
        Require(!store.Interpret("ぷかぷかぐるぐるして").success, "two different aliases rejected");
        LearnedEntry invalid = alias; invalid.replacement = "ぷかぷか";
        Require(!store.Upsert(invalid, error), "alias chaining is not allowed");
        invalid.replacement = "";
        Require(!store.Upsert(invalid, error), "empty alias target rejected");
        invalid = preset; invalid.phrase = "触れたら消して";
        Require(!store.Upsert(invalid, error), "unsupported grammar cannot be taught as an unconditional recipe");
        invalid.phrase = "回転して右に移動して";
        Require(!store.Upsert(invalid, error), "mixed actions cannot be hidden in an exact learned recipe");
        invalid = preset; invalid.recipe.speed = 0;
        Require(!store.Upsert(invalid, error), "invalid recipe rejected");
        invalid = preset; invalid.phrase = std::string(513, 'a');
        Require(!store.Upsert(invalid, error), "phrase length bound");
        LearningStore restarted(path);
        Require(restarted.Reload(error), error);
        Require(restarted.Interpret("周期3秒でぷかぷかさせて").success && restarted.Interpret("宇宙遊泳させて").recipe.amplitude == 3, "restart retains aliases and recipes at a Unicode path");
        const auto good = Read(path);
        Write(path, "broken JSON");
        Require(!restarted.Reload(error) && restarted.Interpret("宇宙遊泳させて").success, "corrupt reload retains previous dictionary");
        Require(!restarted.Upsert(alias, error) && Read(path) == "broken JSON", "corrupt external file is not overwritten");
        LearningStore broken(path);
        Require(!broken.Reload(error) && !broken.IsReady() && !broken.Upsert(alias, error), "initial corrupt file blocks writes");
        Write(path, good);
        Require(restarted.Reload(error), error);
        auto bad = nlohmann::json::parse(good);
        bad["entries"].push_back(bad["entries"][0]);
        Write(path, bad.dump());
        Require(!restarted.Reload(error) && restarted.Entries().size() == 4, "duplicate phrases reject the entire reload");
        bad = nlohmann::json::parse(good);
        bad["entries"][1]["recipe"]["direction"] = 18446744073709551615ULL;
        Write(path, bad.dump());
        Require(!restarted.Reload(error), "integer overflow rejected");
        bad = nlohmann::json::parse(good); bad["version"] = 2;
        Write(path, bad.dump());
        Require(!restarted.Reload(error), "unknown file version rejected");
        Write(path, good);
        LearningStore external(path);
        Require(external.Reload(error), error);
        alias.replacement = "左右にふわふわ";
        Require(external.Upsert(alias, error), error);
        Require(!restarted.Upsert(preset, error), "external changes prevent stale writes");
        Require(restarted.Reload(error) && restarted.Interpret("ぷかぷかさせて").recipe.axis == 0, "manual reload reflects external change");
        Require(restarted.Remove(0, error) && !restarted.Interpret("ぷかぷかさせて").success, "deletion is immediately effective");
        LearningStore afterDelete(path);
        Require(afterDelete.Reload(error) && !afterDelete.Interpret("ぷかぷかさせて").success, "deletion persists across restart");
        const auto blocked = sandbox / "blocked";
        LearningStore cannotWrite(blocked / "dictionary.json");
        Require(cannotWrite.Reload(error), error);
        Write(blocked, "parent is a file");
        Require(!cannotWrite.Upsert(preset, error) && cannotWrite.Entries().empty(), "failed save does not change in-memory dictionary");
        preset.phrase = "回転して"; preset.recipe.kind = Kind::Rotate; preset.recipe.axis = 1; preset.recipe.speed = 42;
        Require(afterDelete.Upsert(preset, error) && afterDelete.Interpret("回転して").recipe.speed == 42, "explicit learned recipe overrides default for the same full sentence");
        LearnedEntry jump;
        jump.phrase = "スペースキーを押したらY方向の速度を16にする";
        jump.recipe.kind = Kind::Rules;
        Rule keyRule; keyRule.event = Event::KeyPressed;
        keyRule.actions = {{Operation::SetVelocity, 1, 16}};
        jump.recipe.rules.push_back(keyRule);
        Require(afterDelete.Upsert(jump, error), error);
        Require(afterDelete.Interpret(jump.phrase).success && afterDelete.Interpret(jump.phrase).recipe.rules[0].actions[0].value == 16, "explicit event recipe is learned");
        LearningStore eventRestart(path);
        Require(eventRestart.Reload(error) && eventRestart.Interpret(jump.phrase).success, "event learning survives restart");
        Require(!eventRestart.Interpret("スペースキーを押したらY方向の速度を100にする").success, "unregistered event text is not guessed");
        std::cout << "Local learning, aliases, numeric composition, restart, edit/delete, malformed data and save failure regression passed.\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; code = 1; }
    if (sandbox.parent_path() == fs::temp_directory_path()) fs::remove_all(sandbox);
    return code;
}
