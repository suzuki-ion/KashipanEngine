#pragma once
#include <cstdio>
#include <string>

#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>

#include "Utilities/Conversion/ConvertString.h"

namespace KashipanEngine {

/// @brief _wfopenを使ってファイルを開くIOStream実装
/// @details Assimpの既定IOStream（DefaultIOStream）は内部でfopen（ANSIコードページ）を使うため、
///          現在のANSIコードページで表現できない文字を含むパスのファイルは開けない。
///          このクラスは常にUTF-8文字列をワイド文字列へ変換してから_wfopenで開くため、
///          コードページに依存せず任意のUnicodeファイル名を扱える。
class AssimpUtf8IOStream final : public Assimp::IOStream {
public:
    explicit AssimpUtf8IOStream(FILE *file) : file_(file) {}
    ~AssimpUtf8IOStream() override {
        if (file_) fclose(file_);
    }

    size_t Read(void *buffer, size_t size, size_t count) override {
        return file_ ? fread(buffer, size, count, file_) : 0;
    }
    size_t Write(const void *buffer, size_t size, size_t count) override {
        return file_ ? fwrite(buffer, size, count, file_) : 0;
    }
    aiReturn Seek(size_t offset, aiOrigin origin) override {
        if (!file_) return aiReturn_FAILURE;
        int whence = SEEK_SET;
        if (origin == aiOrigin_CUR) whence = SEEK_CUR;
        else if (origin == aiOrigin_END) whence = SEEK_END;
        return (_fseeki64(file_, static_cast<long long>(offset), whence) == 0) ? aiReturn_SUCCESS : aiReturn_FAILURE;
    }
    size_t Tell() const override {
        return file_ ? static_cast<size_t>(_ftelli64(file_)) : 0;
    }
    size_t FileSize() const override {
        if (!file_) return 0;
        const long long current = _ftelli64(file_);
        _fseeki64(file_, 0, SEEK_END);
        const long long size = _ftelli64(file_);
        _fseeki64(file_, current, SEEK_SET);
        return static_cast<size_t>(size);
    }
    void Flush() override {
        if (file_) fflush(file_);
    }

private:
    FILE *file_ = nullptr;
};

/// @brief _wfopenベースでUTF-8パス（現在のANSIコードページで表現できない文字を含む場合も）を
///        正しく開けるようにするAssimp用IOSystem
/// @details Assimpの既定IOSystemはWindows上でfopen（ANSIコードページ）を使ってファイルを開くため、
///          エンジン内の「narrow文字列は常にUTF-8」という前提のパスを渡しても、
///          非ANSIコードページ文字を含むパスは開けない。
///          importer.ReadFile()の前にimporter.SetIOHandler(new AssimpUtf8IOSystem())で
///          差し替えて使うこと（Importerが所有権を持ち、破棄時に自動的に削除される）
class AssimpUtf8IOSystem final : public Assimp::IOSystem {
public:
    bool Exists(const char *file) const override {
        FILE *f = OpenRaw(file, "rb");
        if (!f) return false;
        fclose(f);
        return true;
    }
    char getOsSeparator() const override { return '\\'; }
    Assimp::IOStream *Open(const char *file, const char *mode = "rb") override {
        FILE *f = OpenRaw(file, mode);
        return f ? new AssimpUtf8IOStream(f) : nullptr;
    }
    void Close(Assimp::IOStream *file) override { delete file; }

private:
    static FILE *OpenRaw(const char *utf8Path, const char *mode) {
        const std::wstring widePath = ConvertString(std::string(utf8Path));
        const std::wstring wideMode = ConvertString(std::string(mode));
        FILE *file = nullptr;
        _wfopen_s(&file, widePath.c_str(), wideMode.c_str());
        return file;
    }
};

} // namespace KashipanEngine
