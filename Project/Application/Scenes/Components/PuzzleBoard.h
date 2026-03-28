#pragma once

#include <KashipanEngine.h>

#include <cmath>
#include <numbers>
#include <vector>

namespace Application {

enum class PuzzleBlockType {
    Red,
    Blue,
};

enum class PuzzleBlockDirection {
    Up,
    Down,
};

struct PuzzleBlockData {
    KashipanEngine::Triangle2D *triangle = nullptr;
    PuzzleBlockType type = PuzzleBlockType::Red;
    PuzzleBlockDirection direction = PuzzleBlockDirection::Up;
};

class PuzzleBoard final : public KashipanEngine::ISceneComponent {
public:
    PuzzleBoard()
        : ISceneComponent("PuzzleBoard", 2) {}
    ~PuzzleBoard() override = default;

    void Initialize() override {
        ClearBoardObjects();
        boardData_.clear();
        boardRootObject_ = nullptr;
        boardRootTransform_ = nullptr;
        (void)SetBoardSize(kDefaultBoardWidth, kDefaultBoardHeight);
    }

    void Finalize() override {
        ClearBoardObjects();
        boardData_.clear();
        boardRootObject_ = nullptr;
        boardRootTransform_ = nullptr;
    }

    void Update() override {
        const auto *ctx = GetOwnerContext();
        if (!ctx) return;
        if (ctx->GetSceneVariableOr<bool>("IsPuzzleStop", true)) return;
    }

    bool SetBoardSize(int width, int height) {
        if (width <= 0 || height <= 0) {
            return false;
        }

        boardWidth_ = width;
        boardHeight_ = height;

        boardData_.assign(static_cast<size_t>(boardHeight_), std::vector<PuzzleBlockData>(static_cast<size_t>(boardWidth_)));

        for (int y = 0; y < boardHeight_; ++y) {
            for (int x = 0; x < boardWidth_; ++x) {
                boardData_[static_cast<size_t>(y)][static_cast<size_t>(x)].direction = ResolveDirection(x, y);
            }
        }

        RebuildBoardBackground();
        return true;
    }

    bool SetBlockData(int x, int y, const PuzzleBlockData &data) {
        if (!IsInside(x, y)) {
            return false;
        }

        if (data.direction != boardData_[static_cast<size_t>(y)][static_cast<size_t>(x)].direction) {
            if (data.triangle) {
                RemoveObject2D(data.triangle);
            }
            return false;
        }

        if (boardData_[static_cast<size_t>(y)][static_cast<size_t>(x)].triangle != nullptr) {
            if (data.triangle) {
                RemoveObject2D(data.triangle);
            }
            return false;
        }

        boardData_[static_cast<size_t>(y)][static_cast<size_t>(x)] = data;
        return true;
    }

    void ResetBoardData() {
        for (auto &row : boardData_) {
            for (auto &cell : row) {
                if (cell.triangle) {
                    RemoveObject2D(cell.triangle);
                }
                cell = PuzzleBlockData{};
            }
        }

        boardData_.clear();
        boardWidth_ = 0;
        boardHeight_ = 0;
        ClearBoardObjects();
    }

    KashipanEngine::Transform2D *GetBoardRootTransform() const { return boardRootTransform_; }
    KashipanEngine::ScreenBuffer *GetScreenBuffer2D() const { return screenBuffer2D_; }

    int GetBoardWidth() const { return boardWidth_; }
    int GetBoardHeight() const { return boardHeight_; }

    bool IsInside(int x, int y) const {
        return x >= 0 && y >= 0 && x < boardWidth_ && y < boardHeight_;
    }

    bool IsCellEmpty(int x, int y) const {
        if (!IsInside(x, y)) return false;
        return boardData_[static_cast<size_t>(y)][static_cast<size_t>(x)].triangle == nullptr;
    }

    PuzzleBlockDirection GetCellDirection(int x, int y) const {
        if (!IsInside(x, y)) return PuzzleBlockDirection::Up;
        return boardData_[static_cast<size_t>(y)][static_cast<size_t>(x)].direction;
    }

    Vector3 GetCellLocalPosition(int x, int y) const {
        const float centerX = static_cast<float>(boardWidth_ - 1) * 0.5f;
        const float centerY = static_cast<float>(boardHeight_ - 1) * 0.5f;
        const float px = (static_cast<float>(x) - centerX) * GetStepX();
        const float py = (centerY - static_cast<float>(y)) * GetStepY();
        return Vector3(px, py, 0.0f);
    }

    float GetTriangleRadius() const { return kTriangleRadius; }
    float GetTriangleMargin() const { return kTriangleMargin; }
    float GetStepX() const {
        const float triangleWidth = std::sqrt(3.0f) * kTriangleRadius;
        return (triangleWidth * 0.5f) + kTriangleMargin;
    }
    float GetStepY() const {
        const float triangleHeight = 1.5f * kTriangleRadius;
        return triangleHeight + kTriangleMargin;
    }

    const std::vector<std::vector<PuzzleBlockData>> &GetBoardData() const { return boardData_; }

    static Vector4 GetColorByType(PuzzleBlockType type, float alpha = 1.0f) {
        if (type == PuzzleBlockType::Blue) {
            return Vector4(0.2f, 0.45f, 1.0f, alpha);
        }
        return Vector4(1.0f, 0.25f, 0.25f, alpha);
    }

    static void ApplyEquilateralTriangleVertices(KashipanEngine::Triangle2D *triangle) {
        if (!triangle) {
            return;
        }

        auto vertices = triangle->GetVertexData<KashipanEngine::VertexData2D>();
        if (vertices.size() < 3) {
            return;
        }

        const float baseAngle = std::numbers::pi_v<float> * 0.5f;
        const float stepAngle = (2.0f * std::numbers::pi_v<float>) / 3.0f;

        for (size_t i = 0; i < 3; ++i) {
            const float angle = baseAngle + stepAngle * static_cast<float>(i);
            vertices[i].position = Vector4(std::cos(angle), std::sin(angle), 0.0f, 1.0f);
        }

        float minY = vertices[0].position.y;
        float maxY = vertices[0].position.y;
        for (size_t i = 1; i < 3; ++i) {
            minY = (std::min)(minY, vertices[i].position.y);
            maxY = (std::max)(maxY, vertices[i].position.y);
        }

        const float triangleHeight = maxY - minY;
        const float targetCenterY = triangleHeight * 0.5f;
        const float yOffset = targetCenterY - 1.0f;

        for (size_t i = 0; i < 3; ++i) {
            vertices[i].position.y += yOffset;
        }
    }

private:
    static constexpr int kDefaultBoardWidth = 8;
    static constexpr int kDefaultBoardHeight = 6;
    static constexpr float kTriangleRadius = 32.0f;
    static constexpr float kTriangleMargin = kTriangleRadius * 0.1f;

    void RebuildBoardBackground() {
        auto *ctx = GetOwnerContext();
        if (!ctx) {
            return;
        }

        ClearBoardObjects();
        EnsureBoardRoot();

        if (!boardRootTransform_ || !screenBuffer2D_) {
            return;
        }

        for (int y = 0; y < boardHeight_; ++y) {
            for (int x = 0; x < boardWidth_; ++x) {
                auto triangle = std::make_unique<KashipanEngine::Triangle2D>();
                auto *trianglePtr = triangle.get();
                triangle->SetName("PuzzleBoardBackground");
                triangle->SetUniqueBatchKey();
                triangle->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
                ApplyEquilateralTriangleVertices(triangle.get());

                if (auto *material = triangle->GetComponent2D<KashipanEngine::Material2D>()) {
                    material->SetColor(Vector4(0.5f, 0.5f, 0.5f, 0.5f));
                }

                if (auto *transform = triangle->GetComponent2D<KashipanEngine::Transform2D>()) {
                    transform->SetTranslate(GetCellLocalPosition(x, y));
                    transform->SetScale(Vector3(kTriangleRadius, kTriangleRadius, 1.0f));
                    transform->SetRotate(Vector3(0.0f, 0.0f, ResolveDirection(x, y) == PuzzleBlockDirection::Up ? 0.0f : std::numbers::pi_v<float>));
                    transform->SetParentTransform(boardRootTransform_);
                }

                if (!ctx->AddObject2D(std::move(triangle))) {
                    return;
                }

                boardObjects_.push_back(trianglePtr);
            }
        }
    }

    void EnsureBoardRoot() {
        if (boardRootTransform_) {
            return;
        }

        auto *ctx = GetOwnerContext();
        if (!ctx) {
            return;
        }

        if (!screenBuffer2D_) {
            if (auto *sceneDefaultVariables = ctx->GetComponent<KashipanEngine::SceneDefaultVariables>()) {
                screenBuffer2D_ = sceneDefaultVariables->GetScreenBuffer2D();
            }
        }

        if (!screenBuffer2D_) {
            return;
        }

        auto root = std::make_unique<KashipanEngine::Triangle2D>();
        auto *rootPtr = root.get();
        root->SetName("PuzzleBoardRoot");
        root->SetUniqueBatchKey();
        root->AttachToRenderer(screenBuffer2D_, "Object2D.DoubleSidedCulling.BlendNormal");
        ApplyEquilateralTriangleVertices(root.get());

        if (auto *material = root->GetComponent2D<KashipanEngine::Material2D>()) {
            material->SetColor(Vector4(1.0f, 1.0f, 1.0f, 0.0f));
        }

        if (auto *transform = root->GetComponent2D<KashipanEngine::Transform2D>()) {
            transform->SetScale(Vector3(1.0f, 1.0f, 1.0f));
            boardRootTransform_ = transform;
        }

        if (ctx->AddObject2D(std::move(root))) {
            boardRootObject_ = rootPtr;
            boardObjects_.push_back(rootPtr);
        }
    }

    void RemoveObject2D(KashipanEngine::Object2DBase *obj) {
        auto *ctx = GetOwnerContext();
        if (!ctx || !obj) return;
        (void)ctx->RemoveObject2D(obj);
    }

    void ClearBoardObjects() {
        auto *ctx = GetOwnerContext();
        if (!ctx) {
            boardObjects_.clear();
            return;
        }

        for (auto *obj : boardObjects_) {
            if (obj) {
                ctx->RemoveObject2D(obj);
            }
        }

        boardObjects_.clear();
        boardRootObject_ = nullptr;
        boardRootTransform_ = nullptr;
    }

    static PuzzleBlockDirection ResolveDirection(int x, int y) {
        return ((x + y) % 2 == 0) ? PuzzleBlockDirection::Up : PuzzleBlockDirection::Down;
    }

    std::vector<std::vector<PuzzleBlockData>> boardData_;
    int boardWidth_ = 0;
    int boardHeight_ = 0;

    KashipanEngine::ScreenBuffer *screenBuffer2D_ = nullptr;
    KashipanEngine::Object2DBase *boardRootObject_ = nullptr;
    KashipanEngine::Transform2D *boardRootTransform_ = nullptr;
    std::vector<KashipanEngine::Object2DBase *> boardObjects_;
};

} // namespace Application
