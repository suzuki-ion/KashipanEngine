#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Utilities/Passkeys.h"
#include "Assets/TextureManager.h"

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;

/// @brief フォント（TTF/OTF）管理クラス
/// @details TextureManager/MaterialManagerと同じ構築パターン（Passkey<GameEngine>限定コンストラクタ内で
///          Assetsフォルダを走査してロード、以後は静的メソッド群として公開）を踏襲する。
///          グリフはSDF（Signed Distance Field）としてフォントごとに1枚のアトラステクスチャへ
///          遅延ベイクされる（初めて要求されたコードポイントのみビットマップ化）。
///          SDFはベイク解像度に依存せず任意サイズへ拡大縮小できるため、フォントサイズ別に
///          アトラスを作り直す必要はない。
class FontManager final {
public:
    using FontHandle = std::uint32_t;
    static constexpr FontHandle kInvalidHandle = 0;

    /// @brief SDFをベイクする基準ピクセル高さ（GlyphInfoのwidth/height/xoff/yoff/advanceはこの高さ基準の値になる。
    ///        SDFは解像度非依存で拡大縮小できるため、実際の表示サイズはこの値との比率で呼び出し側がスケールする）
    static constexpr float kBakePixelHeight = 64.0f;
    /// @brief SDFの縁からのパディング（テクセル単位）。アンチエイリアス幅の算出に使う
    static constexpr float kSdfPixelRange = 6.0f;

    /// @brief 1グリフ分の情報（アトラス上のUV矩形とレイアウト用メトリクス）
    struct GlyphInfo {
        /// @brief アトラス上のUV矩形（正規化座標。x0,y0が左上 / x1,y1が右下）
        float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
        /// @brief ベイクピクセル単位でのビットマップ幅・高さ
        float width = 0.0f, height = 0.0f;
        /// @brief ベイクピクセル単位での、グリフ原点からビットマップ左上までのオフセット
        float xoff = 0.0f, yoff = 0.0f;
        /// @brief ベイクピクセル単位での送り幅
        float advance = 0.0f;
        bool isValid = false;
    };

    /// @brief 読み込み済みフォント一覧の1エントリ（ImGui選択用）
    struct FontListEntry {
        FontHandle handle = kInvalidHandle;
        std::string fileName;
        std::string assetPath;
        std::string name;
    };

    /// @brief コンストラクタ（GameEngineからのみ生成可能）
    /// @param directXCommon DirectX共通（アトラステクスチャのアップロード用）
    /// @param assetsRootPath Assetsフォルダのルートパス
    FontManager(Passkey<GameEngine>, DirectXCommon *directXCommon, const std::string &assetsRootPath = "Assets");
    ~FontManager();

    FontManager(const FontManager &) = delete;
    FontManager &operator=(const FontManager &) = delete;
    FontManager(FontManager &&) = delete;
    FontManager &operator=(FontManager &&) = delete;

    /// @brief 指定ファイルパスのフォントを読み込む（Assetsルートからの相対 or フルパス）
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

    /// @brief 指定コードポイントのグリフ情報を取得する（未ベイクの場合はこの呼び出しでベイクする）
    /// @details フォントハンドルが無効な場合のみ nullptr を返す。フォントにその文字が存在しない場合は
    ///          非nullptrだが GlyphInfo::isValid が false のポインタを返す（呼び出し側は isValid を確認すること）
    static const GlyphInfo *GetOrBakeGlyph(FontHandle handle, char32_t codepoint);

    /// @brief 下線・取り消し線の装飾矩形描画に使う「常に塗りつぶされたSDF値を返す」合成グリフを取得する
    static const GlyphInfo *GetSolidGlyph(FontHandle handle);

    /// @brief フォントのグリフアトラステクスチャのハンドルを取得する（TextureManager経由で通常のテクスチャとしてバインド可能）
    static TextureManager::TextureHandle GetAtlasTextureHandle(FontHandle handle);

    /// @brief 指定のピクセル高さで表示する際のフォント単位からピクセル単位への倍率を取得する
    static float GetScaleForPixelHeight(FontHandle handle, float pixelHeight);
    /// @brief 指定のピクセル高さで表示する際の行送り幅（アセント-ディセント+行間）を取得する
    static float GetLineHeight(FontHandle handle, float pixelHeight);
    /// @brief 指定のピクセル高さで表示する際のアセント（ベースラインから上端まで）を取得する
    static float GetAscent(FontHandle handle, float pixelHeight);
    /// @brief 指定のピクセル高さで表示する際のディセント（ベースラインから下端までの負値）を取得する
    static float GetDescent(FontHandle handle, float pixelHeight);

#if defined(USE_IMGUI)
    /// @brief デバッグ用: 読み込まれたフォント一覧のImGuiウィンドウを描画
    static void ShowImGuiLoadedFontsWindow();
#endif

    const std::string &GetAssetsRootPath() const noexcept { return assetsRootPath_; }

private:
    void LoadAllFromAssetsFolder();
    /// @brief 指定フォントのアトラスCPUバッファ全体をGPUテクスチャへ再アップロードする
    /// @details 新しいグリフをベイクしてアトラスの内容が変わるたびに呼ばれる
    static void UploadAtlasToGpu(FontHandle handle);

    DirectXCommon *directXCommon_ = nullptr;
    std::string assetsRootPath_;
};

} // namespace KashipanEngine
