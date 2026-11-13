#pragma once
#include "RendererInternal.h"
#include "Debug/Profiler.h"

namespace KashipanEngine::RendererInternal {

// 公開マテリアルはセッターを介さずに編集できる。パック済みテンプレートを
// 毎フレーム1回再検証し、マテリアルのポインターやハンドルを変更の識別に使用しない。
class MaterialDrawCache {
    static constexpr std::array<const char *, 11> kFields = {
        "instanceColor", "instanceColorBlendMode", "uvTransform", "characterColor",
        "uvRect", "boldWeight", "outlineWidth", "outlineColor", "textureIndex",
        "samplerIndex", "useTexture"
    };
    struct Location {
        std::uint32_t offset = UINT32_MAX, size = 0;
        bool operator==(const Location &) const = default;
    };
    struct MaterialRecord {
        std::uint64_t frame = 0, version = 0;
        std::vector<std::byte> bytes;
        Matrix4x4 uv = Matrix4x4::Identity();
        MaterialManager::Material *material = nullptr;
        std::uint32_t texture = 0, sampler = 0;
    };
    struct PipelineRecord {
        std::uint64_t frame = 0, layoutVersion = 0;
        std::uint32_t stride = 0;
        std::array<Location, kFields.size()> locations{};
        std::unordered_map<MaterialManager::MaterialHandle, MaterialRecord> materials;
    };
    struct Input {
        MaterialManager::MaterialHandle material = 0;
        std::uint64_t version = 0;
        Vector4 color, rect, outline;
        Vector2 translate, scale, pivot;
        float rotation = 0, bold = 0, outlineWidth = 0;
        int blend = 0, combine = 0;
        std::uint32_t texture = 0, sampler = 0;
        bool overrideTexture = false;
        bool operator==(const Input &) const = default;
    };
    struct Batch {
        std::uint64_t used = 0, layoutVersion = 0;
        std::string pipeline;
        std::vector<Input> inputs;
        std::vector<std::byte> bytes;
    };
public:
    void BeginFrame() {
        ++frame_;
        textures_.clear();
        if (frame_ % 120 == 0) {
            std::erase_if(batches_, [this](const auto &p) { return p.second.used + 120 < frame_; });
            std::erase_if(pipelines_, [this](const auto &p) { return p.second.frame + 120 < frame_; });
            for (auto &[name, pipeline] : pipelines_) {
                (void)name;
                std::erase_if(pipeline.materials, [this](const auto &p) { return p.second.frame + 120 < frame_; });
            }
        }
    }
    void Clear() { pipelines_.clear(); batches_.clear(); textures_.clear(); }
    std::uint64_t GetPackCount() const { return packs_; }

    const std::vector<std::byte> &Pack(const std::string &key, const PipelineInfo &info,
        const std::string &name, std::span<const SceneRenderer::DrawEntry> entries) {
        Profiler::Scope scope("Render.MaterialPacking");
        auto &pipeline = pipelines_[name];
        if (pipeline.frame != frame_) {
            std::array<Location, kFields.size()> locations{};
            for (size_t i = 0; i < kFields.size(); ++i) {
                if (const auto *f = info.GetMaterialLayout().Find(kFields[i])) locations[i] = {f->byteOffset, f->byteSize};
            }
            if (locations != pipeline.locations || pipeline.stride != info.GetMaterialLayout().totalByteSize) ++pipeline.layoutVersion;
            pipeline.locations = locations;
            pipeline.stride = info.GetMaterialLayout().totalByteSize;
            pipeline.frame = frame_;
        }
        auto &batch = batches_[key];
        scratch_.clear(); scratch_.reserve(entries.size());
        for (const auto &entry : entries) {
            auto &record = pipeline.materials[entry.materialHandle];
            if (record.frame != frame_) {
                record.material = MaterialManager::GetMaterial(entry.materialHandle);
                if (record.material) record.material->ResolveTextureHandles();
                auto bytes = BuildMaterialElementBytes(info, record.material);
                if (bytes != record.bytes) { record.bytes = std::move(bytes); ++record.version; }
                record.uv = record.material ? record.material->MakeUVTransformMatrix() : Matrix4x4::Identity();
                record.texture = ResolveInstanceTextureIndex(TextureManager::kInvalidHandle, record.material);
                record.sampler = ResolveInstanceSamplerIndex(SamplerManager::kInvalidHandle, record.material);
                record.frame = frame_;
            }
            std::uint32_t texture = record.texture;
            if (entry.textureOverrideHandle != TextureManager::kInvalidHandle) {
                auto [it, inserted] = textures_.try_emplace(entry.textureOverrideHandle, 0);
                if (inserted) it->second = ResolveInstanceTextureIndex(entry.textureOverrideHandle, record.material);
                texture = it->second;
            }
            scratch_.push_back({entry.materialHandle, record.version, entry.instanceColor, entry.uvRect, entry.textOutlineColor,
                entry.instanceUvTranslate, entry.instanceUvScale, entry.instanceUvPivot, entry.instanceUvRotation,
                entry.boldWeight, entry.textOutlineWidth, entry.instanceColorBlendMode, entry.instanceUvCombineMode,
                texture, entry.samplerOverrideHandle == SamplerManager::kInvalidHandle ? record.sampler : ResolveInstanceSamplerIndex(entry.samplerOverrideHandle, record.material),
                entry.textureOverrideHandle != TextureManager::kInvalidHandle});
        }
        batch.used = frame_;
        if (batch.pipeline == name && batch.layoutVersion == pipeline.layoutVersion && batch.inputs == scratch_ &&
            batch.bytes.size() == size_t(pipeline.stride) * entries.size()) return batch.bytes;
        batch.pipeline = name; batch.layoutVersion = pipeline.layoutVersion; batch.inputs = scratch_;
        batch.bytes.resize(size_t(pipeline.stride) * entries.size()); ++packs_;
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto &entry = entries[i]; const auto &input = scratch_[i];
            const auto &record = pipeline.materials.at(entry.materialHandle);
            auto *dst = batch.bytes.data() + i * pipeline.stride;
            std::memcpy(dst, record.bytes.data(), pipeline.stride);
            auto write = [&]<class T>(size_t field, const T &value) {
                const auto &location = pipeline.locations[field];
                const auto size = std::min(location.size, static_cast<std::uint32_t>(sizeof(T)));
                if (std::uint64_t(location.offset) + size <= pipeline.stride) std::memcpy(dst + location.offset, &value, size);
            };
            write(0, input.color); write(1, float(input.blend));
            if (pipeline.locations[2].offset != UINT32_MAX) {
                if (entry.instanceUvTranslate == Vector2(0,0) && entry.instanceUvRotation == 0 && entry.instanceUvScale == Vector2(1,1)) write(2, record.uv);
                else write(2, ResolveInstanceUVTransform(entry, record.material));
            }
            write(3, input.color); write(4, input.rect); write(5, input.bold); write(6, input.outlineWidth);
            write(7, input.outline); write(8, input.texture); write(9, input.sampler);
            if (input.overrideTexture) write(10, 1.0f);
        }
        return batch.bytes;
    }
private:
    std::uint64_t frame_ = 0, packs_ = 0;
    std::unordered_map<std::string, PipelineRecord> pipelines_;
    std::unordered_map<std::string, Batch> batches_;
    std::unordered_map<TextureManager::TextureHandle, std::uint32_t> textures_;
    std::vector<Input> scratch_;
};
} // namespace KashipanEngine::RendererInternal
