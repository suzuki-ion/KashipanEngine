#include "ProjectWindow.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>

#include "Core/GameEngine.h"
#include "Core/ProjectPaths.h"
#include "Scene/Editor/EditorWindowChrome.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

void ProjectWindow::RefreshProjectList() {
    projects_ = ProjectManager::GetProjectList();
    hasScannedProjects_ = true;
}

void ProjectWindow::ShowImGui() {
    if (!ImGui::Begin(TranslationLabel("editor.project.window"))) {
        ImGui::End();
        return;
    }
    DrawFloatingWindowChromeButtons();

    // Projects配下の走査はディスクアクセスを伴うため、パネルを開いた直後の1回だけ行う
    if (!hasScannedProjects_) {
        RefreshProjectList();
    }

    //--------- 現在開いているプロジェクト ---------//
    ImGui::SeparatorText(TranslationLabel("editor.project.current"));
    if (ProjectPaths::IsStandalone()) {
        ImGui::TextUnformatted(TranslationC("editor.project.standalone"));
        ImGui::TextDisabled("%s", ProjectPaths::ProjectRoot().c_str());
        ImGui::End();
        return;
    }
    ImGui::Text("%s%s", TranslationC("editor.project.name"), ProjectPaths::ProjectName().c_str());
    ImGui::TextDisabled("%s", ProjectPaths::ProjectRoot().c_str());

    if (!pendingProjectName_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
            "%s%s", TranslationC("editor.project.reopening"), pendingProjectName_.c_str());
    }

    //--------- プロジェクト一覧 ---------//
    ImGui::SeparatorText(TranslationLabel("editor.project.list"));
    ImGui::SameLine();
    if (ImGui::SmallButton(TranslationLabel("editor.common.refresh"))) {
        RefreshProjectList();
    }
    ImGui::SetItemTooltip("%s", TranslationC("editor.project.list.refresh.tooltip"));

    if (ImGui::BeginTable("##ProjectList", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
        for (const auto &project : projects_) {
            const bool isActive = (project.name == ProjectPaths::ProjectName());
            ImGui::PushID(project.name.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(project.name.c_str());
            if (!project.description.empty()) {
                ImGui::SetItemTooltip("%s", project.description.c_str());
            }
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(isActive || !pendingProjectName_.empty());
            if (ImGui::SmallButton(isActive ? TranslationLabel("editor.project.open.current") : TranslationLabel("editor.project.open"))) {
                if (ProjectManager::RequestRestartWithProject(project.name)) {
                    pendingProjectName_ = project.name;
                    // 後始末（シーンの自動保存など）を通常どおり済ませてから、
                    // KashipanEngine.cpp 側が新しいプロセスを起動する
                    GameEngine::RequestQuit();
                }
            }
            ImGui::EndDisabled();
            if (!isActive) {
                ImGui::SetItemTooltip("%s", TranslationC("editor.project.open.tooltip"));
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    //--------- 新規作成 ---------//
    ImGui::SeparatorText(TranslationLabel("editor.project.new"));
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##NewProjectName", TranslationC("editor.project.new.name.hint"), &newProjectNameBuffer_);
    ImGui::SameLine();
    ImGui::BeginDisabled(newProjectNameBuffer_.empty());
    if (ImGui::Button(TranslationLabel("editor.common.create"))) {
        lastErrorMessage_.clear();
        // 作成しただけでは開いているプロジェクトを変えない（次回起動時の対象も変えない）。
        // 切り替えたい場合は上の一覧から Open を押してもらう
        if (ProjectManager::CreateProject(newProjectNameBuffer_,
                ProjectManager::kDefaultTemplateName, &lastErrorMessage_)) {
            newProjectNameBuffer_.clear();
            RefreshProjectList();
        }
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("%s", TranslationC("editor.project.new.create.tooltip"));

    if (!lastErrorMessage_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s", lastErrorMessage_.c_str());
    }

    ImGui::End();
}

} // namespace KashipanEngine
#endif // USE_IMGUI
