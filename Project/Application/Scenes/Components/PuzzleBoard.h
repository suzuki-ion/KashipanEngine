#pragma once

#include <KashipanEngine.h>

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
    PuzzleBoard();
    ~PuzzleBoard() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

    bool SetBoardSize(int width, int height);
    bool SetBlockData(int x, int y, const PuzzleBlockData &data);
    void ResetBoardData();

    KashipanEngine::Transform2D *GetBoardRootTransform() const;
    KashipanEngine::ScreenBuffer *GetScreenBuffer2D() const;

    int GetBoardWidth() const;
    int GetBoardHeight() const;

    bool IsInside(int x, int y) const;
    bool IsCellEmpty(int x, int y) const;
    PuzzleBlockDirection GetCellDirection(int x, int y) const;
    Vector3 GetCellLocalPosition(int x, int y) const;

    float GetTriangleRadius() const;
    float GetTriangleMargin() const;
    float GetStepX() const;
    float GetStepY() const;

    const std::vector<std::vector<PuzzleBlockData>> &GetBoardData() const;

    static Vector4 GetColorByType(PuzzleBlockType type, float alpha = 1.0f);
    static void ApplyEquilateralTriangleVertices(KashipanEngine::Triangle2D *triangle);

private:
    static constexpr int kDefaultBoardWidth = 8;
    static constexpr int kDefaultBoardHeight = 6;
    static constexpr float kTriangleRadius = 32.0f;
    static constexpr float kTriangleMargin = kTriangleRadius * 0.1f;

    void RebuildBoardBackground();
    void EnsureBoardRoot();
    void RemoveObject2D(KashipanEngine::Object2DBase *obj);
    void DebugWriteCell(int x, int y, PuzzleBlockType type);
    void ClearBoardObjects();

    static PuzzleBlockDirection ResolveDirection(int x, int y);

    std::vector<std::vector<PuzzleBlockData>> boardData_;
    int boardWidth_ = 0;
    int boardHeight_ = 0;

    KashipanEngine::ScreenBuffer *screenBuffer2D_ = nullptr;
    KashipanEngine::Object2DBase *boardRootObject_ = nullptr;
    KashipanEngine::Transform2D *boardRootTransform_ = nullptr;
    std::vector<KashipanEngine::Object2DBase *> boardObjects_;

#if defined(USE_IMGUI)
    PuzzleBlockType debugEditType_ = PuzzleBlockType::Red;
#endif
};

} // namespace Application
