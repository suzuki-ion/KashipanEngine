#include "RendererInternal.h"

namespace KashipanEngine {

using namespace RendererInternal;

/// @brief シャドウ用idSeedバッファを取得・構築する（RenderShadowMaps本体の描画と
///        RenderShadowMapsPhaseIntoの両方から呼ばれる共通処理）
/// @param phaseOffset baseIdSeeds各要素へ加算する位相オフセット（通常描画では0.0）
/// @param passIndex キャッシュキーへ含めるパス番号（-1の場合は付与しない）。画面全体Nパス
///        ブレンドで位相だけ異なる複数パスを描画する際、バッファを使い回すとCPU側の書き込みが
///        GPU実行前に競合してしまうため、パスごとに別バッファへ分離する
StructuredBufferResource *Renderer::BuildShadowIdSeedBuffer(const PreparedShadowBatch &batch, float phaseOffset, int passIndex) {
    if (batch.idSeedKeyBase.empty() || batch.baseIdSeeds.empty()) return nullptr;
    std::string key = batch.idSeedKeyBase;
    if (passIndex >= 0) {
        char passSuffix[24];
        std::snprintf(passSuffix, sizeof(passSuffix), "|swd%d", passIndex);
        key += passSuffix;
    }
    std::vector<float> idSeeds(batch.baseIdSeeds.size());
    for (size_t i = 0; i < batch.baseIdSeeds.size(); ++i) {
        idSeeds[i] = batch.baseIdSeeds[i] + phaseOffset;
    }
    return resourceContainer_->GetOrUpdateStructuredBuffer(key, sizeof(float), static_cast<std::uint32_t>(idSeeds.size()), idSeeds.data());
}

void Renderer::RenderShadowMaps(SceneContext *sceneContext, SceneRenderer *sceneRenderer,
    const std::vector<IRenderTarget *> &targets) {
    (void)sceneContext;
    shadowJobs_.clear();
    targetShadowEntries_.clear();
    shadowBatches_.clear();
    shadowGpuParticleBatches_.clear();
    if (!sceneRenderer || !directXCommon_ || !pipelineManager_) return;

    constexpr const char *kShadowPipelineName = "Object3D.ShadowMap.DepthOnly";
    /// 1フレームで使えるシャドウマップ配列のスライス数の予算
    constexpr std::uint32_t kMaxShadowSlices = 64;
    /// シャドウマップ配列のメモリ予算（超える場合は解像度を自動で下げる）
    constexpr std::uint64_t kShadowMemoryBudgetBytes = 512ull * 1024 * 1024;

    // コマンドスロットの確保（初回のみ）
    auto ensureShadowCommands = [this]() -> DX12Commands * {
        if (shadowCommandSlotIndex_ < 0) {
            shadowCommandSlotIndex_ = directXCommon_->AcquireCommandObjects(Passkey<Renderer>{});
            shadowCommands_ = (shadowCommandSlotIndex_ >= 0)
                ? directXCommon_->GetCommandObjects(Passkey<Renderer>{}, shadowCommandSlotIndex_)
                : nullptr;
        }
        return shadowCommands_;
    };

    // 影ジョブが1件も無いフレームでも、シェーダーの gShadowMaps（Texture2DArray）へバインドできる
    // 最小のダミー配列を用意してSRV状態へ遷移させておく（未バインドのデスクリプタアクセス防止）
    auto ensureFallbackArray = [this, &ensureShadowCommands]() {
        if (shadowMapArray_) return;
        // SRVがTexture2DArrayとして作られるようにスライス数は2以上にする
        shadowMapArray_ = std::make_unique<DepthStencilResource>(
            64, 64, DXGI_FORMAT_D32_FLOAT, 1.0f, static_cast<UINT8>(0),
            nullptr, true, DXGI_FORMAT_R32_FLOAT, 2);
        if (!shadowMapArray_ || !shadowMapArray_->HasSrv()) {
            shadowMapArray_.reset();
            return;
        }
        shadowArrayResolution_ = 64;
        shadowArraySliceCount_ = 2;
        shadowArrayReady_ = false;
        auto *commands = ensureShadowCommands();
        auto *commandList = commands ? commands->BeginRecord() : nullptr;
        if (!commandList) return;
        shadowMapArray_->SetCommandList(commandList);
        shadowMapArray_->TransitionToShaderResource();
        if (commands->EndRecord()) {
            directXCommon_->AddRecordCommandList(Passkey<Renderer>{}, commands->GetCommandList());
            shadowArrayReady_ = true;
        }
    };

    //--------- 解像度の決定（影を生成する全ライトの最大値。メモリ予算超過時は自動で下げる） ---------//
    std::uint32_t resolution = 0;
    std::uint64_t estimatedSlices = 0;
    if (pipelineManager_->HasPipeline(kShadowPipelineName)) {
        for (auto *lightRenderer : sceneRenderer->GetLightRenderers()) {
            if (!lightRenderer || !lightRenderer->IsActive()) continue;
            auto *light = lightRenderer->GetLight();
            if (!light || !light->IsActive() || !light->IsCastShadows()) continue;
            resolution = std::max(resolution, light->GetShadowMapResolution());
            switch (light->GetType()) {
                case Light::Type::Directional: estimatedSlices += kShadowCascadeCount; break;
                case Light::Type::Point:
                case Light::Type::Sphere:
                case Light::Type::Tube:
                case Light::Type::Box: estimatedSlices += 6; break;
                default: estimatedSlices += 1; break;
            }
        }
    }
    if (resolution == 0 || estimatedSlices == 0) {
        ensureFallbackArray();
        return;
    }
    resolution = std::clamp(resolution, 256u, 4096u);
    const std::uint64_t budgetSlices = std::min<std::uint64_t>(estimatedSlices, kMaxShadowSlices);
    while (resolution > 256 &&
           static_cast<std::uint64_t>(resolution) * resolution * 4ull * budgetSlices > kShadowMemoryBudgetBytes) {
        resolution /= 2;
    }

    // 行ベクトル規約でNDC座標をワールド座標へ逆射影する
    const auto unprojectNdc = [](const Matrix4x4 &invViewProjection, float x, float y, float z) {
        const auto &m = invViewProjection.m;
        const float outX = x * m[0][0] + y * m[1][0] + z * m[2][0] + m[3][0];
        const float outY = x * m[0][1] + y * m[1][1] + z * m[2][1] + m[3][1];
        const float outZ = x * m[0][2] + y * m[1][2] + z * m[2][2] + m[3][2];
        const float outW = x * m[0][3] + y * m[1][3] + z * m[2][3] + m[3][3];
        const float invW = (std::fabs(outW) > 1e-6f) ? (1.0f / outW) : 1.0f;
        return Vector3(outX * invW, outY * invW, outZ * invW);
    };
    // ライトの向きからビュー行列（左手系の正規直交基底）を作る
    const auto makeLightView = [](const Vector3 &forward, const Vector3 &eye) {
        const Vector3 baseUp = (std::fabs(forward.y) > 0.99f) ? Vector3(0.0f, 0.0f, 1.0f) : Vector3(0.0f, 1.0f, 0.0f);
        const Vector3 right = baseUp.Cross(forward).Normalize();
        const Vector3 up = forward.Cross(right);
        const Matrix4x4 world(
            right.x, right.y, right.z, 0.0f,
            up.x, up.y, up.z, 0.0f,
            forward.x, forward.y, forward.z, 0.0f,
            eye.x, eye.y, eye.z, 1.0f);
        return world.Inverse();
    };

    //--------- 描画先ごとに「その描画先で使うカメラ・ライト」から影ジョブを構築する ---------//
    // ジョブの共有キー: Directionalはカメラ依存のため（ライト, カメラ）、Spot/Pointは（ライト, null）
    std::map<std::pair<const LightRenderer *, const void *>, int> jobIndexByKey;
    std::uint32_t nextSlice = 0;

    for (auto *target : targets) {
        if (!target) continue;

        // 描画先で使うカメラの解決（エディター用描画先はエディターカメラを使用する）
        Matrix4x4 cameraViewProjection = Matrix4x4::Identity();
        Vector3 cameraPosition(0.0f, 0.0f, 0.0f);
        float cameraNear = 0.1f;
        float cameraFar = 1000.0f;
        bool cameraValid = false;
        const void *cameraKey = nullptr;
        if (const auto *editorInfo = sceneRenderer->GetEditorCameraInfo(target)) {
            cameraViewProjection = editorInfo->viewProjection;
            cameraPosition = editorInfo->position;
            cameraNear = editorInfo->nearClip;
            cameraFar = editorInfo->farClip;
            cameraValid = true;
            cameraKey = target;
        } else {
            for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
                if (!cameraRenderer || !cameraRenderer->IsActive()) continue;
                if (IsExcludedAsEditorOnly(cameraRenderer, target, sceneRenderer)) continue;
                if (!IsTargetMatch(cameraRenderer->GetTargetObject(), cameraRenderer->GetTargetObjectID().IsValid(), target, sceneRenderer)) continue;
                if (!cameraRenderer->IsRenderTargetIncluded(target)) continue;
                cameraViewProjection = cameraRenderer->GetViewProjectionMatrix();
                cameraPosition = cameraRenderer->GetWorldPosition();
                cameraNear = cameraRenderer->GetNearClip();
                cameraFar = cameraRenderer->GetFarClip();
                cameraValid = true;
                cameraKey = cameraRenderer;
                break;
            }
        }
        if (!cameraValid) continue; // カメラの無い描画先（シャドウマップ等）には影を適用しない
        cameraNear = std::max(0.01f, cameraNear);
        cameraFar = std::max(cameraNear + 0.01f, cameraFar);

        // この描画先に適用される「影を生成するライト」を収集する
        std::vector<LightRenderer *> candidates;
        for (auto *lightRenderer : sceneRenderer->GetLightRenderers()) {
            if (!lightRenderer || !lightRenderer->IsActive()) continue;
            auto *light = lightRenderer->GetLight();
            if (!light || !light->IsActive() || !light->IsCastShadows()) continue;
            if (IsExcludedAsEditorOnly(lightRenderer, target, sceneRenderer)) continue;
            if (!IsTargetMatch(lightRenderer->GetTargetObject(), lightRenderer->GetTargetObjectID().IsValid(), target, sceneRenderer)) continue;
            if (!lightRenderer->IsRenderTargetIncluded(target)) continue;
            candidates.push_back(lightRenderer);
        }
        if (candidates.empty()) continue;

        // ライトが多すぎる場合はカメラに近い順に優先する
        // （Directionalは画面全体に影響するため距離に関わらず最優先とする）
        std::stable_sort(candidates.begin(), candidates.end(),
            [&cameraPosition](LightRenderer *a, LightRenderer *b) {
                const bool aDirectional = a->GetLight()->GetType() == Light::Type::Directional;
                const bool bDirectional = b->GetLight()->GetType() == Light::Type::Directional;
                if (aDirectional != bDirectional) return aDirectional;
                const Vector3 toA = a->GetWorldPosition() - cameraPosition;
                const Vector3 toB = b->GetWorldPosition() - cameraPosition;
                return toA.Dot(toA) < toB.Dot(toB);
            });

        // 視錐台コーナー（Directionalのカスケード計算用。必要になった時点で一度だけ計算する）
        bool cornersComputed = false;
        Vector3 nearCorners[4]{};
        Vector3 farCorners[4]{};
        constexpr float kCornerX[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
        constexpr float kCornerY[4] = { 1.0f, 1.0f, -1.0f, -1.0f };

        auto &entries = targetShadowEntries_[target];
        for (auto *lightRenderer : candidates) {
            if (entries.size() >= kMaxShadowLightsPerTarget) break;
            auto *light = lightRenderer->GetLight();
            const Light::Type type = light->GetType();

            const void *jobCameraKey = (type == Light::Type::Directional) ? cameraKey : nullptr;
            const auto mapKey = std::make_pair(static_cast<const LightRenderer *>(lightRenderer), jobCameraKey);
            int jobIndex = -1;
            if (auto found = jobIndexByKey.find(mapKey); found != jobIndexByKey.end()) {
                jobIndex = found->second;
            } else {
                const std::uint32_t neededSlices =
                    (type == Light::Type::Directional) ? kShadowCascadeCount :
                    (type == Light::Type::Point || type == Light::Type::Sphere || type == Light::Type::Tube || type == Light::Type::Box) ? 6u : 1u;
                if (nextSlice + neededSlices > kMaxShadowSlices) continue; // スライス予算切れ（優先度の低いライトから影を諦める）

                ShadowJobData job{};
                job.baseSlice = nextSlice;
                job.sliceCount = neededSlices;

                if (type == Light::Type::Directional) {
                    //--------- Directional: カメラ視錐台をカスケード分割して正射影をフィットさせる ---------//
                    job.lightType = 0;
                    if (!cornersComputed) {
                        const Matrix4x4 invViewProjection = cameraViewProjection.Inverse();
                        for (int i = 0; i < 4; ++i) {
                            nearCorners[i] = unprojectNdc(invViewProjection, kCornerX[i], kCornerY[i], 0.0f);
                            farCorners[i] = unprojectNdc(invViewProjection, kCornerX[i], kCornerY[i], 1.0f);
                        }
                        cornersComputed = true;
                    }

                    // カスケード分割距離（対数分割と均等分割のブレンド）
                    const float maxDistance = std::clamp(light->GetShadowDistance(), cameraNear + 0.01f, cameraFar);
                    float splitDistances[kShadowCascadeCount + 1];
                    splitDistances[0] = cameraNear;
                    constexpr float kSplitLambda = 0.75f;
                    for (std::uint32_t i = 1; i <= kShadowCascadeCount; ++i) {
                        const float p = static_cast<float>(i) / static_cast<float>(kShadowCascadeCount);
                        const float logDistance = cameraNear * std::pow(maxDistance / cameraNear, p);
                        const float uniformDistance = cameraNear + (maxDistance - cameraNear) * p;
                        splitDistances[i] = kSplitLambda * logDistance + (1.0f - kSplitLambda) * uniformDistance;
                    }

                    const Vector3 lightDirection = lightRenderer->GetWorldDirection();
                    for (std::uint32_t c = 0; c < kShadowCascadeCount; ++c) {
                        // このカスケードが覆う視錐台スライスの8頂点（視錐台の辺に沿った線形補間で求まる）
                        const float tNear = (splitDistances[c] - cameraNear) / (cameraFar - cameraNear);
                        const float tFar = (splitDistances[c + 1] - cameraNear) / (cameraFar - cameraNear);
                        Vector3 corners[8];
                        for (int i = 0; i < 4; ++i) {
                            corners[i] = nearCorners[i] + (farCorners[i] - nearCorners[i]) * tNear;
                            corners[i + 4] = nearCorners[i] + (farCorners[i] - nearCorners[i]) * tFar;
                        }

                        // スライスを内包する球で正射影範囲をフィットさせる（カメラの回転に対して影が安定する）
                        Vector3 center(0.0f, 0.0f, 0.0f);
                        for (const auto &corner : corners) center = center + corner;
                        center = center * (1.0f / 8.0f);
                        float radius = 0.0f;
                        for (const auto &corner : corners) radius = std::max(radius, (corner - center).Length());
                        radius = std::ceil(radius * 16.0f) / 16.0f;
                        radius = std::max(radius, 0.5f);

                        // ライト後方へ引いた位置から深度範囲を確保する（スライス外のキャスターも影を落とせるように）
                        const float backDistance = radius * 2.0f + 10.0f;
                        const Vector3 eye = center - lightDirection * backDistance;
                        const Matrix4x4 lightView = makeLightView(lightDirection, eye);
                        Matrix4x4 lightProjection;
                        lightProjection.MakeOrthographicMatrix(-radius, radius, radius, -radius, 0.0f, backDistance + radius);
                        Matrix4x4 viewProjection = lightView * lightProjection;

                        // 投影原点をテクセル単位にスナップしてカメラ移動時の影のちらつきを抑える
                        const float halfResolution = static_cast<float>(resolution) * 0.5f;
                        const float offsetX = viewProjection.m[3][0] * halfResolution;
                        const float offsetY = viewProjection.m[3][1] * halfResolution;
                        viewProjection.m[3][0] += (std::round(offsetX) - offsetX) / halfResolution;
                        viewProjection.m[3][1] += (std::round(offsetY) - offsetY) / halfResolution;

                        job.viewProjections[c] = viewProjection;
                        job.eyePositions[c] = eye;
                        job.cascadeSplits[c] = splitDistances[c + 1];
                        // 深度バイアス係数: 1テクセルのワールドサイズを正射影の深度レンジで正規化した値。
                        // シェーダー側で「テクセル数×この値」をNDC深度バイアスとして使うことで、
                        // カスケードの大きさに関わらずワールド空間で一定（テクセル比例）のバイアスになり、
                        // 影が浮くピーターパン現象を防ぐ
                        const float texelWorldSize = (radius * 2.0f) / static_cast<float>(resolution);
                        job.cascadeBiasScales[c] = (texelWorldSize / (backDistance + radius)) * light->GetShadowBias();
                    }
                } else if (type == Light::Type::Spot || type == Light::Type::Disc || type == Light::Type::Rect) {
                    //--------- Spot: ライト位置からコーン方向への透視投影1面 ---------//
                    //--------- Disc/Rect: 片面（半球）発光を、法線方向への広角(約175度)の単一透視投影で近似する ---------//
                    job.lightType = 1;
                    const Vector3 lightDirection = lightRenderer->GetWorldDirection();
                    const Vector3 lightPosition = lightRenderer->GetWorldPosition();
                    // 外側コーン角の2倍を画角にする（コーン全体を覆う）。Disc/Rectはコーン角の概念が無いため固定の広角を使う
                    const float fovY = (type == Light::Type::Spot)
                        ? std::clamp(light->GetOuterAngle() * 2.0f, 0.05f, 3.1f)
                        : 3.05f;
                    const float nearZ = 0.05f;
                    const float farZ = std::max(nearZ + 0.1f, light->GetDistance());
                    Matrix4x4 lightProjection;
                    lightProjection.MakePerspectiveFovMatrix(fovY, 1.0f, nearZ, farZ);
                    job.viewProjections[0] = makeLightView(lightDirection, lightPosition) * lightProjection;
                    job.eyePositions[0] = lightPosition;
                    // 深度バイアス係数: シェーダー側でビュー深度wで割ることで、その距離での
                    // 1テクセルのワールドサイズに比例したNDC深度バイアスになる
                    // （透視投影はNDC深度が非線形のため、定数NDCバイアスだと遠方で影が大きく浮いてしまう）
                    job.perspectiveBiasScale = (2.0f * std::tan(fovY * 0.5f) / static_cast<float>(resolution))
                        * (nearZ * farZ / (farZ - nearZ)) * light->GetShadowBias();
                } else {
                    //--------- Point/Sphere: ライト位置からキューブ6面（画角90度）の透視投影 ---------//
                    //--------- Tube: 中心点からのキューブ6面で近似し、遠平面をチューブの半長分拡張する ---------//
                    //--------- Box: 中心点からのキューブ6面で近似し、遠平面をボックスの半対角分拡張する ---------//
                    job.lightType = 2;
                    const Vector3 lightPosition = lightRenderer->GetWorldPosition();
                    const float rangeExtension = (type == Light::Type::Tube) ? (light->GetSourceLength() * 0.5f) :
                        (type == Light::Type::Box) ? Vector3(light->GetSourceWidth(), light->GetSourceHeight(), light->GetSourceDepth()).Length() * 0.5f : 0.0f;
                    const float nearZ = 0.05f;
                    const float farZ = std::max(nearZ + 0.1f, light->GetRadius() + rangeExtension);
                    Matrix4x4 lightProjection;
                    lightProjection.MakePerspectiveFovMatrix(1.5707963f, 1.0f, nearZ, farZ);
                    // 深度バイアス係数（Spotと同様。画角90度なので tan(fov/2) = 1）
                    job.perspectiveBiasScale = (2.0f / static_cast<float>(resolution))
                        * (nearZ * farZ / (farZ - nearZ)) * light->GetShadowBias();
                    // 面の並び順はシェーダー側の面選択（+X,-X,+Y,-Y,+Z,-Z）と一致させること
                    const Vector3 kFaceDirections[6] = {
                        Vector3(1.0f, 0.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f),
                        Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f),
                        Vector3(0.0f, 0.0f, 1.0f), Vector3(0.0f, 0.0f, -1.0f),
                    };
                    for (int face = 0; face < 6; ++face) {
                        job.viewProjections[face] = makeLightView(kFaceDirections[face], lightPosition) * lightProjection;
                        job.eyePositions[face] = lightPosition;
                    }
                }

                shadowJobs_.push_back(job);
                jobIndex = static_cast<int>(shadowJobs_.size()) - 1;
                jobIndexByKey.emplace(mapKey, jobIndex);
                nextSlice += neededSlices;
            }
            entries.push_back({ lightRenderer, jobIndex });
        }
        if (entries.empty()) targetShadowEntries_.erase(target);
    }

    if (shadowJobs_.empty()) {
        targetShadowEntries_.clear();
        ensureFallbackArray();
        return;
    }
    // 画面全体Nパスブレンド（RenderShadowMapsPhaseInto）のビューポート計算用に保持しておく
    shadowResolutionForRedraw_ = resolution;

    //--------- シャドウマップ配列（Texture2DArray）の生成・拡張 ---------//
    // SRVがTexture2DArrayとして作られるようにスライス数は2以上にする
    const std::uint32_t requiredSlices = std::max(2u, nextSlice);
    if (!shadowMapArray_ || shadowArrayResolution_ != resolution || shadowArraySliceCount_ < requiredSlices) {
        // 頻繁な作り直しを避けるため、同解像度の場合はスライス数を拡大方向にのみ変更する
        const std::uint32_t newSliceCount = (shadowMapArray_ && shadowArrayResolution_ == resolution)
            ? std::max(requiredSlices, shadowArraySliceCount_)
            : requiredSlices;
        shadowMapArray_ = std::make_unique<DepthStencilResource>(
            resolution, resolution, DXGI_FORMAT_D32_FLOAT, 1.0f, static_cast<UINT8>(0),
            nullptr, true, DXGI_FORMAT_R32_FLOAT, newSliceCount);
        if (!shadowMapArray_ || !shadowMapArray_->HasSrv()) {
            shadowMapArray_.reset();
            shadowArrayResolution_ = 0;
            shadowArraySliceCount_ = 0;
            shadowArrayReady_ = false;
            shadowJobs_.clear();
            targetShadowEntries_.clear();
            return;
        }
        shadowArrayResolution_ = resolution;
        shadowArraySliceCount_ = newSliceCount;
        shadowArrayReady_ = false;
    }

    //--------- 影を落とす3Dオブジェクトの収集（MeshRenderer / SkinnedMeshRenderer） ---------//
    struct ShadowDrawSource {
        ModelManager::ModelHandle meshHandle = ModelManager::kInvalidHandle;
        MaterialManager::MaterialHandle materialHandle = MaterialManager::kInvalidHandle;
        Matrix4x4 worldMatrix;
        RWStructuredBufferResource *skinnedVertexBuffer = nullptr;
        /// @brief 描画するインデックス範囲（サブメッシュ。indexCount==0の場合はメッシュ全体）
        std::uint32_t indexStart = 0;
        std::uint32_t indexCount = 0;
        /// @brief オブジェクト固有のディザ用シード値（本体描画と同じ値。ShadowMapPS.hlsl参照）
        float idSeed = 0.0f;
    };
    std::vector<ShadowDrawSource> sources;
    const auto isShadowCastingPipeline = [](const std::string &name) {
        // 3Dオブジェクト描画用パイプラインのみ対象（シャドウマップ用パイプライン自体は除外）
        return name.rfind("Object3D", 0) == 0 && name.rfind("Object3D.ShadowMap", 0) != 0;
    };
    // サブメッシュ（マテリアルごとのインデックス範囲）ごとに1件収集する
    const auto appendShadowSources = [&sources](auto *renderer, RWStructuredBufferResource *skinnedVertexBuffer) {
        const auto &subMeshes = ModelManager::GetModelData(renderer->GetMeshHandle()).GetSubMeshes();
        const size_t subMeshCount = std::max<size_t>(1, subMeshes.size());
        const float idSeed = SceneRenderer::ObjectIdSeedFor(renderer->GetOwnerObject());
        for (size_t subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
            ShadowDrawSource source;
            source.meshHandle = renderer->GetMeshHandle();
            source.materialHandle = renderer->GetMaterialHandleAt(subMeshIndex);
            source.worldMatrix = renderer->GetWorldMatrix();
            source.skinnedVertexBuffer = skinnedVertexBuffer;
            if (!subMeshes.empty()) {
                source.indexStart = subMeshes[subMeshIndex].indexStart;
                source.indexCount = subMeshes[subMeshIndex].indexCount;
            }
            source.idSeed = idSeed;
            sources.push_back(source);
        }
    };
    for (auto *renderer : sceneRenderer->GetMeshRenderers()) {
        if (!renderer || !renderer->IsActive()) continue;
        if (!renderer->GetCastShadows()) continue;
        if (renderer->GetMeshHandle() == ModelManager::kInvalidHandle) continue;
        if (!isShadowCastingPipeline(renderer->GetPipelineName())) continue;
        appendShadowSources(renderer, nullptr);
    }
    for (auto *renderer : sceneRenderer->GetSkinnedMeshRenderers()) {
        if (!renderer || !renderer->IsActive()) continue;
        if (!renderer->GetCastShadows()) continue;
        if (renderer->GetMeshHandle() == ModelManager::kInvalidHandle) continue;
        if (!renderer->HasValidSkinningData()) continue;
        if (!isShadowCastingPipeline(renderer->GetPipelineName())) continue;
        appendShadowSources(renderer, renderer->GetSkinnedVertexBuffer());
    }

    // 同一（メッシュ・サブメッシュ）をまとめてインスタンシング描画できるようにソート。
    // マテリアルは各インスタンスが自分自身のものを参照できる（後段のマテリアル構造化バッファ構築
    // 参照）ため、ソート・バッチ結合条件のどちらにも含めない
    std::stable_sort(sources.begin(), sources.end(),
        [](const ShadowDrawSource &a, const ShadowDrawSource &b) {
            if (a.skinnedVertexBuffer != b.skinnedVertexBuffer) return a.skinnedVertexBuffer < b.skinnedVertexBuffer;
            if (a.meshHandle != b.meshHandle) return a.meshHandle < b.meshHandle;
            return a.indexStart < b.indexStart;
        });

    //--------- コマンド記録開始 ---------//
    auto *commands = ensureShadowCommands();
    auto *commandList = commands ? commands->BeginRecord() : nullptr;
    if (!commandList) {
        shadowJobs_.clear();
        targetShadowEntries_.clear();
        return;
    }

    // このコマンドリストはフレームごとにResetされるため、ヒープとパイプラインの状態を毎回設定する
    auto *srvHeap = IGraphicsResource::GetSRVHeap(Passkey<Renderer>{});
    auto *samplerHeap = IGraphicsResource::GetSamplerHeap(Passkey<Renderer>{});
    if (srvHeap && samplerHeap) {
        ID3D12DescriptorHeap *heaps[] = { srvHeap->GetDescriptorHeap(), samplerHeap->GetDescriptorHeap() };
        commandList->SetDescriptorHeaps(_countof(heaps), heaps);
    }
    PipelineBinder pipelineBinder(commandList, pipelineManager_);
    pipelineBinder.Invalidate();
    pipelineBinder.UsePipeline(kShadowPipelineName);
    auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, kShadowPipelineName);
    shaderBinder.SetCommandList(commandList);

    // バインドレステクスチャ配列（Texture2D gTextures[]）のテーブル。RendererDraw.cpp::DrawBatchと同様、
    // ヒープ先頭の予約レンジ全体を指す固定ハンドルのため、このコマンドリスト上で一度だけバインドすればよい
    shaderBinder.Bind("Pixel:gTextures", directXCommon_->GetTextureBindlessBaseHandleForRenderer(Passkey<Renderer>{}));
    shaderBinder.Bind("Pixel:gSamplers", directXCommon_->GetSamplerBindlessBaseHandleForRenderer(Passkey<Renderer>{}));

    // ブルーノイズによるディザ閾値テーブル（通常描画ではBindCameraAndLightsが毎回バインドするが、
    // シャドウマップ描画はそちらを経由しないため、ここで明示的にバインドしておく必要がある。
    // 忘れるとgBlueNoiseDitherの参照先が未バインドのまま（前のパイプラインの使い回し等で不定）になり、
    // 閾値が画素によらずほぼ一定の値になって、影がアルファに関わらず2値的にON/OFFしてしまう）
    if (blueNoiseGenerator_.IsReady()) {
        shaderBinder.Bind("Pixel:gBlueNoiseDither", blueNoiseGenerator_.GetResultBuffer());
    }

    // グローバル時刻定数（gTime）。同様にBindCameraAndLightsを経由しないため、ここで明示的に
    // アップロード・バインドする。忘れるとShadowMapPS.hlsl側のgTimeが未バインドのまま（不定値で
    // 固定）になり、enableTemporalDitherを有効にしても切り替えた瞬間しか模様が変わらず、
    // 以後フレームが進んでも位相が動き続けない（無相関化が機能しない）
    {
        TimeConstantsData timeData;
        timeData.time = static_cast<float>(GetGameRuntimeMillisecond()) / 1000.0f;
        timeData.deltaTime = GetDeltaTime();
        auto *timeBuffer = resourceContainer_->GetOrCreateConstantBuffer("ShadowTimeConstants", sizeof(TimeConstantsData));
        if (timeBuffer) {
            if (auto *mapped = timeBuffer->Map()) {
                std::memcpy(mapped, &timeData, sizeof(timeData));
                shaderBinder.Bind("Vertex:TimeConstants", timeBuffer);
                shaderBinder.Bind("Pixel:TimeConstants", timeBuffer);
            }
        }
    }

    //--------- バッチごとのGPUバッファ準備（全ジョブ・全スライスで共有する） ---------//
    // PreparedShadowBatch/PreparedGpuParticleShadowBatchはRenderer.hで定義（画面全体Nパスブレンド
    // ＝RenderShadowMapsPhaseIntoが位相違いで再利用できるよう、メンバー変数として保持する）
    auto &batches = shadowBatches_;
    // マテリアルはバッチ結合条件から外れている（異なるマテリアルのインスタンスが同一バッチに
    // 混在し得る）ため、固定フィールドをインスタンスごとに自分自身のマテリアルから解決する。
    // BuildMaterialElementBytes自体を毎インスタンスで再実行しないよう、このRenderShadowMaps呼び出し
    // （＝1フレーム）の間だけ(パイプライン, マテリアルハンドル)単位でテンプレートを使い回す
    MaterialTemplateCache shadowMaterialTemplateCache;
    {
        size_t begin = 0;
        std::uint32_t batchIndex = 0;
        while (begin < sources.size()) {
            const auto &first = sources[begin];
            size_t end = begin;
            while (end < sources.size() &&
                   sources[end].meshHandle == first.meshHandle &&
                   sources[end].skinnedVertexBuffer == first.skinnedVertexBuffer &&
                   sources[end].indexStart == first.indexStart &&
                   sources[end].indexCount == first.indexCount) {
                ++end;
            }
            const std::uint32_t instanceCount = static_cast<std::uint32_t>(end - begin);

            const auto *meshBuffers = resourceContainer_->GetOrCreateMeshBuffers(first.meshHandle);
            if (!meshBuffers || !meshBuffers->indexBuffer ||
                (!first.skinnedVertexBuffer && !meshBuffers->vertexBuffer)) {
                begin = end;
                continue;
            }

            PreparedShadowBatch batch;
            batch.meshBuffers = meshBuffers;
            batch.skinnedVertexBuffer = first.skinnedVertexBuffer;
            batch.instanceCount = instanceCount;
            batch.indexStart = first.indexStart;
            batch.indexCount = first.indexCount;

            // ワールド空間の集合境界球を計算する（全スライス共通で1回だけ計算し、スライス単位の
            // フラスタムカリングで「このバッチが完全に視錐台の外側にあるか」を判定するのに使う）
            {
                const auto transformPoint = [](const Matrix4x4 &world, const Vector3 &p) {
                    return Vector3(
                        p.x * world.m[0][0] + p.y * world.m[1][0] + p.z * world.m[2][0] + world.m[3][0],
                        p.x * world.m[0][1] + p.y * world.m[1][1] + p.z * world.m[2][1] + world.m[3][1],
                        p.x * world.m[0][2] + p.y * world.m[1][2] + p.z * world.m[2][2] + world.m[3][2]);
                };
                const auto maxAxisScale = [](const Matrix4x4 &world) {
                    const auto lenSq = [](float x, float y, float z) { return x * x + y * y + z * z; };
                    const float s0 = lenSq(world.m[0][0], world.m[0][1], world.m[0][2]);
                    const float s1 = lenSq(world.m[1][0], world.m[1][1], world.m[1][2]);
                    const float s2 = lenSq(world.m[2][0], world.m[2][1], world.m[2][2]);
                    return std::sqrt(std::max({ s0, s1, s2 }));
                };

                std::vector<Vector3> instanceCenters(instanceCount);
                std::vector<float> instanceRadii(instanceCount);
                Vector3 avgCenter{ 0.0f, 0.0f, 0.0f };
                for (size_t i = begin; i < end; ++i) {
                    const Vector3 center = transformPoint(sources[i].worldMatrix, meshBuffers->boundsCenter);
                    const float radius = meshBuffers->boundsRadius * maxAxisScale(sources[i].worldMatrix);
                    instanceCenters[i - begin] = center;
                    instanceRadii[i - begin] = radius;
                    avgCenter.x += center.x; avgCenter.y += center.y; avgCenter.z += center.z;
                }
                avgCenter.x /= static_cast<float>(instanceCount);
                avgCenter.y /= static_cast<float>(instanceCount);
                avgCenter.z /= static_cast<float>(instanceCount);
                float maxRadius = 0.0f;
                for (std::uint32_t i = 0; i < instanceCount; ++i) {
                    const float dx = instanceCenters[i].x - avgCenter.x;
                    const float dy = instanceCenters[i].y - avgCenter.y;
                    const float dz = instanceCenters[i].z - avgCenter.z;
                    const float dist = std::sqrt(dx * dx + dy * dy + dz * dz) + instanceRadii[i];
                    maxRadius = std::max(maxRadius, dist);
                }
                batch.boundsCenter = avgCenter;
                batch.boundsRadius = maxRadius;
            }

            // ワールド行列のインスタンスバッファ（カスケード間で内容は共通）
            char key[64];
            std::snprintf(key, sizeof(key), "ShadowPass|%u|transform", batchIndex);
            batch.transformBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, sizeof(Matrix4x4), instanceCount);
            if (batch.transformBuffer) {
                if (auto *mapped = static_cast<Matrix4x4 *>(batch.transformBuffer->Map())) {
                    for (size_t i = begin; i < end; ++i) {
                        mapped[i - begin] = sources[i].worldMatrix;
                    }
                }
            }

            // オブジェクト固有のディザ用シード値（本体描画のgObjectIdSeedsと同じ考え方。
            // ShadowMapPS.hlslが本体と同じ閾値テーブル・同じ位相で影の濃さをアルファに応じて薄くする）。
            // 実際のGPUバッファは基準値だけをここで保持し、位相オフセットを加えた実バッファは
            // 描画直前にBuildShadowIdSeedBufferで構築する（画面全体Nパスブレンドでは位相ごとに
            // 別バッファへ分離する必要があるため。RenderShadowMapsPhaseInto参照）
            std::snprintf(key, sizeof(key), "ShadowPass|%u|idSeed", batchIndex);
            batch.idSeedKeyBase = key;
            batch.baseIdSeeds.resize(instanceCount);
            for (size_t i = begin; i < end; ++i) {
                batch.baseIdSeeds[i - begin] = sources[i].idSeed;
            }

            // マテリアルの構造化バッファ（シャドウマップ用PSがアルファ抜きに使用する）。マテリアルは
            // バッチ結合条件から外れているため、インスタンスごとに自分自身のマテリアルを解決する
            // （textureIndex/samplerIndexしか使わないシャドウ用PSでも、テクスチャが違うインスタンスが
            // 同一バッチに混在し得るため必須）
            const auto &shadowPipelineInfo = pipelineManager_->GetPipeline(kShadowPipelineName);
            const std::uint32_t materialStride = shadowPipelineInfo.GetMaterialLayout().totalByteSize;
            std::snprintf(key, sizeof(key), "ShadowPass|%u|material", batchIndex);
            if (materialStride > 0) {
                std::vector<std::byte> allBytes(static_cast<size_t>(materialStride) * instanceCount);
                for (size_t i = begin; i < end; ++i) {
                    auto *material = MaterialManager::GetMaterial(sources[i].materialHandle);
                    const auto &elementTemplate = shadowMaterialTemplateCache.Get(shadowPipelineInfo, kShadowPipelineName, sources[i].materialHandle);
                    std::byte *elementBytes = allBytes.data() + (i - begin) * materialStride;
                    std::memcpy(elementBytes, elementTemplate.data(), materialStride);
                    WriteMaterialField(shadowPipelineInfo, elementBytes, materialStride,
                        "textureIndex", ResolveInstanceTextureIndex(TextureManager::kInvalidHandle, material));
                    WriteMaterialField(shadowPipelineInfo, elementBytes, materialStride,
                        "samplerIndex", ResolveInstanceSamplerIndex(SamplerManager::kInvalidHandle, material));
                }
                batch.materialBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, materialStride, instanceCount);
                if (batch.materialBuffer) {
                    if (auto *mapped = static_cast<std::byte *>(batch.materialBuffer->Map())) {
                        std::memcpy(mapped, allBytes.data(), allBytes.size());
                    }
                }
            }

            if (batch.transformBuffer && !batch.baseIdSeeds.empty() && batch.materialBuffer) {
                batches.push_back(std::move(batch));
                ++batchIndex;
            }
            begin = end;
        }
    }

    //--------- GPUパーティクルの影キャスターを収集する ---------//
    // GPUパーティクルは通常描画（RenderGpuParticles）と同じ考え方で、CPU側にワールド行列を
    // 持たない（gpuInstanceMatrixBuffer_をそのままgTransformationMatricesとしてバインドする）。
    // マテリアルはエミッター全体で1つのため、instanceCount分だけ複製したバッファを用意する
    // （通常のMeshRenderer由来バッチと違い、他のエミッターとまとめてインスタンシングはできない）。
    // PreparedGpuParticleShadowBatchはRenderer.hで定義（画面全体Nパスブレンドでの再利用のため
    // メンバー変数として保持する。GPUパーティクルはidSeedによる位相分離を持たないため、
    // 再利用時もバッファ自体の再構築は不要）
    auto &gpuParticleBatches = shadowGpuParticleBatches_;
    {
        std::uint32_t emitterIndex = 0;
        for (auto *emitter : sceneRenderer->GetGpuParticleEmitters()) {
            if (!emitter || !emitter->IsActive() || !emitter->IsGPUSimulation() || !emitter->GetCastShadows()) continue;

            const auto meshHandle = emitter->GetMeshHandle();
            if (meshHandle == ModelManager::kInvalidHandle) continue;
            const auto *meshBuffers = resourceContainer_->GetOrCreateMeshBuffers(meshHandle);
            if (!meshBuffers || !meshBuffers->vertexBuffer || !meshBuffers->indexBuffer) continue;

            auto *instanceMatrixBuffer = emitter->GetGpuInstanceMatrixBuffer(Passkey<Renderer>{});
            if (!instanceMatrixBuffer) continue;
            const std::uint32_t instanceCount = emitter->GetGpuParticleCapacity(Passkey<Renderer>{});
            if (instanceCount == 0) continue;

            PreparedGpuParticleShadowBatch batch;
            batch.meshBuffers = meshBuffers;
            batch.transformBuffer = instanceMatrixBuffer;
            batch.instanceCount = instanceCount;

            auto *material = MaterialManager::GetMaterial(emitter->GetMaterialHandle());
            if (material) {
                material->ResolveTextureHandles();
            }
            const auto &shadowPipelineInfo = pipelineManager_->GetPipeline(kShadowPipelineName);
            auto elementTemplate = BuildMaterialElementBytes(shadowPipelineInfo, material);
            const std::uint32_t materialStride = shadowPipelineInfo.GetMaterialLayout().totalByteSize;
            if (materialStride > 0) {
                WriteMaterialField(shadowPipelineInfo, elementTemplate.data(), materialStride,
                    "textureIndex", ResolveInstanceTextureIndex(TextureManager::kInvalidHandle, material));
                WriteMaterialField(shadowPipelineInfo, elementTemplate.data(), materialStride,
                    "samplerIndex", ResolveInstanceSamplerIndex(SamplerManager::kInvalidHandle, material));
            }
            char key[64];
            std::snprintf(key, sizeof(key), "ShadowPass|gpuParticle|%u|material", emitterIndex);
            if (materialStride > 0) {
                batch.materialBuffer = resourceContainer_->GetOrCreateStructuredBuffer(key, materialStride, instanceCount);
                if (batch.materialBuffer) {
                    if (auto *mapped = static_cast<std::byte *>(batch.materialBuffer->Map())) {
                        for (std::uint32_t i = 0; i < instanceCount; ++i) {
                            std::memcpy(mapped + static_cast<size_t>(i) * materialStride, elementTemplate.data(), materialStride);
                        }
                    }
                }
            }

            if (batch.materialBuffer) {
                gpuParticleBatches.push_back(batch);
                ++emitterIndex;
            }
        }
    }

    //--------- シャドウマップ配列を深度書き込み状態にして全スライスを一括クリア ---------//
    shadowMapArray_->SetCommandList(commandList);
    shadowMapArray_->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE);
    {
        const auto fullDsv = shadowMapArray_->GetCPUDescriptorHandle();
        commandList->OMSetRenderTargets(0, nullptr, FALSE, &fullDsv);
        shadowMapArray_->ClearDepthStencilView();
    }
    {
        const float resolutionF = static_cast<float>(resolution);
        D3D12_VIEWPORT viewport{ 0.0f, 0.0f, resolutionF, resolutionF, 0.0f, 1.0f };
        D3D12_RECT scissor{ 0, 0, static_cast<LONG>(resolution), static_cast<LONG>(resolution) };
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissor);
    }

    //--------- 影ジョブ × スライス数だけシャドウマップ描画パスを回す ---------//
    for (size_t jobIndex = 0; jobIndex < shadowJobs_.size(); ++jobIndex) {
        const auto &job = shadowJobs_[jobIndex];
        for (std::uint32_t s = 0; s < job.sliceCount; ++s) {
            const auto dsv = shadowMapArray_->GetSliceDsvHandle(job.baseSlice + s);
            commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);

            // ライトカメラの定数バッファ（ShadowMapVS が gCamera3D.viewProjection を参照する）
            char cameraKey[64];
            std::snprintf(cameraKey, sizeof(cameraKey), "ShadowPass|%zu|%u|camera", jobIndex, s);
            auto *cameraBuffer = resourceContainer_->GetOrCreateConstantBuffer(cameraKey, sizeof(LightCameraConstantData));
            if (!cameraBuffer) continue;
            if (auto *mapped = cameraBuffer->Map()) {
                LightCameraConstantData constant;
                constant.viewProjection = job.viewProjections[s];
                // シャドウマップPS側のディザ閾値計算で、カメラの代わりに距離の基準として使う
                constant.eyePosition = Vector4(job.eyePositions[s].x, job.eyePositions[s].y, job.eyePositions[s].z, 1.0f);
                std::memcpy(mapped, &constant, sizeof(constant));
            }

            // このスライスの視錐台を抽出し、完全に外側にあるバッチは描画自体をスキップする
            const auto frustumPlanes = ExtractFrustumPlanes(job.viewProjections[s]);

            for (const auto &batch : batches) {
                if (!SphereIntersectsFrustum(frustumPlanes, batch.boundsCenter, batch.boundsRadius)) {
                    continue;
                }
                shaderBinder.Bind("Vertex:gCamera3D", cameraBuffer);
                shaderBinder.Bind("Vertex:gTransformationMatrices", batch.transformBuffer);
                // 位相0（通常描画）のidSeedバッファを都度取得・構築する（RenderShadowMapsPhaseInto参照）
                if (auto *idSeedBuffer = BuildShadowIdSeedBuffer(batch, 0.0f, -1)) {
                    shaderBinder.Bind("Vertex:gObjectIdSeeds", idSeedBuffer);
                }
                shaderBinder.Bind("Pixel:gMaterials", batch.materialBuffer);

                if (batch.skinnedVertexBuffer) {
                    batch.skinnedVertexBuffer->SetCommandList(commandList);
                    D3D12_VERTEX_BUFFER_VIEW skinnedView = batch.skinnedVertexBuffer->GetView(sizeof(ResourceContainer::MeshVertex));
                    pipelineBinder.SetVertexBufferView(0, 1, &skinnedView);
                } else {
                    pipelineBinder.SetVertexBuffer(batch.meshBuffers->vertexBuffer.get(), sizeof(ResourceContainer::MeshVertex));
                }
                pipelineBinder.SetIndexBuffer(batch.meshBuffers->indexBuffer.get());
                const std::uint32_t drawIndexCount = batch.indexCount > 0 ? batch.indexCount : batch.meshBuffers->indexCount;
                commandList->DrawIndexedInstanced(drawIndexCount, batch.instanceCount, batch.indexStart, 0, 0);
                ++drawCallCount_;
            }

            for (const auto &batch : gpuParticleBatches) {
                shaderBinder.Bind("Vertex:gCamera3D", cameraBuffer);
                batch.transformBuffer->SetCommandList(commandList);
                shaderBinder.Bind("Vertex:gTransformationMatrices", batch.transformBuffer);
                shaderBinder.Bind("Pixel:gMaterials", batch.materialBuffer);

                pipelineBinder.SetVertexBuffer(batch.meshBuffers->vertexBuffer.get(), sizeof(ResourceContainer::MeshVertex));
                pipelineBinder.SetIndexBuffer(batch.meshBuffers->indexBuffer.get());
                // 死んでいるパーティクルはgpuInstanceMatrixBuffer_内でスケール0の行列になっているため、
                // 追加のカリング無しでcapacity件（常にMax Particles分）そのままインスタンス描画する
                commandList->DrawIndexedInstanced(batch.meshBuffers->indexCount, batch.instanceCount, 0, 0, 0);
                ++drawCallCount_;
            }
        }
    }

    //--------- 配列全体をシェーダーから参照可能な状態へ遷移して記録終了 ---------//
    shadowMapArray_->TransitionToShaderResource();
    if (commands->EndRecord()) {
        directXCommon_->AddRecordCommandList(Passkey<Renderer>{}, commands->GetCommandList());
        shadowArrayReady_ = true;
    } else {
        shadowJobs_.clear();
        targetShadowEntries_.clear();
    }
}

void Renderer::RenderShadowMapsPhaseInto(ScreenBuffer *screenBuffer, PipelineBinder &pipelineBinder,
    float phaseOffset, int passIndex) {
    if (!screenBuffer || !shadowMapArray_ || shadowJobs_.empty() || shadowResolutionForRedraw_ == 0) return;
    auto *commandList = screenBuffer->GetCommandList();
    if (!commandList || !pipelineManager_) return;

    constexpr const char *kShadowPipelineName = "Object3D.ShadowMap.DepthOnly";
    if (!pipelineManager_->HasPipeline(kShadowPipelineName)) return;

    pipelineBinder.UsePipeline(kShadowPipelineName);
    auto &shaderBinder = pipelineManager_->GetShaderVariableBinder(Passkey<Renderer>{}, kShadowPipelineName);
    shaderBinder.SetCommandList(commandList);

    // ブルーノイズ閾値テーブル・時刻定数・バインドレステクスチャ配列は、このコマンドリスト上では
    // まだ一度もバインドしていないため、RenderShadowMaps本体と同様にここで明示的にバインドし直す
    // 必要がある（バインドはコマンドリスト単位）
    if (blueNoiseGenerator_.IsReady()) {
        shaderBinder.Bind("Pixel:gBlueNoiseDither", blueNoiseGenerator_.GetResultBuffer());
    }
    if (auto *timeBuffer = resourceContainer_->GetOrCreateConstantBuffer("ShadowTimeConstants", sizeof(TimeConstantsData))) {
        shaderBinder.Bind("Vertex:TimeConstants", timeBuffer);
        shaderBinder.Bind("Pixel:TimeConstants", timeBuffer);
    }
    shaderBinder.Bind("Pixel:gTextures", directXCommon_->GetTextureBindlessBaseHandleForRenderer(Passkey<Renderer>{}));
    shaderBinder.Bind("Pixel:gSamplers", directXCommon_->GetSamplerBindlessBaseHandleForRenderer(Passkey<Renderer>{}));

    //--------- シャドウマップ配列を深度書き込み状態にして全スライスを一括クリアし、この位相で撮り直す ---------//
    shadowMapArray_->SetCommandList(commandList);
    shadowMapArray_->TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE);
    {
        const auto fullDsv = shadowMapArray_->GetCPUDescriptorHandle();
        commandList->OMSetRenderTargets(0, nullptr, FALSE, &fullDsv);
        shadowMapArray_->ClearDepthStencilView();
    }
    {
        const float resolutionF = static_cast<float>(shadowResolutionForRedraw_);
        D3D12_VIEWPORT viewport{ 0.0f, 0.0f, resolutionF, resolutionF, 0.0f, 1.0f };
        D3D12_RECT scissor{ 0, 0, static_cast<LONG>(shadowResolutionForRedraw_), static_cast<LONG>(shadowResolutionForRedraw_) };
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissor);
    }

    for (size_t jobIndex = 0; jobIndex < shadowJobs_.size(); ++jobIndex) {
        const auto &job = shadowJobs_[jobIndex];
        for (std::uint32_t s = 0; s < job.sliceCount; ++s) {
            const auto dsv = shadowMapArray_->GetSliceDsvHandle(job.baseSlice + s);
            commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);

            char cameraKey[64];
            std::snprintf(cameraKey, sizeof(cameraKey), "ShadowPass|%zu|%u|camera", jobIndex, s);
            auto *cameraBuffer = resourceContainer_->GetOrCreateConstantBuffer(cameraKey, sizeof(LightCameraConstantData));
            if (!cameraBuffer) continue;
            // カメラ自体はジョブ構築時（RenderShadowMaps）に既にこのキーで書き込み済みのため再Mapは不要

            const auto frustumPlanes = ExtractFrustumPlanes(job.viewProjections[s]);

            for (const auto &batch : shadowBatches_) {
                if (!SphereIntersectsFrustum(frustumPlanes, batch.boundsCenter, batch.boundsRadius)) {
                    continue;
                }
                shaderBinder.Bind("Vertex:gCamera3D", cameraBuffer);
                shaderBinder.Bind("Vertex:gTransformationMatrices", batch.transformBuffer);
                if (auto *idSeedBuffer = BuildShadowIdSeedBuffer(batch, phaseOffset, passIndex)) {
                    shaderBinder.Bind("Vertex:gObjectIdSeeds", idSeedBuffer);
                }
                shaderBinder.Bind("Pixel:gMaterials", batch.materialBuffer);

                if (batch.skinnedVertexBuffer) {
                    batch.skinnedVertexBuffer->SetCommandList(commandList);
                    D3D12_VERTEX_BUFFER_VIEW skinnedView = batch.skinnedVertexBuffer->GetView(sizeof(ResourceContainer::MeshVertex));
                    pipelineBinder.SetVertexBufferView(0, 1, &skinnedView);
                } else {
                    pipelineBinder.SetVertexBuffer(batch.meshBuffers->vertexBuffer.get(), sizeof(ResourceContainer::MeshVertex));
                }
                pipelineBinder.SetIndexBuffer(batch.meshBuffers->indexBuffer.get());
                const std::uint32_t drawIndexCount = batch.indexCount > 0 ? batch.indexCount : batch.meshBuffers->indexCount;
                commandList->DrawIndexedInstanced(drawIndexCount, batch.instanceCount, batch.indexStart, 0, 0);
                ++drawCallCount_;
            }

            for (const auto &batch : shadowGpuParticleBatches_) {
                shaderBinder.Bind("Vertex:gCamera3D", cameraBuffer);
                batch.transformBuffer->SetCommandList(commandList);
                shaderBinder.Bind("Vertex:gTransformationMatrices", batch.transformBuffer);
                shaderBinder.Bind("Pixel:gMaterials", batch.materialBuffer);

                pipelineBinder.SetVertexBuffer(batch.meshBuffers->vertexBuffer.get(), sizeof(ResourceContainer::MeshVertex));
                pipelineBinder.SetIndexBuffer(batch.meshBuffers->indexBuffer.get());
                commandList->DrawIndexedInstanced(batch.meshBuffers->indexCount, batch.instanceCount, 0, 0, 0);
                ++drawCallCount_;
            }
        }
    }

    shadowMapArray_->TransitionToShaderResource();
}

} // namespace KashipanEngine
