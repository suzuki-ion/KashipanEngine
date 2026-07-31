#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <string>

#include "Objects/ObjectComponentHeader.h"
#include "Core/ProjectPaths.h"
#include "Graphics/ScreenBuffer.h"
#include "Scene/RenderTargetCarryOverRegistry.h"

namespace KashipanEngine {

class ScreenBufferObject final : public IObjectComponent {
public:
    // 直接書き込み時は、セッターと同様に生成済みバッファへの反映（名前変更・リサイズ）も行う
    OBJECT_COMPONENT_CONSTRUCTOR(ScreenBufferObject, 0xFF,
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(name_, [this] { SetName(name_); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(width_, [this] { SetSize(width_, height_); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(height_, [this] { SetSize(width_, height_); });
    )
    COMPONENT_CATEGORY("Render", "RenderTarget")
        ~ScreenBufferObject() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<ScreenBufferObject>();
        ptr->name_ = name_;
        ptr->width_ = width_;
        ptr->height_ = height_;
        ptr->isShowViewer_ = isShowViewer_;
        ptr->saveDirectory_ = saveDirectory_;
        ptr->saveFileNamePrefix_ = saveFileNamePrefix_;
        ptr->saveFormat_ = saveFormat_;
        return ptr;
    }

    ScreenBuffer *GetScreenBuffer() const noexcept { return buffer_; }
    /// @brief 管理用の名前を設定（TextureManagerへの登録名になる）
    void SetName(const std::string &name) {
        name_ = name;
        if (buffer_ && ScreenBuffer::IsExist(buffer_)) {
            buffer_->SetRenderTargetName(name_);
        }
    }
    const std::string &GetName() const noexcept { return name_; }
    /// @brief バッファサイズを変更する（既存のバッファがあれば実際にリサイズされる）
    void SetSize(std::uint32_t width, std::uint32_t height) {
        width_ = width;
        height_ = height;
        if (buffer_ && ScreenBuffer::IsExist(buffer_)) {
            buffer_->Resize(width_, height_);
        }
    }

    //==================================================
    // 画像ファイル保存
    //==================================================

    /// @brief 保存先ディレクトリを設定（次回以降のRequestSave()の自動生成パスに使われる）
    void SetSaveDirectory(const std::string &directory) { saveDirectory_ = directory; }
    const std::string &GetSaveDirectory() const noexcept { return saveDirectory_; }
    /// @brief 保存ファイル名の接頭辞を設定
    void SetSaveFileNamePrefix(const std::string &prefix) { saveFileNamePrefix_ = prefix; }
    const std::string &GetSaveFileNamePrefix() const noexcept { return saveFileNamePrefix_; }
    /// @brief 保存形式を設定（"png" / "jpg" / "bmp"）
    void SetSaveFormat(const std::string &format) { saveFormat_ = format; }
    const std::string &GetSaveFormat() const noexcept { return saveFormat_; }

    /// @brief 直近で確定した描画結果を画像ファイルへ保存する（エディター・アプリケーション・
    ///        スクリプトのいずれからも呼び出せる即時実行API。設定はJSONへ永続化されるが、
    ///        この呼び出し自体はトリガーであり状態としては保持しない）
    /// @param filePath 保存先パス（空の場合はSaveDirectory/SaveFileNamePrefix+タイムスタンプ+
    ///        SaveFormatから自動生成する）
    /// @return 成功した場合は true
    bool RequestSave(const std::string &filePath = "") {
        if (!buffer_ || !ScreenBuffer::IsExist(buffer_)) return false;
        return buffer_->SaveToFile(filePath.empty() ? BuildAutoSavePath() : filePath);
    }

protected:
    void Initialize() override {
        if (buffer_) return;
        buffer_ = ScreenBuffer::Create(width_, height_, name_);
        // 自動生成された名前を保持する（保存時に確定した名前が残るようにする）
        if (buffer_ && name_.empty()) {
            name_ = buffer_->GetRenderTargetName();
        }
    }
    void Finalize() override {
        // シーン切り替え中は即座に破棄せず、次のシーンの同名コンポーネントへの
        // 引き継ぎ候補として預ける（切り替え中でなければ登録側が即座に破棄する）
        if (buffer_ && ScreenBuffer::IsExist(buffer_)) {
            RenderTargetCarryOverRegistry::Deposit(RenderTargetCarryOverRegistry::Kind::ScreenBuffer, name_, buffer_,
                [b = buffer_] { if (ScreenBuffer::IsExist(b)) b->DestroyNotify(); });
        }
        buffer_ = nullptr;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        if (ImGui::InputText("Name", &name_, ImGuiInputTextFlags_EnterReturnsTrue)) {
            SetName(name_);
        }
        int w = static_cast<int>(width_);
        int h = static_cast<int>(height_);
        bool sizeChanged = false;
        if (ImGuiCustom::EditValue("Width", w)) { w = std::max(1, w); sizeChanged = true; }
        if (ImGuiCustom::EditValue("Height", h)) { h = std::max(1, h); sizeChanged = true; }
        if (sizeChanged) SetSize(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h));

        // 描画内容確認用ビューアウィンドウ
        if (ImGui::Button(isShowViewer_ ? "Close Viewer" : "Open Viewer")) {
            isShowViewer_ = !isShowViewer_;
        }

        if (ImGui::TreeNode("Save to File")) {
            ImGui::InputText("Directory", &saveDirectory_);
            ImGui::InputText("File Name Prefix", &saveFileNamePrefix_);
            static const char *kFormats[] = { "png", "jpg", "bmp" };
            int formatIndex = 0;
            for (int i = 0; i < 3; ++i) {
                if (saveFormat_ == kFormats[i]) { formatIndex = i; break; }
            }
            if (ImGui::Combo("Format", &formatIndex, kFormats, 3)) {
                saveFormat_ = kFormats[formatIndex];
            }
            if (ImGui::Button("Save Screenshot")) {
                if (!RequestSave()) {
                    Log("ScreenBufferObject: failed to save screenshot.", LogSeverity::Warning);
                }
            }
            ImGui::TreePop();
        }
    }

    /// @brief 描画内容確認用のImGuiウィンドウ表示（ポーズ中も表示し続ける）
    void ShowPersistentImGui() override {
        ShowViewerWindow();
    }

    /// @brief 描画内容確認用のImGuiウィンドウ表示
    void ShowViewerWindow() {
        if (!isShowViewer_) return;
        if (!buffer_ || !ScreenBuffer::IsExist(buffer_)) return;

        // タイトルの表示文字列にnameを含めているが、ImGuiはラベル全体をウィンドウIDとして
        // 使うため、名前変更のたびに別ウィンドウ扱いとなりフォーカスが奪われてしまう。
        // "###"以降のみをID化することで、表示名は更新されつつウィンドウの同一性を保つ
        const std::string windowTitle = "ScreenBuffer Viewer: " + name_ +
            "###ScreenBufferViewer" + std::to_string(reinterpret_cast<std::uintptr_t>(this));
        if (ImGui::Begin(windowTitle.c_str(), &isShowViewer_)) {
            ImGui::Text("Size: %ux%u", buffer_->GetWidth(), buffer_->GetHeight());

            // GetSrvHandle()はこの呼び出し時点の読み取り面を返すが、このImGui表示は
            // Renderer::RenderFrame（今フレームのポストエフェクト適用）より前に呼ばれるため、
            // 適用したポストエフェクトの数によっては適用途中の中間結果を指してしまうことがある。
            // 代わりに、前フレームまでの描画完了時点で確定した正しいSRVを参照する
            const auto srvHandle = buffer_->GetPreviewSrvHandle();
            if (srvHandle.ptr != 0) {
                // アスペクト比を維持して表示領域にフィットさせる
                const ImVec2 avail = ImGui::GetContentRegionAvail();
                const float w = static_cast<float>(buffer_->GetWidth());
                const float h = static_cast<float>(buffer_->GetHeight());
                ImVec2 drawSize = avail;
                if (w > 0.0f && h > 0.0f && avail.x > 0.0f && avail.y > 0.0f) {
                    const float scale = std::min(avail.x / w, avail.y / h);
                    drawSize = ImVec2(w * scale, h * scale);
                }
                ImGui::Image(static_cast<ImTextureID>(srvHandle.ptr), drawSize);
            } else {
                ImGui::TextUnformatted("SRV not ready.");
            }
        }
        ImGui::End();
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["name"] = name_;
        json["width"] = width_;
        json["height"] = height_;
        json["isShowViewer"] = isShowViewer_;
        json["saveDirectory"] = saveDirectory_;
        json["saveFileNamePrefix"] = saveFileNamePrefix_;
        json["saveFormat"] = saveFormat_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        const std::string loadedName = json.value("name", std::string{});
        const std::uint32_t loadedWidth = json.value("width", 1280u);
        const std::uint32_t loadedHeight = json.value("height", 720u);
        isShowViewer_ = json.value("isShowViewer", false);
        saveDirectory_ = json.value("saveDirectory", std::string("Screenshots"));
        saveFileNamePrefix_ = json.value("saveFileNamePrefix", std::string("Screenshot"));
        saveFormat_ = json.value("saveFormat", std::string("png"));

        // シーン切り替え中、前のシーンで同名のScreenBufferObjectが使用していたバッファが
        // 引き継ぎプールに残っていれば、新規作成せずそちらを再利用する
        if (!loadedName.empty()) {
            if (auto *carried = static_cast<ScreenBuffer *>(
                    RenderTargetCarryOverRegistry::Claim(RenderTargetCarryOverRegistry::Kind::ScreenBuffer, loadedName))) {
                if (buffer_ && ScreenBuffer::IsExist(buffer_)) {
                    buffer_->DestroyNotify();
                }
                buffer_ = carried;
                if (buffer_->GetWidth() != loadedWidth || buffer_->GetHeight() != loadedHeight) {
                    buffer_->Resize(loadedWidth, loadedHeight);
                }
                name_ = buffer_->GetRenderTargetName();
                width_ = loadedWidth;
                height_ = loadedHeight;
                return true;
            }
        }

        // AddComponent() の時点で Initialize() が先に呼ばれ、
        // この関数が読み込むより前にデフォルト値（空の名前・既定サイズ）で
        // バッファが作成・登録されてしまっている。
        // ここで単に name_/width_/height_ を上書きするだけだと、
        // 実際に TextureManager へ登録されている名前（自動生成された別名）と
        // 表示上の名前が食い違ったままになり、その名前を参照するマテリアル等が
        // テクスチャを見つけられなくなるため、バッファ側も正しい状態へ同期し直す。
        if (buffer_ && ScreenBuffer::IsExist(buffer_)) {
            if (buffer_->GetWidth() != loadedWidth || buffer_->GetHeight() != loadedHeight) {
                buffer_->DestroyNotify();
                buffer_ = ScreenBuffer::Create(loadedWidth, loadedHeight, loadedName);
                name_ = buffer_ ? buffer_->GetRenderTargetName() : loadedName;
            } else {
                name_ = loadedName;
                if (!name_.empty()) {
                    buffer_->SetRenderTargetName(name_);
                }
                name_ = buffer_->GetRenderTargetName();
            }
        } else {
            name_ = loadedName;
        }
        width_ = loadedWidth;
        height_ = loadedHeight;
        return true;
    }

private:
    /// @brief SaveDirectory/SaveFileNamePrefix/SaveFormatとタイムスタンプから保存先パスを組み立てる
    std::string BuildAutoSavePath() const {
        const auto now = std::chrono::system_clock::now();
        const std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char timestamp[32]{};
        std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &tm);

        // 保存先はプロジェクトルート基準の論理パスで保持しているため、ここで物理パスへ変換する
        std::string directory = ProjectPaths::ToPhysical(saveDirectory_.empty() ? "Screenshots" : saveDirectory_);
        std::string prefix = saveFileNamePrefix_.empty() ? "Screenshot" : saveFileNamePrefix_;
        std::string ext = saveFormat_.empty() ? "png" : saveFormat_;
        return directory + "/" + prefix + "_" + timestamp + "." + ext;
    }

    ScreenBuffer *buffer_ = nullptr;
    std::string name_;
    std::uint32_t width_ = 1280;
    std::uint32_t height_ = 720;
    /// @brief ビューアウィンドウ表示フラグ（シリアライズされ、再起動後も維持される）
    bool isShowViewer_ = false;

    /// @brief RequestSave()の自動生成パスに使う保存先ディレクトリ
    std::string saveDirectory_ = "Screenshots";
    /// @brief RequestSave()の自動生成パスに使うファイル名の接頭辞
    std::string saveFileNamePrefix_ = "Screenshot";
    /// @brief RequestSave()の自動生成パスに使う保存形式（"png" / "jpg" / "bmp"）
    std::string saveFormat_ = "png";
};

REGISTER_COMPONENT_OBJECT(ScreenBufferObject);

} // namespace KashipanEngine
