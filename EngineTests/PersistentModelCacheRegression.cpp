#include "Assets/SharedModelImport.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace KashipanEngine;
void Compare(const aiScene &a, const aiScene &b) {
    assert(a.mNumMeshes == b.mNumMeshes && a.mNumTextures == b.mNumTextures && a.mNumMaterials == b.mNumMaterials);
    const auto nodes = [&](const auto &self, const aiNode *x, const aiNode *y) -> void {
        assert(x->mName == y->mName && x->mTransformation == y->mTransformation && x->mNumChildren == y->mNumChildren && x->mNumMeshes == y->mNumMeshes);
        for (unsigned int i = 0; i < x->mNumMeshes; ++i) assert(x->mMeshes[i] == y->mMeshes[i]);
        for (unsigned int i = 0; i < x->mNumChildren; ++i) { assert(y->mChildren[i]->mParent == y); self(self, x->mChildren[i], y->mChildren[i]); }
    };
    nodes(nodes, a.mRootNode, b.mRootNode);
    for (unsigned int i = 0; i < a.mNumMeshes; ++i) {
        const auto &x = *a.mMeshes[i], &y = *b.mMeshes[i];
        assert(x.mNumVertices == y.mNumVertices && x.mNumFaces == y.mNumFaces && x.mNumBones == y.mNumBones && x.mNumAnimMeshes == y.mNumAnimMeshes);
        for (unsigned int v = 0; v < x.mNumVertices; ++v) {
            assert(x.mVertices[v] == y.mVertices[v]);
            if (x.mNormals) assert(x.mNormals[v] == y.mNormals[v]);
            if (x.mTangents) assert(x.mTangents[v] == y.mTangents[v]);
            for (unsigned int u = 0; u < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++u) if (x.mTextureCoords[u]) assert(x.mTextureCoords[u][v] == y.mTextureCoords[u][v]);
        }
        for (unsigned int f = 0; f < x.mNumFaces; ++f) {
            assert(x.mFaces[f].mNumIndices == y.mFaces[f].mNumIndices);
            for (unsigned int j = 0; j < x.mFaces[f].mNumIndices; ++j) assert(x.mFaces[f].mIndices[j] == y.mFaces[f].mIndices[j]);
        }
        for (unsigned int j = 0; j < x.mNumBones; ++j) {
            const auto &xb = *x.mBones[j], &yb = *y.mBones[j];
            assert(xb.mName == yb.mName && xb.mOffsetMatrix == yb.mOffsetMatrix && xb.mNumWeights == yb.mNumWeights);
            for (unsigned int w = 0; w < xb.mNumWeights; ++w) assert(xb.mWeights[w].mVertexId == yb.mWeights[w].mVertexId && xb.mWeights[w].mWeight == yb.mWeights[w].mWeight);
        }
        for (unsigned int j = 0; j < x.mNumAnimMeshes; ++j) {
            const auto &xm = *x.mAnimMeshes[j], &ym = *y.mAnimMeshes[j];
            assert(xm.mName == ym.mName && xm.mNumVertices == ym.mNumVertices);
            for (unsigned int v = 0; v < xm.mNumVertices; ++v) { if (xm.mVertices) assert(xm.mVertices[v] == ym.mVertices[v]); if (xm.mNormals) assert(xm.mNormals[v] == ym.mNormals[v]); }
        }
    }
    // Also checks all material properties, embedded pixels, animation keys and morph channels byte for byte.
    assert(ModelSceneCache::Encode(a) == ModelSceneCache::Encode(b));
}
int main() {
    const auto root = std::filesystem::absolute("Generated/PersistentModelRegression-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    const auto directory = root / "cache", source = root / L"モデル.obj", material = root / "external.mtl";
    std::ofstream(source) << "mtllib external.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl Test\nf 1 2 3\n";
    std::ofstream(material) << "newmtl Test\nKd 1 0 0\n";
    const auto load = [&](const std::filesystem::path &file) {
        SharedModelImport::StartupSession session(directory);
        auto result = SharedModelImport::Load(file, SharedModelImport::ModelFlags);
        assert(result->scene);
        return result;
    };
    auto cold = load(source); assert(!cold->diskCacheHit && SharedModelImport::CacheStatistics().stored == 1);
    auto warm = load(source); assert(warm->diskCacheHit && SharedModelImport::CacheStatistics().parsed == 0); Compare(*cold->scene, *warm->scene);
    const auto time = std::filesystem::last_write_time(material);
    std::ofstream(material) << "newmtl Test\nKd 0 1 0\n";
    std::filesystem::last_write_time(material, time); // Same size and timestamp must still invalidate.
    auto changed = load(source); assert(!changed->diskCacheHit);
    aiColor3D color; changed->scene->mMaterials[changed->scene->mMeshes[0]->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, color); assert(color.g == 1 && color.r == 0);
    assert(load(source)->diskCacheHit);
    const auto cache = ModelSceneCache::CachePath(directory, source, SharedModelImport::ModelFlags);
    std::ofstream(cache, std::ios::binary | std::ios::trunc) << "broken";
    assert(!load(source)->diskCacheHit && load(source)->diskCacheHit);
    // Source file content change.
    std::ofstream(source, std::ios::app) << "#changed\n"; assert(!load(source)->diskCacheHit);
    // Missing external file becoming available also invalidates the old scene.
    std::filesystem::remove(material); assert(!load(source)->diskCacheHit); assert(load(source)->diskCacheHit);
    std::ofstream(material) << "newmtl Test\nKd 0 0 1\n"; assert(!load(source)->diskCacheHit);
    // Cache write failure must still return an imported model.
    const auto blocked = root / "not-a-directory"; std::ofstream(blocked) << "block";
    { SharedModelImport::StartupSession session(blocked); auto result = SharedModelImport::Load(source, SharedModelImport::ModelFlags); assert(result->scene && !result->diskCacheHit); }
    for (const auto &file : {std::filesystem::path(L"GameProjects/Test/Assets/TestModels/Luchika/Luchika.gltf"), std::filesystem::path(L"GameProjects/Test/Assets/TestModels/Meepa/メェパ_FBX.fbx")}) {
        assert(std::filesystem::exists(file));
        auto first = load(file), second = load(file); assert(!first->diskCacheHit && second->diskCacheHit);
        Compare(*first->scene, *second->scene);
        size_t morphs = 0, bones = 0;
        for (unsigned int m = 0; m < first->scene->mNumMeshes; ++m) { morphs += first->scene->mMeshes[m]->mNumAnimMeshes; bones += first->scene->mMeshes[m]->mNumBones; }
        std::cout << "Real model roundtrip: bones=" << bones << ", morphs=" << morphs << "\n";
    }
    // Both embedded formats used by ModelManager: compressed file bytes and raw aiTexel pixels.
    {
        aiScene embedded;
        embedded.mRootNode = new aiNode();
        embedded.mNumTextures = 2; embedded.mTextures = new aiTexture*[2]{};
        for (unsigned int i = 0; i < 2; ++i) {
            auto *texture = embedded.mTextures[i] = new aiTexture();
            texture->mFilename.Set(i ? "raw" : "image.png");
            texture->mWidth = i ? 2 : 7; texture->mHeight = i ? 1 : 0;
            texture->pcData = new aiTexel[2]{};
            std::memcpy(texture->pcData, "abcdefgh", 8);
        }
        const auto decoded = ModelSceneCache::Decode(ModelSceneCache::Encode(embedded));
        assert(decoded && decoded->mNumTextures == 2);
        assert(decoded->GetEmbeddedTexture("*0") && decoded->GetEmbeddedTexture("raw"));
        assert(std::memcmp(decoded->mTextures[0]->pcData, "abcdefg", 7) == 0);
        assert(std::memcmp(decoded->mTextures[1]->pcData, "abcdefgh", 8) == 0);
    }
    // Truncated payload must be rejected safely even when the container hash is valid.
    auto bytes = ModelSceneCache::Encode(*warm->scene); bytes.resize(bytes.size() / 2); assert(!ModelSceneCache::Decode(bytes));
    std::filesystem::remove_all(root);
    std::cout << "Persistent model cache regression passed\n";
}
