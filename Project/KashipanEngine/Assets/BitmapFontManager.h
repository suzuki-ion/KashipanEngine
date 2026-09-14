#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Utilities/Passkeys.h"
#include "Assets/TextureManager.h"

namespace KashipanEngine {

class GameEngine;

/// @brief BMFont形式（.fnt、テキスト形式）のビットマップフォント管理クラス
/// @details TextureManager/FontManagerと同じ構築パターン（Passkey<GameEngine>限定コンストラクタ内で
///          Assetsフォルダを走査してロード、以後は静的メソッド群として公開）を踏襲する。
///          FontManager（TTF/OTFをSDFとしてランタイムベイクする方式）とは完全に独立しており、
///          こちらは事前生成済みのビットマップアトラス画像をそのまま使う。
///          1ページ（テクスチャ1枚）構成の.fntのみを対象とし、複数ページの.fntは
///          先頭ページのみを読み込む（警告ログを出す）。
class BitmapFontManager final {
public:
    using FontHandle = std::uint32_t;
    static constexpr FontHandle kInvalidHandle = 0;

    /// @brief 1文字分の情報（アトラス上のUV矩形とレイアウト用メトリクス。単位はすべて.fntの"size"基準のピクセル）
    struct CharInfo {
        /// @brief アトラス上のUV矩形（正規化座標。x0,y0が左上 / x1,y1が右下）
        float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
        /// @brief ビットマップの幅・高さ（ピクセル）
        float width = 0.0f, height = 0.0f;
        /// @brief 描画位置のオフセット（ペン位置から見たビットマップ左上までのオフセット、ピクセル）
        float xOffset = 0.0f, yOffset = 0.0f;
        /// @brief 次の文字までの送り幅（ピクセル）
        float xAdvance = 0.0f;
    };

    /// @brief 読み込み済みフォント一覧の1エントリ（ImGui選択用）
    struct FontListEntry {
        FontHandle handle = kInvalidHandle;
        std::string fileName;
        std::string assetPath;
        std::string name;
    };

    /// @brief コンストラクタ（GameEngineからのみ生成可能）
    /// @param assetsRootPath Assetsフォルダのルートパス
    /// @details ページ画像（アトラス）はTextureManagerが起動時のAssets走査で既に読み込み済みの
    ///          ものをファイル名で検索して使う（TextureManagerを直接保持する必要はない）
    BitmapFontManager(Passkey<GameEngine>, const std::string &assetsRootPath = "Assets");
    ~BitmapFontManager();

    BitmapFontManager(const BitmapFontManager &) = delete;
    BitmapFontManager &operator=(const BitmapFontManager &) = delete;
    BitmapFontManager(BitmapFontManager &&) = delete;
    BitmapFontManager &operator=(BitmapFontManager &&) = delete;

    /// @brief 指定ファイルパスの.fntを読み込む（Assetsルートからの相対 or フルパス）
    /// @return 読み込んだフォントのハンドル（失敗時は kInvalidHandle）
    static FontHandle LoadFont(const std::string &filePath);

    /// @brief ファイル名単体からフォントハンドルを取得
    static FontHandle GetFontHandleFromFileName(const std::string &fileName);
    /// @brief Assetsルートからの相対パスからフォントハンドルを取得
    static FontHandle GetFontHandleFromAssetPath(const std::string &assetPath);
    /// @brief フォント名（既定はファイル名（拡張子無し））からフォントハンドルを取得
    static FontHandle GetFontHandleFromName(const std::string &name);

    /// @brief 読み込み済みフォント一覧を取得
    static std::vector<FontListEntry> GetLoadedFontListEntries();

    /// @brief 指定コードポイントの文字情報を取得する（存在しない場合はnullptr）
    static const CharInfo *GetCharInfo(FontHandle handle, char32_t codepoint);

    /// @brief フォントのアトラステクスチャ（先頭ページ）のハンドルを取得する
    static TextureManager::TextureHandle GetPageTextureHandle(FontHandle handle);

    /// @brief 指定のフォントサイズ（.fntの"size"を基準としたワールド単位の表示高さ）で表示する際の
    ///        フォント単位からピクセル単位への倍率を取得する
    static float GetScaleForFontSize(FontHandle handle, float fontSize);
    /// @brief 指定のフォントサイズで表示する際の行送り幅（.fntの"lineHeight"をスケールした値）を取得する
    static float GetLineHeight(FontHandle handle, float fontSize);
    /// @brief 指定のフォントサイズで表示する際のベースライン位置（上端からの距離。.fntの"base"をスケールした値）を取得する
    static float GetBase(FontHandle handle, float fontSize);

#if defined(USE_IMGUI)
    /// @brief デバッグ用: 読み込まれたビットマップフォント一覧のImGuiウィンドウを描画
    static void ShowImGuiLoadedFontsWindow();
#endif

    const std::string &GetAssetsRootPath() const noexcept { return assetsRootPath_; }

private:
    void LoadAllFromAssetsFolder();

    std::string assetsRootPath_;
};

} // namespace KashipanEngine
