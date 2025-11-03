#include "RawFile.h"
#include <fstream>
#include <cctype>
#include <algorithm>
#include <stdexcept>

namespace KashipanEngine {
namespace {

inline size_t clampDetectSize(size_t size, size_t detectBytes) {
    if (detectBytes == 0) return 0;
    return std::min(size, detectBytes);
}

inline bool startsWith(const uint8_t* d, size_t n, const uint8_t* sig, size_t m) {
    if (n < m) return false;
    for (size_t i = 0; i < m; ++i) if (d[i] != sig[i]) return false;
    return true;
}

inline bool equalsAt(const uint8_t* d, size_t n, size_t at, const char* s) {
    size_t m = 0; while (s[m] != '\0') ++m;
    if (at + m > n) return false;
    for (size_t i = 0; i < m; ++i) if (d[at + i] != static_cast<uint8_t>(s[i])) return false;
    return true;
}

inline bool isWhitespace(uint8_t c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; }

// Rough UTF-8 validation without NUL bytes
bool IsLikelyUtf8Text(const uint8_t* s, size_t len) {
    if (!s) return false;
    for (size_t i = 0; i < len; ) {
        uint8_t c = s[i];
        if (c == 0x00) return false; // contains NUL -> not text
        if ((c & 0x80) == 0x00) { // ASCII
            ++i;
        } else if ((c & 0xE0) == 0xC0) { // 2-byte
            if (i + 1 >= len) return false;
            if ((s[i+1] & 0xC0) != 0x80) return false;
            uint32_t cp = ((c & 0x1F) << 6) | (s[i+1] & 0x3F);
            if (cp < 0x80) return false; // overlong
            i += 2;
        } else if ((c & 0xF0) == 0xE0) { // 3-byte
            if (i + 2 >= len) return false;
            if ((s[i+1] & 0xC0) != 0x80 || (s[i+2] & 0xC0) != 0x80) return false;
            uint32_t cp = ((c & 0x0F) << 12) | ((s[i+1] & 0x3F) << 6) | (s[i+2] & 0x3F);
            if (cp < 0x800) return false; // overlong
            if (cp >= 0xD800 && cp <= 0xDFFF) return false; // UTF-16 surrogate
            i += 3;
        } else if ((c & 0xF8) == 0xF0) { // 4-byte
            if (i + 3 >= len) return false;
            if ((s[i+1] & 0xC0) != 0x80 || (s[i+2] & 0xC0) != 0x80 || (s[i+3] & 0xC0) != 0x80) return false;
            uint32_t cp = ((c & 0x07) << 18) | ((s[i+1] & 0x3F) << 12) | ((s[i+2] & 0x3F) << 6) | (s[i+3] & 0x3F);
            if (cp < 0x10000 || cp > 0x10FFFF) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

FileType DetectFileTypeFromBytes(const uint8_t* data, size_t n) {
    if (!data || n == 0) return FileType::unknown;

    // PNG
    static const uint8_t pngSig[] = {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A};
    if (startsWith(data, n, pngSig, sizeof(pngSig))) return FileType::png;

    // JPEG
    if (n >= 2 && data[0] == 0xFF && data[1] == 0xD8) return FileType::jpg;

    // GIF
    if (equalsAt(data, n, 0, "GIF87a") || equalsAt(data, n, 0, "GIF89a")) return FileType::gif;

    // BMP
    if (n >= 2 && data[0] == 'B' && data[1] == 'M') return FileType::bmp;

    // ICO (ICONDIR: reserved=0, type=1)
    if (n >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01 && data[3] == 0x00) return FileType::ico;

    // WEBP (RIFF....WEBP)
    if (n >= 12 && equalsAt(data, n, 0, "RIFF") && equalsAt(data, n, 8, "WEBP")) return FileType::webp;

    // TIFF (II*\0 or MM\0*)
    if (n >= 4 && ((data[0] == 'I' && data[1] == 'I' && data[2] == 0x2A && data[3] == 0x00) ||
                   (data[0] == 'M' && data[1] == 'M' && data[2] == 0x00 && data[3] == 0x2A))) return FileType::tiff;

    // WAV (RIFF....WAVE)
    if (n >= 12 && equalsAt(data, n, 0, "RIFF") && equalsAt(data, n, 8, "WAVE")) return FileType::wav;

    // AVI (RIFF....AVI )
    if (n >= 12 && equalsAt(data, n, 0, "RIFF") && equalsAt(data, n, 8, "AVI ")) return FileType::avi;

    // MP4/ISO-BMFF (ftyp box at offset 4)
    if (n >= 12 && equalsAt(data, n, 4, "ftyp")) return FileType::mp4;

    // MKV/WebM (EBML header)
    static const uint8_t mkvSig[] = {0x1A,0x45,0xDF,0xA3};
    if (startsWith(data, n, mkvSig, sizeof(mkvSig))) return FileType::mkv;

    // MP3 (ID3) or frame sync 0xFFE
    if ((n >= 3 && equalsAt(data, n, 0, "ID3")) || (n >= 2 && data[0] == 0xFF && (data[1] & 0xE0) == 0xE0)) return FileType::mp3;

    // OGG (OggS)
    if (equalsAt(data, n, 0, "OggS")) return FileType::ogg;

    // FLAC (fLaC)
    if (equalsAt(data, n, 0, "fLaC")) return FileType::flac;

    // PDF
    if (equalsAt(data, n, 0, "%PDF-")) return FileType::pdf;

    // ZIP family
    if (n >= 4 && data[0] == 'P' && data[1] == 'K' && (data[2] == 0x03 || data[2] == 0x05 || data[2] == 0x07) && (data[3] == 0x04 || data[3] == 0x06 || data[3] == 0x08))
        return FileType::zip;

    // RAR (v4 and v5)
    static const uint8_t rar4[] = {'R','a','r','!',0x1A,0x07,0x00};
    static const uint8_t rar5[] = {'R','a','r','!',0x1A,0x07,0x01,0x00};
    if (startsWith(data, n, rar4, sizeof(rar4)) || startsWith(data, n, rar5, sizeof(rar5))) return FileType::rar;

    // 7z
    static const uint8_t sevenzSig[] = { '7','z', 0xBC, 0xAF, 0x27, 0x1C };
    if (startsWith(data, n, sevenzSig, sizeof(sevenzSig))) return FileType::sevenz;

    // GZip
    if (n >= 2 && data[0] == 0x1F && data[1] == 0x8B) return FileType::gzip;

    // WEB text-like: check BOM then skip whitespace to infer json/xml/html/txt
    size_t i = 0;
    // UTF-8 BOM
    if (n >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) i = 3;
    while (i < n && isWhitespace(data[i])) ++i;
    if (i < n) {
        // JSON
        if (data[i] == '{' || data[i] == '[') {
            if (IsLikelyUtf8Text(data, n)) return FileType::json;
        }
        // XML/HTML
        if (data[i] == '<') {
            // case-insensitive check for <!DOCTYPE html or <html
            auto ciCmp = [](uint8_t a, char b){ return std::tolower(a) == std::tolower(static_cast<unsigned char>(b)); };
            const char* d1 = "!DOCTYPE";
            bool looksDoctype = (i + 2 < n && data[i+1] == '!' && (i + 1 + 8 < n));
            bool isHtml = false;
            if (looksDoctype) {
                bool ok = true;
                for (size_t k = 0; k < 8; ++k) if (!ciCmp(data[i+2+k], d1[k])) { ok = false; break; }
                if (ok) {
                    // find "html" soon after
                    for (size_t k = i; k < std::min(n, i + 256ull); ++k) {
                        if (std::tolower(data[k]) == 'h' && k + 3 < n &&
                            std::tolower(data[k+1]) == 't' && std::tolower(data[k+2]) == 'm' && std::tolower(data[k+3]) == 'l') {
                            isHtml = true; break;
                        }
                    }
                }
            }
            if (isHtml) {
                if (IsLikelyUtf8Text(data, n)) return FileType::html;
            } else {
                // treat as XML if starts with <?xml or general '<'
                if (i + 1 < n && data[i+1] == '?') {
                    // Likely XML declaration
                    if (IsLikelyUtf8Text(data, n)) return FileType::xml;
                }
                // Generic XML heuristic
                if (IsLikelyUtf8Text(data, n)) return FileType::xml;
            }
        }
    }

    // Generic text fallback
    if (IsLikelyUtf8Text(data, n)) return FileType::txt;

    return FileType::bin;
}

} // namespace

RawFileData LoadFile(const std::string &filePath, size_t detectBytes) {
    RawFileData fileData{};
    fileData.filePath = filePath;

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filePath);
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    fileData.data.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char *>(fileData.data.data()), size)) {
        throw std::runtime_error("Failed to read file: " + filePath);
    }
    fileData.size = static_cast<size_t>(size);

    // Detect file type using only the first detectBytes (or the full size if smaller)
    const size_t checkSize = clampDetectSize(fileData.size, detectBytes);
    fileData.fileType = DetectFileTypeFromBytes(fileData.data.data(), checkSize);

    return fileData;
}

void SaveFile(const RawFileData &fileData) {
    std::ofstream file(fileData.filePath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file for writing: " + fileData.filePath);
    }
    if (!file.write(reinterpret_cast<const char *>(fileData.data.data()), static_cast<std::streamsize>(fileData.size))) {
        throw std::runtime_error("Failed to write file: " + fileData.filePath);
    }
}

FileType DetectFileTypeFromFile(const std::string &filePath, size_t detectBytes) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filePath);
    }
    std::vector<uint8_t> buffer(clampDetectSize(static_cast<size_t>(file.seekg(0, std::ios::end).tellg()), detectBytes));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(buffer.size()))) {
        throw std::runtime_error("Failed to read file: " + filePath);
    }
    return DetectFileTypeFromBytes(buffer.data(), buffer.size());
}


} // namespace KashipanEngine