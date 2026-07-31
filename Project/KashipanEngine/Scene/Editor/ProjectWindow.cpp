#include "ProjectWindow.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_stdlib.h>

#include "Core/GameEngine.h"
#include "Core/ProjectPaths.h"

namespace KashipanEngine {

void ProjectWindow::RefreshProjectList() {
    projects_ = ProjectManager::GetProjectList();
    hasScannedProjects_ = true;
}

void ProjectWindow::ShowImGui() {
    if (!ImGui::Begin("Project")) {
        ImGui::End();
        return;
    }

    // Projects配下の走査はディスクアクセスを伴うため、パネルを開いた直後の1回だけ行う
    if (!hasScannedProjects_) {
        RefreshProjectList();
    }

    //--------- 現在開いているプロジェクト ---------//
    ImGui::SeparatorText("Current Project");
    if (ProjectPaths::IsStandalone()) {
        ImGui::TextUnformatted("Running in standalone mode (assets are next to the executable).");
        ImGui::TextDisabled("%s", ProjectPaths::ProjectRoot().c_str());
        ImGui::End();
        return;
    }
    ImGui::Text("Name: %s", ProjectPaths::ProjectName().c_str());
    ImGui::TextDisabled("%s", ProjectPaths::ProjectRoot().c_str());

    if (!pendingProjectName_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
            "Reopening with \"%s\"...", pendingProjectName_.c_str());
    }

    //--------- プロジェクト一覧 ---------//
    ImGui::SeparatorText("Projects");
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh")) {
        RefreshProjectList();
    }
    ImGui::SetItemTooltip("Re-scan the Projects folder (e.g. after adding one outside the editor).");

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
            if (ImGui::SmallButton(isActive ? "Open (current)" : "Open")) {
                if (ProjectManager::RequestRestartWithProject(project.name)) {
                    pendingProjectName_ = project.name;
                    // 後始末（シーンの自動保存など）を通常どおり済ませてから、
                    // KashipanEngine.cpp 側が新しいプロセスを起動する
                    GameEngine::RequestQuit();
                }
            }
            ImGui::EndDisabled();
            if (!isActive) {
                ImGui::SetItemTooltip("Closes the editor and reopens it with this project.");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    //--------- 新規作成 ---------//
    ImGui::SeparatorText("New Project");
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##NewProjectName", "New project name", &newProjectNameBuffer_);
    ImGui::SameLine();
    ImGui::BeginDisabled(newProjectNameBuffer_.empty());
    if (ImGui::Button("Create")) {
        lastErrorMessage_.clear();
        // 作成しただけでは開いているプロジェクトを変えない（次回起動時の対象も変えない）。
        // 切り替えたい場合は上の一覧から Open を押してもらう
        if (ProjectManager::CreateProject(newProjectNameBuffer_, &lastErrorMessage_)) {
            newProjectNameBuffer_.clear();
            RefreshProjectList();
        }
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Creates Projects/<name>/ with a copy of the AssetsTemplate folder.\nUse Open above to switch to it.");

    if (!lastErrorMessage_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s", lastErrorMessage_.c_str());
    }

    ImGui::End();
}

} // namespace KashipanEngine
#endif // USE_IMGUI
