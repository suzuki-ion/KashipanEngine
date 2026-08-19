#if defined(USE_IMGUI)

#include "ImGuiManager.h"

#include "Core/WindowsAPI.h"
#include "Core/DirectXCommon.h"
#include "Core/Window.h"
#include "EngineSettings.h"
#include "Core/ProjectPaths.h"
#include "Core/UserSettings.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/Translation.h"

#include <algorithm>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

#include <d3d12.h>
#include <unordered_map>

namespace KashipanEngine {

namespace {

// 既定配置を変更して既存環境にも一度だけ再適用したい場合は、末尾のバージョンを上げる。
// DockSpaceノードはimgui.iniへ保存されるため、この名前のノードが無い時だけ初期配置を構築できる
constexpr const char *kMainDockSpaceName = "KashipanEngineMainDockSpace.v3";

DXGI_FORMAT ToDxgiFormat_WindowsSwapChain() {
    return DXGI_FORMAT_B8G8R8A8_UNORM;
}

/// @brief 表示言語に応じて、フォントアトラスへ焼き込むグリフ範囲を選ぶ
/// @details 言語ごとに文字コード範囲が大きく異なる（特にハングルは日本語範囲の外）ため、
///          言語を無視して固定範囲を使うと一部言語で文字化け（豆腐文字）になる。
///          未知の言語コード・en-US は、Latin文字とかな漢字を含む安全側の日本語範囲へフォールバックする
///          （これは言語追加前からの既定動作を維持するための措置）
const ImWchar *GetGlyphRangesForLanguage(const std::string &lang) {
    ImGuiIO &io = ImGui::GetIO();
    if (lang == "zh-CN") return io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    if (lang == "ko-KR") return io.Fonts->GetGlyphRangesKorean();
    return io.Fonts->GetGlyphRangesJapanese();
}

/// @brief 言語切り替えコンボボックス用に他言語のlocaleNameから作るグリフ範囲の置き場
/// @details AddFontFromFileTTFへ渡した範囲配列はatlasのBuild()完了まで参照され続けるため、
///          RebuildFontAtlas()のスコープを抜けても生存させる必要がある
std::vector<ImVector<ImWchar>> sMergedGlyphRanges;

/// @brief エディターの初期ドッキング配置を構築する
/// @details DockBuilderは保存済みのユーザー配置を上書きするため、初回起動時（レイアウトバージョン未設定時）
///          に限って呼び出すこと。ウィンドウがまだBeginされていなくても、名前で予約しておけば
///          後から表示された時点で指定ノードへドッキングされる
void BuildDefaultDockLayout(ImGuiID dockSpaceId, const ImVec2 &dockSpaceSize) {
    // DockSpaceOverViewportで作成済みのルート自体は維持し、保存済みの子ノードと
    // ドッキング参照だけを初期化する。これにより現在フレームのホストとの関連を壊さない
    ImGui::DockBuilderRemoveNodeDockedWindows(dockSpaceId);
    ImGui::DockBuilderRemoveNodeChildNodes(dockSpaceId);
    ImGui::DockBuilderSetNodeSize(dockSpaceId, dockSpaceSize);

    ImGuiID centerId = dockSpaceId;
    ImGuiID leftId = 0;
    ImGuiID rightId = 0;
    ImGuiID rightBottomId = 0;
    ImGuiID bottomId = 0;
    ImGuiID topId = 0;

    // 左=階層/履歴、右=インスペクター（下段にプロファイリング）、下=アセット/変数/ログ、
    // 中央上部=シーン編集（ツールバー）、中央=シーンリスト/シーンビュー
    ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Left, 0.15f, &leftId, &centerId);
    ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.22f, &rightId, &centerId);
    ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, 0.20f, &bottomId, &centerId);
    ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Up, 0.10f, &topId, &centerId);
    ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, 0.35f, &rightBottomId, &rightId);

    ImGui::DockBuilderDockWindow(TranslationLabel("editor.sceneobjecthierarchy.window"), leftId);
    ImGui::DockBuilderDockWindow(TranslationLabel("editor.history.window"), leftId);

    ImGui::DockBuilderDockWindow(TranslationLabel("editor.sceneeditor.window"), topId);

    ImGui::DockBuilderDockWindow(TranslationLabel("editor.scenelist.window"), centerId);
    ImGui::DockBuilderDockWindow(TranslationLabel("editor.sceneview.window"), centerId);

    ImGui::DockBuilderDockWindow(TranslationLabel("editor.sceneobjectinspector.window"), rightId);
    ImGui::DockBuilderDockWindow(TranslationLabel("editor.scenecomponentinspector.window"), rightId);
    ImGui::DockBuilderDockWindow(TranslationLabel("editor.gameengine.profiling.window"), rightBottomId);

    ImGui::DockBuilderDockWindow(TranslationLabel("editor.assets.window"), bottomId);
    ImGui::DockBuilderDockWindow(TranslationLabel("editor.scenevariables.window"), bottomId);
    ImGui::DockBuilderDockWindow(TranslationLabel("editor.logger.window"), bottomId);

    ImGui::DockBuilderFinish(dockSpaceId);
}

/// @brief ImGuiStyleの数値項目（角丸・境界線太さ・余白等）へJSONの値を適用する
/// @details キーが存在しない項目はbaseline（引数のstyle自身が持つ値、通常はImGuiStyle()の既定値）を
///          そのまま維持する。ScaleAllSizesを掛ける前の素の値として扱うこと
void ApplyStyleOverrides(ImGuiStyle &style, const JSON &styleJson) {
    if (!styleJson.is_object()) return;
    auto applyFloat = [&](const char *key, float &target) {
        auto it = styleJson.find(key);
        if (it != styleJson.end() && it->is_number()) target = it->get<float>();
    };
    applyFloat("windowRounding", style.WindowRounding);
    applyFloat("childRounding", style.ChildRounding);
    applyFloat("frameRounding", style.FrameRounding);
    applyFloat("popupRounding", style.PopupRounding);
    applyFloat("scrollbarRounding", style.ScrollbarRounding);
    applyFloat("grabRounding", style.GrabRounding);
    applyFloat("tabRounding", style.TabRounding);
    applyFloat("windowBorderSize", style.WindowBorderSize);
    applyFloat("childBorderSize", style.ChildBorderSize);
    applyFloat("frameBorderSize", style.FrameBorderSize);
    applyFloat("popupBorderSize", style.PopupBorderSize);
    applyFloat("tabBarBorderSize", style.TabBarBorderSize);
    applyFloat("windowPaddingX", style.WindowPadding.x);
    applyFloat("windowPaddingY", style.WindowPadding.y);
    applyFloat("framePaddingX", style.FramePadding.x);
    applyFloat("framePaddingY", style.FramePadding.y);
    applyFloat("itemSpacingX", style.ItemSpacing.x);
    applyFloat("itemSpacingY", style.ItemSpacing.y);
    applyFloat("itemInnerSpacingX", style.ItemInnerSpacing.x);
    applyFloat("itemInnerSpacingY", style.ItemInnerSpacing.y);
    applyFloat("indentSpacing", style.IndentSpacing);
    applyFloat("scrollbarSize", style.ScrollbarSize);
    applyFloat("grabMinSize", style.GrabMinSize);
}

// ImGuiの内部テクスチャ（フォントアトラス等）用SRVディスクリプタの割り当て先ヒープ。
// Dear ImGui 1.92以降はフォントのグリフを実際に描画された時点で遅延読み込みするため、
// 未表示だった文字（例: スクロールで初めて見えたログの日本語文字）が新たに描画される度に
// アトラステクスチャが再構築され、複数のテクスチャが同時に生存しうる。
// そのため単一ディスクリプタ（レガシーモード）ではなく、必要な数だけ動的に確保できる
// ようにする必要がある（ImGuiSrvDescriptorAllocFn/FreeFn参照）
SRVHeap *sImGuiSrvHeap = nullptr;
// GPUディスクリプタハンドルのポインタ値をキーに、確保済みディスクリプタを保持する
// （キーで解放対象を特定する。unique_ptrの破棄でDescriptorHeapBaseへ返却される）
std::unordered_map<UINT64, std::unique_ptr<DescriptorHandleInfo>> sImGuiTextureDescriptors;

void ImGuiSrvDescriptorAllocFn(ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE *outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE *outGpuHandle) {
    auto handle = sImGuiSrvHeap->AllocateDescriptorHandle();
    *outCpuHandle = handle->cpuHandle;
    *outGpuHandle = handle->gpuHandle;
    sImGuiTextureDescriptors.emplace(handle->gpuHandle.ptr, std::move(handle));
}

void ImGuiSrvDescriptorFreeFn(ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) {
    sImGuiTextureDescriptors.erase(gpuHandle.ptr);
}

HWND PlatformHwndFromViewport(ImGuiViewport* vp) {
    if (!vp) return nullptr;
    return (HWND)vp->PlatformHandleRaw;
}

} // namespace

ImGuiManager::ImGuiManager(Passkey<GameEngine>, WindowsAPI* windowsAPI, DirectXCommon* directXCommon)
    : windowsAPI_(windowsAPI), directXCommon_(directXCommon) {
    InitializeInternal();
}

ImGuiManager::~ImGuiManager() {
    ShutdownInternal();
}

void ImGuiManager::InitializeInternal() {
    if (isInitialized_) return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // 専用のImGui Windowをメインビューポートとして維持しながら、ドッキングを外した
    // ImGuiウィンドウを独立したWindowsウィンドウとして表示できるようにする
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    // io.ConfigViewportsNoDecoration = false; でOSネイティブ装飾（タイトルバー・最小化/最大化）を
    // 有効化できるが、ImGui自身が描くタイトルバーと二重表示になる上、OS側のタイトルバーを
    // ドラッグ/リサイズするとWindowsの内部モーダルループにより単一スレッドのメッセージポンプ
    // （Window::ProcessMessage）がブロックされ描画が止まるため、既定（true）のまま使用しない

    // imgui.iniの既定パス（io.IniFilename）は未指定だとカレントディレクトリからの相対パス
    // "imgui.ini"になる。Visual Studioのデバッガー経由で起動した場合（既定のWorking Directoryは
    // プロジェクトディレクトリ）と、実行ファイルを直接（またはハブ経由で）起動した場合とで
    // カレントディレクトリが異なるため、保存先がずれてドッキングレイアウトが復元されなくなる。
    // 起動方法によらず同じ場所を指すよう、他の個人設定（UserSettings.json等）と同じ
    // EngineRoot直下への絶対パスを明示する（ポインタが有効であり続けるようstaticで保持する）
    static std::string sIniFilePath = ProjectPaths::InEngineRoot("imgui.ini");
    io.IniFilename = sIniFilePath.c_str();

    // マルチビューポート有効時も、従来どおりこのウィンドウをメインのドッキング先として使う
    Window::CreateNormal(
        "ImGui Window",
        1280, 720
    );

    // エディターUI設定（UserSettings）を初期適用する（フォント・スケール・配色）
    // これらはプロジェクトごとではなく全プロジェクト共有の個人設定として扱う。
    // ImGuiManagerがUserSettingsを直接読みに行く（SceneEditor側からの受け渡しはしない）
    // 表示言語はKashipanEngine::Execute()がUserSettingsの保存値をこの時点より前に
    // 適用済み（未設定ならOS既定のまま）。ここでは現在値をそのまま基準として記録するだけでよい
    appliedLanguage_ = GetCurrentLanguage();
    appliedFontPath_ = UserSettings::GetString("editorUI.fontPath", "");
    RebuildFontAtlas(appliedFontPath_);
    appliedUIScale_ = UserSettings::GetFloat("editorUI.uiScale", 1.0f);
    appliedColors_ = UserSettings::GetJSON("editorUI.colors", JSON());
    appliedStyle_ = UserSettings::GetJSON("editorUI.style", JSON::object());
    ReapplyStyle(appliedUIScale_, appliedColors_, appliedStyle_);
    io.FontGlobalScale = std::max(0.1f, UserSettings::GetFloat("editorUI.fontScale", 1.0f));

    isInitialized_ = true;
}

void ImGuiManager::RebuildFontAtlas(const std::string &fontPath) {
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    io.FontDefault = nullptr;
    // AddFontFromFileTTFへ渡すグリフ範囲はBuild()が実行されるまで参照され続けるため、
    // 関数を抜けた後も生存させる必要がある（staticでatlasのClear()を跨いで使い回す）
    sMergedGlyphRanges.clear();

    // フォントの大きさをDPIに基づいて設定
    auto dpi = GetDpiForSystem();
    const float fontSizeDefault = 16.0f;
    const float fontSize = fontSizeDefault * (static_cast<float>(dpi) / 96.0f);

    const std::string currentLanguage = GetCurrentLanguage();

    // 空文字の場合は現在の言語のデフォルトフォントを使う。
    // フォントの指定は全プロジェクト共有だが値は論理パスのため、開いているプロジェクト基準で解決する
    // （他プロジェクトにしか無いフォントを指していた場合は下のフォールバックが効く）
    const std::string resolvedPath = ProjectPaths::ToPhysical(
        fontPath.empty() ? GetCurrentLanguageFontPath() : fontPath);
    ImFont *font = nullptr;
    if (!resolvedPath.empty()) {
        font = io.Fonts->AddFontFromFileTTF(
            resolvedPath.c_str(), fontSize, nullptr, GetGlyphRangesForLanguage(currentLanguage));
    }
    if (!font) {
        // 指定フォントの読み込みに失敗した場合はImGui組み込みのデフォルトフォントにフォールバックする
        font = io.Fonts->AddFontDefault();
    }
    io.FontDefault = font;

    // 表示言語切り替え用コンボボックス（EditorPreferences）は、切り替え前の言語のフォントのまま
    // 全言語のlocaleName（「日本語」「简体中文」「한국어」等）を並べて表示する。
    // 現在のフォントには他言語の文字が含まれていないため、各言語のlocaleNameで使われている
    // 文字だけをその言語のフォントファイルから少量マージしておき、「？」化を防ぐ
    if (font) {
        for (const auto &lang : GetLoadedLanguages()) {
            if (lang == currentLanguage) continue;
            const std::string otherResolvedPath = ProjectPaths::ToPhysical(GetLanguageFontPath(lang));
            if (otherResolvedPath.empty() || otherResolvedPath == resolvedPath) continue;

            ImFontGlyphRangesBuilder builder;
            builder.AddText(GetLanguageDisplayName(lang).c_str());
            sMergedGlyphRanges.emplace_back();
            builder.BuildRanges(&sMergedGlyphRanges.back());
            if (sMergedGlyphRanges.back().empty()) continue;

            ImFontConfig mergeConfig;
            mergeConfig.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(
                otherResolvedPath.c_str(), fontSize, &mergeConfig, sMergedGlyphRanges.back().Data);
        }
    }
}

void ImGuiManager::ReapplyStyle(float uiScale, const JSON &colorsJson, const JSON &styleJson) {
    ImGuiStyle &style = ImGui::GetStyle();
    // サイズ・配色を素の状態（スケール1.0相当）へ戻してから組み立て直す。
    // ScaleAllSizesは呼ぶたびに現在値へ倍率をかける累積処理のため、都度リセットしないと
    // スケールを変更するたびに値がどんどん膨れ上がってしまう
    style = ImGuiStyle();

    if (colorsJson.is_array() && colorsJson.size() == ImGuiCol_COUNT) {
        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            const auto &c = colorsJson[i];
            if (c.is_array() && c.size() == 4) {
                style.Colors[i] = ImVec4(c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>());
            }
        }
    } else {
        ImGui::StyleColorsDark(&style);
    }

    // 角丸・境界線太さ・余白等の詳細設定（未指定の項目はImGuiStyle()の既定値のまま）
    ApplyStyleOverrides(style, styleJson);

    const ImGuiIO &io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    style.ScaleAllSizes(std::max(0.1f, uiScale));
}

void ImGuiManager::ApplyEditorPreferencesIfChanged() {
    // 文字サイズ（FontGlobalScale）はアトラスの作り直しが不要なため、変更検知なしで毎フレーム同期する
    ImGuiIO &io = ImGui::GetIO();
    io.FontGlobalScale = std::max(0.1f, UserSettings::GetFloat("editorUI.fontScale", 1.0f));

    // フォント差し替えはアトラスの作り直しが必要なため、パスまたは表示言語が変わった場合のみ行う。
    // fontPathが空（言語既定フォントを使用中）の場合、表示言語の切り替えだけではfontPath自体は
    // 変化しないため、言語も別途比較しないとフォントが切り替わらない
    const std::string fontPath = UserSettings::GetString("editorUI.fontPath", "");
    const std::string currentLanguage = GetCurrentLanguage();
    if (fontPath != appliedFontPath_ || currentLanguage != appliedLanguage_) {
        RebuildFontAtlas(fontPath);
        appliedFontPath_ = fontPath;
        appliedLanguage_ = currentLanguage;
    }

    // 全体スケール・配色・詳細スタイルは変化した場合のみ再構築する
    const float uiScale = UserSettings::GetFloat("editorUI.uiScale", 1.0f);
    const JSON colors = UserSettings::GetJSON("editorUI.colors", JSON());
    const JSON styleOverrides = UserSettings::GetJSON("editorUI.style", JSON::object());
    if (uiScale != appliedUIScale_ || colors != appliedColors_ || styleOverrides != appliedStyle_) {
        ReapplyStyle(uiScale, colors, styleOverrides);
        appliedUIScale_ = uiScale;
        appliedColors_ = colors;
        appliedStyle_ = styleOverrides;
    }
}

void ImGuiManager::ResetDockLayoutToDefault() {
    const ImGuiID dockSpaceId = ImHashStr(kMainDockSpaceName);
    if (ImGui::DockBuilderGetNode(dockSpaceId) != nullptr) {
        ImGui::DockBuilderRemoveNode(dockSpaceId);
    }
}

void ImGuiManager::RequestLoadIniSettings(std::string iniText) {
    sPendingIniSettings_ = std::move(iniText);
    sHasPendingIniSettings_ = true;
}

void ImGuiManager::ShutdownInternal() {
    if (!isInitialized_) return;

    if (isBackendInitialized_) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        isBackendInitialized_ = false;
    }

    ImGui::DestroyContext();
    // ImGui_ImplDX12_Shutdown() の中でImGuiが保持する全テクスチャの解放（SrvDescriptorFreeFn呼び出し）が
    // 行われるため通常は空になっているはずだが、念のため残りがあればここでヒープへ返却しておく
    sImGuiTextureDescriptors.clear();
    sImGuiSrvHeap = nullptr;

    SetMainHwnd(nullptr);
    isInitialized_ = false;
}

void ImGuiManager::BeginFrame(Passkey<GameEngine>) {
    if (!isInitialized_) return;

    // エディターUI設定の変更を毎フレーム確認して反映する（フォントアトラスの作り直しを伴いうるため、
    // NewFrame()より前に行う）
    ApplyEditorPreferencesIfChanged();

    // バックエンド初期化はウィンドウ生成後に行う
    if (!isBackendInitialized_) {
        Window *window = Window::GetWindow("ImGui Window");
        if (!window) return;
        SetMainHwnd(window->GetWindowHandle());
        if (!mainHwnd_) return;

        if (!ImGui_ImplWin32_Init(mainHwnd_)) {
            return;
        }

        auto* device = directXCommon_->GetDeviceForImGui({});
        auto* srvHeap = directXCommon_->GetSRVHeapForImGui({});
        if (!device || !srvHeap) {
            return;
        }
        sImGuiSrvHeap = srvHeap;

        ImGui_ImplDX12_InitInfo info{};
        info.Device = device;
        info.CommandQueue = directXCommon_->GetCommandQueueForImGui({});
        info.NumFramesInFlight = 2;
        info.RTVFormat = ToDxgiFormat_WindowsSwapChain();
        info.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        info.SrvDescriptorHeap = srvHeap->GetDescriptorHeap();
        // フォントアトラスの遅延グリフ読み込みで複数テクスチャが同時に生存しうるため、
        // 単一ディスクリプタのレガシーモードではなく、共有SRVヒープから動的に確保する
        info.SrvDescriptorAllocFn = &ImGuiSrvDescriptorAllocFn;
        info.SrvDescriptorFreeFn = &ImGuiSrvDescriptorFreeFn;

        if (!ImGui_ImplDX12_Init(&info)) {
            return;
        }

        isBackendInitialized_ = true;
    }

    // レイアウトプリセットの適用予約があれば、NewFrame()より前のこのタイミングで読み込む。
    // フレーム途中（多数のウィンドウが既にBeginされた後）に読み込むと、その時点より後に
    // 描画されるウィンドウだけドッキングが解除されてしまうため、必ず次フレームの先頭で行う
    if (sHasPendingIniSettings_) {
        ImGui::LoadIniSettingsFromMemory(sPendingIniSettings_.c_str(), sPendingIniSettings_.size());
        sPendingIniSettings_.clear();
        sHasPendingIniSettings_ = false;
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    // マルチビューポートの有無にかかわらず、専用のImGui Window全体をメインDockSpaceにする
    ImGuiViewport *mainViewport = ImGui::GetMainViewport();
    const ImGuiID dockSpaceId = ImHashStr(kMainDockSpaceName);
    // NewFrameでimgui.iniは読み込み済み。このIDのノードが無い場合だけを初回起動として扱う
    const bool shouldBuildDefaultDockLayout = ImGui::DockBuilderGetNode(dockSpaceId) == nullptr;
    ImGui::DockSpaceOverViewport(
        dockSpaceId,
        mainViewport,
        ImGuiDockNodeFlags_PassthruCentralNode);

    if (shouldBuildDefaultDockLayout && mainViewport) {
        const ImVec2 dockSpaceSize = mainViewport->WorkSize.x > 0.0f && mainViewport->WorkSize.y > 0.0f
            ? mainViewport->WorkSize
            : mainViewport->Size;
        BuildDefaultDockLayout(dockSpaceId, dockSpaceSize);
    }
}

void ImGuiManager::Render(Passkey<GameEngine>) {
    if (!isInitialized_) return;
    if (!isBackendInitialized_) return;

    ImGui::Render();

    // メイン viewport（= 現在のエンジンの描画ターゲット）に対してのみ描画する
    {
        ImGuiViewport* mainVp = ImGui::GetMainViewport();
        if (mainVp) {
            HWND hwnd = PlatformHwndFromViewport(mainVp);
            if (hwnd) {
                // mainHwnd_ が未解決の場合は補完
                if (!mainHwnd_) SetMainHwnd(hwnd);

                if (auto* cmd = directXCommon_->GetRecordedCommandListForImGui({}, hwnd)) {
                    Window::GetWindow(mainHwnd_)->BeginDraw();
                    ImGui_ImplDX12_RenderDrawData(mainVp->DrawData, cmd);
                }
            }
        }
    }

    // マルチビューポートは ImGui backend に任せる
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

} // namespace KashipanEngine

#endif // defined(USE_IMGUI)
