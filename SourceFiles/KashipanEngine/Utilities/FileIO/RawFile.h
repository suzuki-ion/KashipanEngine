#pragma once
#include <string>
#include <vector>

namespace KashipanEngine {

/// @brief ファイルの種類の列挙型
enum class FileType {
    // テキスト系
    txt,    ///< プレーンテキスト (UTF-8 を前提)
    json,   ///< JSON テキスト
    xml,    ///< XML テキスト
    html,   ///< HTML テキスト

    // 画像系
    png,    ///< PNG画像ファイル
    jpg,    ///< JPEG画像ファイル
    gif,    ///< GIF画像ファイル
    bmp,    ///< BMP画像ファイル
    ico,    ///< ICO 画像ファイル
    webp,   ///< WebP 画像ファイル
    tiff,   ///< TIFF 画像ファイル

    // 音声・動画・メディア系
    wav,    ///< WAV音声ファイル
    mp3,    ///< MP3音声ファイル
    ogg,    ///< Ogg コンテナ（Vorbis/Opus 等）
    flac,   ///< FLAC 音声ファイル
    mp4,    ///< MP4/ISO-BMFF ファイル
    avi,    ///< AVI 動画ファイル
    mkv,    ///< Matroska/WebM コンテナ

    // アーカイブ/ドキュメント/その他
    pdf,    ///< PDF ドキュメント
    zip,    ///< ZIP アーカイブ
    rar,    ///< RAR アーカイブ
    sevenz, ///< 7z アーカイブ
    gzip,   ///< GZip 圧縮

    bin,    ///< 汎用バイナリ
    unknown ///< 不明なファイルタイプ
};

/// @brief ファイルの生データ構造体
struct RawFileData final {
    std::string filePath;       ///< ファイルパス
    std::vector<uint8_t> data;  ///< ファイルデータバッファ
    size_t size;                ///< ファイルサイズ
    FileType fileType;          ///< ファイルタイプ（バッファからの推測）
};

/// @brief ファイルが存在するかどうか確認
bool IsFileExist(const std::string &filePath);

/// @brief ファイルの読み込み
/// @param filePath ファイルパス
/// @param detectBytes 先頭からこのバイト数以内でファイル種別の判定を行う（小さすぎると判定できない場合があります）
/// @return 読み込んだファイルの生データ（fileType に推測結果が格納されます）
RawFileData LoadFile(const std::string &filePath, size_t detectBytes = 64 * 1024);

/// @brief ファイルの書き込み
/// @param fileData 書き込むファイルの生データ
void SaveFile(const RawFileData &fileData);

/// @brief 指定のファイルが何の種類かをバイト列から推測する
/// @param filePath ファイルパス
/// @param detectBytes 先頭からこのバイト数以内でファイル種別の判定を行う（小さすぎると判定できない場合があります）
FileType DetectFileTypeFromFile(const std::string &filePath, size_t detectBytes = 64 * 1024);

} // namespace KashipanEngine
