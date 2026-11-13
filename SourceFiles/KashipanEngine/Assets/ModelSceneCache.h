#pragma once
#include "Utilities/FileIO/ContentCache.h"
#include <assimp/scene.h>
#include <assimp/version.h>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace KashipanEngine::ModelSceneCache {
// エンジンが使用する後処理済みシーンデータのみを保存する。使用側で新たなAssimpフィールドを
// 参照する場合はスキーマを更新する。ポインターやAssimpオブジェクトのメモリ配置は保存しない。
inline constexpr const char *Schema = "EngineModelScene-v1";
template<bool Reading> class Archive final {
public:
    std::vector<char> output;
    explicit Archive(const std::vector<char> *input = nullptr) : input_(input) {}
    template<class T> void Value(T &value) {
        static_assert(std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>);
        Bytes(&value, sizeof(T));
    }
    void Text(aiString &text) {
        unsigned int length = text.length;
        Value(length);
        if (length >= sizeof(text.data)) throw std::runtime_error("model string length");
        Bytes(text.data, length);
        if constexpr (Reading) { text.length = length; text.data[length] = 0; }
    }
    template<class T> void Values(T *&values, unsigned int &count) {
        Value(count);
        if constexpr (Reading) {
            RequireAllocation(count, sizeof(T));
            values = count ? new T[count]{} : nullptr;
        }
        if (count) Bytes(values, static_cast<size_t>(count) * sizeof(T));
    }
    template<class T> void Optional(T *&values, unsigned int count) {
        bool present = values != nullptr;
        Value(present);
        if (present) {
            unsigned int size = count;
            Values(values, size);
            if (size != count) throw std::runtime_error("model vertex count");
        }
    }
    template<class T, class F> void Objects(T **&values, unsigned int &count, F visit) {
        unsigned int size = count;
        Value(size);
        if constexpr (Reading) {
            RequireAllocation(size, sizeof(T*) + sizeof(T));
            values = size ? new T*[size]{} : nullptr;
            count = size;
        }
        for (unsigned int i = 0; i < size; ++i) {
            if constexpr (Reading) values[i] = new T();
            visit(*values[i]);
        }
    }
    void Node(aiNode &node, aiNode *parent = nullptr, unsigned int depth = 0) {
        if (depth > 256) throw std::runtime_error("model hierarchy depth");
        if constexpr (Reading) node.mParent = parent;
        Text(node.mName); Value(node.mTransformation); Values(node.mMeshes, node.mNumMeshes);
        Objects(node.mChildren, node.mNumChildren, [&](aiNode &child) { Node(child, &node, depth + 1); });
    }
    void Mesh(aiMesh &mesh) {
        Text(mesh.mName); Value(mesh.mPrimitiveTypes); Value(mesh.mMaterialIndex); Value(mesh.mNumVertices);
        Optional(mesh.mVertices, mesh.mNumVertices); Optional(mesh.mNormals, mesh.mNumVertices);
        Optional(mesh.mTangents, mesh.mNumVertices); Optional(mesh.mBitangents, mesh.mNumVertices);
        for (unsigned int i = 0; i < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++i) {
            Value(mesh.mNumUVComponents[i]); Optional(mesh.mTextureCoords[i], mesh.mNumVertices);
        }
        for (auto &colors : mesh.mColors) Optional(colors, mesh.mNumVertices);
        unsigned int faces = mesh.mNumFaces;
        Value(faces);
        if constexpr (Reading) {
            RequireAllocation(faces, sizeof(aiFace));
            mesh.mFaces = faces ? new aiFace[faces] : nullptr; mesh.mNumFaces = faces;
        }
        for (unsigned int i = 0; i < faces; ++i) Values(mesh.mFaces[i].mIndices, mesh.mFaces[i].mNumIndices);
        Objects(mesh.mBones, mesh.mNumBones, [&](aiBone &bone) {
            Text(bone.mName); Value(bone.mOffsetMatrix); Values(bone.mWeights, bone.mNumWeights);
        });
        Value(mesh.mMethod);
        Objects(mesh.mAnimMeshes, mesh.mNumAnimMeshes, [&](aiAnimMesh &morph) {
            Text(morph.mName); Value(morph.mWeight); Value(morph.mNumVertices);
            Optional(morph.mVertices, morph.mNumVertices); Optional(morph.mNormals, morph.mNumVertices);
            Optional(morph.mTangents, morph.mNumVertices); Optional(morph.mBitangents, morph.mNumVertices);
            for (auto &uv : morph.mTextureCoords) Optional(uv, morph.mNumVertices);
            for (auto &colors : morph.mColors) Optional(colors, morph.mNumVertices);
        });
    }
    void Material(aiMaterial &material) {
        unsigned int count = material.mNumProperties;
        Value(count);
        if (count > 100000) throw std::runtime_error("model material properties");
        if constexpr (Reading) material.Clear();
        for (unsigned int i = 0; i < count; ++i) {
            aiString key;
            unsigned int semantic = 0, index = 0, size = 0;
            aiPropertyTypeInfo type = aiPTI_Buffer;
            if constexpr (!Reading) {
                const auto &property = *material.mProperties[i];
                key = property.mKey; semantic = property.mSemantic; index = property.mIndex;
                size = property.mDataLength; type = property.mType;
            }
            Text(key); Value(semantic); Value(index); Value(type);
            char *data = nullptr;
            if constexpr (!Reading) data = material.mProperties[i]->mData;
            // aiMaterialがコピーするまで、復号したプロパティのメモリは一時オブジェクトが所有する。
            std::unique_ptr<char[]> owned;
            if constexpr (Reading) {
                Value(size); RequireAllocation(size, 1); owned.reset(size ? new char[size] : nullptr);
                data = owned.get(); if (size) Bytes(data, size);
                if (material.AddBinaryProperty(data, size, key.C_Str(), semantic, index, type) != AI_SUCCESS)
                    throw std::runtime_error("model material property");
            } else { Value(size); if (size) Bytes(data, size); }
        }
    }
    void Animation(aiAnimation &animation) {
        Text(animation.mName); Value(animation.mDuration); Value(animation.mTicksPerSecond);
        Objects(animation.mChannels, animation.mNumChannels, [&](aiNodeAnim &channel) {
            Text(channel.mNodeName); Value(channel.mPreState); Value(channel.mPostState);
            Values(channel.mPositionKeys, channel.mNumPositionKeys);
            Values(channel.mRotationKeys, channel.mNumRotationKeys);
            Values(channel.mScalingKeys, channel.mNumScalingKeys);
        });
        // 現在のAnimationManagerはノードキーのみを使用するが、これらのチャンネルも保持する。
        Objects(animation.mMeshChannels, animation.mNumMeshChannels, [&](aiMeshAnim &channel) {
            Text(channel.mName); Values(channel.mKeys, channel.mNumKeys);
        });
        Objects(animation.mMorphMeshChannels, animation.mNumMorphMeshChannels, [&](aiMeshMorphAnim &channel) {
            Text(channel.mName);
            unsigned int count = channel.mNumKeys; Value(count);
            if constexpr (Reading) {
                RequireAllocation(count, sizeof(aiMeshMorphKey));
                channel.mKeys = count ? new aiMeshMorphKey[count] : nullptr; channel.mNumKeys = count;
            }
            for (unsigned int i = 0; i < count; ++i) {
                auto &key = channel.mKeys[i]; Value(key.mTime);
                unsigned int weights = key.mNumValuesAndWeights;
                try {
                    Values(key.mValues, key.mNumValuesAndWeights); Values(key.mWeights, weights);
                    if (weights != key.mNumValuesAndWeights) throw std::runtime_error("model morph key");
                } catch (...) {
                    if constexpr (Reading) {
                        // Assimpのデストラクターは両方のポインターが存在する場合にのみ解放する。
                        delete[] key.mValues; delete[] key.mWeights;
                        key.mValues = nullptr; key.mWeights = nullptr; key.mNumValuesAndWeights = 0;
                    }
                    throw;
                }
            }
        });
    }
    void Scene(aiScene &scene) {
        Value(scene.mFlags); Text(scene.mName);
        bool root = scene.mRootNode != nullptr; Value(root);
        if constexpr (Reading) { if (root) scene.mRootNode = new aiNode(); }
        if (root) Node(*scene.mRootNode);
        Objects(scene.mMeshes, scene.mNumMeshes, [&](aiMesh &mesh) { Mesh(mesh); });
        Objects(scene.mMaterials, scene.mNumMaterials, [&](aiMaterial &material) { Material(material); });
        Objects(scene.mAnimations, scene.mNumAnimations, [&](aiAnimation &animation) { Animation(animation); });
        Objects(scene.mTextures, scene.mNumTextures, [&](aiTexture &texture) {
            Value(texture.mWidth); Value(texture.mHeight); Bytes(texture.achFormatHint, sizeof(texture.achFormatHint));
            Text(texture.mFilename);
            const uint64_t bytes = texture.mHeight ? uint64_t(texture.mWidth) * texture.mHeight * sizeof(aiTexel) : texture.mWidth;
            if (bytes > 1024ULL * 1024 * 1024) throw std::runtime_error("model embedded texture size");
            if constexpr (Reading) {
                RequireAllocation(static_cast<size_t>(bytes), 1);
                texture.pcData = bytes ? new aiTexel[(bytes + sizeof(aiTexel) - 1) / sizeof(aiTexel)]{} : nullptr;
            }
            if (bytes) Bytes(texture.pcData, static_cast<size_t>(bytes));
        });
        // カメラ・ライト・メタデータは、エンジンの3つのモデル関連マネージャーでは使用しない。
    }
    bool Complete() const { return input_ && cursor_ == input_->size(); }
private:
    void RequireAllocation(size_t count, size_t stride) {
        if (!input_ || count > input_->size() - cursor_ || count > (1024ULL * 1024 * 1024 - allocated_) / stride)
            throw std::runtime_error("model cache allocation");
        allocated_ += count * stride;
    }
    void Bytes(void *data, size_t size) {
        if constexpr (Reading) {
            if (!input_ || size > input_->size() - cursor_) throw std::runtime_error("model cache truncated");
            if (size) std::memcpy(data, input_->data() + cursor_, size);
            cursor_ += size;
        } else {
            if (size > 1024ULL * 1024 * 1024 - output.size()) throw std::runtime_error("model cache too large");
            const auto *first = static_cast<const char *>(data);
            if (size) output.insert(output.end(), first, first + size);
        }
    }
    const std::vector<char> *input_ = nullptr;
    size_t cursor_ = 0, allocated_ = 0;
};
inline std::vector<char> Encode(const aiScene &scene) {
    Archive<false> archive;
    archive.Scene(const_cast<aiScene &>(scene)); // 書き込み処理はシーンを変更しない。
    return std::move(archive.output);
}
inline std::unique_ptr<aiScene> Decode(const std::vector<char> &bytes) {
    try {
        auto scene = std::make_unique<aiScene>();
        Archive<true> archive(&bytes); archive.Scene(*scene);
        if (!archive.Complete() || !scene->mRootNode) return {};
        // 復元したシーンを使用側へ渡す前に相互参照を検証する。
        const auto checkNode = [&](const auto &self, const aiNode *node) -> bool {
            for (unsigned int i = 0; i < node->mNumMeshes; ++i) if (node->mMeshes[i] >= scene->mNumMeshes) return false;
            for (unsigned int i = 0; i < node->mNumChildren; ++i) if (!self(self, node->mChildren[i])) return false;
            return true;
        };
        if (!checkNode(checkNode, scene->mRootNode)) return {};
        for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
            const auto &mesh = *scene->mMeshes[i];
            if (!mesh.mVertices || mesh.mMaterialIndex >= scene->mNumMaterials) return {};
            for (unsigned int f = 0; f < mesh.mNumFaces; ++f)
                for (unsigned int j = 0; j < mesh.mFaces[f].mNumIndices; ++j)
                    if (mesh.mFaces[f].mIndices[j] >= mesh.mNumVertices) return {};
            for (unsigned int b = 0; b < mesh.mNumBones; ++b)
                for (unsigned int w = 0; w < mesh.mBones[b]->mNumWeights; ++w)
                    if (mesh.mBones[b]->mWeights[w].mVertexId >= mesh.mNumVertices) return {};
        }
        return scene;
    } catch (...) { return {}; }
}
inline std::string LibraryIdentity() {
    // 読み込んだDLL自体もハッシュ化し、Assimpの独自ビルド時に古いデータを無効化する。
    static const std::string identity = [] {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&aiGetVersionMajor), &module)) return std::string{};
        wchar_t path[32768]{};
        const auto count = GetModuleFileNameW(module, path, 32768);
        return count && count < 32768 ? ContentCache::FileDigest(std::filesystem::path(path)) : std::string{};
    }();
    return identity;
}
inline std::string Key(const std::filesystem::path &source, unsigned int flags) {
    return std::string(Schema) + "|" + LibraryIdentity() + "|" + std::to_string(sizeof(ai_real)) +
        "|" + std::to_string(sizeof(aiVectorKey)) + "|" + std::to_string(sizeof(aiQuatKey)) + "|" + std::to_string(flags) +
        "|" + ContentCache::PathText(std::filesystem::absolute(source).lexically_normal());
}
inline std::filesystem::path CachePath(const std::filesystem::path &directory, const std::filesystem::path &source, unsigned int flags) {
    return directory / (ContentCache::Digest(Key(source, flags)) + ".cache");
}
} // namespace KashipanEngine::ModelSceneCache

