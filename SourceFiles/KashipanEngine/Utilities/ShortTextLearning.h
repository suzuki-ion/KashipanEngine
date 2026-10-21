#pragma once
#include <filesystem>
#include "Utilities/ShortTextBehavior.h"

namespace KashipanEngine::ShortText {
enum class LearningMode { Recipe, Alias };
struct LearnedEntry {
    LearningMode mode = LearningMode::Recipe;
    std::string phrase;
    std::string replacement;
    Recipe recipe;
};

// Project-local dictionary. Mutations become visible only after successful disk persistence.
class LearningStore {
public:
    explicit LearningStore(std::filesystem::path path) : path_(std::move(path)) {}
    bool Reload(std::string &error);
    bool Upsert(const LearnedEntry &entry, std::string &error);
    bool Remove(size_t index, std::string &error);
    Result Interpret(const std::string &input) const;
    bool IsReady() const { return loaded_; }
    const std::vector<LearnedEntry> &Entries() const { return entries_; }
private:
    bool Persist(const std::vector<LearnedEntry> &next, std::string &error);
    std::filesystem::path path_;
    std::vector<LearnedEntry> entries_;
    std::string diskContents_;
    bool diskExists_ = false;
    bool loaded_ = false;
};
} // namespace KashipanEngine::ShortText
