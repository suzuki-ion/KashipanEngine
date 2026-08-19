#include "TranslationEditor.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <vector>

#include "Scene/Editor/EditorWindowChrome.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

namespace {

/// @brief 大文字小文字を区別せずに部分一致を判定する
bool ContainsIgnoreCase(const std::string &haystack, const std::string &needle) {
    if (needle.empty()) return true;
    const auto it = std::search(
        haystack.begin(), haystack.end(), needle.begin(), needle.end(),
        [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
    return it != haystack.end();
}

/// @brief 翻訳マップをキーの昇順に並べ替えて返す（描画順を安定させるため）
std::vector<std::pair<std::string, std::string>> SortedEntries(
    const std::unordered_map<std::string, std::string> &translations) {
    std::vector<std::pair<std::string, std::string>> entries(translations.begin(), translations.end());
    std::sort(entries.begin(), entries.end(),
        [](const auto &a, const auto &b) { return a.first < b.first; });
    return entries;
}

} // namespace

void TranslationEditor::ShowImGui() {
    if (!ImGui::Begin(TranslationLabel("editor.translationeditor.window"))) {
        ImGui::End();
        return;
    }
    DrawFloatingWindowChromeButtons();

    // 初回は現在エンジンが使っている言語を編集対象にする
    if (language_.empty()) language_ = GetCurrentLanguage();

    ImGui::TextDisabled("%s", TranslationC("editor.translationeditor.description"));

    ShowLanguageSelector();
    ImGui::Separator();
    ShowProjectTranslations();
    ImGui::Separator();
    ShowGlobalTranslationReference();

    ImGui::End();
}

void TranslationEditor::ShowLanguageSelector() {
    const std::vector<std::string> languages = GetLoadedLanguages();

    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo(TranslationLabel("editor.translationeditor.language"),
            GetLanguageDisplayName(language_).c_str())) {
        for (const auto &lang : languages) {
            const bool selected = (lang == language_);
            ImGui::PushID(lang.c_str());
            if (ImGui::Selectable(GetLanguageDisplayName(lang).c_str(), selected) && !selected) {
                // 未保存の変更はメモリ上に残るため、言語を切り替えても失われはしない
                language_ = lang;
                statusMessage_.clear();
            }
            if (selected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    // 保存先が未設定の言語もありうるため、実際の書き出し先を明示しておく
    const std::string &filePath = GetProjectTranslationFilePath(language_);
    ImGui::SameLine();
    if (filePath.empty()) {
        ImGui::TextDisabled("%s", TranslationC("editor.translationeditor.nofilepath"));
    } else {
        ImGui::TextDisabled("%s", filePath.c_str());
    }
}

void TranslationEditor::ShowProjectTranslations() {
    ImGui::SeparatorText(TranslationLabel("editor.translationeditor.project.section"));

    ShowAddRow();

    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputTextWithHint("##TranslationFilter",
        TranslationC("editor.translationeditor.filter.hint"), &filter_);

    const auto entries = SortedEntries(GetProjectTranslations(language_));
    if (entries.empty()) {
        ImGui::TextDisabled("%s", TranslationC("editor.translationeditor.project.empty"));
    }

    // 反復中にマップを書き換えないよう、操作は一旦控えてから適用する
    std::string pendingRemoveKey;
    std::string pendingEditKey;
    std::string pendingEditText;

    if (!entries.empty() && ImGui::BeginTable("##ProjectTranslations", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
            ImGuiTableFlags_ScrollY, ImVec2(0.0f, 260.0f))) {
        ImGui::TableSetupColumn(TranslationLabel("editor.translationeditor.column.key"),
            ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupColumn(TranslationLabel("editor.translationeditor.column.text"),
            ImGuiTableColumnFlags_WidthStretch, 0.55f);
        ImGui::TableSetupColumn(TranslationLabel("editor.translationeditor.column.remove"),
            ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (const auto &[key, text] : entries) {
            if (!ContainsIgnoreCase(key, filter_) && !ContainsIgnoreCase(text, filter_)) continue;

            ImGui::PushID(key.c_str());
            ImGui::TableNextRow();

            //--------- キー（変更するとキー名の変更になるため読み取り専用） ---------//
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(key.c_str());
            // エンジン側にも同じキーがある場合は上書きしていることが分かるようにする
            if (IsGlobalTranslationKey(language_, key)) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s",
                    TranslationC("editor.translationeditor.overriding"));
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", TranslationC("editor.translationeditor.overriding.tooltip"));
                }
            }

            //--------- 翻訳テキスト ---------//
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            std::string editableText = text;
            if (ImGui::InputText("##text", &editableText)) {
                pendingEditKey = key;
                pendingEditText = editableText;
            }

            //--------- 削除 ---------//
            ImGui::TableNextColumn();
            if (ImGui::SmallButton(TranslationLabel("editor.common.remove"))) {
                pendingRemoveKey = key;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", IsGlobalTranslationKey(language_, key)
                    ? TranslationC("editor.translationeditor.remove.tooltip.override")
                    : TranslationC("editor.translationeditor.remove.tooltip"));
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (!pendingEditKey.empty()) {
        SetProjectTranslation(language_, pendingEditKey, pendingEditText);
        isDirty_ = true;
    }
    if (!pendingRemoveKey.empty()) {
        RemoveProjectTranslation(language_, pendingRemoveKey);
        isDirty_ = true;
    }

    //--------- 保存 ---------//
    ImGui::BeginDisabled(!isDirty_);
    if (ImGui::Button(TranslationLabel("editor.common.save"))) {
        if (SaveProjectTranslationFile(language_)) {
            isDirty_ = false;
            isStatusError_ = false;
            statusMessage_ = Translation("editor.translationeditor.save.succeeded");
        } else {
            isStatusError_ = true;
            statusMessage_ = Translation("editor.translationeditor.save.failed");
        }
    }
    ImGui::EndDisabled();
    if (isDirty_) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s",
            TranslationC("editor.translationeditor.unsaved"));
    }
    if (!statusMessage_.empty()) {
        ImGui::SameLine();
        const ImVec4 color = isStatusError_
            ? ImVec4(1.0f, 0.5f, 0.3f, 1.0f)
            : ImVec4(0.4f, 0.9f, 0.4f, 1.0f);
        ImGui::TextColored(color, "%s", statusMessage_.c_str());
    }
}

void TranslationEditor::ShowAddRow() {
    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputTextWithHint("##NewTranslationKey",
        TranslationC("editor.translationeditor.newkey.hint"), &newKeyBuffer_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputTextWithHint("##NewTranslationText",
        TranslationC("editor.translationeditor.newtext.hint"), &newTextBuffer_);
    ImGui::SameLine();

    const bool alreadyDefinedInProject = GetProjectTranslations(language_).contains(newKeyBuffer_);
    ImGui::BeginDisabled(newKeyBuffer_.empty() || alreadyDefinedInProject);
    if (ImGui::Button(TranslationLabel("editor.common.add"))) {
        SetProjectTranslation(language_, newKeyBuffer_, newTextBuffer_);
        newKeyBuffer_.clear();
        newTextBuffer_.clear();
        isDirty_ = true;
        statusMessage_.clear();
    }
    ImGui::EndDisabled();

    // 入力中のキーがどういう扱いになるのかをその場で伝える
    if (alreadyDefinedInProject) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s",
            TranslationC("editor.translationeditor.newkey.duplicate"));
    } else if (!newKeyBuffer_.empty() && IsGlobalTranslationKey(language_, newKeyBuffer_)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s",
            TranslationC("editor.translationeditor.newkey.willoverride"));
    }
}

void TranslationEditor::ShowGlobalTranslationReference() {
    // 折りたたんだ状態が既定。エンジン側のキーは数が多く、普段は畳んでおきたいため
    if (!ImGui::CollapsingHeader(TranslationLabel("editor.translationeditor.global.section"))) {
        return;
    }

    ImGui::TextDisabled("%s", TranslationC("editor.translationeditor.global.description"));

    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputTextWithHint("##GlobalTranslationFilter",
        TranslationC("editor.translationeditor.filter.hint"), &globalFilter_);

    const auto &globalTranslations = GetGlobalTranslations(language_);
    // 全件を毎フレーム並べ替えるのは重いため、絞り込んでから並べ替える
    std::vector<std::pair<std::string, std::string>> entries;
    entries.reserve(globalTranslations.size());
    for (const auto &[key, text] : globalTranslations) {
        if (!ContainsIgnoreCase(key, globalFilter_) && !ContainsIgnoreCase(text, globalFilter_)) continue;
        entries.emplace_back(key, text);
    }
    std::sort(entries.begin(), entries.end(),
        [](const auto &a, const auto &b) { return a.first < b.first; });

    ImGui::Text("%s%d", TranslationC("editor.translationeditor.global.count"), static_cast<int>(entries.size()));

    if (!ImGui::BeginTable("##GlobalTranslations", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
            ImGuiTableFlags_ScrollY, ImVec2(0.0f, 220.0f))) {
        return;
    }
    ImGui::TableSetupColumn(TranslationLabel("editor.translationeditor.column.key"),
        ImGuiTableColumnFlags_WidthStretch, 0.45f);
    ImGui::TableSetupColumn(TranslationLabel("editor.translationeditor.column.text"),
        ImGuiTableColumnFlags_WidthStretch, 0.55f);
    ImGui::TableSetupColumn(TranslationLabel("editor.translationeditor.column.override"),
        ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    // 行数が多いのでクリッピングして必要な行だけ描画する
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(entries.size()));
    std::string pendingOverrideKey;
    std::string pendingOverrideText;
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const auto &[key, text] = entries[static_cast<size_t>(i)];
            ImGui::PushID(key.c_str());
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(key.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(text.c_str());

            //--------- このキーをプロジェクト側へ複製して上書き対象にする ---------//
            ImGui::TableNextColumn();
            const bool alreadyOverridden = GetProjectTranslations(language_).contains(key);
            ImGui::BeginDisabled(alreadyOverridden);
            if (ImGui::SmallButton(TranslationLabel("editor.translationeditor.override"))) {
                pendingOverrideKey = key;
                pendingOverrideText = text;
            }
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", alreadyOverridden
                    ? TranslationC("editor.translationeditor.override.tooltip.already")
                    : TranslationC("editor.translationeditor.override.tooltip"));
            }
            ImGui::PopID();
        }
    }
    ImGui::EndTable();

    if (!pendingOverrideKey.empty()) {
        // エンジン側の文言を初期値としてプロジェクト層へ複製する（そのまま編集して差し替えられる）
        SetProjectTranslation(language_, pendingOverrideKey, pendingOverrideText);
        isDirty_ = true;
        statusMessage_.clear();
    }
}

} // namespace KashipanEngine

#endif // USE_IMGUI
