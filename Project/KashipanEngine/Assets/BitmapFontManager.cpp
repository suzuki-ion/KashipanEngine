#include "BitmapFontManager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <unordered_map>

#include "Assets/CaseInsensitive.h"
#include "Debug/Logger.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/Translation.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

namespace {

using FontHandle = BitmapFontManager::FontHandle;
using CharInfo = BitmapFontManager::CharInfo;

/// @brief BMFont(.fnt)1つ分の内部管理データ
struct FontEntry final {
    std::string fullPath;
    std::string assetPath;
    std::string fileName;
    std::string name;

    /// @brief .fntの"info size"（このフォントのメトリクス・アトラス座標の基準となるピクセル高さ）
    float bakeSize = 32.0f;
    float lineHeight = 32.0f;
    float base = 26.0f;

    std::unordered_map<char32_t, CharInfo> chars;

    TextureManager::TextureHandle pageTextureHandle = TextureManager::kInvalidHandle;
};

std::string sAssetsRootPath;
std::unordered_map<FontHandle, FontEntry> sFonts;
FileMap<FontHandle> sFileNameToHandle;
FileMap<FontHandle> sAssetPathToHandle;
FileMap<FontHandle> sNameToHandle;
FontHandle sNextHandle = 1;

std::string NormalizePathSlashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

std::string ToLower(std::string s) {
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool HasSupportedFontExtension(const std::filesystem::path &p) {
    return ToLower(p.extension().string()) == ".fnt";
}

std::string MakeAssetRelativePath(const std::string &assetsRoot, const std::string &fullPath) {
    const std::filesystem::path root = Utf8StringToPath(assetsRoot);
    const std::filesystem::path full = Utf8StringToPath(fullPath);

    std::error_code ec;
    auto rel = std::filesystem::relative(full, root, ec);
    if (ec) {
        return NormalizePathSlashes(PathToUtf8String(full.filename()));
    }
    return NormalizePathSlashes(PathToUtf8String(rel));
}

/// @brief 文字列の両端にあるダブルクォーテーションを取り除く
std::string StripQuotes(const std::string &input) {
    if (input.size() >= 2 && input.front() == '"' && input.back() == '"') {
        return input.substr(1, input.size() - 2);
    }
    return input;
}

/// @brief BMFont(.fnt)の1行を空白区切りの"key=value"トークン列として走査する
template <typename Callback>
void ForEachKeyValue(std::istringstream &iss, Callback &&callback) {
    std::string token;
    while (iss >> token) {
        const size_t pos = token.find('=');
        if (pos == std::string::npos) continue;
        callback(token.substr(0, pos), token.substr(pos + 1));
    }
}

} // namespace

BitmapFontManager::BitmapFontManager(Passkey<GameEngine>, const std::string &assetsRootPath)
    : assetsRootPath_(NormalizePathSlashes(assetsRootPath)) {
    LogScope scope;
    sAssetsRootPath = assetsRootPath_;
    sFonts.clear();
    sFileNameToHandle.clear();
    sAssetPathToHandle.clear();
    sNameToHandle.clear();
    sNextHandle = 1;
    LoadAllFromAssetsFolder();
}

BitmapFontManager::~BitmapFontManager() {
    LogScope scope;
    sFonts.clear();
    sFileNameToHandle.clear();
    sAssetPathToHandle.clear();
    sNameToHandle.clear();
}

void BitmapFontManager::LoadAllFromAssetsFolder() {
    LogScope scope;
    const auto dir = GetDirectoryData(sAssetsRootPath, true, true);
    const auto filtered = GetDirectoryDataByExtension(dir, { ".fnt" });

    std::vector<std::string> files;
    std::function<void(const DirectoryData &)> flatten = [&](const DirectoryData &d) {
        for (const auto &f : d.files) files.push_back(f);
        for (const auto &sd : d.subdirectories) flatten(sd);
    };
    flatten(filtered);

    for (const auto &f : files) {
        LoadFont(f);
    }
}

BitmapFontManager::FontHandle BitmapFontManager::LoadFont(const std::string &filePath) {
    LogScope scope;
    if (filePath.empty()) return kInvalidHandle;

    const std::filesystem::path p = Utf8StringToPath(filePath);
    if (!std::filesystem::exists(p)) {
        Log(Translation("engine.bitmapfont.loading.failed.notfound") + PathToUtf8String(p), LogSeverity::Warning);
        return kInvalidHandle;
    }
    if (!HasSupportedFontExtension(p)) {
        Log(Translation("engine.bitmapfont.loading.failed.unsupported") + PathToUtf8String(p), LogSeverity::Warning);
        return kInvalidHandle;
    }

    const std::string full = NormalizePathSlashes(PathToUtf8String(p));
    const std::string asset = MakeAssetRelativePath(sAssetsRootPath, full);

    // 既に読み込み済みかチェック
    {
        auto it = sAssetPathToHandle.find(asset);
        if (it != sAssetPathToHandle.end()) return it->second;
    }

    std::ifstream file(p);
    if (!file.is_open()) {
        Log(Translation("engine.bitmapfont.loading.failed.read") + PathToUtf8String(p), LogSeverity::Warning);
        return kInvalidHandle;
    }

    FontEntry entry;
    entry.fullPath = full;
    entry.assetPath = asset;
    entry.fileName = PathToUtf8String(p.filename());
    entry.name = PathToUtf8String(p.stem());

    int pageCount = 0;
    std::string pageFileName;
    float scaleW = 256.0f, scaleH = 256.0f;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string identifier;
        iss >> identifier;

        if (identifier == "info") {
            ForEachKeyValue(iss, [&](const std::string &key, const std::string &value) {
                if (key == "size") {
                    // BMFontの"size"は負値（TrueType由来の負のセルハイトを反映したもの）を
                    // 取ることがあるため、常に正の基準ピクセル高さとして扱う
                    entry.bakeSize = std::abs(std::stof(value));
                }
            });
        } else if (identifier == "common") {
            ForEachKeyValue(iss, [&](const std::string &key, const std::string &value) {
                if (key == "lineHeight") entry.lineHeight = std::stof(value);
                else if (key == "base") entry.base = std::stof(value);
                else if (key == "scaleW") scaleW = std::stof(value);
                else if (key == "scaleH") scaleH = std::stof(value);
                else if (key == "pages") pageCount = std::stoi(value);
            });
        } else if (identifier == "page") {
            int id = 0;
            std::string file2;
            ForEachKeyValue(iss, [&](const std::string &key, const std::string &value) {
                if (key == "id") id = std::stoi(value);
                else if (key == "file") file2 = StripQuotes(value);
            });
            // 先頭ページ（id==0、または最初に出現したpage行）のみを使う
            if (pageFileName.empty() && (id == 0)) pageFileName = file2;
        } else if (identifier == "char") {
            CharInfo charInfo{};
            int id = -1;
            int page = 0;
            ForEachKeyValue(iss, [&](const std::string &key, const std::string &value) {
                if (key == "id") id = std::stoi(value);
                else if (key == "x") charInfo.u0 = std::stof(value);
                else if (key == "y") charInfo.v0 = std::stof(value);
                else if (key == "width") charInfo.width = std::stof(value);
                else if (key == "height") charInfo.height = std::stof(value);
                else if (key == "xoffset") charInfo.xOffset = std::stof(value);
                else if (key == "yoffset") charInfo.yOffset = std::stof(value);
                else if (key == "xadvance") charInfo.xAdvance = std::stof(value);
                else if (key == "page") page = std::stoi(value);
            });
            // 複数ページの.fntは先頭ページ(0)のみをサポートするため、他ページの文字は読み飛ばす
            if (id >= 0 && page == 0) {
                entry.chars.emplace(static_cast<char32_t>(id), charInfo);
            }
        }
    }
    file.close();

    if (pageCount > 1) {
        Log(Translation("engine.bitmapfont.loading.warning.multipage") + PathToUtf8String(p), LogSeverity::Warning);
    }

    if (pageFileName.empty()) {
        Log(Translation("engine.bitmapfont.loading.failed.nopage") + PathToUtf8String(p), LogSeverity::Error);
        return kInvalidHandle;
    }

    // ページ画像はTextureManagerが起動時のAssets走査で既に読み込み済みである前提で、
    // ファイル名から検索する（TextureManager::LoadTextureは事前に走査済みのファイルの
    // ミップチェインを引くだけで、未知のパスを渡しても新規デコードはしてくれないため、
    // ここでLoadTextureを直接呼んではいけない）。ページ画像は.fntと同じディレクトリに
    // 置く必要はなく、Assetsフォルダ内のどこかにあればよい
    const std::string pageFileNameOnly = PathToUtf8String(Utf8StringToPath(pageFileName).filename());
    entry.pageTextureHandle = TextureManager::GetTextureFromFileName(pageFileNameOnly);
    if (entry.pageTextureHandle == TextureManager::kInvalidHandle) {
        Log(Translation("engine.bitmapfont.loading.failed.pagenotfound") + pageFileNameOnly, LogSeverity::Error);
        return kInvalidHandle;
    }

    // xy/width/heightをアトラスサイズで正規化し、u0,v0,u1,v1として確定させる
    // （xOffset/yOffset/xAdvance/width/heightはピクセル単位のまま保持する）
    if (scaleW <= 0.0f) scaleW = 1.0f;
    if (scaleH <= 0.0f) scaleH = 1.0f;
    for (auto &[codepoint, charInfo] : entry.chars) {
        const float px = charInfo.u0;
        const float py = charInfo.v0;
        charInfo.u0 = px / scaleW;
        charInfo.v0 = py / scaleH;
        charInfo.u1 = (px + charInfo.width) / scaleW;
        charInfo.v1 = (py + charInfo.height) / scaleH;
    }

    const FontHandle handle = sNextHandle++;
    sFileNameToHandle[entry.fileName] = handle;
    sAssetPathToHandle[entry.assetPath] = handle;
    sNameToHandle[entry.name] = handle;
    sFonts.emplace(handle, std::move(entry));

    Log(Translation("engine.bitmapfont.loading.succeeded") + PathToUtf8String(p), LogSeverity::Info);
    return handle;
}

BitmapFontManager::FontHandle BitmapFontManager::GetFontHandleFromFileName(const std::string &fileName) {
    LogScope scope;
    auto it = sFileNameToHandle.find(fileName);
    return it != sFileNameToHandle.end() ? it->second : kInvalidHandle;
}

BitmapFontManager::FontHandle BitmapFontManager::GetFontHandleFromAssetPath(const std::string &assetPath) {
    LogScope scope;
    auto it = sAssetPathToHandle.find(NormalizePathSlashes(assetPath));
    return it != sAssetPathToHandle.end() ? it->second : kInvalidHandle;
}

BitmapFontManager::FontHandle BitmapFontManager::GetFontHandleFromName(const std::string &name) {
    LogScope scope;
    auto it = sNameToHandle.find(name);
    return it != sNameToHandle.end() ? it->second : kInvalidHandle;
}

std::vector<BitmapFontManager::FontListEntry> BitmapFontManager::GetLoadedFontListEntries() {
    LogScope scope;
    std::vector<FontListEntry> result;
    result.reserve(sFonts.size());
    for (const auto &[handle, entry] : sFonts) {
        result.push_back(FontListEntry{ handle, entry.fileName, entry.assetPath, entry.name });
    }
    return result;
}

const BitmapFontManager::CharInfo *BitmapFontManager::GetCharInfo(FontHandle handle, char32_t codepoint) {
    LogScope scope;
    auto it = sFonts.find(handle);
    if (it == sFonts.end()) return nullptr;
    auto charIt = it->second.chars.find(codepoint);
    return charIt != it->second.chars.end() ? &charIt->second : nullptr;
}

TextureManager::TextureHandle BitmapFontManager::GetPageTextureHandle(FontHandle handle) {
    LogScope scope;
    auto it = sFonts.find(handle);
    return it != sFonts.end() ? it->second.pageTextureHandle : TextureManager::kInvalidHandle;
}

float BitmapFontManager::GetScaleForFontSize(FontHandle handle, float fontSize) {
    LogScope scope;
    auto it = sFonts.find(handle);
    if (it == sFonts.end() || it->second.bakeSize <= 0.0f) return 1.0f;
    return fontSize / it->second.bakeSize;
}

float BitmapFontManager::GetLineHeight(FontHandle handle, float fontSize) {
    LogScope scope;
    auto it = sFonts.find(handle);
    if (it == sFonts.end()) return fontSize;
    return it->second.lineHeight * GetScaleForFontSize(handle, fontSize);
}

float BitmapFontManager::GetBase(FontHandle handle, float fontSize) {
    LogScope scope;
    auto it = sFonts.find(handle);
    if (it == sFonts.end()) return fontSize;
    return it->second.base * GetScaleForFontSize(handle, fontSize);
}

#if defined(USE_IMGUI)
void BitmapFontManager::ShowImGuiLoadedFontsWindow() {
    if (!ImGui::Begin(TranslationLabel("editor.bitmapfontmanager.window"))) {
        ImGui::End();
        return;
    }
    if (ImGui::BeginTable("BitmapFontsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("File");
        ImGui::TableSetupColumn("Chars");
        ImGui::TableHeadersRow();
        for (const auto &[handle, entry] : sFonts) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(entry.assetPath.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text(TranslationC("editor.fontmanager.desc_1"), entry.chars.size());
        }
        ImGui::EndTable();
    }
    ImGui::End();
}
#endif

} // namespace KashipanEngine
