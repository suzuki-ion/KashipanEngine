#include <windows.h>
#include <string>
#include "ConvertString.h"

namespace KashipanEngine {

std::wstring ConvertString(const std::string &str) {
    if (str.empty()) {
        return std::wstring();
    }

    auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *>(&str[0]), static_cast<int>(str.size()), NULL, 0);
    if (sizeNeeded == 0) {
        return std::wstring();
    }
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
    return result;
}

std::string ConvertString(const std::wstring &str) {
    if (str.empty()) {
        return std::string();
    }

    auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
    if (sizeNeeded == 0) {
        return std::string();
    }
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
    return result;
}

std::wstring ShiftJISToUTF16(const std::string &sjis) {
    int utf16Size = MultiByteToWideChar(932, 0, sjis.c_str(), -1, nullptr, 0);
    std::wstring utf16(utf16Size, L'\0');
    MultiByteToWideChar(932, 0, sjis.c_str(), -1, &utf16[0], utf16Size);
    return utf16;
}

std::string UTF16ToUTF8(const std::wstring &utf16) {
    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(utf8Size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, &utf8[0], utf8Size, nullptr, nullptr);
    return utf8;
}

std::string ShiftJISToUTF8(const std::string &sjis) {
    std::wstring utf16 = ShiftJISToUTF16(sjis);
    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(utf8Size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, &utf8[0], utf8Size, nullptr, nullptr);
    return utf8;
}

std::vector<char32_t> Utf8ToCodepoints(const std::string &utf8) {
    std::vector<char32_t> result;
    result.reserve(utf8.size());

    const auto *bytes = reinterpret_cast<const unsigned char *>(utf8.data());
    const size_t size = utf8.size();
    size_t i = 0;
    while (i < size) {
        const unsigned char lead = bytes[i];
        size_t extraBytes = 0;
        char32_t codepoint = 0;

        if ((lead & 0x80) == 0x00) {
            codepoint = lead;
            extraBytes = 0;
        } else if ((lead & 0xE0) == 0xC0) {
            codepoint = lead & 0x1Fu;
            extraBytes = 1;
        } else if ((lead & 0xF0) == 0xE0) {
            codepoint = lead & 0x0Fu;
            extraBytes = 2;
        } else if ((lead & 0xF8) == 0xF0) {
            codepoint = lead & 0x07u;
            extraBytes = 3;
        } else {
            // 不正な先頭バイト
            result.push_back(U'�');
            ++i;
            continue;
        }

        if (i + extraBytes >= size) {
            // 末尾が欠けている不正な列
            result.push_back(U'�');
            ++i;
            continue;
        }

        bool valid = true;
        for (size_t k = 1; k <= extraBytes; ++k) {
            const unsigned char cont = bytes[i + k];
            if ((cont & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6) | (cont & 0x3Fu);
        }

        if (!valid) {
            result.push_back(U'�');
            ++i;
            continue;
        }

        result.push_back(codepoint);
        i += extraBytes + 1;
    }

    return result;
}

} // namespace KashipanEngine